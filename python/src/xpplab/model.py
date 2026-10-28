"""Models: load an XPPAUT .ode file, change it, integrate it."""

from __future__ import annotations

import ctypes
import os
import tempfile
from collections.abc import Iterator, MutableMapping
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from . import _engine as E
from .analysis import AnalysisMixin

__all__ = ["Model", "Trajectory", "RunList", "Values", "Equilibrium",
           "EquilibriumList", "load", "METHODS"]

# XPPAUT's integrators, by the names its ODE files use. The value is the
# key XPPAUT recognises (the first letter it reads).
METHODS = {
    "discrete": "d", "euler": "e", "modeuler": "m", "rungekutta": "r",
    "adams": "a", "gear": "g", "volterra": "v", "backeul": "b",
    "qualrk": "q", "stiff": "s", "cvode": "c", "5dp": "5", "83dp": "8",
    "2rb": "2", "ymp": "y",
}
# Friendlier aliases.
METHODS.update({
    "rk4": "r", "runge-kutta": "r", "modified-euler": "m",
    "backward-euler": "b", "dopri5": "5", "dormand-prince": "5",
    "dopri8": "8", "dopri853": "8", "rosenbrock": "2", "symplectic": "y",
    "map": "d",
})
_METHOD_BY_INDEX = ["discrete", "euler", "modeuler", "rungekutta", "adams",
                    "gear", "volterra", "backeul", "qualrk", "stiff", "cvode",
                    "5dp", "83dp", "2rb", "ymp"]

# Numerical options readable through the engine (see xppe_get).
OPTIONS = ["TOTAL", "DT", "T0", "TRANS", "BOUND", "TOL", "ATOL", "DTMIN",
           "DTMAX", "NOUT", "MAXSTOR", "METH", "DELAY", "NEWT_TOL",
           "NEWT_ITER", "JAC_EPS", "POIMAP", "POIVAR", "POISGN", "POIPLN",
           "POISTOP", "NMESH"]


def _b(s: str) -> bytes:
    return s.encode("latin-1")


def _key(name: str) -> str:
    return name.strip().upper()


class _NamedValues(MutableMapping):
    """A live, case-insensitive view of some of the model's values."""

    def __init__(self, model: "Model", names: list[str], what: str):
        self._model = model
        self._names = names
        self._what = what

    def _lookup(self, name: str) -> str:
        for n in self._names:
            if _key(n) == _key(name):
                return n
        raise KeyError(f"no {self._what} named {name!r}; "
                       f"available: {', '.join(self._names)}")

    def __getitem__(self, name: str) -> float:
        return self._model.get(self._lookup(name))

    def __setitem__(self, name: str, value: float) -> None:
        self._model.set(**{self._lookup(name): value})

    def __delitem__(self, name):
        raise TypeError(f"cannot delete a {self._what}")

    def __iter__(self) -> Iterator[str]:
        return iter(self._names)

    def __len__(self) -> int:
        return len(self._names)

    def __repr__(self) -> str:
        items = ", ".join(f"{n}={self[n]:.6g}" for n in self._names)
        return "{" + items + "}"


class Model(AnalysisMixin):
    """An XPPAUT model loaded from an .ode file.

    ```python
    m = Model("lecar.ode")
    m.parameters["iapp"] = 0.1
    sol = m.run(total=200)
    sol["V"]          # NumPy array
    ```
    """

    def __init__(self, path: str | os.PathLike, *, options: list[str] = (),
                 engine_library: str | None = None):
        self.path = Path(path).resolve()
        if not self.path.exists():
            raise FileNotFoundError(self.path)
        self._engine = E.Engine(engine_library)
        self._engine.cwd = self.path.parent
        opts = [_b(o) for o in options]
        argv = (ctypes.c_char_p * max(1, len(opts)))(*opts)
        try:
            self._engine.call("xppe_load", _b(self.path.name), len(opts), argv)
        except E.EngineError:
            self._engine.close()
            raise
        self.load_log = self._engine.log
        self._engine.call("xppe_auto_set_workdir",
                          _b(str(self._engine.workdir)))
        self.variables = self._engine.names(E.VARIABLES)
        self.columns = self._engine.names(E.COLUMNS)
        self.sets = self._engine.names(E.SETS)
        self._parameter_names = self._engine.names(E.PARAMETERS)
        from .auto import Auto  # avoid an import cycle
        self.auto = Auto(self)

    @classmethod
    def from_text(cls, text: str, name: str = "model.ode", **kw) -> "Model":
        """Load a model from the text of an .ode file."""
        d = Path(tempfile.mkdtemp(prefix="xpplab-model-"))
        p = d / name
        p.write_text(text)
        return cls(p, **kw)

    # -- names and values -------------------------------------------------

    @property
    def name(self) -> str:
        return self.path.name

    @property
    def parameters(self) -> _NamedValues:
        """Parameters, as a live dictionary: ``m.parameters["a"] = 2``."""
        return _NamedValues(self, self._parameter_names, "parameter")

    @property
    def initial_conditions(self) -> _NamedValues:
        """Initial conditions of the state variables, as a live dictionary."""
        return _NamedValues(self, self.variables, "variable")

    ics = initial_conditions

    @property
    def options(self) -> dict[str, float | str]:
        """The numerical options (TOTAL, DT, METH, ...), as a snapshot."""
        out: dict[str, float | str] = {}
        for o in OPTIONS:
            try:
                out[o] = self.get(o)
            except E.EngineError:
                pass
        return out

    def get(self, name: str) -> float | str:
        """Value of a parameter, an initial condition or a numerical option."""
        v = ctypes.c_double()
        self._engine.call("xppe_get", _b(name), ctypes.byref(v))
        if _key(name) == "METH":
            i = int(v.value)
            return _METHOD_BY_INDEX[i] if 0 <= i < len(_METHOD_BY_INDEX) else i
        return v.value

    def set(self, **values) -> "Model":
        """Set parameters, initial conditions or numerical options.

        ``m.set(iapp=0.1, total=200, meth="stiff")``. Names ignore case.
        This works exactly like a ``set`` line in an ODE file.
        """
        for name, value in values.items():
            if _key(name) == "METH" and isinstance(value, str):
                key = METHODS.get(value.lower())
                if key is None:
                    raise ValueError(f"unknown method {value!r}; choose from "
                                     f"{', '.join(sorted(METHODS))}")
                value = key
            if isinstance(value, str):
                self._engine.call("xppe_set", _b(name), _b(value))
            else:
                self._engine.call("xppe_set_number", _b(name), float(value))
        return self

    def apply_set(self, name: str) -> "Model":
        """Apply a named parameter set from the model file (``set name {...}``)."""
        self._engine.call("xppe_apply_set", _b(name))
        return self

    # -- computation --------------------------------------------------------

    def run(self, **values) -> "Trajectory":
        """Integrate from the current initial conditions.

        Keyword arguments are applied with :meth:`set` first, so
        ``m.run(total=100, iapp=0.2)`` is shorthand for a set followed by a run.
        """
        if values:
            self.set(**values)
        self._engine.call("xppe_run")
        return self._stopped(self._collect())

    def _stopped(self, sol: "Trajectory") -> "Trajectory":
        """Record which global flag, if any, stopped the run."""
        sol.event = self._engine.call("xppe_halt_flag", check=False)
        return sol

    @property
    def flags(self) -> list[tuple[str, int]]:
        """The model's global flags (``global sign condition {events}``), in
        order, as ``(condition, sign)``: sign 1 when the condition crosses
        zero going up, -1 going down, 0 either way. A run stopped by one of
        them (an ``{arret=1}`` event) has its number, counted from 1, in
        :attr:`Trajectory.event`."""
        out = []
        sign = ctypes.c_int()
        for i in range(self._engine.call("xppe_flag_count", check=False)):
            cond = self._engine._lib.xppe_flag_condition(i, ctypes.byref(sign))
            out.append((E._text(cond).strip(), sign.value))
        return out

    def run_many(self, initial, **values) -> "RunList":
        """Run once from each of several initial conditions and keep every
        run (the desktop app's Many initial values).

        ``initial`` is a list of dicts of variable values (other variables
        keep their initial value), or an array with one row per run and one
        column per variable. Keyword arguments are applied with :meth:`set`
        first. Returns a :class:`RunList`: the trajectories in order, with
        ``events``, the global flag that stopped each run (0 for none). The
        initial conditions are restored afterwards."""
        if values:
            self.set(**values)
        if isinstance(initial, np.ndarray) or (
                len(initial) and not isinstance(initial[0], dict)):
            arr = np.atleast_2d(np.asarray(initial, dtype=float))
            initial = [dict(zip(self.variables, row)) for row in arr]
        ics = {v: float(self.get(v)) for v in self.variables}
        out = RunList()
        try:
            for start in initial:
                self.set(**ics)
                self.set(**{k: float(v) for k, v in start.items()})
                out.append(self.run())
        finally:
            self.set(**ics)
        return out

    def sample_initial(self, n: int, box: dict, *, require: dict | None = None,
                       seed: int = 1, tries: int = 100000) -> list[dict]:
        """``n`` initial conditions drawn at random, uniformly, in a box.

        ``box`` maps variables to ``(low, high)``; the others keep their
        initial value. ``require`` maps output columns (variables or
        auxiliaries) to ``(low, high)``: a draw is kept only if every one of
        them lies in its range there, so ``require={"tens": (0, inf)}``
        keeps the states where an auxiliary ``tens`` is positive. The draws
        come from a SplitMix64 generator started at ``seed``, the one the
        desktop app uses, so the same call gives the same states in both.
        Returns a list of dicts, ready for :meth:`run_many`."""
        names = [self.variables[self._var_index(k)] for k in box]
        bounds = [(float(a), float(b)) for a, b in box.values()]
        need = [(name, float(a), float(b)) for name, (a, b) in (require or {}).items()]
        rng = _SplitMix64(seed)
        out: list[dict] = []
        for _ in range(int(tries)):
            if len(out) >= n:
                break
            start = Values((v, lo + (hi - lo) * rng.uniform())
                           for v, (lo, hi) in zip(names, bounds))
            if need:
                row = self.evaluate(start)
                if not all(lo <= row[c] <= hi for c, lo, hi in need):
                    continue
            out.append(start)
        if len(out) < n:
            raise ValueError(f"only {len(out)} of {n} draws met require= in "
                             f"{tries} tries")
        return out

    def continue_run(self, trajectory: "Trajectory",
                     time: float | None = None) -> "Trajectory":
        """XPPAUT's Continue (C C): integrate ``time`` more (default: the
        total time) from where ``trajectory`` ended, and return it with the
        new points appended. The initial conditions, T0 and the total time
        are left as they were."""
        if trajectory is None or not len(trajectory):
            return self.run()
        t0, total = float(self.get("T0")), float(self.get("TOTAL"))
        ics = {v: float(self.get(v)) for v in self.variables}
        more = float(time) if time is not None and time > 0 else total
        self.set(T0=float(trajectory.t[-1]), TOTAL=more)
        self.set(**{v: float(trajectory[v][-1]) for v in self.variables
                    if v in trajectory})
        try:
            new = self.run()
        finally:
            self.set(T0=t0, TOTAL=total, **ics)
        data = {k: np.concatenate([trajectory[k], new[k][1:]])
                for k in trajectory.columns if k in new}
        return Trajectory(data, log=new.log)

    def ensemble(self, trials: int, **values) -> "Ensemble":
        """Run a noisy model ``trials`` times and average (XPPAUT's
        Numerics > stocHastic > Compute).

        Every run starts from the same initial conditions while the random
        numbers (``wiener`` and ``ran()``) go on, so the runs differ. The
        result holds the mean and the variance of every column at each
        output time, computed by XPPAUT as its batch mode does with
        ``@ stoch=1`` (in single precision). Keyword arguments are applied
        with :meth:`set` first. The last run stays as the engine's run.
        """
        if values:
            self.set(**values)
        n = self._engine.call("xppe_ensemble", int(trials))
        parts = []
        for which in (0, 1):
            data = {}
            buf = (ctypes.c_double * max(n, 1))()
            for j, name in enumerate(self.columns):
                k = self._engine.call("xppe_ensemble_column", j, which, buf, n)
                data[name] = np.frombuffer(buf, dtype=float, count=k).copy()
            if "T" not in data:  # an "only" statement dropped time
                k = self._engine.call("xppe_ensemble_column", -1, which, buf, n)
                data = {"T": np.frombuffer(buf, dtype=float, count=k).copy(), **data}
            parts.append(Trajectory(data, log=self._engine.log))
        return Ensemble(parts[0], parts[1], int(trials))

    def last_run(self) -> "Trajectory":
        """The engine's most recent run, such as the one period of the limit
        cycle that :meth:`phase_response` leaves behind."""
        return self._collect()

    def _collect(self) -> "Trajectory":
        """The engine's current run as a Trajectory."""
        rows = self._engine.call("xppe_rows", check=False)
        data = {}
        buf = (ctypes.c_double * max(rows, 1))()
        if "T" not in self.columns:  # an "only" statement dropped time
            n = self._engine.call("xppe_column", -1, buf, rows)
            data["T"] = np.frombuffer(buf, dtype=float, count=n).copy()
        for j, name in enumerate(self.columns):
            n = self._engine.call("xppe_column", j, buf, rows)
            data[name] = np.frombuffer(buf, dtype=float, count=n).copy()
        return Trajectory(data, log=self._engine.log)

    def equilibrium(self, guess: dict | None = None, *,
                    set_initial: bool = False) -> "Equilibrium":
        """Find an equilibrium with Newton's method.

        The search starts from ``guess`` (a dict of variable values) or from
        the current initial conditions. With ``set_initial=True`` the
        equilibrium becomes the new initial condition, which is how AUTO
        continuations usually start.
        """
        n = len(self.variables)
        x = (ctypes.c_double * n)()
        for i, v in enumerate(self.variables):
            x[i] = float(self.get(v))
        if guess:
            for k, val in guess.items():
                i = [_key(v) for v in self.variables].index(_key(k))
                x[i] = float(val)
        re = (ctypes.c_double * n)()
        im = (ctypes.c_double * n)()
        self._engine.call("xppe_equilibrium", x, re, im)
        values = {v: x[i] for i, v in enumerate(self.variables)}
        if set_initial:
            self.set(**values)
        eig = np.array(re[:]) + 1j * np.array(im[:])
        discrete = self.get("METH") == "discrete"
        return Equilibrium(values, eig, discrete)

    def linearize(self, at: dict | None = None, inputs=(), *,
                  step: float | None = None, tol: float | None = None):
        """Linearise at an equilibrium and test controllability.

        ``at`` is a dict of variable values (others keep their initial
        value); by default the equilibrium found by :meth:`equilibrium`.
        ``inputs`` lists the parameters that act as controls. A is the
        Jacobian of the right-hand side and B the derivative with respect to
        each input, both by central differences of :meth:`rhs` extrapolated to
        zero step (Ridders' method, which picks its own step; ``step=`` sets
        a fixed relative step instead). Returns a
        :class:`~xpplab.control.Linearization` with ``A``, ``B``,
        ``eigenvalues``, ``controllability`` (the matrix [B, AB, ...]),
        ``rank`` and ``controllable``. ``tol`` is the singular-value cutoff
        for the rank."""
        from .control import linearize
        return linearize(self, at, inputs, step=step, tol=tol)

    def manifolds(self, guess: dict | None = None):
        """Stable and unstable manifolds of a saddle, as XPPAUT draws them.

        Finds the equilibrium near ``guess`` (or the initial values). If it
        is a saddle with a one-dimensional stable or unstable direction,
        each branch is integrated from just off the equilibrium along its
        eigenvector: forward in time for unstable branches, backward for
        stable ones. Returns ``(equilibrium, [(kind, Trajectory), ...])``
        with kind "stable" or "unstable"; the list is empty for non-saddles.
        """
        eq = self.equilibrium(guess)
        n = self._engine.call("xppe_manifold_count", check=False)
        branches = []
        kind = ctypes.c_int()
        for k in range(n):
            self._engine.call("xppe_manifold", k, ctypes.byref(kind))
            branches.append(("unstable" if kind.value > 0 else "stable",
                             self._stopped(self._collect())))
        return eq, branches

    def eigenvector(self, A, eigenvalue: float) -> np.ndarray:
        """The eigenvector of the matrix ``A`` for one of its real
        eigenvalues, by XPPAUT's own inverse iteration (the one Sing pts
        uses to draw invariant sets), with length 1."""
        A = np.asarray(A, dtype=float)
        n = A.shape[0]
        a = (ctypes.c_double * (n * n))(*A.ravel())
        v = (ctypes.c_double * n)()
        self._engine.call("xppe_eigenvector", n, a, float(eigenvalue), v)
        return np.array(v[:])

    def shoot(self, at=None, *, eps: float = 1e-5, total: float | None = None,
              directions: str = "unstable") -> "RunList":
        """Integrate away from an equilibrium along its real eigenvectors,
        both ways: the start of its unstable (or stable) manifolds, as
        XPPAUT's Initial conds > sHoot does after Sing pts.

        ``at`` is an :class:`Equilibrium`, or a dict guess for Newton's
        method (by default the initial values). Each real eigenvector ``v``
        of the Jacobian (from :meth:`linearize`) with an eigenvalue of the
        chosen sign gives two runs, from ``x_e + eps*v`` and ``x_e - eps*v``:
        forward in time for ``directions="unstable"`` (eigenvalue > 0),
        backward for ``"stable"`` (eigenvalue < 0); ``"all"`` does both. A
        node's two unstable directions both count, unlike XPPAUT's
        invariant sets, which are drawn for saddles only. ``total`` sets
        the time of each run. Returns a :class:`RunList` whose
        ``directions`` holds ``(eigenvalue, sign)`` for each run."""
        if directions not in ("unstable", "stable", "all"):
            raise ValueError('directions is "unstable", "stable" or "all"')
        eq = at if isinstance(at, Equilibrium) else self.equilibrium(at)
        lin = self.linearize(at=eq.values)
        x0 = np.array([eq.values[v] for v in self.variables])
        ics = {v: float(self.get(v)) for v in self.variables}
        old_total, old_dt = float(self.get("TOTAL")), float(self.get("DT"))
        out = RunList()
        out.equilibrium = eq
        out.directions = []
        try:
            if total is not None:
                self.set(TOTAL=float(total))
            for lam in lin.eigenvalues:
                if lam.imag != 0 or lam.real == 0:
                    continue
                forward = lam.real > 0
                if (directions == "unstable" and not forward) or (
                        directions == "stable" and forward):
                    continue
                v = self.eigenvector(lin.A, lam.real)
                self.set(DT=abs(old_dt) if forward else -abs(old_dt))
                for sign in (1, -1):
                    self.set(**dict(zip(self.variables, x0 + sign * eps * v)))
                    out.append(self.run())
                    out.directions.append((float(lam.real), sign))
        finally:
            self.set(TOTAL=old_total, DT=old_dt)
            self.set(**ics)
        return out

    def equilibrium_sweep(self, parameter: str, values, *,
                          guess: dict | None = None):
        """Follow an equilibrium as a parameter changes: XPPAUT's Sing pts >
        Range, a quick diagram of steady states without AUTO.

        For each value in turn the parameter takes it, and Newton's method
        starts from the equilibrium found at the value before (the first
        time from the initial conditions, or from ``guess``). Returns a
        Table: the parameter, each variable, then ``unstable`` and
        ``stable``, the numbers of eigenvalues with positive and negative
        real parts (for maps, outside and inside the unit circle). Where
        Newton's method failed the row holds NaN. The parameter is restored
        afterwards."""
        name = self.parameters._lookup(parameter)
        vals = [float(v) for v in values]
        buf = (ctypes.c_double * max(len(vals), 1))(*vals)
        self._engine.call("xppe_equilibrium_sweep", _b(name), buf, len(vals),
                          self._state(guess))
        return self._result(f"Equilibria as {name} changes")

    def find_equilibria(self, guesses: int = 100, box: dict | None = None,
                        tol: float = 0.001) -> "EquilibriumList":
        """Every equilibrium Newton's method finds from random starting
        points: XPPAUT's Sing pts > Monte Carlo.

        The ``guesses`` starting points are spread uniformly over a box:
        ``box`` maps variables to ``(low, high)``, and the others span -10
        to 10, XPPAUT's default. Equilibria closer than ``tol`` (the sum of
        the absolute differences) count as one. Returns the equilibria in
        the order found, as a list of :class:`Equilibrium`. XPPAUT's random
        numbers repeat from run to run, so the same calls find the same
        list."""
        n = len(self.variables)
        lo = (ctypes.c_double * n)(*([-10.0] * n))
        hi = (ctypes.c_double * n)(*([10.0] * n))
        for k, (a, b) in (box or {}).items():
            i = self._var_index(k)
            lo[i], hi[i] = float(a), float(b)
        count = self._engine.call("xppe_find_equilibria", int(guesses),
                                  float(tol), lo, hi)
        x = (ctypes.c_double * n)()
        re = (ctypes.c_double * n)()
        im = (ctypes.c_double * n)()
        discrete = self.get("METH") == "discrete"
        found = EquilibriumList(self.variables)
        for i in range(count):
            self._engine.call("xppe_found_equilibrium", i, x, re, im)
            found.append(Equilibrium({v: x[j] for j, v in enumerate(self.variables)},
                                     np.array(re[:]) + 1j * np.array(im[:]),
                                     discrete))
        return found

    # -- phase plane --------------------------------------------------------

    def _var_index(self, name: str) -> int:
        for i, v in enumerate(self.variables):
            if _key(v) == _key(name):
                return i
        raise KeyError(f"no variable {name!r}; available: {', '.join(self.variables)}")

    def _state(self, state=None) -> "ctypes.Array":
        n = len(self.variables)
        x = (ctypes.c_double * n)()
        for i, v in enumerate(self.variables):
            x[i] = float(self.get(v))
        if state is not None:
            if isinstance(state, dict):
                for k, val in state.items():
                    x[self._var_index(k)] = float(val)
            else:
                arr = np.asarray(state, dtype=float).ravel()
                if arr.size != n:
                    raise ValueError(f"expected {n} values, got {arr.size}")
                for i in range(n):
                    x[i] = arr[i]
        return x

    def rhs(self, state=None, t: float = 0.0) -> np.ndarray:
        """The right-hand side f(t, x) of the ODEs, as an array.

        ``state`` is a dict of variable values (others keep their initial
        value) or a full array; by default the initial condition. For maps it
        is the next state."""
        x = self._state(state)
        f = (ctypes.c_double * len(self.variables))()
        self._engine.call("xppe_rhs", float(t), x, f)
        return np.array(f[:])

    def evaluate(self, state=None, t: float = 0.0) -> "Values":
        """Every output column at one state: time, the variables and the
        auxiliary quantities (``aux`` lines), computed as XPPAUT computes a
        row of a run. ``state`` is a dict of variable values (others keep
        their initial value) or a full array; by default the initial
        condition. Returns a dict whose keys ignore case:
        ``m.evaluate({"gam": 1.0, "om": 0})["tens"]``."""
        x = self._state(state)
        out = (ctypes.c_double * len(self.columns))()
        self._engine.call("xppe_columns_at", float(t), x, out, len(self.columns))
        return Values(zip(self.columns, out[:]))

    def nullclines(self, x: str, y: str, xlim, ylim, grid: int = 100):
        """XPPAUT's nullclines in the (x, y) plane over the given window.

        Returns ``(x_nullcline, y_nullcline)``: arrays of line segments with
        shape (n, 4) holding x1, y1, x2, y2. The other variables keep their
        initial values."""
        ix, iy = self._var_index(x), self._var_index(y)
        maxseg = 40000
        out = []
        for which in (0, 1):
            buf = (ctypes.c_float * (4 * maxseg))()
            n = self._engine.call("xppe_nullcline", which, ix, iy,
                                  float(xlim[0]), float(xlim[1]),
                                  float(ylim[0]), float(ylim[1]), int(grid),
                                  buf, maxseg)
            out.append(np.frombuffer(buf, dtype=np.float32,
                                     count=4 * n).reshape(n, 4).astype(float))
        return out[0], out[1]

    def nullcline_sweep(self, parameter: str, values, x: str, y: str, xlim, ylim,
                        grid: int = 100) -> list:
        """The nullclines for each value of a parameter (XPPAUT's Nullcline
        > Freeze > Range): a list of ``(x_nullcline, y_nullcline)`` pairs as
        :meth:`nullclines` returns them, one per value. The parameter is
        put back afterwards."""
        old = float(self.get(parameter))
        out = []
        try:
            for v in values:
                self.set(**{parameter: float(v)})
                out.append(self.nullclines(x, y, xlim, ylim, grid=grid))
        finally:
            self.set(**{parameter: old})
        return out

    def vector_field(self, x: str, y: str, xlim, ylim, n: int = 20):
        """The vector field on an n x n grid: returns X, Y, U, V for
        matplotlib's quiver. Other variables keep their initial values."""
        ix, iy = self._var_index(x), self._var_index(y)
        xs = np.linspace(xlim[0], xlim[1], n)
        ys = np.linspace(ylim[0], ylim[1], n)
        X, Y = np.meshgrid(xs, ys)
        U, V = np.zeros_like(X), np.zeros_like(Y)
        base = np.array(self._state()[:])
        discrete = self.get("METH") == "discrete"
        for i in range(n):
            for j in range(n):
                s = base.copy()
                s[ix], s[iy] = X[i, j], Y[i, j]
                f = self.rhs(s)
                U[i, j] = f[ix] - (s[ix] if discrete else 0)
                V[i, j] = f[iy] - (s[iy] if discrete else 0)
        return X, Y, U, V

    def phase_plane(self, x: str | None = None, y: str | None = None, *,
                    xlim=None, ylim=None, trajectory: bool = True,
                    nullclines: bool = True, field: bool = True,
                    equilibrium: bool = True, frozen=None, ax=None):
        """Draw the phase plane: trajectory, nullclines, direction field and
        the nearby equilibrium, in one call. Returns the matplotlib axes.

        ``frozen`` is a list of earlier nullclines to draw faded, such as
        :meth:`nullcline_sweep` returns."""
        from .plotting import plot_phase_plane
        x = x or self.variables[0]
        y = y or self.variables[1]
        return plot_phase_plane(self, x, y, xlim=xlim, ylim=ylim,
                                trajectory=trajectory, nullclines=nullclines,
                                field=field, equilibrium=equilibrium,
                                frozen=frozen, ax=ax)

    def ic_last(self, *, shift: bool = False) -> "Model":
        """Start the next run where the last one ended (XPPAUT's Initial
        conds > Last, I L): the final state, in full precision, becomes the
        initial condition.

        With ``shift=True`` (Initial conds > Shift, I S) the start time T0
        also becomes the time the run ended, so that the time goes on: it
        matters for models with forcing terms in t.
        """
        self._engine.call("xppe_ic_shift" if shift else "xppe_ic_last")
        return self

    def read_ics(self, path) -> "Model":
        """Read the initial conditions from a text file (XPPAUT's Initial
        conds > File, I F): the first numbers in it, one per variable in
        order, separated by spaces, new lines or commas. A file with fewer
        numbers than variables raises EngineError and changes nothing."""
        self._engine.call("xppe_ic_file", _b(str(Path(path).resolve())))
        return self

    def write_ics(self, path) -> Path:
        """Write the initial conditions to a text file, one per line in the
        order of the variables, exactly: :meth:`read_ics` reads it back."""
        path = Path(path)
        path.write_text("".join(repr(float(self.get(v))) + "\n"
                                for v in self.variables))
        return path

    def ic_formula(self, target: str, formula: str) -> "Model":
        """Set initial conditions from a formula (XPPAUT's Initial conds >
        formUla, I U), evaluated by XPPAUT with the model's parameters and
        functions.

        ``target`` is a variable, ``m.ic_formula("V", "vk + 0.1")``, or a
        range of numbered variables in XPPAUT's array form, where ``[j]`` in
        the formula stands for the index::

            m.ic_formula("u[1..50]", "exp(-([j]-25)^2/20)")
        """
        self._engine.call("xppe_ic_formula", _b(target), _b(formula))
        return self

    def sweep(self, parameter: str, values, *, measure=None,
              continue_runs: bool = False, **run_kw):
        """Run once per value of a parameter or initial condition and collect
        the results: XPPAUT's Initial conds > Range.

        Without ``measure`` it returns a list of Trajectory objects. With a
        function, e.g. ``measure=lambda s: s["V"].max()``, it returns a NumPy
        array of the measure at each value: a brute-force bifurcation
        diagram (``measure=lambda s: m.lyapunov()`` gives the Liapunov
        exponent at each value).

        Each run starts from the initial conditions (XPPAUT's "Use old ICs"
        = Yes), or with ``continue_runs=True`` from where the previous run
        ended ("Use old ICs" = No), which follows an attractor as the
        parameter changes. The parameter and initial conditions are restored
        afterwards.

        Two values can move together along a line (XPPAUT's 2par range,
        Crv): give a tuple of names and pairs of values,
        ``m.sweep(("iapp", "gca"), zip(i_values, g_values))``. For every
        pair on a grid, see :meth:`sweep_grid`."""
        names = tuple(parameter) if isinstance(parameter, (tuple, list)) else (parameter,)
        old = [self.get(n) for n in names]
        ics = {v: float(self.get(v)) for v in self.variables}
        out = []
        try:
            for i, v in enumerate(values):
                if continue_runs and i > 0:
                    self.ic_last()
                for name, x in zip(names, v if len(names) > 1 else (v,)):
                    self.set(**{name: float(x)})
                sol = self.run(**run_kw)
                out.append(measure(sol) if measure else sol)
        finally:
            self.set(**ics)
            for name, o in zip(names, old):
                self.set(**{name: o})
        return np.array(out) if measure else out

    def sweep_grid(self, parameter: str, values, parameter2: str, values2, *,
                   measure=None, continue_runs: bool = False, **run_kw):
        """Run once for every pair of values on a grid: XPPAUT's 2par range
        (Array). The runs go through ``values`` for each of ``values2`` in
        turn, setting ``parameter`` then ``parameter2``; either may be an
        initial condition.

        With a ``measure``, returns a NumPy array of shape
        ``(len(values2), len(values))`` (plus the measure's own shape): a map
        over the two values, such as an amplitude or the Liapunov exponent,
        ready for ``plt.imshow(..., origin="lower")``. Without one, a list of
        rows of Trajectory objects. ``continue_runs`` starts each run where
        the previous one ended, row after row. The values and initial
        conditions are restored afterwards."""
        old, old2 = self.get(parameter), self.get(parameter2)
        ics = {v: float(self.get(v)) for v in self.variables}
        out, k = [], 0
        try:
            for v2 in values2:
                row = []
                for v in values:
                    if continue_runs and k > 0:
                        self.ic_last()
                    self.set(**{parameter: float(v)})
                    self.set(**{parameter2: float(v2)})
                    sol = self.run(**run_kw)
                    row.append(measure(sol) if measure else sol)
                    k += 1
                out.append(row)
        finally:
            self.set(**ics)
            self.set(**{parameter: old})
            self.set(**{parameter2: old2})
        return np.array(out) if measure else out

    def interact(self, *parameters: str, y: str | list | None = None,
                 **ranges):
        """Jupyter sliders for parameters that re-integrate as you drag.

        ``m.interact("I", "eps")`` uses ranges around the current values;
        ``m.interact(I=(0, 2))`` sets them. Needs ipywidgets and matplotlib."""
        import ipywidgets as widgets
        import matplotlib.pyplot as plt

        names = list(parameters) + [k for k in ranges if k not in parameters]
        if not names:
            names = list(self.parameters)[:6]
        sliders = {}
        for name in names:
            v = float(self.get(name))
            lo, hi = ranges.get(name, (min(0.0, 2 * v), max(0.0, 2 * v) or 1.0))
            sliders[name] = widgets.FloatSlider(
                value=v, min=lo, max=hi, step=(hi - lo) / 200 or 0.01,
                description=name, continuous_update=True)
        cols = [y] if isinstance(y, str) else (y or self.variables[:2])

        def update(**vals):
            self.set(**vals)
            sol = self.run()
            fig, ax = plt.subplots(figsize=(8, 3.5))
            for c in cols:
                ax.plot(sol.t, sol[c], label=c)
            ax.set_xlabel("t")
            ax.legend(loc="upper right")
            plt.show()

        return widgets.interactive(update, **sliders)

    # -- housekeeping -------------------------------------------------------

    @property
    def log(self) -> str:
        """What the engine printed during the last call."""
        return self._engine.log

    def close(self) -> None:
        """Free the engine behind this model."""
        self._engine.close()

    def __enter__(self) -> "Model":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def __repr__(self) -> str:
        return (f"<Model {self.name}: {len(self.variables)} variables "
                f"({', '.join(self.variables[:6])}"
                f"{', ...' if len(self.variables) > 6 else ''}), "
                f"{len(self._parameter_names)} parameters>")

    def _repr_html_(self) -> str:
        from . import _html as H
        ics = self.initial_conditions
        variables = [(v, H.fmt(ics[v])) for v in self.variables]
        pars = self.parameters
        params = [(p, H.fmt(pars[p])) for p in self._parameter_names]
        return H.block(
            f"Model {self.name}",
            H.table(["variable", "initial value"], variables[:H.MAX_ROWS],
                    len(variables)),
            H.table(["parameter", "value"], params[:H.MAX_ROWS], len(params)))


def load(path: str | os.PathLike, **kw) -> Model:
    """Load an XPPAUT .ode file. Same as ``Model(path)``."""
    return Model(path, **kw)


class Trajectory:
    """The result of an integration: time and every output column."""

    def __init__(self, data: dict[str, np.ndarray], log: str = ""):
        self._data = data
        self.log = log
        #: The global flag that stopped the run (its number from 1, see
        #: :attr:`Model.flags`), or 0 when it ran to the end.
        self.event = 0

    @property
    def columns(self) -> list[str]:
        return list(self._data)

    @property
    def t(self) -> np.ndarray:
        return self._data["T"]

    def __getitem__(self, name: str) -> np.ndarray:
        for k, v in self._data.items():
            if _key(k) == _key(name):
                return v
        raise KeyError(f"no column {name!r}; available: {', '.join(self._data)}")

    def __contains__(self, name: str) -> bool:
        return any(_key(k) == _key(name) for k in self._data)

    def __len__(self) -> int:
        return len(self.t)

    def to_numpy(self) -> np.ndarray:
        """All columns as one array, one row per output time."""
        return np.column_stack(list(self._data.values()))

    def to_pandas(self):
        """A pandas DataFrame indexed by time."""
        import pandas as pd
        df = pd.DataFrame(self._data)
        return df.set_index("T")

    def save(self, path: str | os.PathLike) -> None:
        """Write the data in XPPAUT's output.dat format."""
        np.savetxt(path, self.to_numpy(), fmt="%.8g")

    def to_csv(self, path: str | os.PathLike) -> None:
        """Write the run as CSV with a header, every number exact (the
        desktop app's ``export FILE.csv`` writes the same file)."""
        from .analysis import write_csv
        write_csv(path, self.columns, self.to_numpy())

    def plot(self, *y: str, x: str = "T", ax=None, **kw):
        """Plot columns against time (or against another column with x=)."""
        from .plotting import plot_trajectory
        return plot_trajectory(self, list(y), x=x, ax=ax, **kw)

    def plot3d(self, x: str, y: str, z: str, ax=None, **kw):
        """A 3-D plot of three columns (XPPAUT's 3D view), with matplotlib."""
        import matplotlib.pyplot as plt
        if ax is None:
            ax = plt.figure().add_subplot(projection="3d")
        ax.plot(self[x], self[y], self[z], **kw)
        ax.set_xlabel(x)
        ax.set_ylabel(y)
        ax.set_zlabel(z)
        return ax

    def __repr__(self) -> str:
        if not len(self):
            return "<Trajectory: empty>"
        return (f"<Trajectory: {len(self)} points, t={self.t[0]:g}..{self.t[-1]:g}, "
                f"columns {', '.join(self.columns)}>")

    def _repr_html_(self) -> str:
        from . import _html as H
        n = len(self)
        if not n:
            return H.block("Trajectory: empty")
        head = f"Trajectory: {n} points, t = {self.t[0]:g} to {self.t[-1]:g}"
        k = min(n, 3)
        idx = list(range(k)) if n <= 2 * k else list(range(k)) + list(range(n - k, n))
        rows = [[H.fmt(self._data[c][i]) for c in self.columns] for i in idx]
        return H.block(head, H.table(self.columns, rows),
                       "<p>... first and last rows</p>" if n > len(idx) else "")


class Values(dict):
    """A dict whose keys ignore case when read, as the model's names do."""

    def __getitem__(self, name):
        try:
            return super().__getitem__(name)
        except KeyError:
            for k in self:
                if isinstance(k, str) and _key(k) == _key(name):
                    return super().__getitem__(k)
            raise KeyError(f"no {name!r}; available: {', '.join(map(str, self))}") from None

    def __contains__(self, name) -> bool:
        return super().__contains__(name) or any(
            isinstance(k, str) and _key(k) == _key(name) for k in self)

    def get(self, name, default=None):
        return self[name] if name in self else default


class _SplitMix64:
    """The SplitMix64 generator. The desktop app draws its random initial
    values with the same one, so a seed gives the same numbers in both."""

    MASK = (1 << 64) - 1

    def __init__(self, seed: int):
        self.state = int(seed) & self.MASK

    def next(self) -> int:
        self.state = (self.state + 0x9E3779B97F4A7C15) & self.MASK
        z = self.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & self.MASK
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & self.MASK
        return z ^ (z >> 31)

    def uniform(self) -> float:
        """A double in [0, 1) from the top 53 bits."""
        return (self.next() >> 11) * (1.0 / 9007199254740992.0)


class RunList(list):
    """Several runs kept together (:meth:`Model.run_many`,
    :meth:`Model.shoot`): a list of :class:`Trajectory`."""

    @property
    def events(self) -> np.ndarray:
        """The global flag that stopped each run (0: it ran to the end)."""
        return np.array([s.event for s in self], dtype=int)

    def counts(self) -> dict[int, int]:
        """How many runs each flag stopped, and (key 0) how many ran to the
        end."""
        out: dict[int, int] = {}
        for e in self.events:
            out[int(e)] = out.get(int(e), 0) + 1
        return dict(sorted(out.items()))

    def plot(self, y: str, x: str = "T", *, ax=None, colors=None, **kw):
        """Every run, ``y`` against ``x``. ``colors`` maps each flag number
        (0 for runs that reached the end) to a colour, to see at a glance
        how each run ended."""
        import matplotlib.pyplot as plt
        ax = ax or plt.gca()
        for s in self:
            c = colors.get(s.event) if colors else None
            ax.plot(s[x], s[y], color=c, **kw)
        ax.set_xlabel("t" if x.upper() == "T" else x)
        ax.set_ylabel(y)
        return ax

    def __repr__(self) -> str:
        return f"<RunList: {len(self)} runs, ended by {self.counts()}>"


@dataclass
class Ensemble:
    """The mean and the variance over many runs of a noisy model, at each
    output time (see :meth:`Model.ensemble`)."""

    mean: Trajectory
    variance: Trajectory
    trials: int

    @property
    def std(self) -> Trajectory:
        """The standard deviation, column by column (time as it is)."""
        data = {k: (v if k == "T" else np.sqrt(np.clip(v, 0, None)))
                for k, v in self.variance._data.items()}
        return Trajectory(data)

    @property
    def t(self) -> np.ndarray:
        return self.mean.t

    def to_table(self) -> "Table":
        """Time, then each column's mean and variance (``V_mean``,
        ``V_var``, ...): the desktop app's ``export FILE.csv analysis``
        after an ensemble writes the same."""
        from .analysis import Table
        data = {"T": self.mean.t}
        for name in self.mean.columns:
            if name != "T":
                data[name + "_mean"] = self.mean[name]
                data[name + "_var"] = self.variance[name]
        return Table(data, title="ensemble of %d runs" % self.trials)

    def to_csv(self, path) -> None:
        self.to_table().to_csv(path)

    def plot(self, *names: str, ax=None, band: float = 1.0):
        """Each column's mean against time, with a band of ``band``
        standard deviations around it."""
        import matplotlib.pyplot as plt
        ax = ax or plt.gca()
        sd = self.std
        for name in names or [c for c in self.mean.columns if c != "T"][:1]:
            line, = ax.plot(self.t, self.mean[name], label=name + " mean")
            ax.fill_between(self.t, self.mean[name] - band * sd[name],
                            self.mean[name] + band * sd[name],
                            color=line.get_color(), alpha=0.25, lw=0)
        ax.set_xlabel("t")
        ax.legend()
        return ax


@dataclass
class Equilibrium:
    """An equilibrium, with the eigenvalues of the Jacobian there."""

    values: dict[str, float]
    eigenvalues: np.ndarray
    discrete: bool = False

    @property
    def unstable_count(self) -> int:
        if self.discrete:
            return int(np.sum(np.abs(self.eigenvalues) > 1))
        return int(np.sum(self.eigenvalues.real > 0))

    @property
    def stable(self) -> bool:
        return self.unstable_count == 0

    @property
    def kind(self) -> str:
        """A plain-language classification, e.g. "stable focus" or "saddle"."""
        ev = self.eigenvalues
        if self.discrete:
            return "stable" if self.stable else "unstable"
        complex_pair = np.any(np.abs(ev.imag) > 1e-10)
        n_unstable = self.unstable_count
        if 0 < n_unstable < len(ev):
            return "saddle-focus" if complex_pair else "saddle"
        base = "focus" if complex_pair else "node"
        return ("stable " if n_unstable == 0 else "unstable ") + base

    def __getitem__(self, name: str) -> float:
        for k, v in self.values.items():
            if _key(k) == _key(name):
                return v
        raise KeyError(name)

    def __repr__(self) -> str:
        vals = ", ".join(f"{k}={v:.6g}" for k, v in self.values.items())
        return f"<Equilibrium {self.kind}: {vals}>"

    def _repr_html_(self) -> str:
        from . import _html as H
        label = "stable" if self.stable else "unstable"
        rows = [[k, H.fmt(v)] for k, v in self.values.items()]
        evs = [[H.fmt(e.real), H.fmt(e.imag)] for e in self.eigenvalues]
        return H.block(f"Equilibrium: {label} ({self.kind})",
                       H.table(["variable", "value"], rows),
                       H.table(["eigenvalue re", "eigenvalue im"], evs))


class EquilibriumList(list):
    """Equilibria found together (:meth:`Model.find_equilibria`): a list of
    :class:`Equilibrium` that can also be written as a table."""

    def __init__(self, variables, items=()):
        super().__init__(items)
        self.variables = list(variables)

    def _table(self):
        n = len(self.variables)
        cols = list(self.variables) + ["unstable"]
        cols += [f"{part}{k}" for k in range(1, n + 1) for part in ("re", "im")]
        rows = [[e.values[v] for v in self.variables] + [e.unstable_count]
                + [c for ev in e.eigenvalues for c in (ev.real, ev.imag)]
                for e in self]
        return cols, rows

    def to_csv(self, path: str | os.PathLike) -> None:
        """One row per equilibrium: the variables, the number of unstable
        eigenvalues, then each eigenvalue's real and imaginary parts (re1,
        im1, re2, ...). The desktop app's ``export FILE.csv equilibria``
        writes the same file."""
        from .analysis import write_csv
        write_csv(path, *self._table())

    def to_pandas(self):
        """The same table as a pandas DataFrame."""
        import pandas as pd
        cols, rows = self._table()
        return pd.DataFrame(rows, columns=cols)

    def __repr__(self) -> str:
        kinds = ", ".join(e.kind for e in self)
        return f"<{len(self)} equilibria: {kinds or 'none'}>"

    def _repr_html_(self) -> str:
        from . import _html as H
        if not len(self):
            return H.block("0 equilibria")
        rows = [[i + 1] + [H.fmt(e.values[v]) for v in self.variables]
                + ["stable" if e.stable else "unstable", e.kind]
                for i, e in enumerate(self)]
        return H.block(f"{len(self)} equilibria",
                       H.table(["#"] + self.variables + ["stability", "kind"],
                               rows[:H.MAX_ROWS], len(rows)))
