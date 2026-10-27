"""Control design: linearise a model and test controllability."""

from __future__ import annotations

import ctypes
import re
import tempfile
import warnings
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

__all__ = ["Linearization", "linearize"]


@dataclass
class Linearization:
    """The linear system dx = A dx + B du around a point of a model.

    ``A`` is the Jacobian of the right-hand side (for a map, of the map),
    ``B`` has one column per input parameter. ``controllability`` is
    [B, AB, ..., A^(n-1) B]; the pair is ``controllable`` when its rank is n.
    ``rank`` is -1 when it cannot be computed (A^(n-1) B overflows, which
    can happen for models with very many states).
    """

    point: dict[str, float]
    inputs: list[str]
    A: np.ndarray
    B: np.ndarray
    eigenvalues: np.ndarray
    controllability: np.ndarray
    rank: int
    controllable: bool
    residual: float  # largest |f| at the point: ~0 at an equilibrium
    discrete: bool = False
    A_error: np.ndarray | None = None  # error estimates of A and B (NaN with step=)
    B_error: np.ndarray | None = None
    singular_values: np.ndarray | None = None  # of the controllability matrix
    _model: object = field(default=None, repr=False)
    _x0: np.ndarray | None = field(default=None, repr=False)
    _p0: np.ndarray | None = field(default=None, repr=False)

    def error(self, dx, du=None) -> float:
        """Relative error of the linear model for one perturbation.

        Compares the model's right-hand side at (x0 + dx, p0 + du) with the
        prediction A dx + B du, and returns |difference| / |prediction|.
        This is the quantity that decides whether a controller designed on
        (A, B) will behave on the real model."""
        m = self._model
        dx = np.asarray(dx, dtype=float).ravel()
        du = np.zeros(len(self.inputs)) if du is None else np.asarray(du, float).ravel()
        saved = {p: m.get(p) for p in self.inputs}
        base = m.rhs(self._x0)
        try:
            m.set(**{p: v + d for p, v, d in zip(self.inputs, self._p0, du)})
            actual = m.rhs(self._x0 + dx) - base
        finally:
            m.set(**saved)
        predicted = self.A @ dx + self.B @ du
        # the linear part at the point itself may be tiny: compare absolutely then
        return float(np.linalg.norm(actual - predicted)
                     / max(np.linalg.norm(predicted), 1e-300))

    def valid_radius(self, tol: float = 0.05, directions: int = 32,
                     seed: int = 0) -> float:
        """Size of the perturbation (a norm in state space, with the inputs
        scaled the same way) for which the linear model keeps its relative
        error below ``tol`` in every one of a fixed set of random directions.

        Found by scanning outwards in factors of two, then bisecting. A
        controller designed on (A, B) can be trusted within about this
        distance of the point; beyond it, use the nonlinear model."""
        rng = np.random.default_rng(seed)
        n = len(self._x0)
        k = len(self.inputs)
        dirs = rng.standard_normal((directions, n + k))
        dirs /= np.linalg.norm(dirs, axis=1)[:, None]

        def worst(r):
            return max(self.error(d[:n] * r, d[n:] * r) for d in dirs)

        lo = hi = 1e-6
        if worst(lo) > tol:
            return 0.0
        while worst(hi) <= tol and hi < 1e6:
            lo, hi = hi, hi * 2
        if hi >= 1e6:
            return float("inf")
        for _ in range(30):
            mid = 0.5 * (lo + hi)
            if worst(mid) <= tol:
                lo = mid
            else:
                hi = mid
        return lo

    def to_csv(self, path) -> None:
        """Write the linearisation as CSV, every number exact: one row per
        state with its value at the point, its row of A and of B, and an
        eigenvalue; the rank and the residual on the first row. The desktop
        app's ``export FILE.csv analysis`` after ``linearize`` writes the
        same file."""
        from .analysis import write_csv
        names = list(self.point)
        cols = (["state", "at"] + [f"A_{v}" for v in names]
                + [f"B_{p}" for p in self.inputs]
                + ["eig_re", "eig_im", "rank", "residual"])
        rows = []
        for i, v in enumerate(names):
            ev = complex(self.eigenvalues[i])
            rows.append([v, float(self.point[v])] + [float(a) for a in self.A[i]]
                        + [float(b) for b in self.B[i]]
                        + [ev.real, ev.imag,
                           int(self.rank) if i == 0 else None,
                           float(self.residual) if i == 0 else None])
        write_csv(path, cols, rows)

    def plot(self, ax=None, **kw):
        """The eigenvalues in the complex plane, as the desktop app shows
        them: the stable region shaded (the left half-plane; for a map, the
        inside of the unit circle) and its edge drawn."""
        import matplotlib.pyplot as plt
        ax = ax or plt.gca()
        ev = np.asarray(self.eigenvalues, dtype=complex)
        x0, x1, y0, y1 = _complex_plane_window(ev, self.discrete)
        if self.discrete:
            t = np.linspace(0, 2 * np.pi, 257)
            ax.fill(np.cos(t), np.sin(t), color="tab:green", alpha=0.12, lw=0,
                    label="stable (inside the unit circle)")
            ax.plot(np.cos(t), np.sin(t), color="0.45", lw=1)
        else:
            ax.axvspan(x0, 0, color="tab:green", alpha=0.12, lw=0,
                       label="stable (negative real part)")
            ax.axvline(0, color="0.45", lw=1)
        ax.axhline(0, color="0.75", lw=0.8)
        style = {"marker": "x", "ls": "none", "color": "tab:red", "ms": 9, "mew": 2}
        style.update(kw)
        ax.plot(ev.real, ev.imag, label="eigenvalues", **style)
        ax.set_xlim(x0, x1)
        ax.set_ylim(y0, y1 + 0.25 * (y1 - y0))  # room for the legend on top
        if self.discrete:
            ax.set_aspect("equal", adjustable="box")  # a round unit circle
        ax.set_xlabel("real part")
        ax.set_ylabel("imaginary part")
        ax.legend(loc="upper center", ncol=2, fontsize="small", frameon=False)
        return ax

    def place_poles(self, poles) -> np.ndarray:
        """Feedback gain K (inputs x states) so that A - B K has ``poles``.

        With the control law du = -K dx the closed loop dx' = (A - B K) dx
        has the requested eigenvalues. One input uses Ackermann's formula
        with the controllability matrix; several inputs use the robust
        method of Kautsky, Nichols and Van Dooren (the one behind
        ``scipy.signal.place_poles``), which spends the spare inputs on
        keeping the closed-loop eigenvectors well conditioned. Only NumPy is
        needed. The poles are n numbers, complex ones in conjugate pairs; for
        a map they belong inside the unit circle.

        Raises ``ValueError`` when the pair (A, B) is not controllable, when
        there are no inputs, or when the poles are not admissible (wrong
        count, not conjugate pairs, a pole repeated more often than there
        are inputs)."""
        n, k = self.B.shape
        if k == 0:
            raise ValueError("cannot place poles: no inputs were chosen "
                             "(linearize(inputs=[...]))")
        if self.rank < 0:
            raise ValueError("cannot place poles: the controllability rank "
                             "could not be computed")
        if not self.controllable:
            raise ValueError(
                f"cannot place poles: the pair (A, B) is not controllable "
                f"(rank {self.rank} of {n} states); the poles of the "
                "uncontrollable part cannot be moved")
        poles = np.atleast_1d(np.asarray(poles, dtype=complex)).ravel()
        if len(poles) != n:
            raise ValueError(f"need {n} poles, one per state, got {len(poles)}")
        if not np.all(np.isfinite(poles)):
            raise ValueError("the poles must be finite numbers")
        _check_conjugate(poles)
        K = self._ackermann(poles) if k == 1 else self._kautsky(poles)
        got = np.linalg.eigvals(self.A - self.B @ K)
        scale = max(1.0, float(np.max(np.abs(poles))))
        err = _match_distance(got, poles)
        if err > 1e-6 * scale:
            warnings.warn(f"the placed poles differ from the requested ones by "
                          f"{err:.1e}: the problem is badly conditioned",
                          stacklevel=2)
        elif np.linalg.norm(K) > 1e6 * scale:
            warnings.warn("the gain is very large (norm "
                          f"{np.linalg.norm(K):.1e}): the pair is nearly "
                          "uncontrollable", stacklevel=2)
        return K

    def _ackermann(self, poles) -> np.ndarray:
        n = self.A.shape[0]
        coef = np.poly(poles).real
        phi = np.zeros((n, n))
        for c in coef:  # Horner: phi(A)
            phi = phi @ self.A + c * np.eye(n)
        C = self.controllability[:, :n]
        last = np.zeros(n)
        last[-1] = 1.0
        return np.linalg.solve(C.T, last)[None, :] @ phi

    def _kautsky(self, poles) -> np.ndarray:
        A, B = self.A, self.B
        n, k = B.shape
        # B spans r <= min(n, k) directions; with more inputs than states
        # (or dependent inputs) the extra columns add nothing to place
        U, sv, _ = np.linalg.svd(B)
        r = int(np.sum(sv > max(n, k) * np.finfo(float).eps * sv[0]))
        U0, U1 = U[:, :r], U[:, r:]
        Z = U0.T @ B  # r x k
        # second[j] is True for the conjugate partner of an earlier pole
        partner = [-1] * n
        second = [False] * n
        for j in range(n):
            if second[j] or poles[j].imag == 0:
                continue
            for i in range(j + 1, n):
                if not second[i] and abs(poles[i] - poles[j].conjugate()) \
                        <= 1e-12 * max(1.0, abs(poles[j])):
                    partner[j], second[i] = i, True
                    break
        # null spaces of U1^T (A - lambda I): the allowed eigenvector directions
        S = []
        for lam in poles:
            M = U1.T @ (A - lam * np.eye(n))
            if M.shape[0] == 0:  # B has full row rank: any direction will do
                S.append(np.eye(n, dtype=complex))
                continue
            _, _, vh = np.linalg.svd(M)
            S.append(vh[M.shape[0]:].conj().T)  # n x r basis
        X = np.zeros((n, n), dtype=complex)
        for j in range(n):
            if second[j]:
                continue
            X[:, j] = S[j][:, 0] / np.linalg.norm(S[j][:, 0])
            if partner[j] >= 0:
                X[:, partner[j]] = X[:, j].conj()
        for _ in range(50):
            moved = 0.0
            for j in range(n):
                if second[j]:
                    continue
                drop = [j] + ([partner[j]] if partner[j] >= 0 else [])
                rest = np.delete(X, drop, axis=1)
                # directions orthogonal to the other eigenvectors, projected into S_j
                u, _, _ = np.linalg.svd(rest, full_matrices=True)
                cand = S[j] @ (S[j].conj().T @ u[:, rest.shape[1]:])
                norms = np.linalg.norm(cand, axis=0)
                order = np.argsort(-norms)
                col = cand[:, order[0]]
                if partner[j] >= 0 and len(order) > 1:
                    # a complex pair needs a genuinely complex vector, or
                    # it and its conjugate would be parallel
                    col = col + 1j * cand[:, order[1]]
                nrm = np.linalg.norm(col)
                if nrm < 1e-14:
                    continue
                col = col / nrm
                col = col * np.exp(-1j * np.angle(col[int(np.argmax(np.abs(col)))]))
                moved = max(moved, float(np.linalg.norm(col - X[:, j])))
                X[:, j] = col
                if partner[j] >= 0:
                    X[:, partner[j]] = col.conj()
            if moved < 1e-10:
                break
        if np.linalg.cond(X) > 1e12:
            raise ValueError("cannot place these poles: a pole is repeated more "
                             "often than there are inputs")
        M = X @ np.diag(poles) @ np.linalg.inv(X)  # = A - B K
        K = -np.linalg.pinv(Z) @ (U0.T @ (M - A))  # B (-K) = M - A
        if np.max(np.abs(K.imag)) > 1e-8 * max(1.0, float(np.max(np.abs(K.real)))):
            raise ValueError("cannot place these poles: the gain is not real")
        return K.real

    def closed_loop(self, K, path=None):
        """Write the closed-loop model, u = u0 - K (x - x*), and load it.

        Each input parameter is removed from the model's parameter lines and
        defined instead as a fixed variable: its nominal value minus the row
        of ``K`` times the distance of the states from the linearisation
        point. The rest of the model text is kept as it is. The file goes to
        ``path`` (a temporary directory by default); the new
        :class:`~xpplab.Model` is returned, and the path is its ``.path``."""
        K = np.atleast_2d(np.asarray(K, dtype=float))
        names = list(self.point)
        if K.shape != (len(self.inputs), len(names)):
            raise ValueError(f"K must have shape ({len(self.inputs)}, "
                             f"{len(names)}), got {K.shape}")
        src = Path(self._model.path).read_text(errors="replace")
        lines = src.splitlines()
        for p in self.inputs:
            lines = _remove_parameter(lines, p)
        laws = []
        for p, p0, row in zip(self.inputs, self._p0, K):
            terms = "".join(f"-({float(g)!r})*({v}-({float(self.point[v])!r}))"
                            for g, v in zip(row, names))
            laws.append(f"{p}={float(p0)!r}{terms}")
        # XPPAUT evaluates fixed variables in the order written, so the laws
        # stand where the first input's parameter entry was: before anything
        # that could use an input
        marks = [i for i, ln in enumerate(lines) if ln == _LAW_HERE]
        lines = [ln for ln in lines if ln != _LAW_HERE]
        at = marks[0]
        lines[at:at] = ["# feedback u = u0 - K (x - x*), from place_poles"] + laws
        if path is None:
            path = Path(tempfile.mkdtemp(prefix="xpplab-closed-")) / (
                Path(self._model.path).stem + "_closed_loop.ode")
        path = Path(path)
        path.write_text("\n".join(lines) + "\n")
        from .model import Model
        new = Model(path, options=self._model._options,
                    engine_library=self._model._engine_library)
        # carry the session over: current parameters, initial conditions and
        # numerical options (the replaced inputs are now fixed variables)
        old = self._model
        for p in old._parameter_names:
            if p not in self.inputs and p in new._parameter_names:
                new.set(**{p: old.get(p)})
        for v in old.variables:
            if v in new.variables:
                new.set(**{v: old.get(v)})
        for o, val in old.options.items():
            if o in new.options:
                new.set(**{o: val})
        return new

    def __repr__(self) -> str:
        return (f"Linearization(n={self.A.shape[0]}, inputs={self.inputs}, "
                f"rank={self.rank}, controllable={self.controllable})")


def _check_conjugate(poles) -> None:
    """Complex poles must come with their conjugates, or K would be complex."""
    left = list(poles)
    while left:
        z = left.pop(0)
        if z.imag == 0:
            continue
        hit = [i for i, w in enumerate(left)
               if abs(w - z.conjugate()) <= 1e-12 * max(1.0, abs(z))]
        if not hit:
            raise ValueError(f"complex pole {z} has no conjugate partner: "
                             "poles of a real system come in conjugate pairs")
        left.pop(hit[0])


def _match_distance(got, want) -> float:
    """Largest distance between two pole sets, matched greedily."""
    rest = list(want)
    worst = 0.0
    for z in got:
        i = int(np.argmin([abs(z - w) for w in rest]))
        worst = max(worst, abs(z - rest[i]))
        rest.pop(i)
    return worst


_LAW_HERE = "@@law@@"

_PAR_LINE = re.compile(
    r"^(\s*)(par|params?|parameters?|p)\b(?!\s*[=('])\s*(.*)$",
    re.IGNORECASE)


def _remove_parameter(lines: list[str], name: str) -> list[str]:
    """The model lines without parameter ``name`` on its par line."""
    out, found = [], False
    for line in lines:
        m = _PAR_LINE.match(line)
        if m is None:
            out.append(line)
            continue
        body = re.sub(r"\s*=\s*", "=", m.group(3))
        items = [i for i in re.split(r"[,\s]+", body.strip()) if i]
        keep = [i for i in items if i.split("=")[0].lower() != name.lower()]
        if len(keep) != len(items):
            found = True
            if line.rstrip().endswith("\\"):
                raise ValueError("the parameter line of an input is continued "
                                 "with a backslash and cannot be rewritten")
            if keep:
                out.append(f"{m.group(1)}{m.group(2)} " + ", ".join(keep))
            out.append(_LAW_HERE)  # the feedback law goes where the input was
        else:
            out.append(line)
    if not found:
        raise ValueError(
            f"input {name!r} is not declared on a line starting with p, par, "
            "param or parameters, so the feedback law cannot replace it")
    return out


def _complex_plane_window(ev, discrete: bool):
    """Axis limits that show the eigenvalues ev and the edge of the stable
    region: the imaginary axis, or the unit circle for a map (the desktop
    app frames its plot the same way)."""
    ev = ev[np.isfinite(ev)]
    edge = 1.0 if discrete else 0.0
    lo = min(float(np.min(ev.real, initial=0.0)), -edge)
    hi = max(float(np.max(ev.real, initial=0.0)), edge)
    h = max(float(np.max(np.abs(ev.imag), initial=0.0)), edge)
    span = max(hi - lo, 2 * h) or 1.0
    pad = 0.15 * span
    lo, hi = lo - pad, hi + pad
    h = max(h + pad, 0.3 * (hi - lo))
    return lo, hi, -h, h


def _find(names: list[str], name: str, what: str) -> str:
    for n in names:
        if n.lower() == name.lower():
            return n
    raise KeyError(f"no {what} {name!r}; available: {', '.join(names)}")


def _doubles(n: int):
    return (ctypes.c_double * max(n, 1))()


def linearize(model, at: dict | None = None, inputs=(), *,
              step: float | None = None, tol: float | None = None) -> Linearization:
    """Linearise ``model`` and test controllability; see ``Model.linearize``.

    The numbers come from the engine (``xppe_linearize``,
    ``xppe_eigenvalues``, ``xppe_controllability``), which the desktop app
    calls too, so both give the same A, B, eigenvalues and rank."""
    names = list(model.variables)
    n = len(names)
    if at is None:
        eq = model.equilibrium()
        point = dict(eq.values)
        eigenvalues = np.asarray(eq.eigenvalues, dtype=complex)
    else:
        point = {names[names.index(_find(names, k, "variable"))]: float(v)
                 for k, v in at.items()}
        for v in names:
            point.setdefault(v, float(model.get(v)))
        point = {v: point[v] for v in names}
        eigenvalues = None
    x0 = np.array([point[v] for v in names])

    f0 = model.rhs(x0)
    discrete = model.get("METH") == "discrete"
    # for a continuous model the equilibrium has f = 0; for a map x = F(x)
    residual = float(np.max(np.abs(f0 - x0 if discrete else f0), initial=0.0))

    if isinstance(inputs, str):
        inputs = [inputs]
    pnames = list(model._parameter_names)
    inputs = [_find(pnames, p, "parameter") for p in inputs]
    k = len(inputs)
    if step is not None and not step > 0:
        raise ValueError("step must be positive")
    p0 = np.array([float(model.get(p)) for p in inputs])

    x = (ctypes.c_double * n)(*x0)
    names_in = (ctypes.c_char_p * max(k, 1))(*[p.encode("latin-1") for p in inputs])
    a, b, a_err, b_err = _doubles(n * n), _doubles(n * k), _doubles(n * n), _doubles(n * k)
    model._engine.call("xppe_linearize", x, k, names_in,
                       float(step) if step is not None else 0.0, a, b, a_err, b_err)
    A = np.array(a[:n * n]).reshape(n, n)
    B = np.array(b[:n * k]).reshape(n, k)
    A_err = np.array(a_err[:n * n]).reshape(n, n)
    B_err = np.array(b_err[:n * k]).reshape(n, k)

    if eigenvalues is None:
        re, im = _doubles(n), _doubles(n)
        model._engine.call("xppe_eigenvalues", n, a, re, im)
        eigenvalues = np.array(re[:n]) + 1j * np.array(im[:n])

    if k:
        c, sv = _doubles(n * n * k), _doubles(n)
        rank = model._engine.call("xppe_controllability", n, k, a, b,
                                  float(tol) if tol else 0.0, c, sv, check=False)
        C = np.array(c[:n * n * k]).reshape(n, n * k)
        s = np.array(sv[:n])
        if rank < 0:
            why = model._engine.call("xppe_error", check=False) or b""
            warnings.warn("the controllability rank could not be computed: "
                          + why.decode("latin-1"), stacklevel=3)
            rank = -1
    else:
        C = np.zeros((n, 0))
        s = np.zeros(0)
        rank = 0
    if step is None:
        bad = max(float(np.max(A_err, initial=0.0)), float(np.max(B_err, initial=0.0)))
        scale = max(1.0, float(np.max(np.abs(A), initial=0.0)),
                    float(np.max(np.abs(B), initial=0.0)))
        if bad > 1e-5 * scale:
            warnings.warn(
                f"the derivatives are only good to about {bad:.1e}: the "
                "right-hand side may not be smooth at this point "
                "(see A_error, B_error)", stacklevel=3)
    return Linearization(point, inputs, A, B, eigenvalues, C, int(rank),
                         rank == n, residual, discrete, A_err, B_err, s,
                         model, x0, p0)
