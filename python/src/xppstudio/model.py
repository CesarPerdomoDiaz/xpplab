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

__all__ = ["Model", "Trajectory", "Equilibrium", "load", "METHODS"]

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


class Model:
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
        d = Path(tempfile.mkdtemp(prefix="xppstudio-model-"))
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
                    equilibrium: bool = True, ax=None):
        """Draw the phase plane: trajectory, nullclines, direction field and
        the nearby equilibrium, in one call. Returns the matplotlib axes."""
        from .plotting import plot_phase_plane
        x = x or self.variables[0]
        y = y or self.variables[1]
        return plot_phase_plane(self, x, y, xlim=xlim, ylim=ylim,
                                trajectory=trajectory, nullclines=nullclines,
                                field=field, equilibrium=equilibrium, ax=ax)

    def sweep(self, parameter: str, values, *, measure=None, **run_kw):
        """Run once per value of ``parameter`` and collect the results.

        Without ``measure`` it returns a list of Trajectory objects. With a
        function, e.g. ``measure=lambda s: s["V"].max()``, it returns a NumPy
        array of the measure at each value: a brute-force bifurcation
        diagram. The parameter is restored afterwards."""
        old = self.get(parameter)
        out = []
        try:
            for v in values:
                self.set(**{parameter: float(v)})
                sol = self.run(**run_kw)
                out.append(measure(sol) if measure else sol)
        finally:
            self.set(**{parameter: old})
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


def load(path: str | os.PathLike, **kw) -> Model:
    """Load an XPPAUT .ode file. Same as ``Model(path)``."""
    return Model(path, **kw)


class Trajectory:
    """The result of an integration: time and every output column."""

    def __init__(self, data: dict[str, np.ndarray], log: str = ""):
        self._data = data
        self.log = log

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

    def plot(self, *y: str, x: str = "T", ax=None, **kw):
        """Plot columns against time (or against another column with x=)."""
        from .plotting import plot_trajectory
        return plot_trajectory(self, list(y), x=x, ax=ax, **kw)

    def __repr__(self) -> str:
        if not len(self):
            return "<Trajectory: empty>"
        return (f"<Trajectory: {len(self)} points, t={self.t[0]:g}..{self.t[-1]:g}, "
                f"columns {', '.join(self.columns)}>")


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
