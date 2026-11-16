"""AUTO continuation and bifurcation diagrams.

The continuation is XPPAUT's own AUTO, run exactly as XPPAUT's AUTO window
runs it, without the window:

```python
m = xpplab.load("lecar.ode")
m.apply_set("hopf")
d = m.auto.equilibria("iapp")            # steady states vs. iapp
d.special()                              # labelled points: HB, LP, ...
hb = d.first("HB")
orbits = m.auto.continue_from(hb, "periodic")
m.auto.diagram.plot()                    # the whole diagram
```
"""

from __future__ import annotations

import ctypes
import os
from pathlib import Path
from dataclasses import dataclass, field

import numpy as np

from . import _engine as E
from .model import Values

__all__ = ["Auto", "Diagram", "Point", "SETTINGS", "TYPE_NAMES"]

# AUTO numerical settings (XPPAUT's AutoNum dialog), with what they mean.
SETTINGS = {
    "ntst": "mesh intervals for periodic orbits and BVPs",
    "nmax": "maximum number of steps along a branch",
    "npr": "save a labelled point every NPR steps",
    "ds": "initial step size (its sign sets the direction)",
    "dsmin": "minimum step size",
    "dsmax": "maximum step size",
    "parmin": "stop when the main parameter falls below this",
    "parmax": "stop when the main parameter rises above this",
    "normmin": "stop when the norm falls below this",
    "normmax": "stop when the norm rises above this",
    "ncol": "collocation points per mesh interval",
    "epsl": "Newton tolerance for parameters",
    "epsu": "Newton tolerance for the solution",
    "epss": "tolerance for locating special points",
    "iad": "mesh adaptation every IAD steps (0 = never)",
    "mxbf": "maximum number of branch switches (automatic branching)",
    "iid": "amount of diagnostic output",
    "itmx": "maximum iterations to locate special points",
    "itnw": "maximum Newton iterations",
    "nwtn": "steps after which the Jacobian is frozen",
    "iads": "adapt the step size every IADS steps (0 = fixed)",
    "suppbp": "1 to skip branch point detection",
}

TYPE_NAMES = {
    "BP": "branch point",
    "LP": "limit point (fold)",
    "HB": "Hopf bifurcation",
    "PD": "period doubling",
    "TR": "torus (Neimark-Sacker) bifurcation",
    "EP": "end point",
    "UZ": "user-defined point",
    "MX": "failed to converge",
    "": "regular point",
}

# What you can do from each kind of point, with the key XPPAUT's menu uses.
_ACTIONS = {
    "start": {"steady": "s", "periodic": "p", "bvp": "b",
              "homoclinic": "h", "heteroclinic": "e"},
    "HB": {"periodic": "p", "extend": "e", "new": "n", "two-parameter": "t"},
    "PD": {"doubling": "d", "two-parameter": "t", "fixed-period": "f",
           "extend": "e"},
    "TR": {"two-parameter": "t", "fixed-period": "f", "extend": "e"},
    "BP": {"switch": "s", "extend": "e", "new": "n", "two-parameter": "t"},
    "LP": {"two-parameter": ""},
    "periodic": {"extend": "e", "fixed-period": "f"},
    "steady": {"extend": ""},
}


@dataclass
class Point:
    """One computed point of a bifurcation diagram: a steady state or a
    periodic orbit, at one value of the continuation parameter (two values
    on a two-parameter curve).

    Attributes:
        index (int): the point's place in the whole diagram, from 0.
        branch (int): AUTO's branch number; negative for periodic orbits.
        point (int): AUTO's point number on its branch.
        type_code (int): AUTO's type code (``ITP``).
        type (str): ``"HB"``, ``"LP"``, ``"BP"``, ``"PD"``, ``"TR"``,
            ``"EP"``, ``"UZ"``, ``"MX"``, or ``""`` for a regular point.
        label (int): AUTO's label, 0 for a point without one. Labels count
            on across runs and identify a point to continue from.
        stable (bool): whether the steady state or the orbit is stable.
        periodic (bool): a periodic orbit rather than a steady state.
        two_parameter (bool): a point of a two-parameter curve.
        parameter (str): the continuation parameter.
        value (float): its value.
        parameter2 (str | None): the second parameter, on a two-parameter
            curve.
        value2 (float | None): its value.
        norm (float): AUTO's L2 norm of the solution.
        period (float): the period of an orbit.
        parameters (Values): every parameter AUTO knows, by name.
        u0 (Values): the state: the steady state, or the orbit's point at
            the start of its period.
        umax (Values): each variable's largest value over the orbit.
        umin (Values): each variable's smallest value over the orbit.
        umean (Values): each variable's mean over the orbit.
        multipliers (numpy.ndarray): what XPPAUT's AUTO stores, on the unit
            circle's terms: $e^{\\lambda}$ for steady states and the
            Floquet multipliers for orbits. A point is stable when all of
            them are inside the unit circle (for orbits, apart from the
            trivial multiplier 1).

    Examples:
        >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
        >>> d = m.auto.equilibria("iapp")
        >>> hb = d.first("HB")
        >>> hb
        <HB #2: unstable steady state, iapp=0.262464, norm=0.289108>
        >>> hb.label, round(hb.value, 6), round(hb.u0["V"], 6)
        (2, 0.262464, -0.198944)

    See also:
        [auto steady](../reference/auto-steady.md) in the command window,
        whose page explains the columns.
    """

    index: int
    branch: int
    point: int
    type_code: int
    type: str
    label: int
    stable: bool
    periodic: bool
    two_parameter: bool
    parameter: str
    value: float
    parameter2: str | None
    value2: float | None
    norm: float
    period: float
    parameters: dict[str, float] = field(repr=False)
    u0: dict[str, float] = field(repr=False)
    umax: dict[str, float] = field(repr=False)
    umin: dict[str, float] = field(repr=False)
    umean: dict[str, float] = field(repr=False)
    #: What XPPAUT's AUTO stores, on the unit circle's terms: exp(lambda) for
    #: steady states and the Floquet multipliers for periodic orbits. A
    #: point is stable when all of them lie inside the unit circle (for
    #: orbits, apart from the trivial multiplier 1).
    multipliers: np.ndarray = field(repr=False)

    @property
    def eigenvalues(self) -> np.ndarray | None:
        """The eigenvalues of the Jacobian at a steady state, recovered
        from ``multipliers``, which are $e^{\\lambda}$.

        The real parts are exact; the imaginary parts are known only modulo
        $2\\pi$, as XPPAUT stores them. ``None`` for a periodic orbit,
        whose ``multipliers`` are its Floquet multipliers.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> np.round(d.first("HB").eigenvalues, 4)
            array([0.0001+0.435j, 0.0001-0.435j])

        See also:
            [`Model.equilibrium`][xpplab.Model.equilibrium].
        """
        if self.periodic:
            return None
        with np.errstate(divide="ignore"):
            return np.log(self.multipliers.astype(complex))

    @property
    def description(self) -> str:
        """The point's type in words, such as ``"Hopf bifurcation"``.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> [p.description for p in d.labelled()]
            ['end point', 'Hopf bifurcation', 'Hopf bifurcation', 'end point']

        See also:
            [`type`][xpplab.Point] (an attribute).
        """
        return TYPE_NAMES.get(self.type, self.type)

    def __repr__(self) -> str:
        what = self.type or "point"
        kind = "periodic" if self.periodic else "steady state"
        stab = "stable" if self.stable else "unstable"
        lab = f" #{self.label}" if self.label else ""
        extra = f", period={self.period:.6g}" if self.periodic else ""
        two = (f", {self.parameter2}={self.value2:.6g}"
               if self.two_parameter and self.parameter2 else "")
        return (f"<{what}{lab}: {stab} {kind}, {self.parameter}={self.value:.6g}"
                f"{two}, norm={self.norm:.6g}{extra}>")

    def _repr_html_(self) -> str:
        from . import _html as H
        rows = [["type", f"{self.type or 'point'} ({self.description})"],
                ["label", self.label],
                ["stability", "stable" if self.stable else "unstable"],
                ["kind", "periodic orbit" if self.periodic else "steady state"],
                [self.parameter, H.fmt(self.value)]]
        if self.two_parameter and self.parameter2:
            rows.append([self.parameter2, H.fmt(self.value2)])
        rows.append(["norm", H.fmt(self.norm)])
        if self.periodic:
            rows.append(["period", H.fmt(self.period)])
        return H.block("Diagram point", H.table(None, rows))


class Diagram:
    """Points of a bifurcation diagram, in the order AUTO computed them.

    [`Auto.equilibria`][xpplab.auto.Auto.equilibria],
    [`Auto.continue_from`][xpplab.auto.Auto.continue_from] and the others
    return the points of their run; [`Auto.diagram`][xpplab.auto.Auto.diagram]
    is the whole diagram. A diagram is a sequence of
    [`Point`][xpplab.Point]: ``d[0]``, ``len(d)``, ``for p in d``, and
    slices, which are diagrams.

    Attributes:
        points (list[Point]): the points.
        variables (list[str]): the model's variables.

    Examples:
        >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
        >>> d = m.auto.equilibria("iapp")
        >>> d
        <Diagram: 49 points; special: HB#2, HB#3>
        >>> d[0]
        <EP #1: unstable steady state, iapp=0.05, norm=0.444483>

    See also:
        [auto steady](../reference/auto-steady.md) in the command window.
    """

    def __init__(self, points: list[Point], variables: list[str]):
        self.points = points
        self.variables = variables

    def __len__(self) -> int:
        return len(self.points)

    def __iter__(self):
        return iter(self.points)

    def __getitem__(self, i):
        if isinstance(i, slice):
            return Diagram(self.points[i], self.variables)
        return self.points[i]

    def special(self, type: str | None = None) -> list[Point]:
        """The special points: the bifurcations AUTO found (HB, LP, BP, PD,
        TR, ...), without the end points and the regular labelled ones.

        Args:
            type: only the points of this type, such as ``"HB"``.

        Returns:
            The points, in the diagram's order.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> d.special()
            [<HB #2: unstable steady state, iapp=0.262464, norm=0.289108>, <HB #3: unstable steady state, iapp=0.456949, norm=0.6087>]
            >>> d.special("LP")
            []

        See also:
            [`labelled`][xpplab.Diagram.labelled],
            [`first`][xpplab.Diagram.first].
        """
        out = [p for p in self.points if p.label and p.type not in ("", "EP")]
        if type:
            out = [p for p in out if p.type == type.upper()]
        return out

    def labelled(self) -> list[Point]:
        """Every labelled point, the end points and the regular ones saved
        every NPR steps included.

        Returns:
            The points, in the diagram's order.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> [(p.type, p.label) for p in d.labelled()]
            [('EP', 1), ('HB', 2), ('HB', 3), ('EP', 4)]

        See also:
            [`special`][xpplab.Diagram.special].
        """
        return [p for p in self.points if p.label]

    def first(self, type: str) -> Point:
        """The first special point of a type.

        Args:
            type: the type, such as ``"HB"``.

        Returns:
            The point.

        Raises:
            LookupError: the diagram has no point of that type.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> d.first("HB")
            <HB #2: unstable steady state, iapp=0.262464, norm=0.289108>

        See also:
            [`special`][xpplab.Diagram.special],
            [`label`][xpplab.Diagram.label].
        """
        pts = self.special(type)
        if not pts:
            raise LookupError(f"no {type} point in this diagram")
        return pts[0]

    def label(self, label: int) -> Point:
        """The point with a label.

        Args:
            label: the label.

        Returns:
            The point.

        Raises:
            LookupError: no point has that label.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> d.label(3)
            <HB #3: unstable steady state, iapp=0.456949, norm=0.6087>

        See also:
            [`first`][xpplab.Diagram.first].
        """
        for p in self.points:
            if p.label == label:
                return p
        raise LookupError(f"no point with label {label}")

    def branches(self) -> dict[int, "Diagram"]:
        """The diagram split by AUTO's branch number.

        Returns:
            A dictionary from the branch number (negative for periodic
            orbits) to a [`Diagram`][xpplab.Diagram] of its points.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> orbits = m.auto.continue_from(d.first("HB"), "periodic", nmax=50)
            >>> {b: len(part) for b, part in m.auto.diagram.branches().items()}
            {1: 49, -2: 50}

        See also:
            [`Point`][xpplab.Point] (its ``branch``).
        """
        out: dict[int, list[Point]] = {}
        for p in self.points:
            out.setdefault(p.branch, []).append(p)
        return {b: Diagram(ps, self.variables) for b, ps in out.items()}

    def to_pandas(self):
        """The points as a pandas DataFrame, one row per point. Needs
        pandas.

        The columns are ``branch``, ``point``, ``type``, ``label``,
        ``stable``, ``periodic``, the parameters, ``norm``, ``period``, and
        the largest and smallest value of every variable (``V_max``,
        ``V_min``, ...).

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> df = d.to_pandas()
            >>> df[df.type == "HB"][["label", "iapp", "V_max"]].round(4)
                label    iapp   V_max
            18      2  0.2625 -0.1989
            45      3  0.4569  0.0641

        See also:
            [`to_csv`][xpplab.Diagram.to_csv].
        """
        import pandas as pd
        rows = []
        for p in self.points:
            r = {"branch": p.branch, "point": p.point, "type": p.type,
                 "label": p.label, "stable": p.stable, "periodic": p.periodic,
                 p.parameter: p.value, "norm": p.norm, "period": p.period}
            if p.two_parameter and p.parameter2:
                r[p.parameter2] = p.value2
            for v in self.variables:
                r[f"{v}_max"] = p.umax[v]
                r[f"{v}_min"] = p.umin[v]
            rows.append(r)
        return pd.DataFrame(rows)

    def to_csv(self, path) -> None:
        """Write the points as a CSV file, every number exact.

        The desktop app's ``export FILE.csv auto`` writes the same file.

        Args:
            path: the file to write.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> d.to_csv("diagram.csv")
            >>> print(open("diagram.csv").readline().strip())
            branch,point,type,label,stable,periodic,two_parameter,parameter,value,parameter2,value2,norm,period,V_max,W_max,V_min,W_min

        See also:
            [export](../reference/export.md) in the command window;
            [`to_mat`][xpplab.Diagram.to_mat].
        """
        from .analysis import write_csv
        cols = ["branch", "point", "type", "label", "stable", "periodic",
                "two_parameter", "parameter", "value", "parameter2", "value2",
                "norm", "period"]
        cols += [f"{v}_max" for v in self.variables]
        cols += [f"{v}_min" for v in self.variables]
        rows = []
        for p in self.points:
            two = p.two_parameter and p.parameter2
            rows.append([p.branch, p.point, p.type, p.label, p.stable, p.periodic,
                         p.two_parameter, p.parameter, p.value,
                         p.parameter2 if two else "", p.value2 if two else None,
                         p.norm, p.period]
                        + [p.umax[v] for v in self.variables]
                        + [p.umin[v] for v in self.variables])
        write_csv(path, cols, rows)

    def to_mat(self, path) -> None:
        """Write the points as a MATLAB .mat file.

        The numeric columns of [`to_csv`][xpplab.Diagram.to_csv] become
        column vectors: ``branch``, ``point``, ``label``, ``stable``,
        ``periodic``, ``two_parameter``, ``value``, ``value2`` (NaN for a
        one-parameter point), ``norm``, ``period``, then ``V_max`` and
        ``V_min`` of every variable. The text columns (``type``,
        ``parameter``, ``parameter2``) are left out. The desktop app's
        ``export FILE.mat auto`` writes the same file.

        Args:
            path: the file to write.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> d.to_mat("diagram.mat")

        See also:
            [export](../reference/export.md) in the command window;
            [`to_csv`][xpplab.Diagram.to_csv].
        """
        from .analysis import write_mat
        pts = self.points
        two = [p.two_parameter and p.parameter2 for p in pts]
        cols = {
            "branch": [p.branch for p in pts], "point": [p.point for p in pts],
            "label": [p.label for p in pts], "stable": [p.stable for p in pts],
            "periodic": [p.periodic for p in pts],
            "two_parameter": [p.two_parameter for p in pts],
            "value": [p.value for p in pts],
            "value2": [p.value2 if t else np.nan for p, t in zip(pts, two)],
            "norm": [p.norm for p in pts], "period": [p.period for p in pts]}
        for v in self.variables:
            cols[f"{v}_max"] = [p.umax[v] for p in pts]
        for v in self.variables:
            cols[f"{v}_min"] = [p.umin[v] for p in pts]
        write_mat(path, list(cols), [np.array(c, dtype=float) for c in cols.values()])

    def plot(self, y: str | None = None, *, ax=None, labels: bool = True, **kw):
        """Plot the diagram, as XPPAUT's AUTO window draws it. Needs
        matplotlib.

        Stable branches are solid and unstable ones dashed; periodic orbits
        show their largest and smallest value; the special points are
        labelled. Two-parameter curves are drawn in the plane of the two
        parameters.

        Args:
            y: what to plot against the parameter: a variable (by default
                the first), ``"norm"`` or ``"period"``.
            ax: the matplotlib axes to draw in; by default new ones.
            labels: label the special points.
            **kw: passed on to matplotlib.

        Returns:
            The matplotlib axes.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> orbits = m.auto.continue_from(d.first("HB"), "periodic", nmax=50)
            >>> ax = m.auto.diagram.plot()
            >>> ax.get_xlabel(), ax.get_ylabel()
            ('iapp', 'V (max/min)')

        See also:
            [auto steady](../reference/auto-steady.md) in the command
            window, whose page shows such a diagram.
        """
        from .plotting import plot_diagram
        return plot_diagram(self, y=y, ax=ax, labels=labels, **kw)

    def __repr__(self) -> str:
        sp = ", ".join(f"{p.type}#{p.label}" for p in self.special()[:12])
        more = " ..." if len(self.special()) > 12 else ""
        return f"<Diagram: {len(self)} points; special: {sp or 'none'}{more}>"

    def _repr_html_(self) -> str:
        from . import _html as H
        special = self.special()
        rows = [[p.type, p.label, "stable" if p.stable else "unstable",
                 p.parameter, H.fmt(p.value), H.fmt(p.norm)]
                for p in special[:H.MAX_ROWS]]
        return H.block(
            f"Diagram: {len(self)} points, {len(special)} special",
            H.table(["type", "label", "stability", "parameter", "value", "norm"],
                    rows, len(special)))


class Auto:
    """AUTO's continuation of a model: ``m.auto``.

    The continuation is XPPAUT's own AUTO, run as XPPAUT's AUTO window runs
    it. The diagram is kept from run to run, as in XPPAUT: each run adds its
    points, its labels count on from the last, and any labelled point can
    be continued from.

    Examples:
        >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
        >>> d = m.auto.equilibria("iapp")
        >>> orbits = m.auto.continue_from(d.first("HB"), "periodic", nmax=50)
        >>> m.auto.diagram
        <Diagram: 99 points; special: HB#2, HB#3, LP#5, LP#6>

    See also:
        [auto steady](../reference/auto-steady.md),
        [auto from](../reference/auto-from.md) and the other ``auto``
        commands; the guide [Bifurcations with AUTO](auto.md).
    """

    def __init__(self, model):
        self._model = model
        self._eng = model._engine

    # -- settings -------------------------------------------------------------

    def get(self, name: str) -> float:
        """The value of one of AUTO's settings.

        Args:
            name: the setting, in any case: ``"ds"``, ``"nmax"``, ... (see
                [`set`][xpplab.auto.Auto.set]).

        Returns:
            The value.

        Raises:
            EngineError: there is no such setting.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> m.auto.get("ds"), m.auto.get("parmax"), m.auto.get("nmax")
            (0.02, 0.5, 2000.0)

        See also:
            [auto set](../reference/auto-set.md) in the command window;
            [`settings`][xpplab.auto.Auto.settings].
        """
        v = ctypes.c_double()
        self._eng.call("xppe_auto_get", name.upper().encode(), ctypes.byref(v))
        return v.value

    def set(self, **settings) -> "Auto":
        """Set AUTO's numerical settings, as XPPAUT's AUTO *Numerics*
        dialog does. They stay for the later runs.

        Each continuation method also takes them as keyword arguments, for
        its run and the later ones.

        Args:
            **settings: settings and values, in any case: ``ntst``,
                ``nmax``, ``npr``, ``ds`` (whose sign sets the direction),
                ``dsmin``, ``dsmax``, ``parmin``, ``parmax``, ``normmin``,
                ``normmax``, ``ncol``, ``epsl``, ``epsu``, ``epss``,
                ``iad``, ``mxbf``, ``iid``, ``itmx``, ``itnw``, ``nwtn``,
                ``iads``, ``suppbp``.

        Returns:
            ``m.auto``, so that calls chain.

        Raises:
            KeyError: no setting has one of the names.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.set(nmax=200, ds=0.01).equilibria("iapp")
            >>> m.auto.get("nmax")
            200.0

        See also:
            [auto set](../reference/auto-set.md) in the command window,
            whose page says what each setting does;
            [`get`][xpplab.auto.Auto.get].
        """
        for k, v in settings.items():
            if k.lower() not in SETTINGS:
                raise KeyError(f"unknown AUTO setting {k!r}; choose from "
                               f"{', '.join(SETTINGS)}")
            self._eng.call("xppe_auto_set", k.upper().encode(), float(v))
        return self

    @property
    def settings(self) -> dict[str, float]:
        """Every one of AUTO's settings, as a snapshot.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> s = m.auto.settings
            >>> s["ntst"], s["npr"], s["parmin"], s["parmax"]
            (15.0, 500.0, -0.2, 0.5)

        See also:
            [auto set](../reference/auto-set.md) in the command window;
            [`set`][xpplab.auto.Auto.set].
        """
        return {k: self.get(k) for k in SETTINGS}

    def parameters(self, main: str, second: str | None = None) -> "Auto":
        """Choose the continuation parameter, and the second one for
        two-parameter curves.

        The continuation methods choose them from their arguments; this is
        for a run that keeps them.

        Args:
            main: the continuation parameter.
            second: the second parameter of a two-parameter curve.

        Returns:
            ``m.auto``, so that calls chain.

        Raises:
            EngineError: the model has no such parameter.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> m.auto.parameters("iapp", "gca")
            <xpplab.auto.Auto object at ...>

        See also:
            [`continue_from`][xpplab.auto.Auto.continue_from], whose
            ``second=`` does the same.
        """
        self._eng.call("xppe_auto_parameters", main.encode(),
                       second.encode() if second else None)
        return self

    # -- running --------------------------------------------------------------

    def reset(self) -> "Auto":
        """Forget the whole bifurcation diagram; the next run's labels
        start at 1 again.

        Returns:
            ``m.auto``, so that calls chain.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> m.auto.reset().diagram
            <Diagram: 0 points; special: none>

        See also:
            [auto reset](../reference/auto-reset.md) in the command window.
        """
        self._eng.call("xppe_auto_reset")
        return self

    # -- files ----------------------------------------------------------------

    def save(self, path) -> None:
        """Save the diagram as an .auto file, as XPPAUT's *AUTO > File >
        Save diagram* does.

        The file holds AUTO's settings, every point and AUTO's restart data,
        so that [`load`][xpplab.auto.Auto.load] brings back a diagram that
        can be continued from its labelled points. Its last line names the
        model's variables and AUTO's parameters. XPPAUT opens these files
        too.

        Args:
            path: the file to write.

        Raises:
            EngineError: there is no diagram, or the file cannot be written.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> m.auto.save("lecar.auto")
            >>> open("lecar.auto").read().splitlines()[-1]
            '# XPP Lab model: variables V W; parameters iapp phi v1 v2 v3 v4 gca vk'

        See also:
            [auto save](../reference/auto-save.md) in the command window;
            [`load`][xpplab.auto.Auto.load].
        """
        self._eng.call("xppe_auto_save", os.fsencode(Path(path).resolve()))

    def load(self, path) -> Diagram:
        """Replace the diagram with one saved by
        [`save`][xpplab.auto.Auto.save] or by XPPAUT's *AUTO > File > Save
        diagram*.

        Its settings and restart data come with it, so
        [`continue_from`][xpplab.auto.Auto.continue_from] works on its
        labels. A file that names another model's variables or parameters
        is refused, and the diagram shown stays.

        Args:
            path: the .auto file.

        Returns:
            The loaded diagram.

        Raises:
            EngineError: the file cannot be read, or it was saved from
                another model.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> m.auto.save("lecar.auto")
            >>> m.auto.reset().load("lecar.auto")
            <Diagram: 49 points; special: HB#2, HB#3>

        See also:
            [auto load](../reference/auto-load.md) in the command window;
            [`save`][xpplab.auto.Auto.save].
        """
        self._eng.call("xppe_auto_load", os.fsencode(Path(path).resolve()))
        return self.diagram

    def orbit(self, point: Point | int):
        """The periodic orbit at a labelled point, one period long, from
        AUTO's own solution, as XPPAUT's *AUTO > File > Import orbit* does.

        It becomes the model's last run, so the analysis methods see it.

        Args:
            point: a labelled [`Point`][xpplab.Point] of a periodic orbit,
                or its label.

        Returns:
            The orbit, as a [`Trajectory`][xpplab.Trajectory].

        Raises:
            EngineError: no orbit has that label.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> orbits = m.auto.continue_from(d.first("HB"), "periodic", nmax=50)
            >>> m.auto.orbit(orbits.first("LP"))
            <Trajectory: 61 points, t=0..25.7585, columns T, V, W, CUR, ICA, IK, RATE>

        See also:
            [auto orbit](../reference/auto-orbit.md) in the command window;
            [`along`][xpplab.auto.Auto.along].
        """
        label = point.label if isinstance(point, Point) else int(point)
        self._eng.call("xppe_auto_orbit", label)
        return self._model._collect()

    def along(self, column: str, points=None):
        """An output column along the diagram: at each steady state its
        value there, and over each periodic orbit its smallest and largest
        value.

        The column can be an auxiliary quantity, which AUTO does not
        follow. AUTO keeps a whole orbit only at labelled points (every NPR
        steps, and the special points), so orbits elsewhere give NaN: lower
        NPR before the continuation to sample more of them. Each orbit is
        imported as by [`orbit`][xpplab.auto.Auto.orbit], so it becomes the
        last run. The parameters are restored afterwards.

        Args:
            column: an output column: a variable or an auxiliary quantity.
            points: a [`Diagram`][xpplab.Diagram] or a list of points; by
                default the whole diagram. Two-parameter points give NaN.

        Returns:
            A [`Table`][xpplab.Table], one row per point: the continuation
            parameter, ``min``, ``max``, ``stable``, ``periodic``,
            ``branch`` and ``label``.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> orbits = m.auto.continue_from(d.first("HB"), "periodic", nmax=50)
            >>> t = m.auto.along("IK", orbits.special())
            >>> t
            <Table IK along the diagram: 2 rows, columns iapp, min, max, stable, periodic, branch, label>

        See also:
            [`orbit`][xpplab.auto.Auto.orbit],
            [`Model.evaluate`][xpplab.Model.evaluate].
        """
        from .analysis import Table
        m = self._model
        pts = list(self.diagram if points is None else points)
        model_pars = {p.upper(): p for p in m._parameter_names}
        saved = {p: float(m.get(p)) for p in m._parameter_names}
        lo, hi = np.full(len(pts), np.nan), np.full(len(pts), np.nan)
        try:
            for i, p in enumerate(pts):
                if p.two_parameter or (p.periodic and not p.label):
                    continue
                m.set(**{model_pars[k.upper()]: v for k, v in p.parameters.items()
                         if k and k.upper() in model_pars})
                if p.periodic:
                    sol = self.orbit(p)
                    lo[i], hi[i] = float(np.min(sol[column])), float(np.max(sol[column]))
                else:
                    lo[i] = hi[i] = m.evaluate(p.u0)[column]
        finally:
            m.set(**saved)
        name = pts[0].parameter if pts else "parameter"
        data = {name: np.array([p.value for p in pts]), "min": lo, "max": hi,
                "stable": np.array([p.stable for p in pts]),
                "periodic": np.array([p.periodic for p in pts]),
                "branch": np.array([p.branch for p in pts]),
                "label": np.array([p.label for p in pts])}
        return Table(data, title=f"{column} along the diagram")

    def _run(self, key: str, settings: dict, form: str | None = None) -> Diagram:
        if settings:
            self.set(**settings)
        before = self._eng.call("xppe_auto_count", check=False)
        if form is None:
            self._eng.call("xppe_auto_run", key.encode())
        else:
            self._eng.call("xppe_auto_run_form", key.encode(), form.encode())
        return self._points(before)

    def equilibria(self, parameter: str, *, from_equilibrium: bool = True,
                   **settings) -> Diagram:
        """Follow a branch of steady states as a parameter changes, as
        XPPAUT's *AUTO > Run > Steady state* does.

        AUTO starts from the initial conditions, which should be an
        equilibrium: by default Newton's method first moves them onto the
        nearest one. It goes from the parameter's current value, in the
        direction of the sign of ``ds``, until ``parmin``, ``parmax`` or
        ``nmax`` steps, and labels the folds (LP), Hopf points (HB) and
        branch points (BP) it meets.

        Args:
            parameter: the continuation parameter.
            from_equilibrium: move the initial conditions onto an
                equilibrium first, with [`Model.equilibrium`][xpplab.Model.equilibrium].
            **settings: AUTO settings, as for
                [`set`][xpplab.auto.Auto.set]: ``ds=-0.01``, ``nmax=300``.

        Returns:
            The points of this run, as a [`Diagram`][xpplab.Diagram]; the
            whole diagram is in [`diagram`][xpplab.auto.Auto.diagram].

        Raises:
            EngineError: AUTO computed no points; the message says why.
            KeyError: an unknown setting.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> d.special()
            [<HB #2: unstable steady state, iapp=0.262464, norm=0.289108>, <HB #3: unstable steady state, iapp=0.456949, norm=0.6087>]

        See also:
            [auto steady](../reference/auto-steady.md) in the command
            window; [`continue_from`][xpplab.auto.Auto.continue_from].
        """
        if from_equilibrium:
            try:
                self._model.equilibrium(set_initial=True)
            except E.EngineError:
                pass  # let AUTO try from the initial condition as given
        self.parameters(parameter)
        self._eng.call("xppe_auto_ungrab")
        return self._run("s", settings)

    def start(self, kind: str, parameter: str, *, second: str | None = None,
              left=None, right=None, unstable: int | None = None,
              stable: int | None = None, **settings) -> Diagram:
        """Start a new branch from the model's current state or last run,
        as XPPAUT's *AUTO > Run* menu does.

        A ``"steady"`` start continues the steady state at the initial
        conditions, as [`equilibria`][xpplab.auto.Auto.equilibria] does
        without moving them. A ``"periodic"`` start continues the last run,
        which must be one period of an orbit. A ``"bvp"`` start continues
        the boundary-value problem of the model's ``b`` lines from the last
        run. A ``"homoclinic"`` or ``"heteroclinic"`` start continues the
        last run, a long periodic orbit (see [`orbit`][xpplab.auto.Auto.orbit]),
        in ``parameter`` and ``second``.

        Args:
            kind: ``"steady"``, ``"periodic"``, ``"bvp"``,
                ``"homoclinic"`` or ``"heteroclinic"``.
            parameter: the continuation parameter.
            second: the second parameter; needed for a homoclinic or
                heteroclinic orbit.
            left: the saddle the orbit leaves (and for a homoclinic one,
                returns to), as a dictionary by variable or a sequence.
            right: the saddle a heteroclinic orbit reaches.
            unstable: the dimension of the saddle's unstable manifold (the
                *Homoclinic info* box).
            stable: the dimension of its stable manifold.
            **settings: AUTO settings, as for
                [`set`][xpplab.auto.Auto.set].

        Returns:
            The points of this run, as a [`Diagram`][xpplab.Diagram].

        Raises:
            ValueError: an unknown kind, or a homoclinic or heteroclinic
                start without ``second`` or the saddles.
            EngineError: AUTO computed no points; the message says why.

        Examples:
            A periodic orbit of ``lecar.ode``, found by integrating, then
            followed in ``iapp``:

            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> period, orbit = m.set(iapp=0.3).limit_cycle()
            >>> orbits = m.auto.start("periodic", "iapp", nmax=30)
            >>> orbits
            <Diagram: 30 points; special: LP#2>
            >>> orbits[0].periodic, round(orbits[0].period, 2)
            (True, 15.6)

        See also:
            [auto steady](../reference/auto-steady.md) in the command
            window, whose page covers the other starts too; the
            [AUTO guide](../manual/auto07p.md).
        """
        key = _ACTIONS["start"].get(kind)
        if key is None:
            raise ValueError(f"unknown start {kind!r}; choose from "
                             f"{', '.join(_ACTIONS['start'])}")
        form = None
        if kind in ("homoclinic", "heteroclinic"):
            if second is None:
                raise ValueError(f"a {kind} start needs second=<parameter>")
            if left is None or (kind == "heteroclinic" and right is None):
                raise ValueError(f"a {kind} start needs the equilibria "
                                 "left= (and right= for a heteroclinic)")
            names = list(self._model.variables)

            def values(v):
                v = [v[n] for n in names] if isinstance(v, dict) else list(v)
                if len(v) != len(names):
                    raise ValueError(f"expected {len(names)} values, "
                                     f"got {len(v)}")
                # float() and int(): the repr of a NumPy number is
                # "np.float64(0.14)", which XPPAUT's atof() reads as 0.
                return [float(x) for x in v]

            fields = []
            if unstable is not None:
                fields.append(("dim unstable", int(unstable)))
            if stable is not None:
                fields.append(("dim stable", int(stable)))
            fields += [(f"{n}_L", x) for n, x in zip(names, values(left))]
            if kind == "heteroclinic":
                fields += [(f"{n}_R", x) for n, x in zip(names, values(right))]
            form = ";".join(f"{k}={v!r}" for k, v in fields)
            # The engine keeps 511 characters of a dialog answer.
            if len(form.encode()) >= 512:
                raise ValueError(f"the {kind} form is {len(form.encode())} "
                                 "bytes long; the engine reads at most 511")
        self.parameters(parameter, second)
        self._eng.call("xppe_auto_ungrab")
        return self._run(key, settings, form)

    def continue_from(self, point: Point | int, action: str | None = None, *,
                      parameter: str | None = None,
                      second: str | None = None, **settings) -> Diagram:
        """Continue from a labelled point, as XPPAUT's AUTO *Grab* then
        *Run* does.

        What can be done depends on the point:

        | Point | ``action`` |
        |---|---|
        | Hopf (HB) | ``"periodic"`` (the default), ``"extend"``, ``"new"``, ``"two-parameter"`` |
        | fold (LP) | ``"two-parameter"`` |
        | branch point (BP) | ``"switch"`` (of orbits or a boundary-value problem), ``"extend"``, ``"new"``, ``"two-parameter"`` |
        | period doubling (PD) | ``"doubling"``, ``"two-parameter"``, ``"fixed-period"``, ``"extend"`` |
        | torus (TR) | ``"two-parameter"``, ``"fixed-period"``, ``"extend"`` |
        | other orbit | ``"extend"``, ``"fixed-period"`` |
        | other steady state | ``"extend"`` |

        At a branch point of steady states AUTO has already followed the
        other branch, in the run that found it; ``"switch"`` there raises.

        Args:
            point: a labelled [`Point`][xpplab.Point], or its label.
            action: what to do, from the table; by default its first
                entry.
            parameter: the continuation parameter; by default the point's.
            second: the second parameter, for a two-parameter curve.
            **settings: AUTO settings, as for
                [`set`][xpplab.auto.Auto.set].

        Returns:
            The points of this run, as a [`Diagram`][xpplab.Diagram].

        Raises:
            ValueError: an action the point does not allow, a two-parameter
                curve without ``second``, or a switch at a branch point of
                steady states.
            LookupError: no point has the label.
            EngineError: AUTO computed no points; the message says why.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> orbits = m.auto.continue_from(d.first("HB"), "periodic", nmax=50)
            >>> orbits.special()
            [<LP #5: unstable periodic, iapp=0.248415, norm=0.381873, period=25.7585>, <LP #6: unstable periodic, iapp=0.465697, norm=0.548541, period=19.061>]
            >>> curve = m.auto.continue_from(2, "two-parameter", second="gca", nmax=100)
            >>> curve[5]
            <point: unstable steady state, iapp=0.280238, gca=1.01067, norm=0.290812>

        See also:
            [auto from](../reference/auto-from.md) in the command window.
        """
        p = point if isinstance(point, Point) else self.diagram.label(point)
        kind = p.type if p.type in _ACTIONS and p.type != "" else (
            "periodic" if p.periodic else "steady")
        choices = _ACTIONS[kind]
        if action is None:
            # at a branch point of steady states, extend: see "switch" below
            action = ("extend" if kind == "BP" and p.type_code % 10 == 1
                      else next(iter(choices)))
        if action not in choices:
            raise ValueError(
                f"from a {TYPE_NAMES.get(p.type, kind)} you can: "
                f"{', '.join(choices)} (got {action!r})")
        if action == "switch" and p.type_code % 10 == 1:
            # AUTO's type 1: a branch point of an algebraic problem, whose
            # other branch AUTO follows by itself in the run that finds it.
            raise ValueError(
                f"BP #{p.label} is a branch point of steady states: AUTO "
                "follows the other branch of such a point by itself, in the "
                "run that finds it, and switch would only retrace this one. If "
                "the other branch is missing, the run met more branch points "
                "than AUTO follows (MXBF, 5 a run): m.auto.set(mxbf=10) "
                "follows more.")
        if parameter or second:
            self.parameters(parameter or p.parameter, second)
        if action == "two-parameter" and second is None and kind != "steady":
            raise ValueError("two-parameter continuation needs second=<parameter>")
        self._eng.call("xppe_auto_grab", p.label)
        return self._run(choices[action], settings)

    # -- results --------------------------------------------------------------

    @property
    def diagram(self) -> Diagram:
        """Every point computed so far, from every run since the model was
        loaded or the diagram reset.

        Examples:
            >>> m = xs.load(xs.example("lecar.ode")).apply_set("hopf")
            >>> d = m.auto.equilibria("iapp")
            >>> orbits = m.auto.continue_from(d.first("HB"), "periodic", nmax=50)
            >>> m.auto.diagram
            <Diagram: 99 points; special: HB#2, HB#3, LP#5, LP#6>

        See also:
            [`reset`][xpplab.auto.Auto.reset], [`save`][xpplab.auto.Auto.save].
        """
        return self._points(0)

    def _points(self, start: int) -> Diagram:
        eng = self._eng
        n = eng.call("xppe_auto_count", check=False)
        nvar = len(self._model.variables)
        npar = eng.call("xppe_auto_parameter_count", check=False)
        slots = [E._text(eng._lib.xppe_auto_parameter_name(i))
                 for i in range(npar)]
        info = E.AutoPointInfo()
        buf = (ctypes.c_double * max(nvar, npar, 1))()

        def data(i, what, count):
            eng.call("xppe_auto_point_data", i, what, buf, count)
            return list(buf[:count])

        pts = []
        # One change of directory for the whole diagram, not two per call.
        with eng.batch():
            for i in range(start, n):
                eng.call("xppe_auto_point", i, ctypes.byref(info))
                var = self._model.variables
                pars = dict(zip(slots, data(i, E.AUTO_PARAMS, npar)))
                ev = (np.array(data(i, E.AUTO_EIG_RE, nvar))
                      + 1j * np.array(data(i, E.AUTO_EIG_IM, nvar)))
                p2 = slots[info.par2_slot] if 0 <= info.par2_slot < npar else None
                pts.append(Point(
                    index=i, branch=info.branch, point=info.point,
                    type_code=info.type, type=info.type_name.decode(),
                    label=info.label, stable=bool(info.stable),
                    periodic=bool(info.periodic),
                    two_parameter=bool(info.two_parameter),
                    parameter=slots[info.par1_slot], value=info.par1,
                    parameter2=p2, value2=info.par2 if p2 else None,
                    norm=info.norm, period=info.period, parameters=Values(pars),
                    u0=Values(zip(var, data(i, E.AUTO_U0, nvar))),
                    umax=Values(zip(var, data(i, E.AUTO_UMAX, nvar))),
                    umin=Values(zip(var, data(i, E.AUTO_UMIN, nvar))),
                    umean=Values(zip(var, data(i, E.AUTO_UMEAN, nvar))),
                    multipliers=ev))
        return Diagram(pts, self._model.variables)
