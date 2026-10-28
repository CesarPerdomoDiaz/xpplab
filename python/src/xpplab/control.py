"""Control design: linearise a model and test controllability."""

from __future__ import annotations

import ctypes
import tempfile
import warnings
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from ._engine import EngineError

__all__ = ["LQR", "Linearization", "RootLocus", "linearize"]


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
        keeping the closed-loop eigenvectors well conditioned. The engine
        computes the gain (``xppe_place_poles``), so the desktop app's pole
        placement gives the same numbers. The poles are n numbers, complex
        ones in conjugate pairs; for a map they belong inside the unit
        circle.

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
        if self.residual > 1e-4:
            warnings.warn(
                f"the point is not an equilibrium (residual {self.residual:.2g}): "
                "the closed loop u = u0 - K (x - x*) will not settle at x*, "
                "because the model still moves there with no feedback",
                stacklevel=2)
        K = _doubles(k * n)
        try:
            # -0 as 0 (+ 0.0), as the Python the desktop app writes reads it
            self._model._engine.call(
                "xppe_place_poles", n, k, _array(self.A), _array(self.B),
                _array(poles.real + 0.0), _array(poles.imag + 0.0), K)
        except EngineError as e:
            raise ValueError(e.args[0]) from None
        K = np.array(K[:k * n]).reshape(k, n)
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

    def closed_loop(self, K, path=None):
        """Write the closed-loop model, u = u0 - K (x - x*), and load it.

        Each input parameter is removed from the model's parameter lines and
        defined instead as a fixed variable: its nominal value minus the row
        of ``K`` times the distance of the states from the linearisation
        point. The laws stand at the top of the model, so that fixed
        variables using an input see them wherever they are written; a long
        law is split into partial sums (``kf1a1``, ...) to fit XPPAUT's
        lines. The rest of the model text is kept as it is. The file goes to
        ``path`` (a temporary directory by default); the new
        :class:`~xpplab.Model` is returned, and the path is its ``.path``."""
        K = np.atleast_2d(np.asarray(K, dtype=float))
        names = list(self.point)
        if K.shape != (len(self.inputs), len(names)):
            raise ValueError(f"K must have shape ({len(self.inputs)}, "
                             f"{len(names)}), got {K.shape}")
        names_in = [p.encode("latin-1") for p in self.inputs]
        states = [v.encode("latin-1") for v in names]
        args = (Path(self._model.path).read_bytes(), len(self.inputs),
                (ctypes.c_char_p * len(names_in))(*names_in),
                _array(self._p0), len(names),
                (ctypes.c_char_p * len(states))(*states),
                _array([self.point[v] for v in names]), _array(K))
        try:
            size = self._model._engine.call("xppe_closed_loop", *args, None, 0)
            text = ctypes.create_string_buffer(size + 1)
            self._model._engine.call("xppe_closed_loop", *args, text, size + 1)
        except EngineError as e:
            raise ValueError(e.args[0]) from None
        if path is None:
            path = Path(tempfile.mkdtemp(prefix="xpplab-closed-")) / (
                Path(self._model.path).stem + "_closed_loop.ode")
        path = Path(path)
        path.write_bytes(text.raw[:size])
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

    def root_locus(self, K, gain=(0.0, 1.0), steps: int = 201) -> "RootLocus":
        """How the poles move as the feedback u = u0 - g K (x - x*) grows:
        the eigenvalues of A - g B K for ``steps`` gains g evenly from
        ``gain[0]`` to ``gain[1]``.

        ``K`` is a gain (inputs x states), for example the one
        :meth:`place_poles` gave (then g = 0 is the open loop and g = 1 the
        poles placed), or the feedback of one state through one input
        (``[[0, 1]]``: u = -g * (the second state)). The engine computes
        and matches the eigenvalues (``xppe_gain_locus``), so the desktop
        app's root locus gives the same numbers."""
        n, k = self.B.shape
        if k == 0:
            raise ValueError("a root locus over a gain needs inputs "
                             "(linearize(inputs=[...]))")
        K = np.atleast_2d(np.asarray(K, dtype=float))
        if K.shape != (k, n):
            raise ValueError(f"K must have shape ({k}, {n}), got {K.shape}")
        lo, hi = (float(g) for g in gain)
        steps = int(steps)
        if steps < 2:
            raise ValueError("steps must be at least 2")
        g, re, im = _doubles(steps), _doubles(steps * n), _doubles(steps * n)
        try:
            self._model._engine.call("xppe_gain_locus", n, k, _array(self.A),
                                     _array(self.B), _array(K), lo, hi, steps,
                                     g, re, im)
        except EngineError as e:
            raise ValueError(e.args[0]) from None
        poles = (np.array(re[:steps * n]) + 1j * np.array(im[:steps * n])).reshape(steps, n)
        return RootLocus("gain", np.array(g[:steps]), poles, self.discrete)

    def lqr(self, Q=None, R=None) -> "LQR":
        """The optimal state feedback: the gain K that minimises the cost
        J = integral of dx^T Q dx + du^T R du (for a map, the sum over the
        steps), with du = -K dx.

        ``Q`` weighs the states' distances from the point and ``R`` the
        inputs: a number (times the identity), one number per state or
        input (a diagonal matrix), or a full symmetric matrix. Both default
        to the identity. Returns an :class:`LQR`: the gain ``K`` (inputs x
        states), the solution ``P`` of the Riccati equation (the cost from
        dx is dx^T P dx) and the closed loop's ``poles``. Raises
        ``ValueError`` when R is not positive definite, or when no gain
        stabilises the loop (an unstable mode the inputs cannot move, or
        one Q does not see). The engine solves the Riccati equation
        (``xppe_lqr``: the matrix sign function for a continuous model,
        the doubling algorithm for a map), so the desktop app's LQR gives
        the same numbers."""
        n, k = self.B.shape
        if k == 0:
            raise ValueError("LQR needs inputs (linearize(inputs=[...]))")
        Q = _weights(Q, n, "Q")
        R = _weights(R, k, "R")
        K, P = _doubles(k * n), _doubles(n * n)
        try:
            self._model._engine.call("xppe_lqr", n, k, _array(self.A), _array(self.B),
                                     _array(Q), _array(R), int(self.discrete), K, P)
        except EngineError as e:
            raise ValueError(e.args[0]) from None
        K = np.array(K[:k * n]).reshape(k, n)
        P = np.array(P[:n * n]).reshape(n, n)
        return LQR(K, P, np.linalg.eigvals(self.A - self.B @ K), Q, R)

    def __repr__(self) -> str:
        return (f"Linearization(n={self.A.shape[0]}, inputs={self.inputs}, "
                f"rank={self.rank}, controllable={self.controllable})")


@dataclass
class RootLocus:
    """How the eigenvalues move as a gain or a parameter changes.

    ``values`` are the gains (or the parameter's values), ``poles`` the
    eigenvalues there: one row per value and one column per branch. The
    engine matches each step's eigenvalues to the last ones, so a column
    follows one eigenvalue. Over a parameter, ``x`` holds the equilibrium at
    each value (NaN where Newton's method failed) and ``variables`` names
    its columns."""

    name: str  # "gain", or the parameter
    values: np.ndarray
    poles: np.ndarray
    discrete: bool = False
    x: np.ndarray | None = None
    variables: list[str] = field(default_factory=list)

    def crossings(self) -> list[tuple[float, complex]]:
        """Where a branch crosses the edge of stability (the imaginary axis,
        for a map the unit circle): (value, pole) pairs, interpolated
        linearly between the two steps around each crossing, in order of
        value."""
        out = []
        edge = (np.abs(self.poles) - 1.0) if self.discrete else self.poles.real
        for b in range(self.poles.shape[1]):
            d = edge[:, b]
            for i in range(len(d) - 1):
                d0, d1 = d[i], d[i + 1]
                if not (np.isfinite(d0) and np.isfinite(d1)) or d0 == d1:
                    continue
                if (d0 < 0) != (d1 < 0):
                    f = -d0 / (d1 - d0)
                    v = self.values[i] + f * (self.values[i + 1] - self.values[i])
                    z = self.poles[i, b] + f * (self.poles[i + 1, b] - self.poles[i, b])
                    out.append((float(v), complex(z)))
        return sorted(out, key=lambda c: c[0])

    def to_csv(self, path) -> None:
        """One row per value, every number exact: the value, the
        equilibrium (over a parameter), then each branch's real and
        imaginary part (re1, im1, re2, ...). The desktop app's
        ``export FILE.csv locus`` writes the same file."""
        from .analysis import write_csv
        n = self.poles.shape[1]
        cols = [self.name] + list(self.variables)
        for b in range(n):
            cols += [f"re{b + 1}", f"im{b + 1}"]
        rows = []
        for i, v in enumerate(self.values):
            row = [float(v)] + ([float(c) for c in self.x[i]] if self.x is not None else [])
            for b in range(n):
                z = complex(self.poles[i, b])
                row += [z.real, z.imag]
            rows.append(row)
        write_csv(path, cols, rows)

    def plot(self, ax=None):
        """The branches in the complex plane, each from an x (the first
        value) to an o (the last), with the stable region shaded."""
        import matplotlib.pyplot as plt
        if ax is None:
            _, ax = plt.subplots()
        ev = self.poles[np.isfinite(self.poles)]
        lo, hi, ylo, yhi = _complex_plane_window(ev, self.discrete)
        if self.discrete:
            t = np.linspace(0, 2 * np.pi, 257)
            ax.fill(np.cos(t), np.sin(t), color="#2ca02c", alpha=0.15, lw=0)
            ax.plot(np.cos(t), np.sin(t), color="0.4", lw=1)
        else:
            ax.axvspan(min(lo, 0), 0, color="#2ca02c", alpha=0.15, lw=0)
            ax.axvline(0, color="0.4", lw=1)
        ax.axhline(0, color="0.6", lw=0.8)
        for b in range(self.poles.shape[1]):
            z = self.poles[:, b]
            line, = ax.plot(z.real, z.imag, lw=1.5)
            ok = np.flatnonzero(np.isfinite(z))
            if len(ok):
                ax.plot(z.real[ok[0]], z.imag[ok[0]], "x", color=line.get_color())
                ax.plot(z.real[ok[-1]], z.imag[ok[-1]], "o", mfc="none",
                        color=line.get_color())
        ax.set_xlim(lo, hi)
        ax.set_ylim(ylo, yhi)
        if self.discrete:
            ax.set_aspect("equal", adjustable="box")
        ax.set_xlabel("real part")
        ax.set_ylabel("imaginary part")
        ax.set_title(f"root locus: {self.name} from {self.values[0]:.6g} (x) "
                     f"to {self.values[-1]:.6g} (o)")
        return ax

    def __repr__(self) -> str:
        return (f"RootLocus({self.name} from {self.values[0]:.6g} to "
                f"{self.values[-1]:.6g}, {len(self.values)} steps, "
                f"{self.poles.shape[1]} branches)")


@dataclass
class LQR:
    """The result of :meth:`Linearization.lqr`."""

    K: np.ndarray      # the gain: du = -K dx
    P: np.ndarray      # the Riccati solution: the cost from dx is dx^T P dx
    poles: np.ndarray  # the eigenvalues of A - B K
    Q: np.ndarray
    R: np.ndarray

    def __repr__(self) -> str:
        return (f"LQR(K={np.array2string(self.K, precision=4)}, poles="
                f"{np.array2string(self.poles, precision=4)})")


def _weights(W, n: int, name: str) -> np.ndarray:
    """Q or R as an n x n matrix: the identity, a number times it, a
    diagonal from n numbers, or the matrix itself."""
    if W is None:
        return np.eye(n)
    W = np.asarray(W, dtype=float)
    if W.ndim == 0:
        return float(W) * np.eye(n)
    if W.ndim == 1:
        if W.size != n:
            raise ValueError(f"{name} needs {n} weights, got {W.size}")
        return np.diag(W)
    if W.shape != (n, n):
        raise ValueError(f"{name} must be {n} x {n}, got {W.shape[0]} x {W.shape[1]}")
    return W


def root_locus(model, parameter: str, values, steps: int = 101,
               guess=None) -> RootLocus:
    """See :meth:`xpplab.Model.root_locus`."""
    name = model.parameters._lookup(parameter)
    lo, hi = (float(v) for v in values)
    steps = int(steps)
    if steps < 2:
        raise ValueError("steps must be at least 2")
    model._engine.call("xppe_parameter_locus", name.encode("latin-1"), lo, hi,
                       steps, model._state(guess))
    t = model._result(f"Root locus over {name}")
    # the ODE variables only: Markov variables have no equilibrium equation,
    # so the table holds the parameter, n states and n eigenvalues
    n = (len(t.columns) - 1) // 3
    variables = t.columns[1:1 + n]
    x = np.column_stack([t[v] for v in variables])
    poles = np.column_stack([t[f"re{b + 1}"] + 1j * t[f"im{b + 1}"] for b in range(n)])
    return RootLocus(name, t[name], poles, model.get("METH") == "discrete", x,
                     list(variables))


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


def _array(values):
    """The numbers, as a C array of doubles (row by row for a matrix)."""
    flat = np.ravel(np.asarray(values, dtype=float))
    return (ctypes.c_double * max(len(flat), 1))(*flat)


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
