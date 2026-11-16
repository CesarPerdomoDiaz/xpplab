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
from .sensitivity import Sensitivity

__all__ = ["Model", "Trajectory", "RunList", "Values", "Equilibrium",
           "EquilibriumList", "load", "METHODS", "Sensitivity"]

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

    Each model has its own copy of XPPAUT's engine, so several can be open
    at once. [`xs.load`][xpplab.load] is the usual way to make one.

    Args:
        path: the .ode file. Files it includes, tables and data files are
            looked up in its folder.
        options: XPPAUT command-line options, such as ``["-silent"]``.
        engine_library: the engine's shared library; by default the one
            in the package, or ``$XPPLAB_ENGINE``.

    Attributes:
        path (Path): the .ode file, as an absolute path.
        variables (list[str]): the state variables, in the file's order and
            in capitals, as XPPAUT stores them.
        columns (list[str]): the output columns of a run: ``T``, the
            variables, then the auxiliary quantities (``aux``).
        sets (list[str]): the named parameter sets (``set name {...}``).
        auto (Auto): AUTO's continuation of this model; see
            [`Auto`][xpplab.auto.Auto].
        load_log (str): what XPPAUT printed while reading the file.

    Raises:
        FileNotFoundError: there is no such file.
        EngineError: XPPAUT could not read the file; the message gives the
            line.

    Examples:
        >>> m = xs.load(xs.example("lecar.ode"))
        >>> m
        <Model lecar.ode: 2 variables (V, W), 12 parameters>
        >>> m.columns
        ['T', 'V', 'W', 'CUR', 'ICA', 'IK', 'RATE']
        >>> m.sets
        ['vvst', 'pp', 'snic', 'hopf', 'hom', 'cool']

        A model can be used in a ``with`` block, which frees its engine at
        the end:

        >>> with xs.load(xs.example("lecar.ode")) as m:
        ...     sol = m.run(iapp=0.1, total=100)
        >>> len(sol)
        2001

    See also:
        [open](../reference/open.md) and [new](../reference/new.md) in the
        command window; [`from_text`][xpplab.Model.from_text]; the
        [ODE file format](../manual/odefile.md).
    """

    def __init__(self, path: str | os.PathLike, *, options: list[str] = (),
                 engine_library: str | None = None):
        self.path = Path(path).resolve()
        self._options = list(options)
        self._engine_library = engine_library
        if not self.path.exists():
            raise FileNotFoundError(self.path)
        self._engine = E.Engine(engine_library)
        self._engine.cwd = self.path.parent
        opts = [_b(o) for o in options]
        argv = (ctypes.c_char_p * max(1, len(opts)))(*opts)
        try:
            self._engine.call("xppe_load", os.fsencode(self.path.name), len(opts), argv)
        except E.EngineError:
            self._engine.close()
            raise
        self.load_log = self._engine.log
        self._engine.call("xppe_auto_set_workdir",
                          os.fsencode(self._engine.workdir))
        self.variables = self._engine.names(E.VARIABLES)
        self.columns = self._engine.names(E.COLUMNS)
        self.sets = self._engine.names(E.SETS)
        self._parameter_names = self._engine.names(E.PARAMETERS)
        from .auto import Auto  # avoid an import cycle
        self.auto = Auto(self)

    @classmethod
    def from_text(cls, text: str, name: str = "model.ode", **kw) -> "Model":
        """Load a model from the text of an .ode file.

        The text is written to a new temporary folder, which is where files
        the model writes or reads by a relative name are.

        Args:
            text: the model, as it would be in an .ode file.
            name: the file name to give it, which the model's
                [`name`][xpplab.Model.name] reports.
            **kw: passed on to [`Model`][xpplab.Model].

        Returns:
            The loaded model.

        Raises:
            EngineError: XPPAUT could not read the text; the message gives
                the line.

        Examples:
            >>> m = xs.Model.from_text('''
            ... x' = -a*x
            ... par a=1
            ... init x=1
            ... @ total=2
            ... done
            ... ''', name="decay.ode")
            >>> m
            <Model decay.ode: 1 variables (X), 1 parameters>
            >>> sol = m.run()
            >>> print(f"{sol['X'][-1]:.4f}")
            0.1353

        See also:
            [new](../reference/new.md) in the command window;
            [`xs.load`][xpplab.load].
        """
        d = Path(tempfile.mkdtemp(prefix="xpplab-model-"))
        p = d / name
        p.write_text(text)
        return cls(p, **kw)

    # -- names and values -------------------------------------------------

    @property
    def name(self) -> str:
        """The file name of the model, without its folder.

        Examples:
            >>> xs.load(xs.example("lecar.ode")).name
            'lecar.ode'

        See also:
            [`path`][xpplab.Model] (an attribute), the whole path.
        """
        return self.path.name

    @property
    def parameters(self) -> _NamedValues:
        """The parameters, as a live dictionary.

        Reading an entry asks the engine for the current value; writing one
        sets it, as [`set`][xpplab.Model.set] does. Names ignore case. The
        keys are in the file's order. Parameters cannot be added or removed.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> m.parameters
            {iapp=0.05, phi=0.333, v1=-0.01, v2=0.15, v3=0.1, v4=0.145, gca=1, vk=-0.7, vl=-0.5, gk=2, gl=0.5, om=1}
            >>> m.parameters["IAPP"] = 0.1
            >>> m.parameters["iapp"]
            0.1
            >>> m.parameters["gna"]
            Traceback (most recent call last):
            KeyError: "no parameter named 'gna'; available: iapp, phi, v1, v2, v3, v4, gca, vk, vl, gk, gl, om"

        See also:
            [set](../reference/set.md) and [whos](../reference/whos.md) in
            the command window; [`set`][xpplab.Model.set],
            [`initial_conditions`][xpplab.Model.initial_conditions].
        """
        return _NamedValues(self, self._parameter_names, "parameter")

    @property
    def initial_conditions(self) -> _NamedValues:
        """The initial conditions of the state variables, as a live
        dictionary. ``m.ics`` is the same.

        Each run starts from these values. Reading an entry asks the engine;
        writing one sets it, as [`set`][xpplab.Model.set] does. Names ignore
        case.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> m.initial_conditions
            {V=-0.144, W=0.03}
            >>> m.ics["v"] = -0.3
            >>> m.ics["V"]
            -0.3

        See also:
            [ic](../reference/ic.md) in the command window;
            [`ic_last`][xpplab.Model.ic_last],
            [`read_ics`][xpplab.Model.read_ics].
        """
        return _NamedValues(self, self.variables, "variable")

    ics = initial_conditions

    @property
    def options(self) -> dict[str, float | str]:
        """The numerical options, as a snapshot.

        These are the options that [`get`][xpplab.Model.get] reads: the
        integration (``TOTAL``, ``DT``, ``METH``, ``TOL``, ...), the
        Poincaré map (``POIMAP``, ...) and Newton's method for equilibria
        (``NEWT_TOL``, ...). The dictionary is a copy: change an option
        with [`set`][xpplab.Model.set].

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> o = m.options
            >>> o["TOTAL"], o["DT"], o["METH"]
            (30.0, 0.05, 'rungekutta')

        See also:
            [set](../reference/set.md) in the command window, which lists
            every option; [`get`][xpplab.Model.get].
        """
        out: dict[str, float | str] = {}
        for o in OPTIONS:
            try:
                out[o] = self.get(o)
            except E.EngineError:
                pass
        return out

    def get(self, name: str) -> float | str:
        """The value of a parameter, an initial condition or a numerical
        option.

        Args:
            name: the name, in any case: ``"iapp"``, ``"V"``, ``"total"``.

        Returns:
            The value, as a float. ``METH`` gives the method's name, such as
            ``"rungekutta"``.

        Raises:
            EngineError: nothing has that name.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> m.get("iapp"), m.get("v"), m.get("total"), m.get("meth")
            (0.05, -0.144, 30.0, 'rungekutta')

        See also:
            [get](../reference/get.md) in the command window;
            [`set`][xpplab.Model.set], [`options`][xpplab.Model.options].
        """
        v = ctypes.c_double()
        self._engine.call("xppe_get", _b(name), ctypes.byref(v))
        if _key(name) == "METH":
            i = int(v.value)
            return _METHOD_BY_INDEX[i] if 0 <= i < len(_METHOD_BY_INDEX) else i
        return v.value

    def set(self, **values) -> "Model":
        """Set parameters, initial conditions or numerical options.

        This works like an ``@`` line of an .ode file: names ignore case,
        and the values stay until they are set again. The method, ``meth``,
        takes a name: one of XPPAUT's (``"rungekutta"``, ``"stiff"``,
        ``"cvode"``, ...) or an alias (``"rk4"``, ``"dopri5"``); the
        dictionary ``xs.METHODS`` has them all.

        Args:
            **values: names and values: parameters, variables (their initial
                conditions) or options (``total``, ``dt``, ``meth``, ...).

        Returns:
            The model itself, so that calls chain:
            ``m.set(iapp=0.1).run()``.

        Raises:
            EngineError: nothing has one of the names. The values before it
                are set.
            ValueError: ``meth`` names no method.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> m.set(iapp=0.1, total=200, meth="stiff")
            <Model lecar.ode: 2 variables (V, W), 12 parameters>
            >>> m.get("iapp"), m.get("total"), m.get("meth")
            (0.1, 200.0, 'stiff')

        See also:
            [set](../reference/set.md) in the command window;
            [`get`][xpplab.Model.get], [`parameters`][xpplab.Model.parameters],
            [`apply_set`][xpplab.Model.apply_set].
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
        """Apply a named parameter set of the .ode file.

        A line ``set hopf {v3=0, v4=.3, gca=1.1, phi=.2}`` in the file
        names a set; this sets those values, as if by
        [`set`][xpplab.Model.set]. The names are in the model's ``sets``.

        Args:
            name: the set's name.

        Returns:
            The model itself, so that calls chain.

        Raises:
            EngineError: the file has no set of that name.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> m.sets
            ['vvst', 'pp', 'snic', 'hopf', 'hom', 'cool']
            >>> m.apply_set("hopf").get("phi")
            0.2

        See also:
            [use](../reference/use.md) and [sets](../reference/sets.md) in
            the command window; [`set`][xpplab.Model.set].
        """
        self._engine.call("xppe_apply_set", _b(name))
        return self

    # -- computation --------------------------------------------------------

    def run(self, **values) -> "Trajectory":
        """Integrate from the current initial conditions.

        The run uses the model's method and options, from ``T0`` for
        ``TOTAL`` time units, and keeps every ``NOUT``-th step. XPPAUT
        stores the points in single precision, so they have about seven
        significant digits.

        A run stops early when a variable leaves ``BOUND`` (the model's
        [`log`][xpplab.Model.log] then says which), or when a global flag
        with ``{arret=1}`` fires (its number is then in the result's
        ``event``; see [`flags`][xpplab.Model.flags]).

        Args:
            **values: set first, as by [`set`][xpplab.Model.set], and kept
                afterwards: ``m.run(iapp=0.2, total=100)`` is
                ``m.set(iapp=0.2, total=100).run()``.

        Returns:
            The run: time and every output column. See
            [`Trajectory`][xpplab.Trajectory].

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> sol
            <Trajectory: 2001 points, t=0..100, columns T, V, W, CUR, ICA, IK, RATE>
            >>> print(f"V goes from {sol['V'].min():.4f} to {sol['V'].max():.4f}")
            V goes from -0.3804 to 0.2615
            >>> m.get("iapp"), m.get("total")
            (0.1, 100.0)

        See also:
            [run](../reference/run.md) in the command window;
            [`continue_run`][xpplab.Model.continue_run],
            [`run_many`][xpplab.Model.run_many],
            [`sweep`][xpplab.Model.sweep].
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
        """The model's global flags, in the file's order.

        A line ``global sign condition {events}`` of the .ode file is a
        flag: when ``condition`` crosses zero, the events happen. Each flag
        is given as ``(condition, sign)``: sign 1 when the condition crosses
        zero going up, -1 going down, 0 either way. A run stopped by one of
        them (an ``{arret=1}`` event) has its number, counted from 1, in the
        run's ``event``.

        Examples:
            >>> m = xs.Model.from_text('''
            ... x' = 1
            ... global 1 x-2 {arret=1}
            ... @ total=10
            ... done
            ... ''')
            >>> m.flags
            [('x-2', 1)]
            >>> sol = m.run()
            >>> sol.event, float(sol.t[-1])
            (1, 2.0)

        See also:
            [`run_many`][xpplab.Model.run_many], whose result counts the
            runs each flag stopped; the [ODE file format](../manual/odefile.md).
        """
        out = []
        sign = ctypes.c_int()
        for i in range(self._engine.call("xppe_flag_count", check=False)):
            cond = self._engine._lib.xppe_flag_condition(i, ctypes.byref(sign))
            out.append((E._text(cond).strip(), sign.value))
        return out

    def run_many(self, initial, **values) -> "RunList":
        """Run once from each of several initial conditions and keep every
        run, as the desktop app's *Many initial values* does.

        The initial conditions are restored afterwards.

        Args:
            initial: the starting states: a list of dictionaries of variable
                values (the variables left out keep their initial value), or
                an array with one row per run and one column per variable.
                [`sample_initial`][xpplab.Model.sample_initial] draws them at
                random.
            **values: set first, as by [`set`][xpplab.Model.set], and kept.

        Returns:
            The runs, in order, as a [`RunList`][xpplab.RunList]: a list of
            [`Trajectory`][xpplab.Trajectory] with ``events``, the global
            flag that stopped each run (0 for none), and ``counts()``.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> runs = m.run_many([{"V": -0.3}, {"V": 0.2}], iapp=0.1, total=50)
            >>> runs
            <RunList: 2 runs, ended by {0: 2}>
            >>> [round(float(r["V"][0]), 4) for r in runs]
            [-0.3, 0.2]
            >>> m.get("V")
            -0.144

        See also:
            [many](../reference/many.md) in the command window;
            [`sample_initial`][xpplab.Model.sample_initial],
            [`run`][xpplab.Model.run].
        """
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
        """Initial conditions drawn at random, uniformly, in a box.

        The draws come from a SplitMix64 generator started at ``seed``, the
        one the desktop app uses, so the same call gives the same states in
        both.

        Args:
            n: how many states to return.
            box: the variables to draw, each with its range ``(low, high)``.
                The other variables keep their initial value.
            require: output columns (variables or auxiliaries), each with a
                range ``(low, high)``. A draw is kept only if every one of
                them is in its range there: ``require={"tens": (0, inf)}``
                keeps the states where an auxiliary ``tens`` is positive.
            seed: where the generator starts.
            tries: how many draws to make at most.

        Returns:
            ``n`` dictionaries of variable values, ready for
            [`run_many`][xpplab.Model.run_many].

        Raises:
            ValueError: fewer than ``n`` draws met ``require`` in ``tries``
                draws.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> starts = m.sample_initial(3, {"V": (-0.5, 0.5), "W": (0, 0.5)})
            >>> [round(s["V"], 4) for s in starts]
            [0.0666, 0.471, -0.0557]
            >>> starts = m.sample_initial(3, {"V": (-0.5, 0.5)},
            ...                           require={"ICA": (-0.1, 0)})
            >>> all(-0.1 <= m.evaluate(s)["ICA"] <= 0 for s in starts)
            True

        See also:
            [many](../reference/many.md) in the command window;
            [`run_many`][xpplab.Model.run_many].
        """
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
        """Integrate more from where a run ended, as XPPAUT's Continue does.

        The initial conditions, ``T0`` and ``TOTAL`` are left as they were.

        Args:
            trajectory: the run to continue. An empty one, or ``None``, gives
                a plain [`run`][xpplab.Model.run].
            time: how long to go on; by default ``TOTAL``.

        Returns:
            ``trajectory`` with the new points appended: one
            [`Trajectory`][xpplab.Trajectory] from the first run's start to
            the new end.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> longer = m.continue_run(sol, 50)
            >>> longer
            <Trajectory: 3001 points, t=0..150, columns T, V, W, CUR, ICA, IK, RATE>
            >>> m.get("total"), m.get("t0")
            (100.0, 0.0)

        See also:
            [continue](../reference/continue.md) in the command window;
            [`run`][xpplab.Model.run], [`ic_last`][xpplab.Model.ic_last].
        """
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
        out = Trajectory(data, log=new.log)
        out.params = new.params
        return out

    def sensitivity(self, params=None, **values) -> "Sensitivity":
        """How the run changes with the parameters: $S(t) = \\partial x(t) / \\partial p$.

        The variational equations $\\dot S = (\\partial f/\\partial x)\\,S +
        \\partial f/\\partial p$ are built from the model's formulas by
        symbolic differentiation, added to the model, and integrated with it
        by the model's own method and options, from the current initial
        conditions with $S = 0$.

        Args:
            params: a parameter's name, or a list of them; by default every
                parameter.
            **values: set first, as by [`set`][xpplab.Model.set], and kept.

        Returns:
            A [`Sensitivity`][xpplab.Sensitivity]: ``s["a"]`` is an array
            with one row per output time (``s.t``) and one column per
            variable.

        Raises:
            ValueError: the model has tables, delays, noise, is a map, or
                uses a function the symbolic derivative does not know.

        Examples:
            For $x' = -a x$ with $x(0) = 1$, $x = e^{-at}$ and
            $\\partial x/\\partial a = -t e^{-at}$, which is $-e^{-1}$ at
            $t = 1$:

            >>> m = xs.Model.from_text('''
            ... x' = -a*x
            ... par a=1
            ... init x=1
            ... @ total=2
            ... done
            ... ''')
            >>> s = m.sensitivity("a")
            >>> s
            Sensitivity(41 points, variables X; parameters A)
            >>> print(f"{s.t[20]:g} {s['a'][20, 0]:.6f} {-np.exp(-1):.6f}")
            1 -0.367879 -0.367879

        See also:
            [`linearize`][xpplab.Model.linearize]; the
            [MCP tool](../agents.md) ``sensitivity``.
        """
        from . import sensitivity as _sens
        return _sens.run(self, params, **values)

    def ensemble(self, trials: int, **values) -> "Ensemble":
        """Run a noisy model many times and average, as XPPAUT's
        *Numerics > stocHastic > Compute* does.

        Every run starts from the same initial conditions while the random
        numbers (``wiener`` and ``ran()``) go on, so the runs differ. XPPAUT
        computes the mean and the variance of every column at each output
        time, as its batch mode does with ``@ stoch=1``, in single
        precision. The last run stays as the engine's run
        ([`last_run`][xpplab.Model.last_run]).

        Args:
            trials: how many runs.
            **values: set first, as by [`set`][xpplab.Model.set], and kept.

        Returns:
            An [`Ensemble`][xpplab.Ensemble]: ``mean`` and ``variance``, each
            a [`Trajectory`][xpplab.Trajectory], ``std`` and ``trials``.

        Examples:
            An Ornstein-Uhlenbeck process, $dx = -x\\,dt + s\\,dW$ with
            $x(0) = 1$: its mean is $e^{-t}$.

            >>> m = xs.Model.from_text('''
            ... x' = -x + s*w
            ... wiener w
            ... par s=0.5
            ... init x=1
            ... @ total=5, dt=0.01, nout=100
            ... done
            ... ''')
            >>> e = m.ensemble(200)
            >>> e.mean
            <Trajectory: 6 points, t=0..5, columns T, X>
            >>> bool(abs(e.mean["X"][1] - np.exp(-1)) < 0.05)
            True

        See also:
            [ensemble](../reference/ensemble.md) in the command window;
            [`run`][xpplab.Model.run].
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
        """The engine's most recent run, wherever it came from.

        Some methods integrate as a step of their work and return something
        else; this gives their run. [`phase_response`][xpplab.Model.phase_response],
        for example, leaves one period of the limit cycle.

        Returns:
            The run, as a [`Trajectory`][xpplab.Trajectory].

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> m.last_run()
            <Trajectory: 2001 points, t=0..100, columns T, V, W, CUR, ICA, IK, RATE>

        See also:
            [`run`][xpplab.Model.run].
        """
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
        out = Trajectory(data, log=self._engine.log)
        with self._engine.batch():
            out.params = {p: float(self.get(p)) for p in self._parameter_names}
        return out

    def equilibrium(self, guess: dict | None = None, *,
                    set_initial: bool = False) -> "Equilibrium":
        """Find an equilibrium with Newton's method, as XPPAUT's *Sing pts*
        does, with its eigenvalues.

        Newton's method uses the options ``NEWT_TOL``, ``NEWT_ITER`` and
        ``JAC_EPS``. For a map (``meth="discrete"``) it finds a fixed point.

        Args:
            guess: where Newton's method starts, as a dictionary of variable
                values; the variables left out start at their initial value.
                By default the initial conditions.
            set_initial: make the equilibrium the new initial condition,
                which is how AUTO continuations usually start.

        Returns:
            An [`Equilibrium`][xpplab.Equilibrium]: its ``values``,
            ``eigenvalues``, ``kind`` and ``stable``.

        Raises:
            EngineError: Newton's method did not converge.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> eq = m.equilibrium()
            >>> eq
            <Equilibrium saddle: V=-0.142535, W=0.034049>
            >>> np.round(eq.eigenvalues.real, 4)
            array([ 1.03 , -0.301])
            >>> m.equilibrium({"V": -0.4})
            <Equilibrium stable node: V=-0.382505, W=0.0012856>

        See also:
            [eq](../reference/eq.md) in the command window;
            [`find_equilibria`][xpplab.Model.find_equilibria],
            [`equilibrium_sweep`][xpplab.Model.equilibrium_sweep],
            [`linearize`][xpplab.Model.linearize].
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
        """Linearise at an equilibrium, $\\dot{\\delta x} = A\\,\\delta x +
        B\\,\\delta u$, and test controllability.

        $A$ is the Jacobian of the right-hand side and $B$ its derivative
        with respect to each input, both by central differences of
        [`rhs`][xpplab.Model.rhs] extrapolated to zero step (Ridders'
        method, which picks its own step).

        Args:
            at: the state, as a dictionary of variable values (the variables
                left out keep their initial value); by default the
                equilibrium [`equilibrium`][xpplab.Model.equilibrium] finds.
            inputs: the parameters that act as controls, the columns of $B$.
            step: a fixed relative step for the differences, in place of
                Ridders' extrapolation.
            tol: the cutoff for the singular values of the controllability
                matrix in its rank.

        Returns:
            A [`Linearization`][xpplab.Linearization]: ``A``, ``B``,
            ``eigenvalues``, ``controllability`` (the matrix
            $[B, AB, \\ldots]$), ``rank`` and ``controllable``, and the design
            methods ``place_poles``, ``lqr`` and ``root_locus``.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> lin = m.linearize(inputs=["iapp"])
            >>> lin
            Linearization(n=2, inputs=['iapp'], rank=2, controllable=True)
            >>> np.round(lin.A, 4)
            array([[ 1.1843, -1.1149],
                   [ 0.207 , -0.4564]])
            >>> lin.B
            array([[1.],
                   [0.]])

        See also:
            [linearize](../reference/linearize.md) in the command window;
            [`equilibrium`][xpplab.Model.equilibrium]; the
            [control design guide](control.md).
        """
        from .control import linearize
        return linearize(self, at, inputs, step=step, tol=tol)

    def manifolds(self, guess: dict | None = None):
        """The stable and unstable manifolds of a saddle, as XPPAUT's
        *Sing pts* draws them.

        If the equilibrium is a saddle with a one-dimensional stable or
        unstable direction, each branch is integrated from just off the
        equilibrium along its eigenvector: forward in time for the unstable
        branches, backward for the stable ones. Each runs for ``TOTAL``.

        Args:
            guess: where Newton's method starts, as for
                [`equilibrium`][xpplab.Model.equilibrium].

        Returns:
            ``(equilibrium, branches)``: the [`Equilibrium`][xpplab.Equilibrium]
            and a list of ``(kind, trajectory)``, kind ``"stable"`` or
            ``"unstable"``. The list is empty for an equilibrium that is not
            a saddle.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> eq, branches = m.manifolds()
            >>> eq
            <Equilibrium saddle: V=-0.142535, W=0.034049>
            >>> [kind for kind, run in branches]
            ['unstable', 'unstable', 'stable', 'stable']

        See also:
            [`shoot`][xpplab.Model.shoot], which also works for nodes;
            [`phase_plane`][xpplab.Model.phase_plane].
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
        """The eigenvector of a matrix for one of its real eigenvalues, by
        XPPAUT's own inverse iteration, the one *Sing pts* uses to draw
        invariant sets.

        Args:
            A: a square matrix.
            eigenvalue: one of its real eigenvalues.

        Returns:
            The eigenvector, of length 1. Its sign is the one the iteration
            gives.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> m.eigenvector([[2, 0], [1, -1]], 2)
            array([0.9486833 , 0.31622777])

        See also:
            [`linearize`][xpplab.Model.linearize],
            [`shoot`][xpplab.Model.shoot].
        """
        A = np.asarray(A, dtype=float)
        n = A.shape[0]
        a = (ctypes.c_double * (n * n))(*A.ravel())
        v = (ctypes.c_double * n)()
        self._engine.call("xppe_eigenvector", n, a, float(eigenvalue), v)
        return np.array(v[:])

    def shoot(self, at=None, *, eps: float = 1e-5, total: float | None = None,
              directions: str = "unstable") -> "RunList":
        """Integrate away from an equilibrium along its real eigenvectors,
        both ways, as XPPAUT's *Initial conds > sHoot* does after *Sing pts*:
        the start of its unstable or stable manifolds.

        Each real eigenvector $v$ of the Jacobian (from
        [`linearize`][xpplab.Model.linearize]) with an eigenvalue of the
        chosen sign gives two runs, from $x_e + \\varepsilon v$ and
        $x_e - \\varepsilon v$: forward in time for an unstable direction,
        backward for a stable one. A node's two unstable directions both
        count, unlike XPPAUT's invariant sets, which are drawn for saddles
        only. The initial conditions, ``TOTAL`` and ``DT`` are restored
        afterwards.

        Args:
            at: an [`Equilibrium`][xpplab.Equilibrium], or where Newton's
                method starts, as for [`equilibrium`][xpplab.Model.equilibrium].
            eps: how far from the equilibrium each run starts.
            total: how long each run is; by default ``TOTAL``.
            directions: ``"unstable"`` (eigenvalues > 0), ``"stable"``
                (eigenvalues < 0) or ``"all"``.

        Returns:
            A [`RunList`][xpplab.RunList], two runs per direction, with
            ``equilibrium`` and ``directions``: ``(eigenvalue, sign)`` for
            each run.

        Raises:
            ValueError: ``directions`` is not one of the three.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> runs = m.shoot()
            >>> runs
            <RunList: 2 runs, ended by {0: 2}>
            >>> [(round(lam, 4), sign) for lam, sign in runs.directions]
            [(1.0289, 1), (1.0289, -1)]

        See also:
            [shoot](../reference/shoot.md) in the command window;
            [`manifolds`][xpplab.Model.manifolds].
        """
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

    def root_locus(self, parameter: str, values, steps: int = 101, *,
                   guess: dict | None = None):
        """How the eigenvalues of an equilibrium move as a parameter
        changes: a root locus over the parameter.

        The parameter takes ``steps`` values evenly from ``low`` to ``high``
        and, as in [`equilibrium_sweep`][xpplab.Model.equilibrium_sweep],
        Newton's method follows the equilibrium from one value to the next.
        The eigenvalues are matched from step to step. The parameter is
        restored afterwards. The engine computes the locus, so the desktop
        app's gives the same numbers.

        Args:
            parameter: the parameter to vary.
            values: ``(low, high)``.
            steps: how many values.
            guess: where Newton's method starts at the first value, as for
                [`equilibrium`][xpplab.Model.equilibrium].

        Returns:
            A [`RootLocus`][xpplab.RootLocus]: ``poles`` has one row per
            value and one column per eigenvalue; ``crossings()`` gives the
            values where one crosses into or out of the stable region (a
            Hopf bifurcation, say); ``plot()`` draws the locus in the
            complex plane.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> rl = m.root_locus("iapp", (0, 0.4), 41)
            >>> rl
            RootLocus(iapp from 0 to 0.4, 41 steps, 2 branches)
            >>> [(round(p, 4), complex(round(z.real, 4), round(z.imag, 4)))
            ...  for p, z in rl.crossings()]
            [(0.2622, 0.4341j), (0.2622, -0.4341j)]

        See also:
            [locus](../reference/locus.md) in the command window;
            [`Linearization.root_locus`][xpplab.Linearization.root_locus],
            a locus over a feedback gain;
            [`auto.equilibria`][xpplab.auto.Auto.equilibria], which finds
            the Hopf point exactly.
        """
        from .control import root_locus
        return root_locus(self, parameter, values, steps, guess)

    def equilibrium_sweep(self, parameter: str, values, *,
                          guess: dict | None = None):
        """Follow an equilibrium as a parameter changes, as XPPAUT's
        *Sing pts > Range* does: a quick diagram of steady states without
        AUTO.

        For each value in turn the parameter takes it, and Newton's method
        starts from the equilibrium found at the value before. It cannot go
        round a fold: past one, it jumps to another branch or fails. The
        parameter is restored afterwards.

        Args:
            parameter: the parameter to vary.
            values: its values, in order.
            guess: where Newton's method starts at the first value, as for
                [`equilibrium`][xpplab.Model.equilibrium].

        Returns:
            A [`Table`][xpplab.Table]: the parameter, each variable, then
            ``unstable`` and ``stable``, the numbers of eigenvalues with
            positive and negative real parts (for maps, outside and inside
            the unit circle). Where Newton's method failed, the row holds
            NaN.

        Examples:
            The saddle of ``lecar.ode`` meets a fold between 0.05 and 0.1,
            and the sweep jumps to the upper branch:

            >>> m = xs.load(xs.example("lecar.ode"))
            >>> t = m.equilibrium_sweep("iapp", [0, 0.05, 0.1, 0.15])
            >>> t
            <Table Equilibria as iapp changes: 4 rows, columns iapp, V, W, unstable, stable>
            >>> np.round(t.to_numpy(), 4)
            array([[ 0.    , -0.079 ,  0.078 ,  1.    ,  1.    ],
                   [ 0.05  , -0.1425,  0.034 ,  1.    ,  1.    ],
                   [ 0.1   ,  0.0443,  0.3167,  2.    ,  0.    ],
                   [ 0.15  ,  0.0574,  0.3573,  2.    ,  0.    ]])

        See also:
            [`equilibrium`][xpplab.Model.equilibrium],
            [`root_locus`][xpplab.Model.root_locus];
            [`auto.equilibria`][xpplab.auto.Auto.equilibria], which follows
            the branch round its folds.
        """
        name = self.parameters._lookup(parameter)
        vals = [float(v) for v in values]
        buf = (ctypes.c_double * max(len(vals), 1))(*vals)
        self._engine.call("xppe_equilibrium_sweep", _b(name), buf, len(vals),
                          self._state(guess))
        return self._result(f"Equilibria as {name} changes")

    def find_equilibria(self, guesses: int = 100, box: dict | None = None,
                        tol: float = 0.001) -> "EquilibriumList":
        """Every equilibrium Newton's method finds from random starting
        points, as XPPAUT's *Sing pts > Monte Carlo* does.

        XPPAUT's random numbers repeat from run to run, so the same calls
        find the same list.

        Args:
            guesses: how many starting points.
            box: the variables' ranges, ``{"V": (low, high)}``; the
                variables left out span -10 to 10, XPPAUT's default.
            tol: equilibria closer than this (the sum of the absolute
                differences) count as one.

        Returns:
            The equilibria in the order found, as an
            [`EquilibriumList`][xpplab.EquilibriumList]: a list of
            [`Equilibrium`][xpplab.Equilibrium] that can be written as a
            table.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> found = m.find_equilibria()
            >>> found
            <3 equilibria: unstable focus, stable node, saddle>
            >>> found[1].kind, round(found[1]["V"], 4)
            ('stable node', -0.3825)

        See also:
            [eq all](../reference/eq-all.md) in the command window;
            [`equilibrium`][xpplab.Model.equilibrium].
        """
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
        """The right-hand side $f(t, x)$ of the equations, evaluated by
        XPPAUT.

        Args:
            state: a dictionary of variable values (the variables left out
                keep their initial value) or an array of every variable; by
                default the initial conditions.
            t: the time.

        Returns:
            An array with one entry per variable: the derivatives, or for a
            map the next state.

        Raises:
            ValueError: an array of the wrong length.
            KeyError: a dictionary names no variable.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> m.rhs()
            array([0.00278328, 0.0015529 ])
            >>> m.rhs({"V": 0})
            array([0.29128404, 0.0604044 ])

        See also:
            [`evaluate`][xpplab.Model.evaluate], which also gives the
            auxiliary quantities; [`linearize`][xpplab.Model.linearize].
        """
        x = self._state(state)
        f = (ctypes.c_double * len(self.variables))()
        self._engine.call("xppe_rhs", float(t), x, f)
        return np.array(f[:])

    def evaluate(self, state=None, t: float = 0.0) -> "Values":
        """Every output column at one state, as XPPAUT computes a row of a
        run: time, the variables and the auxiliary quantities (``aux``).

        Args:
            state: a dictionary of variable values (the variables left out
                keep their initial value) or an array of every variable; by
                default the initial conditions.
            t: the time.

        Returns:
            A [`Values`][xpplab.Values]: a dictionary of the columns whose
            keys ignore case.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> row = m.evaluate()
            >>> list(row)
            ['T', 'V', 'W', 'CUR', 'ICA', 'IK', 'RATE']
            >>> round(row["ica"], 6)
            -0.164143

        See also:
            [`rhs`][xpplab.Model.rhs],
            [`sample_initial`][xpplab.Model.sample_initial], whose
            ``require`` tests these columns.
        """
        x = self._state(state)
        out = (ctypes.c_double * len(self.columns))()
        self._engine.call("xppe_columns_at", float(t), x, out, len(self.columns))
        return Values(zip(self.columns, out[:]))

    def nullclines(self, x: str, y: str, xlim, ylim, grid: int = 100):
        """The nullclines in a plane of two variables, computed by XPPAUT's
        own routine.

        The ``x`` nullcline is where $\\dot x = 0$, the ``y`` nullcline where
        $\\dot y = 0$. XPPAUT finds them on a grid of ``grid`` by ``grid``
        cells, as line segments. The other variables keep their initial
        values.

        Args:
            x: the variable along the horizontal axis.
            y: the variable along the vertical axis.
            xlim: ``(low, high)`` of ``x``.
            ylim: ``(low, high)`` of ``y``.
            grid: the number of cells along each side.

        Returns:
            ``(x_nullcline, y_nullcline)``: two arrays of line segments, one
            row per segment holding x1, y1, x2, y2.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> nv, nw = m.nullclines("V", "W", (-0.6, 0.6), (-0.1, 0.6))
            >>> nv.shape, nw.shape
            ((274, 4), (146, 4))

        See also:
            [nullclines](../reference/nullclines.md) in the command window;
            [`nullcline_sweep`][xpplab.Model.nullcline_sweep],
            [`phase_plane`][xpplab.Model.phase_plane].
        """
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
        """The nullclines for each value of a parameter, as XPPAUT's
        *Nullcline > Freeze > Range* does. The parameter is restored
        afterwards.

        Args:
            parameter: the parameter to vary.
            values: its values.
            x: the variable along the horizontal axis.
            y: the variable along the vertical axis.
            xlim: ``(low, high)`` of ``x``.
            ylim: ``(low, high)`` of ``y``.
            grid: the number of cells along each side.

        Returns:
            One ``(x_nullcline, y_nullcline)`` pair per value, as
            [`nullclines`][xpplab.Model.nullclines] returns them.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> frozen = m.nullcline_sweep("iapp", [0, 0.1, 0.2],
            ...                            "V", "W", (-0.6, 0.6), (-0.1, 0.6))
            >>> len(frozen), m.get("iapp")
            (3, 0.05)

        See also:
            [nullclines freeze](../reference/nullclines-freeze.md) in the
            command window; [`phase_plane`][xpplab.Model.phase_plane],
            whose ``frozen`` draws them.
        """
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
        """The vector field on a grid in a plane of two variables. The other
        variables keep their initial values.

        Args:
            x: the variable along the horizontal axis.
            y: the variable along the vertical axis.
            xlim: ``(low, high)`` of ``x``.
            ylim: ``(low, high)`` of ``y``.
            n: the number of points along each side.

        Returns:
            ``(X, Y, U, V)``, each an ``n`` by ``n`` array, ready for
            matplotlib's ``quiver``: the grid and the derivatives there (for
            a map, the step to the next state).

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> X, Y, U, V = m.vector_field("V", "W", (-0.6, 0.6), (-0.1, 0.6), n=5)
            >>> X.shape
            (5, 5)
            >>> print(f"{U[0, 0]:.4f} {V[0, 0]:.4f}")
            0.1206 0.1877

        See also:
            [dirfield](../reference/dirfield.md) in the command window;
            [`phase_plane`][xpplab.Model.phase_plane].
        """
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
        """Draw the phase plane in one call: a run, the nullclines, the
        direction field and the nearest equilibrium. Needs matplotlib.

        The run starts from the initial conditions and lasts ``TOTAL``.

        Args:
            x: the variable along the horizontal axis; by default the first.
            y: the variable along the vertical axis; by default the second.
            xlim: ``(low, high)`` of ``x``; by default the run's range with
                a margin of 15%.
            ylim: ``(low, high)`` of ``y``; likewise.
            trajectory: draw a run from the initial conditions.
            nullclines: draw the nullclines.
            field: draw the direction field.
            equilibrium: mark the equilibrium Newton's method finds from the
                initial conditions, filled if it is stable, with its
                manifolds if it is a saddle (see
                [`manifolds`][xpplab.Model.manifolds]).
            frozen: earlier nullclines to draw faded, such as
                [`nullcline_sweep`][xpplab.Model.nullcline_sweep] returns.
            ax: the matplotlib axes to draw in; by default new ones.

        Returns:
            The matplotlib axes.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> ax = m.phase_plane(xlim=(-0.6, 0.6), ylim=(-0.1, 0.6))
            >>> ax.get_xlabel(), ax.get_ylabel()
            ('V', 'W')

        See also:
            [nullclines](../reference/nullclines.md) and
            [dirfield](../reference/dirfield.md) in the command window;
            the [phase plane guide](phase-plane.md).
        """
        from .plotting import plot_phase_plane
        x = x or self.variables[0]
        y = y or self.variables[1]
        return plot_phase_plane(self, x, y, xlim=xlim, ylim=ylim,
                                trajectory=trajectory, nullclines=nullclines,
                                field=field, equilibrium=equilibrium,
                                frozen=frozen, ax=ax)

    def ic_last(self, *, shift: bool = False) -> "Model":
        """Start the next run where the last one ended, as XPPAUT's
        *Initial conds > Last* does.

        The final state of the engine's last run becomes the initial
        condition, in full precision (not the single precision of the
        stored run).

        Args:
            shift: also make ``T0`` the time the run ended, as *Initial
                conds > Shift* does, so that time goes on. It matters for
                models with forcing terms in ``t``.

        Returns:
            The model itself, so that calls chain:
            ``m.ic_last().run()``.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> m.ic_last().ics
            {V=0.0277074, W=0.0412408}
            >>> round(m.ic_last(shift=True).get("t0"), 6)
            100.0

        See also:
            [ic last](../reference/ic-last.md) and
            [ic shift](../reference/ic-shift.md) in the command window;
            [`continue_run`][xpplab.Model.continue_run].
        """
        self._engine.call("xppe_ic_shift" if shift else "xppe_ic_last")
        return self

    def read_ics(self, path) -> "Model":
        """Read the initial conditions from a text file, as XPPAUT's
        *Initial conds > File* does.

        The file's first numbers are taken, one per variable in the model's
        order, separated by spaces, new lines or commas.

        Args:
            path: the file.

        Returns:
            The model itself, so that calls chain.

        Raises:
            EngineError: the file has fewer numbers than the model has
                variables. Nothing is changed.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> with open("start.ics", "w") as f:
            ...     _ = f.write("-0.3 0.01\\n")
            >>> m.read_ics("start.ics").ics
            {V=-0.3, W=0.01}

        See also:
            [ic read](../reference/ic-read.md) in the command window;
            [`write_ics`][xpplab.Model.write_ics].
        """
        self._engine.call("xppe_ic_file", os.fsencode(Path(path).resolve()))
        return self

    def write_ics(self, path) -> Path:
        """Write the initial conditions to a text file, one per line in the
        order of the variables, with every digit, so that
        [`read_ics`][xpplab.Model.read_ics] reads back the same values.

        Args:
            path: the file to write.

        Returns:
            The file's path.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> path = m.ic_last().write_ics("end.ics")
            >>> print(path.read_text())
            0.027707405522076907
            0.041240834023850106
            <BLANKLINE>

        See also:
            [ic write](../reference/ic-write.md) in the command window;
            [`read_ics`][xpplab.Model.read_ics].
        """
        path = Path(path)
        path.write_text("".join(repr(float(self.get(v))) + "\n"
                                for v in self.variables))
        return path

    def ic_formula(self, target: str, formula: str) -> "Model":
        """Set initial conditions from a formula, as XPPAUT's *Initial
        conds > formUla* does.

        XPPAUT evaluates the formula with the model's parameters and
        functions. A variable named in it stands for its initial value
        before the call.

        Args:
            target: a variable, or a range of numbered variables in XPPAUT's
                array form, ``"u[1..50]"``, where ``[j]`` in the formula
                stands for the index: ``m.ic_formula("u[1..50]",
                "exp(-([j]-25)^2/20)")``.
            formula: the formula, in the syntax of an .ode file.

        Returns:
            The model itself, so that calls chain.

        Raises:
            EngineError: the target is no variable, or XPPAUT cannot parse
                the formula.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> m.ic_formula("W", "V/10").ics
            {V=-0.144, W=-0.0144}
            >>> m.ic_formula("V", "vk + 0.1").ics
            {V=-0.6, W=-0.0144}

        See also:
            [ic](../reference/ic.md) in the command window;
            [`initial_conditions`][xpplab.Model.initial_conditions].
        """
        self._engine.call("xppe_ic_formula", _b(target), _b(formula))
        return self

    def sweep(self, parameter: str, values, *, measure=None,
              continue_runs: bool = False, **run_kw):
        """Run once per value of a parameter or initial condition, as
        XPPAUT's *Initial conds > Range* does.

        Each run starts from the initial conditions (XPPAUT's "Use old ICs"
        = Yes), or with ``continue_runs=True`` from where the run before
        ended ("Use old ICs" = No), which follows an attractor as the
        parameter changes. The parameter and the initial conditions are
        restored afterwards.

        Args:
            parameter: a parameter or a variable (its initial condition). A
                tuple of two names moves both along a line, as XPPAUT's
                *2par range* (Crv) does: ``m.sweep(("iapp", "gca"),
                zip(i_values, g_values))``.
            values: the values, in order; pairs for two names.
            measure: a function of a run, such as
                ``lambda s: s["V"].max()``. The result is then its value at
                each parameter value: a brute-force bifurcation diagram.
            continue_runs: start each run where the one before ended.
            **run_kw: set before the first run, as by
                [`set`][xpplab.Model.set], and kept: ``total=200``.

        Returns:
            Without ``measure``, a list of [`Trajectory`][xpplab.Trajectory];
            with it, a NumPy array of the measure.

        Examples:
            The amplitude of the oscillation of ``lecar.ode`` over the
            second half of each run:

            >>> m = xs.load(xs.example("lecar.ode"))
            >>> amp = m.sweep("iapp", [0, 0.05, 0.1, 0.15], total=200,
            ...               measure=lambda s: np.ptp(s["V"][len(s) // 2:]))
            >>> np.round(amp, 4)
            array([0.    , 0.    , 0.6419, 0.6143])
            >>> m.get("iapp")
            0.05

        See also:
            [range](../reference/range.md) in the command window;
            [`sweep_grid`][xpplab.Model.sweep_grid],
            [`run_many`][xpplab.Model.run_many].
        """
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
        """Run once for every pair of values on a grid, as XPPAUT's *2par
        range* (Array) does.

        The runs go through ``values`` for each of ``values2`` in turn,
        setting ``parameter`` then ``parameter2``. The values and the
        initial conditions are restored afterwards.

        Args:
            parameter: a parameter or a variable (its initial condition).
            values: its values, along each row.
            parameter2: the second parameter or variable.
            values2: its values, one per row.
            measure: a function of a run, as for
                [`sweep`][xpplab.Model.sweep].
            continue_runs: start each run where the one before ended, row
                after row.
            **run_kw: set before the first run, as by
                [`set`][xpplab.Model.set], and kept.

        Returns:
            With ``measure``, a NumPy array of shape
            ``(len(values2), len(values))`` (plus the measure's own shape),
            ready for ``plt.imshow(..., origin="lower")``. Without it, a list
            of rows of [`Trajectory`][xpplab.Trajectory].

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> amp = m.sweep_grid("iapp", [0, 0.1, 0.2], "gca", [1, 1.2], total=200,
            ...                    measure=lambda s: np.ptp(s["V"][len(s) // 2:]))
            >>> np.round(amp, 4)
            array([[0.    , 0.6419, 0.5657],
                   [0.    , 0.7081, 0.    ]])

        See also:
            [range](../reference/range.md) in the command window;
            [`sweep`][xpplab.Model.sweep].
        """
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
        """Sliders in a Jupyter notebook that set parameters and run again
        as they move. Needs ipywidgets and matplotlib.

        Each slider changes its parameter for good, as
        [`set`][xpplab.Model.set] does, and the plot shows a new run.

        Args:
            *parameters: the parameters to give sliders, each between 0
                and twice its value; by default the first six.
            y: the columns to plot against time; by default the first two
                variables.
            **ranges: parameters with their ranges: ``iapp=(0, 0.3)``.

        Returns:
            The ipywidgets ``interactive`` widget, which a notebook shows.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> m.interact("phi", iapp=(0, 0.3), y="V")  # doctest: +SKIP

        See also:
            [`sweep`][xpplab.Model.sweep], which runs over a list of values;
            the desktop app's parameter sliders.
        """
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
        """What XPPAUT printed during the last call to the engine.

        XPPAUT reports some things only in its printed output: warnings,
        the steps of Newton's method, AUTO's progress. Errors are raised as
        [`EngineError`][xpplab.EngineError] with this text in the message.
        What was printed while reading the file is in ``load_log``.

        Examples:
            A run of x' = x leaves ``BOUND`` and stops; the log says where:

            >>> m = xs.Model.from_text('''
            ... x' = x
            ... init x=1
            ... @ total=10, bound=100
            ... done
            ... ''')
            >>> m.run()
            <Trajectory: 93 points, t=0..4.6, columns T, X>
            >>> print(m.log)
            variable	f(t-1)	f(t)
             X	99.4843	104.585
             X out of bounds at t = 4.650000
             Integration not completed -- will write anyway...
             Run complete ...

        See also:
            [`EngineError`][xpplab.EngineError].
        """
        return self._engine.log

    def close(self) -> None:
        """Free the engine behind this model.

        The model cannot be used afterwards. A model in a ``with`` block is
        closed at its end; one that is no longer referenced is closed by
        Python's garbage collector.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> m.close()
            >>> with xs.load(xs.example("lecar.ode")) as m:
            ...     sol = m.run()

        See also:
            [quit](../reference/quit.md) in the command window.
        """
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
    """Load an XPPAUT .ode file.

    Args:
        path: the .ode file. [`example`][xpplab.example] gives the path of a
            shipped example.
        **kw: passed on to [`Model`][xpplab.Model]: ``options``,
            ``engine_library``.

    Returns:
        The model, as ``Model(path, **kw)`` would.

    Raises:
        FileNotFoundError: there is no such file.
        EngineError: XPPAUT could not read the file; the message gives the
            line.

    Examples:
        >>> m = xs.load(xs.example("lecar.ode"))
        >>> m
        <Model lecar.ode: 2 variables (V, W), 12 parameters>
        >>> xs.load("nothing.ode")
        Traceback (most recent call last):
        FileNotFoundError: ...nothing.ode

    See also:
        [open](../reference/open.md) in the command window;
        [`Model.from_text`][xpplab.Model.from_text].
    """
    return Model(path, **kw)


class Trajectory:
    """A run: time and every output column, one point per output time.

    [`Model.run`][xpplab.Model.run] returns one. A column is read by its
    name, which ignores case: ``sol["V"]`` is a NumPy array. ``"V" in sol``
    tells whether there is such a column, and ``len(sol)`` is the number of
    points. XPPAUT stores runs in single precision, so the values have about
    seven significant digits.

    Attributes:
        log (str): what XPPAUT printed during the run.
        params (dict | None): the parameter values the run used, by name.
        event (int): the global flag that stopped the run, counted from 1
            (see [`Model.flags`][xpplab.Model.flags]), or 0 when it ran to
            the end.

    Examples:
        >>> m = xs.load(xs.example("lecar.ode"))
        >>> sol = m.run(iapp=0.1, total=100)
        >>> sol
        <Trajectory: 2001 points, t=0..100, columns T, V, W, CUR, ICA, IK, RATE>
        >>> sol["v"][:3]
        array([-0.14399999, -0.14128359, -0.13840356])
        >>> "ICA" in sol, len(sol), sol.params["iapp"], sol.event
        (True, 2001, 0.1, 0)

    See also:
        [run](../reference/run.md) in the command window;
        [`Model.run`][xpplab.Model.run].
    """

    def __init__(self, data: dict[str, np.ndarray], log: str = ""):
        self._data = data
        self.log = log
        self.params = None
        self.event = 0

    @property
    def columns(self) -> list[str]:
        """The names of the columns: ``T``, the variables, then the
        auxiliary quantities.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> sol.columns
            ['T', 'V', 'W', 'CUR', 'ICA', 'IK', 'RATE']

        See also:
            [`Model.columns`][xpplab.Model] (an attribute).
        """
        return list(self._data)

    @property
    def t(self) -> np.ndarray:
        """The output times, the column ``T``.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> sol.t[:3]
            array([0.  , 0.05, 0.1 ])

        See also:
            [`columns`][xpplab.Trajectory.columns].
        """
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
        """Every column as one array.

        Returns:
            An array with one row per output time and one column per
            output column, in the order of
            [`columns`][xpplab.Trajectory.columns].

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> sol.to_numpy().shape
            (2001, 7)

        See also:
            [`to_pandas`][xpplab.Trajectory.to_pandas],
            [`to_csv`][xpplab.Trajectory.to_csv].
        """
        return np.column_stack(list(self._data.values()))

    def to_pandas(self):
        """The run as a pandas DataFrame indexed by time. Needs pandas.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> df = sol.to_pandas()
            >>> list(df.columns), df.index.name
            (['V', 'W', 'CUR', 'ICA', 'IK', 'RATE'], 'T')

        See also:
            [`to_numpy`][xpplab.Trajectory.to_numpy].
        """
        import pandas as pd
        df = pd.DataFrame(self._data)
        return df.set_index("T")

    def save(self, path: str | os.PathLike) -> None:
        """Write the run in the format of XPPAUT's ``output.dat``: one row
        per output time, the columns separated by spaces, with eight
        significant digits and no header.

        Args:
            path: the file to write.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> sol.save("run.dat")
            >>> print(open("run.dat").read().splitlines()[1])
            0.050000001 -0.14128359 0.030090233 0.1 -0.16889939 0.033623815 0.333

        See also:
            [`to_csv`][xpplab.Trajectory.to_csv], which keeps every digit.
        """
        np.savetxt(path, self.to_numpy(), fmt="%.8g")

    def to_csv(self, path: str | os.PathLike) -> None:
        """Write the run as a CSV file with a header, every number exact.

        The desktop app's ``export FILE.csv`` writes the same file.

        Args:
            path: the file to write.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> sol.to_csv("run.csv")
            >>> open("run.csv").readline()
            'T,V,W,CUR,ICA,IK,RATE\\n'

        See also:
            [export](../reference/export.md) in the command window;
            [`to_mat`][xpplab.Trajectory.to_mat],
            [`Model.fit`][xpplab.Model.fit], which reads the file.
        """
        from .analysis import write_csv
        write_csv(path, self.columns, self.to_numpy())

    def to_mat(self, path: str | os.PathLike) -> None:
        """Write the run as a MATLAB .mat file.

        The file holds ``t`` and one column vector per variable and
        auxiliary quantity, named as in the model (made valid MATLAB names),
        and the struct ``params`` with the parameter values the run used.
        The desktop app's ``export FILE.mat`` writes the same file.

        Args:
            path: the file to write.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> sol.to_mat("run.mat")

            In MATLAB, ``load run.mat`` then ``plot(t, V)``.

        See also:
            [export](../reference/export.md) in the command window;
            [`to_csv`][xpplab.Trajectory.to_csv].
        """
        from .analysis import write_mat
        names = ["t" if k == "T" else k for k in self.columns]
        write_mat(path, names, self._data.values(), self.params)

    def plot(self, *y: str, x: str = "T", ax=None, **kw):
        """Plot columns against time, or against another column. Needs
        matplotlib.

        Args:
            *y: the columns to plot; by default every variable.
            x: the column along the horizontal axis.
            ax: the matplotlib axes to draw in; by default new ones.
            **kw: passed on to matplotlib's ``plot``.

        Returns:
            The matplotlib axes.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> sol = m.run(iapp=0.1, total=100)
            >>> ax = sol.plot("V", "W")
            >>> [line.get_label() for line in ax.lines]
            ['V', 'W']
            >>> ax = sol.plot("W", x="V")
            >>> ax.get_xlabel(), ax.get_ylabel()
            ('V', 'W')

        See also:
            [`plot3d`][xpplab.Trajectory.plot3d],
            [`Model.phase_plane`][xpplab.Model.phase_plane].
        """
        from .plotting import plot_trajectory
        return plot_trajectory(self, list(y), x=x, ax=ax, **kw)

    def plot3d(self, x: str, y: str, z: str, ax=None, **kw):
        """A three-dimensional plot of three columns, as XPPAUT's 3D view
        draws it. Needs matplotlib.

        Args:
            x: the first column.
            y: the second column.
            z: the third column.
            ax: matplotlib axes with ``projection="3d"``; by default new ones.
            **kw: passed on to matplotlib's ``plot``.

        Returns:
            The matplotlib axes.

        Examples:
            >>> m = xs.load(xs.example("classics/lorenz"))
            >>> ax = m.run().plot3d("X", "Y", "Z")
            >>> ax.get_zlabel()
            'Z'

        See also:
            [view3d](../reference/view3d.md) in the command window;
            [`plot`][xpplab.Trajectory.plot].
        """
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
    """A dictionary whose keys ignore case when read, as the model's names
    do. [`Model.evaluate`][xpplab.Model.evaluate] and
    [`Model.sample_initial`][xpplab.Model.sample_initial] return them.

    Examples:
        >>> m = xs.load(xs.example("lecar.ode"))
        >>> row = m.evaluate()
        >>> row["ica"] == row["ICA"], "Ica" in row
        (True, True)

    See also:
        [`Model.evaluate`][xpplab.Model.evaluate].
    """

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
        """The value of a key, in any case, or ``default`` when there is no
        such key.

        Args:
            name: the key.
            default: what to return when there is no such key.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> row = m.evaluate()
            >>> round(row.get("Ica"), 6), row.get("gna", 0)
            (-0.164143, 0)

        See also:
            [`Model.evaluate`][xpplab.Model.evaluate].
        """
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
    """Several runs kept together: a list of
    [`Trajectory`][xpplab.Trajectory].

    [`Model.run_many`][xpplab.Model.run_many] and
    [`Model.shoot`][xpplab.Model.shoot] return one.

    Examples:
        >>> m = xs.load(xs.example("lecar.ode"))
        >>> runs = m.run_many([{"V": -0.3}, {"V": 0.2}], iapp=0.1, total=50)
        >>> runs
        <RunList: 2 runs, ended by {0: 2}>
        >>> runs[1]
        <Trajectory: 1001 points, t=0..50, columns T, V, W, CUR, ICA, IK, RATE>

    See also:
        [many](../reference/many.md) in the command window.
    """

    @property
    def events(self) -> np.ndarray:
        """The global flag that stopped each run, counted from 1, or 0 for
        a run that reached the end.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> runs = m.run_many([{"V": -0.3}, {"V": 0.2}], iapp=0.1, total=50)
            >>> runs.events
            array([0, 0])

        See also:
            [`Model.flags`][xpplab.Model.flags],
            [`counts`][xpplab.RunList.counts].
        """
        return np.array([s.event for s in self], dtype=int)

    def counts(self) -> dict[int, int]:
        """How many runs each global flag stopped.

        Returns:
            A dictionary from the flag's number to the count; the key 0
            counts the runs that reached the end.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> runs = m.run_many([{"V": -0.3}, {"V": 0.2}], iapp=0.1, total=50)
            >>> runs.counts()
            {0: 2}

        See also:
            [`events`][xpplab.RunList.events].
        """
        out: dict[int, int] = {}
        for e in self.events:
            out[int(e)] = out.get(int(e), 0) + 1
        return dict(sorted(out.items()))

    def plot(self, y: str, x: str = "T", *, ax=None, colors=None, **kw):
        """Plot every run, one column against another. Needs matplotlib.

        Args:
            y: the column along the vertical axis.
            x: the column along the horizontal axis.
            ax: the matplotlib axes to draw in; by default the current ones.
            colors: a colour for each flag number (0 for the runs that
                reached the end), to see how each run ended.
            **kw: passed on to matplotlib's ``plot``.

        Returns:
            The matplotlib axes.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> runs = m.run_many([{"V": -0.3}, {"V": 0.2}], iapp=0.1, total=50)
            >>> ax = runs.plot("W", x="V", colors={0: "tab:blue"})
            >>> ax.get_xlabel(), ax.get_ylabel()
            ('V', 'W')

        See also:
            [`Trajectory.plot`][xpplab.Trajectory.plot].
        """
        import matplotlib.pyplot as plt
        ax = ax or plt.gca()
        for s in self:
            c = colors.get(s.event) if colors else None
            ax.plot(s[x], s[y], color=c, **kw)
        ax.set_xlabel("t" if x.upper() == "T" else x)
        ax.set_ylabel(y)
        return ax

    def to_csv(self, path) -> None:
        """Write every run in one CSV file, every number exact.

        The columns are ``run`` (counted from 0), ``event``, then the runs'
        columns, one row per output time. The desktop app's
        ``export FILE.csv many`` writes the same file.

        Args:
            path: the file to write.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> runs = m.run_many([{"V": -0.3}, {"V": 0.2}], iapp=0.1, total=50)
            >>> runs.to_csv("many.csv")
            >>> open("many.csv").readline()
            'run,event,T,V,W,CUR,ICA,IK,RATE\\n'

        See also:
            [export](../reference/export.md) in the command window;
            [`to_mat`][xpplab.RunList.to_mat].
        """
        from .analysis import write_csv
        rows = []
        cols = self[0].columns if len(self) else ["T"]
        for k, s in enumerate(self):
            data = s.to_numpy()
            rows += [[k, s.event] + list(r) for r in data]
        write_csv(path, ["run", "event"] + cols, rows)

    def to_mat(self, path) -> None:
        """Write every run in one MATLAB .mat file, with the columns of
        [`to_csv`][xpplab.RunList.to_csv] as column vectors. There is no
        ``params`` struct, since the runs may differ.

        Args:
            path: the file to write.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> runs = m.run_many([{"V": -0.3}, {"V": 0.2}], iapp=0.1, total=50)
            >>> runs.to_mat("many.mat")

        See also:
            [export](../reference/export.md) in the command window;
            [`to_csv`][xpplab.RunList.to_csv].
        """
        from .analysis import write_mat
        cols = self[0].columns if len(self) else ["T"]
        names = ["t" if k == "T" else k for k in cols]
        runs = [np.concatenate([np.full(len(s), k, float) for k, s in enumerate(self)])
                if len(self) else np.zeros(0),
                np.concatenate([np.full(len(s), s.event, float) for s in self])
                if len(self) else np.zeros(0)]
        runs += [np.concatenate([s[c] for s in self]) if len(self) else np.zeros(0)
                 for c in cols]
        write_mat(path, ["run", "event"] + names, runs)

    def __repr__(self) -> str:
        return f"<RunList: {len(self)} runs, ended by {self.counts()}>"


@dataclass
class Ensemble:
    """The mean and the variance over many runs of a noisy model, at each
    output time. [`Model.ensemble`][xpplab.Model.ensemble] returns one.

    Attributes:
        mean (Trajectory): the mean of every column.
        variance (Trajectory): the variance of every column.
        trials (int): the number of runs.

    Examples:
        >>> m = xs.load(xs.example("classics/ornstein_uhlenbeck"))
        >>> e = m.ensemble(100)
        >>> e.trials, len(e.t) == len(e.mean)
        (100, True)

    See also:
        [ensemble](../reference/ensemble.md) in the command window.
    """

    mean: Trajectory
    variance: Trajectory
    trials: int

    @property
    def std(self) -> Trajectory:
        """The standard deviation of every column; the time column as it
        is.

        Examples:
            >>> m = xs.load(xs.example("classics/ornstein_uhlenbeck"))
            >>> e = m.ensemble(100)
            >>> bool(np.allclose(e.std["X"] ** 2, e.variance["X"].clip(0)))
            True

        See also:
            [`plot`][xpplab.Ensemble.plot], which draws a band of it.
        """
        data = {k: (v if k == "T" else np.sqrt(np.clip(v, 0, None)))
                for k, v in self.variance._data.items()}
        return Trajectory(data)

    @property
    def t(self) -> np.ndarray:
        """The output times.

        Examples:
            >>> m = xs.load(xs.example("classics/ornstein_uhlenbeck"))
            >>> e = m.ensemble(10)
            >>> bool((e.t == e.mean["T"]).all())
            True

        See also:
            [`Trajectory.t`][xpplab.Trajectory.t].
        """
        return self.mean.t

    def to_table(self) -> "Table":
        """The mean and the variance as one table.

        Returns:
            A [`Table`][xpplab.Table]: ``T``, then each column's mean and
            variance (``X_mean``, ``X_var``, ...). The desktop app's
            ``export FILE.csv analysis`` after an ensemble writes the same.

        Examples:
            >>> m = xs.load(xs.example("classics/ornstein_uhlenbeck"))
            >>> m.ensemble(10).to_table().columns
            ['T', 'X_mean', 'X_var']

        See also:
            [`to_csv`][xpplab.Ensemble.to_csv].
        """
        from .analysis import Table
        data = {"T": self.mean.t}
        for name in self.mean.columns:
            if name != "T":
                data[name + "_mean"] = self.mean[name]
                data[name + "_var"] = self.variance[name]
        return Table(data, title="ensemble of %d runs" % self.trials)

    def to_csv(self, path) -> None:
        """Write the table of [`to_table`][xpplab.Ensemble.to_table] as a
        CSV file, every number exact.

        Args:
            path: the file to write.

        Examples:
            >>> m = xs.load(xs.example("classics/ornstein_uhlenbeck"))
            >>> m.ensemble(10).to_csv("ensemble.csv")
            >>> open("ensemble.csv").readline()
            'T,X_mean,X_var\\n'

        See also:
            [export](../reference/export.md) in the command window.
        """
        self.to_table().to_csv(path)

    def to_mat(self, path) -> None:
        """Write the table of [`to_table`][xpplab.Ensemble.to_table] as a
        MATLAB .mat file.

        Args:
            path: the file to write.

        Examples:
            >>> m = xs.load(xs.example("classics/ornstein_uhlenbeck"))
            >>> m.ensemble(10).to_mat("ensemble.mat")

        See also:
            [export](../reference/export.md) in the command window.
        """
        self.to_table().to_mat(path)

    def plot(self, *names: str, ax=None, band: float = 1.0):
        """Plot each column's mean against time, with a band of ``band``
        standard deviations around it. Needs matplotlib.

        Args:
            *names: the columns; by default the first after time.
            ax: the matplotlib axes to draw in; by default the current ones.
            band: the half-width of the band, in standard deviations.

        Returns:
            The matplotlib axes.

        Examples:
            >>> m = xs.load(xs.example("classics/ornstein_uhlenbeck"))
            >>> ax = m.ensemble(100).plot("X", band=2)
            >>> ax.get_legend_handles_labels()[1]
            ['X mean']

        See also:
            [ensemble](../reference/ensemble.md) in the command window.
        """
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
    """An equilibrium, with the eigenvalues of the Jacobian there.

    [`Model.equilibrium`][xpplab.Model.equilibrium] and
    [`Model.find_equilibria`][xpplab.Model.find_equilibria] return them.
    ``eq["V"]`` is a variable's value, its name in any case.

    Attributes:
        values (dict[str, float]): the variables' values.
        eigenvalues (numpy.ndarray): the eigenvalues, complex.
        discrete (bool): the model is a map, so stability is about the unit
            circle.

    Examples:
        >>> m = xs.load(xs.example("lecar.ode"))
        >>> eq = m.equilibrium()
        >>> eq
        <Equilibrium saddle: V=-0.142535, W=0.034049>
        >>> round(eq["v"], 6), eq.eigenvalues.shape
        (-0.142535, (2,))

    See also:
        [eq](../reference/eq.md) in the command window.
    """

    values: dict[str, float]
    eigenvalues: np.ndarray
    discrete: bool = False

    @property
    def unstable_count(self) -> int:
        """The number of unstable eigenvalues: with a positive real part,
        or for a map outside the unit circle.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> [e.unstable_count for e in m.find_equilibria()]
            [2, 0, 1]

        See also:
            [`stable`][xpplab.Equilibrium.stable],
            [`kind`][xpplab.Equilibrium.kind].
        """
        if self.discrete:
            return int(np.sum(np.abs(self.eigenvalues) > 1))
        return int(np.sum(self.eigenvalues.real > 0))

    @property
    def stable(self) -> bool:
        """Whether no eigenvalue is unstable.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> [e.stable for e in m.find_equilibria()]
            [False, True, False]

        See also:
            [`unstable_count`][xpplab.Equilibrium.unstable_count].
        """
        return self.unstable_count == 0

    @property
    def kind(self) -> str:
        """The kind of equilibrium, in words: ``"stable node"``,
        ``"unstable focus"``, ``"saddle"``, ``"saddle-focus"``, ...; for a
        map ``"stable"`` or ``"unstable"``.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> [e.kind for e in m.find_equilibria()]
            ['unstable focus', 'stable node', 'saddle']

        See also:
            [`stable`][xpplab.Equilibrium.stable].
        """
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
    """Equilibria found together: a list of
    [`Equilibrium`][xpplab.Equilibrium] that can also be written as a
    table. [`Model.find_equilibria`][xpplab.Model.find_equilibria] returns
    one.

    Attributes:
        variables (list[str]): the model's variables, the table's first
            columns.

    Examples:
        >>> m = xs.load(xs.example("lecar.ode"))
        >>> found = m.find_equilibria()
        >>> found
        <3 equilibria: unstable focus, stable node, saddle>
        >>> found[2]
        <Equilibrium saddle: V=-0.142535, W=0.034049>

    See also:
        [eq all](../reference/eq-all.md) in the command window.
    """

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
        """Write the equilibria as a CSV file, every number exact.

        There is one row per equilibrium: the variables, ``unstable`` (the
        number of unstable eigenvalues), then each eigenvalue's real and
        imaginary parts (``re1``, ``im1``, ``re2``, ...). The desktop app's
        ``export FILE.csv equilibria`` writes the same file.

        Args:
            path: the file to write.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> m.find_equilibria().to_csv("eq.csv")
            >>> open("eq.csv").readline()
            'V,W,unstable,re1,im1,re2,im2\\n'

        See also:
            [export](../reference/export.md) in the command window;
            [`to_pandas`][xpplab.EquilibriumList.to_pandas].
        """
        from .analysis import write_csv
        write_csv(path, *self._table())

    def to_mat(self, path: str | os.PathLike) -> None:
        """Write the table of [`to_csv`][xpplab.EquilibriumList.to_csv] as
        a MATLAB .mat file, one column vector per column. The desktop app's
        ``export FILE.mat equilibria`` writes the same file.

        Args:
            path: the file to write.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> m.find_equilibria().to_mat("eq.mat")

        See also:
            [export](../reference/export.md) in the command window.
        """
        from .analysis import write_mat
        cols, rows = self._table()
        write_mat(path, cols, np.array(rows, dtype=float).reshape(len(rows), len(cols)).T)

    def to_pandas(self):
        """The table of [`to_csv`][xpplab.EquilibriumList.to_csv] as a
        pandas DataFrame. Needs pandas.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode"))
            >>> df = m.find_equilibria().to_pandas()
            >>> list(df.columns)
            ['V', 'W', 'unstable', 're1', 'im1', 're2', 'im2']
            >>> df["unstable"].tolist()
            [2, 0, 1]

        See also:
            [`to_csv`][xpplab.EquilibriumList.to_csv].
        """
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
