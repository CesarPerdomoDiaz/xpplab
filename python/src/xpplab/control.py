"""Control design: linearise a model and test controllability."""

from __future__ import annotations

import warnings
from dataclasses import dataclass, field

import numpy as np

__all__ = ["Linearization", "linearize"]


@dataclass
class Linearization:
    """The linear system dx = A dx + B du around a point of a model.

    ``A`` is the Jacobian of the right-hand side (for a map, of the map),
    ``B`` has one column per input parameter. ``controllability`` is
    [B, AB, ..., A^(n-1) B]; the pair is ``controllable`` when its rank is n.
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

    def __repr__(self) -> str:
        return (f"Linearization(n={self.A.shape[0]}, inputs={self.inputs}, "
                f"rank={self.rank}, controllable={self.controllable})")


def _find(names: list[str], name: str, what: str) -> str:
    for n in names:
        if n.lower() == name.lower():
            return n
    raise KeyError(f"no {what} {name!r}; available: {', '.join(names)}")


def _ridders(g, scale: float, start: float = 0.1, shrink: float = 1.4,
             rows: int = 10):
    """Derivative by Ridders' method: central differences g(h) at shrinking
    steps, extrapolated to h = 0 (Neville's tableau, as in Numerical Recipes'
    ``dfridr``). Each component keeps the extrapolation with the smallest
    error estimate. Returns (derivative, error estimate)."""
    n = np.size(g(start * scale))
    best = np.zeros(n)
    err = np.full(n, np.inf)
    h = start * scale
    tab = [None] * rows
    for i in range(rows):
        tab_i = [np.asarray(g(h), dtype=float)]
        fac = shrink ** 2
        for k in range(1, i + 1):
            prev = tab[k - 1]
            tab_i.append((tab_i[k - 1] * fac - prev) / (fac - 1.0))
            e = np.maximum(np.abs(tab_i[k] - tab_i[k - 1]),
                           np.abs(tab_i[k] - prev))
            better = e <= err
            best = np.where(better, tab_i[k], best)
            err = np.where(better, e, err)
            fac *= shrink ** 2
        tab = tab_i
        if i > 0:
            # stop once the extrapolation gets worse: rounding noise has won
            if np.all(np.abs(tab[i][-1] - prev_top) >= 2 * err):
                break
        prev_top = tab[i][-1]
        h /= shrink
    return best, err


def _derivative(g, scale: float, step: float | None):
    """d/dx of a vector function through its central difference g(h).

    With ``step`` the difference uses that relative step; otherwise
    Ridders' method is run from several starting steps, from coarse to fine,
    and the estimate with the smallest error wins, so no step size has to
    be chosen for the model."""
    if step is not None:
        d = np.asarray(g(step * scale), dtype=float)
        return d, np.full(d.shape, np.nan)
    d = err = None
    for start in (1e-1, 1e-3, 1e-5):
        di, ei = _ridders(g, scale, start)
        if d is None:
            d, err = di, ei
        else:
            better = ei < err
            d, err = np.where(better, di, d), np.where(better, ei, err)
    return d, err


def linearize(model, at: dict | None = None, inputs=(), *,
              step: float | None = None, tol: float | None = None) -> Linearization:
    """Linearise ``model`` and test controllability; see ``Model.linearize``."""
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
        eigenvalues = None
    x0 = np.array([point[v] for v in names])

    f0 = model.rhs(x0)
    discrete = model.get("METH") == "discrete"
    # for a continuous model the equilibrium has f = 0; for a map x = F(x)
    residual = float(np.max(np.abs(f0 - x0 if discrete else f0), initial=0.0))

    A = np.zeros((n, n))
    A_err = np.zeros((n, n))
    for j in range(n):
        def g(h, j=j):
            xp, xm = x0.copy(), x0.copy()
            xp[j] += h
            xm[j] -= h
            return (model.rhs(xp) - model.rhs(xm)) / (2 * h)
        A[:, j], A_err[:, j] = _derivative(g, max(1.0, abs(x0[j])), step)

    if isinstance(inputs, str):
        inputs = [inputs]
    pnames = list(model._parameter_names)
    inputs = [_find(pnames, p, "parameter") for p in inputs]
    B = np.zeros((n, len(inputs)))
    B_err = np.zeros((n, len(inputs)))
    saved = {p: model.get(p) for p in inputs}
    try:
        for k, p in enumerate(inputs):
            p0 = saved[p]

            def g(h, p=p, p0=p0):
                model.set(**{p: p0 + h})
                fp = model.rhs(x0)
                model.set(**{p: p0 - h})
                fm = model.rhs(x0)
                return (fp - fm) / (2 * h)
            B[:, k], B_err[:, k] = _derivative(g, max(1.0, abs(p0)), step)
            model.set(**{p: p0})
    finally:
        model.set(**saved)

    if eigenvalues is None:
        eigenvalues = np.linalg.eigvals(A)

    blocks = []
    if inputs:
        M = B
        for _ in range(n):
            blocks.append(M)
            M = A @ M
        C = np.hstack(blocks)
        s = np.linalg.svd(C, compute_uv=False)
        if tol is None:
            # the derivatives are good to about 1e-10 (see A_error), so the
            # cutoff is relative to the largest singular value
            tol = max(C.shape) * max(s[0], 1e-300) * 1e-8
        rank = int(np.sum(s > tol))
    else:
        C = np.zeros((n, 0))
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
    return Linearization(point, inputs, A, B, eigenvalues, C, rank,
                         rank == n, residual, discrete, A_err, B_err,
                         model, x0, np.array([saved[p] for p in inputs]))
