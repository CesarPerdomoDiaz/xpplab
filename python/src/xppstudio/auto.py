"""AUTO continuation and bifurcation diagrams.

The continuation is XPPAUT's own AUTO, run exactly as XPPAUT's AUTO window
runs it, without the window:

>>> m = xppstudio.load("lecar.ode")
>>> m.apply_set("hopf")
>>> d = m.auto.equilibria("iapp")            # steady states vs. iapp
>>> d.special()                              # labelled points: HB, LP, ...
>>> hb = d.first("HB")
>>> orbits = m.auto.continue_from(hb, "periodic")
>>> m.auto.diagram.plot()                    # the whole diagram
"""

from __future__ import annotations

import ctypes
from dataclasses import dataclass, field

import numpy as np

from . import _engine as E

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
    """One computed point of a bifurcation diagram."""

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
    eigenvalues: np.ndarray = field(repr=False)

    @property
    def description(self) -> str:
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


class Diagram:
    """A list of bifurcation-diagram points with helpers to explore them."""

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
        """Labelled special points (HB, LP, BP, PD, TR, ...), optionally of
        one type."""
        out = [p for p in self.points if p.label and p.type not in ("", "EP")]
        if type:
            out = [p for p in out if p.type == type.upper()]
        return out

    def labelled(self) -> list[Point]:
        """Every labelled point, including end points and regular saves."""
        return [p for p in self.points if p.label]

    def first(self, type: str) -> Point:
        pts = self.special(type)
        if not pts:
            raise LookupError(f"no {type} point in this diagram")
        return pts[0]

    def label(self, label: int) -> Point:
        for p in self.points:
            if p.label == label:
                return p
        raise LookupError(f"no point with label {label}")

    def branches(self) -> dict[int, "Diagram"]:
        out: dict[int, list[Point]] = {}
        for p in self.points:
            out.setdefault(p.branch, []).append(p)
        return {b: Diagram(ps, self.variables) for b, ps in out.items()}

    def to_pandas(self):
        """One row per point: branch, type, label, stability, parameters,
        norm, period, and the max/min of every variable."""
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

    def plot(self, y: str | None = None, *, ax=None, labels: bool = True, **kw):
        """Plot the diagram. Stable branches are solid, unstable dashed;
        periodic orbits show their max and min; special points are labelled.

        ``y`` is a variable name (default: the first variable), "norm", or
        "period". Two-parameter curves are drawn in the parameter plane.
        """
        from .plotting import plot_diagram
        return plot_diagram(self, y=y, ax=ax, labels=labels, **kw)

    def __repr__(self) -> str:
        sp = ", ".join(f"{p.type}#{p.label}" for p in self.special()[:12])
        more = " ..." if len(self.special()) > 12 else ""
        return f"<Diagram: {len(self)} points; special: {sp or 'none'}{more}>"


class Auto:
    """AUTO continuation for a model. Reach it as ``model.auto``."""

    def __init__(self, model):
        self._model = model
        self._eng = model._engine

    # -- settings -------------------------------------------------------------

    def get(self, name: str) -> float:
        v = ctypes.c_double()
        self._eng.call("xppe_auto_get", name.upper().encode(), ctypes.byref(v))
        return v.value

    def set(self, **settings) -> "Auto":
        """Set AUTO numerics: ``m.auto.set(ds=0.01, nmax=500, parmax=2)``."""
        for k, v in settings.items():
            if k.lower() not in SETTINGS:
                raise KeyError(f"unknown AUTO setting {k!r}; choose from "
                               f"{', '.join(SETTINGS)}")
            self._eng.call("xppe_auto_set", k.upper().encode(), float(v))
        return self

    @property
    def settings(self) -> dict[str, float]:
        return {k: self.get(k) for k in SETTINGS}

    def parameters(self, main: str, second: str | None = None) -> "Auto":
        """Choose the continuation parameter (and the second one for
        two-parameter curves)."""
        self._eng.call("xppe_auto_parameters", main.encode(),
                       second.encode() if second else None)
        return self

    # -- running --------------------------------------------------------------

    def reset(self) -> "Auto":
        """Forget the whole bifurcation diagram."""
        self._eng.call("xppe_auto_reset")
        return self

    def _run(self, key: str, settings: dict) -> Diagram:
        if settings:
            self.set(**settings)
        before = self._eng.call("xppe_auto_count", check=False)
        self._eng.call("xppe_auto_run", key.encode())
        return self._points(before)

    def equilibria(self, parameter: str, *, from_equilibrium: bool = True,
                   **settings) -> Diagram:
        """Continue a branch of steady states in ``parameter``.

        By default Newton's method first moves the initial condition onto a
        nearby equilibrium, the usual way to start AUTO. Extra keyword
        arguments are AUTO settings (``ds=-0.01``, ``nmax=300``, ...).
        Returns the newly computed points.
        """
        if from_equilibrium:
            try:
                self._model.equilibrium(set_initial=True)
            except E.EngineError:
                pass  # let AUTO try from the initial condition as given
        self.parameters(parameter)
        self._eng.call("xppe_auto_ungrab")
        return self._run("s", settings)

    def start(self, kind: str, parameter: str, **settings) -> Diagram:
        """Start a new branch: kind is "steady", "periodic" (from the
        current orbit), "bvp", "homoclinic" or "heteroclinic"."""
        key = _ACTIONS["start"].get(kind)
        if key is None:
            raise ValueError(f"unknown start {kind!r}; choose from "
                             f"{', '.join(_ACTIONS['start'])}")
        self.parameters(parameter)
        self._eng.call("xppe_auto_ungrab")
        return self._run(key, settings)

    def continue_from(self, point: Point | int, action: str | None = None, *,
                      parameter: str | None = None,
                      second: str | None = None, **settings) -> Diagram:
        """Continue from a labelled point, like Grab + Run in XPPAUT.

        ``action`` depends on the point: from a Hopf point "periodic",
        "two-parameter", "extend" or "new"; from a fold "two-parameter";
        from a branch point "switch", "extend", "two-parameter"; from period
        doubling "doubling", "two-parameter", "fixed-period"; from a torus
        point "two-parameter", "fixed-period"; from an ordinary point
        "extend". Two-parameter continuations need ``second=``.
        """
        p = point if isinstance(point, Point) else self.diagram.label(point)
        kind = p.type if p.type in _ACTIONS and p.type != "" else (
            "periodic" if p.periodic else "steady")
        choices = _ACTIONS[kind]
        if action is None:
            action = next(iter(choices))
        if action not in choices:
            raise ValueError(
                f"from a {TYPE_NAMES.get(p.type, kind)} you can: "
                f"{', '.join(choices)} (got {action!r})")
        if parameter or second:
            self.parameters(parameter or p.parameter, second)
        if action == "two-parameter" and second is None and kind != "steady":
            raise ValueError("two-parameter continuation needs second=<parameter>")
        self._eng.call("xppe_auto_grab", p.label)
        return self._run(choices[action], settings)

    # -- results --------------------------------------------------------------

    @property
    def diagram(self) -> Diagram:
        """Every point computed so far."""
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
                norm=info.norm, period=info.period, parameters=pars,
                u0=dict(zip(var, data(i, E.AUTO_U0, nvar))),
                umax=dict(zip(var, data(i, E.AUTO_UMAX, nvar))),
                umin=dict(zip(var, data(i, E.AUTO_UMIN, nvar))),
                umean=dict(zip(var, data(i, E.AUTO_UMEAN, nvar))),
                eigenvalues=ev))
        return Diagram(pts, self._model.variables)
