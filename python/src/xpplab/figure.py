"""Figures for papers: panels, curves, labels and export, in the words the
desktop app's Figure window uses.

The app writes every change to a figure as one of these calls (its
echo), so a figure made with the mouse can be made again, or changed, in
Python. The same calls also work on their own::

    import xpplab as xs
    fig = xs.Figure(width=86, height=65)          # millimetres
    p = fig[0]
    p.line(sol["T"], sol["GAM"], scale=(1, 180 / 3.14159), label=r"$\\Gamma$")
    p.set(xlabel=r"$\\tau$", ylabel=r"$\\Gamma$ [deg]")
    fig.save("elevation.pdf")

Labels take mathtext, matplotlib's subset of TeX between dollar signs. The
fonts, sizes, line widths and colours are the ones the app draws with
(DejaVu Sans, or STIX for ``font="serif"``), so both give the same figure.
Drawing needs matplotlib.
"""

from __future__ import annotations

import math
import re

import numpy as np

__all__ = ["Figure", "Panel", "Series", "diagram_xy", "evaluate_expression"]

TAB10 = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd",
         "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf"]
LINES = {"-": "-", "solid": "-", "--": "--", "dashed": "--", ":": ":",
         "dotted": ":", "-.": "-.", "dashdot": "-.", "none": "none", "": "none",
         None: "none"}
MARKERS = {None: None, "": None, "none": None, "o": "o", "s": "s", "^": "^",
           "D": "D", "x": "x", "+": "+", "*": "*", ".": "."}
LEGENDS = ["none", "best", "upper right", "upper left", "lower left",
           "lower right", "outside"]

# matplotlib's dash patterns, which it scales by the line width as the app does
_DASH = {"--": (3.7, 1.6), ":": (1.0, 1.65), "-.": (6.4, 1.6, 1.0, 1.6)}


class Series:
    """One curve or set of markers in a panel; change its look with
    :meth:`set`."""

    def __init__(self, x, y, **style):
        self.x = np.asarray(x, dtype=float).ravel()
        self.y = np.asarray(y, dtype=float).ravel()
        if self.x.shape != self.y.shape:
            raise ValueError(f"x has {self.x.size} values and y {self.y.size}")
        self.label = ""
        self.color = None
        self.line = "-"
        self.width = 1.5
        self.marker = None
        self.markersize = 6.0
        self.markerfill = True
        self.markercolor = None
        self.every = 1
        self.scale = (1.0, 1.0)
        self.set(**style)

    def set(self, **style) -> "Series":
        """label, color ("#rrggbb"), line ("-", "--", ":", "-.", "none"),
        width (points), marker ("o", "s", "^", "D", "x", "+", "*", ".",
        None), markersize, markerfill, markercolor, every (a marker every
        so many points), scale ((sx, sy): the data are shown as x*sx,
        y*sy, e.g. radians as degrees)."""
        for k, v in style.items():
            if k == "line":
                if v not in LINES:
                    raise ValueError(f"line is one of {', '.join(map(str, LINES))}")
                v = LINES[v]
            elif k == "marker":
                if v not in MARKERS:
                    raise ValueError(f"marker is one of {', '.join(k for k in MARKERS if k)}")
                v = MARKERS[v]
            elif k == "scale":
                v = (float(v[0]), float(v[1]))
            elif k not in ("label", "color", "width", "markersize", "markerfill",
                           "markercolor", "every"):
                raise TypeError(f"unknown style {k!r}")
            setattr(self, k, v)
        return self

    def __repr__(self) -> str:
        return f"<Series {self.label or '(no label)'}: {self.x.size} points>"


class Panel:
    """A set of axes: its series, labels, limits and legend."""

    def __init__(self, inset=None):
        self.series: list[Series] = []
        self.title = self.xlabel = self.ylabel = ""
        self.xlim = self.ylim = None   # None: from the data, widened by 5%
        self.logx = self.logy = False
        self.grid = False
        self.xstep = self.ystep = None
        self.legend = "best"
        self.equal = False
        self.inset = inset             # (parent, (x, y, w, h)) for an inset

    def set(self, **props) -> "Panel":
        """title, xlabel, ylabel, xlim ((lo, hi) or None), ylim, logx, logy,
        grid, xstep, ystep (tick spacing, None: chosen), legend ("best",
        "upper right", "upper left", "lower left", "lower right",
        "outside", "none"), equal (the same scale on both axes)."""
        for k, v in props.items():
            if k == "legend" and v not in LEGENDS:
                raise ValueError(f"legend is one of {', '.join(LEGENDS)}")
            if k in ("xlim", "ylim") and v is not None:
                v = (float(v[0]), float(v[1]))
            if k not in ("title", "xlabel", "ylabel", "xlim", "ylim", "logx", "logy",
                         "grid", "xstep", "ystep", "legend", "equal"):
                raise TypeError(f"unknown panel property {k!r}")
            setattr(self, k, v)
        return self

    def line(self, x, y, **style) -> Series:
        """Add a curve (NaN breaks it). Its colour is the next of matplotlib's
        ten unless ``color`` is given."""
        s = Series(x, y, **style)
        if s.color is None:
            s.color = TAB10[len(self.series) % 10]
        self.series.append(s)
        return s

    def __getitem__(self, k: int) -> Series:
        return self.series[k]

    def remove(self, k: int) -> None:
        del self.series[k]

    # -- what the app adds -------------------------------------------------

    def run(self, sol, x: str, y: str, **style) -> Series:
        """Two columns of a run."""
        return self.line(sol[x], sol[y], **style)

    def runs(self, runs, x: str, y: str, *, event: int | None = None,
             **style) -> Series:
        """Every run of a :class:`~xpplab.RunList` as one series, broken
        between runs; with ``event``, only the runs that this global flag
        stopped (0: those that ran to the end)."""
        xs, ys = [], []
        for s in runs:
            if event is not None and s.event != event:
                continue
            xs += [s[x], [np.nan]]
            ys += [s[y], [np.nan]]
        if not xs:
            return self.line([], [], **style)
        return self.line(np.concatenate(xs[:-1]), np.concatenate(ys[:-1]), **style)

    def diagram(self, d, y: str, *, x: str | None = None, which: str = "max",
                stable: bool | None = None, periodic: bool | None = None,
                branch: int | None = None, between=None, where=None,
                model=None, along=None, **style) -> Series:
        """A quantity along an AUTO diagram (see :func:`diagram_xy`): a
        variable's ``which`` ("max" or "min") value, "period", "norm",
        eigenvalues ("re1", "im2", ...), Floquet multipliers ("mu1_re",
        "mu2_im", "mu1_abs", ...), or an auxiliary quantity (give
        ``model``, whose ``auto.along`` computes it)."""
        dx, dy = diagram_xy(d, y, x=x, which=which, stable=stable,
                            periodic=periodic, branch=branch, between=between,
                            where=where, model=model, along=along)
        return self.line(dx, dy, **style)

    def function(self, expression: str, xlim, n: int = 400, **style) -> Series:
        """y = f(x) for a formula in x, such as ``"1.109724/sqrt(x)"``:
        numbers, + - * / ^ (or **), parentheses, pi, e, and sqrt, exp, log,
        log10, sin, cos, tan, asin, acos, atan, sinh, cosh, tanh, abs."""
        x = np.linspace(float(xlim[0]), float(xlim[1]), int(n))
        return self.line(x, evaluate_expression(expression, x), **style)

    def curve(self, model, y: str, x: str, xlim, *, at: dict | None = None,
              n: int = 400, **style) -> Series:
        """A right-hand side or an output column of ``model`` as one
        variable ``x`` sweeps ``xlim``, the others held at ``at`` (or their
        initial values): ``y`` is ``"om'"`` for the derivative of om, or a
        column such as an auxiliary quantity."""
        xv, yv = model_curve(model, y, x, xlim, at=at, n=n)
        return self.line(xv, yv, **style)

    def __repr__(self) -> str:
        return f"<Panel: {len(self.series)} series>"


class Figure:
    """A page of panels: ``rows`` x ``cols`` axes, then any insets.

    ``width`` and ``height`` are in millimetres (a journal column is about
    86 mm wide, a full page about 178 mm), ``fontsize`` the labels' size
    in points (ticks and legend are 0.85 of it), ``font`` "sans" (DejaVu
    Sans) or "serif" (STIX, close to Times)."""

    def __init__(self, width: float = 86, height: float = 65, *, rows: int = 1,
                 cols: int = 1, font: str = "sans", fontsize: float = 9):
        if font not in ("sans", "serif"):
            raise ValueError('font is "sans" or "serif"')
        self.width, self.height = float(width), float(height)
        self.font, self.fontsize = font, float(fontsize)
        self.rows, self.cols = int(rows), int(cols)
        self.panels = [Panel() for _ in range(self.rows * self.cols)]

    def __getitem__(self, k: int) -> Panel:
        return self.panels[k]

    def set(self, **props) -> "Figure":
        """width, height (mm), font, fontsize."""
        for k, v in props.items():
            if k not in ("width", "height", "font", "fontsize"):
                raise TypeError(f"unknown figure property {k!r}")
            setattr(self, k, v)
        return self

    def grid(self, rows: int, cols: int) -> "Figure":
        """Change the grid of panels; panels beyond it go."""
        n_old, n_new = self.rows * self.cols, int(rows) * int(cols)
        insets = self.panels[n_old:]
        grid = self.panels[:n_old][:n_new] + [Panel() for _ in range(n_new - n_old)]
        for p in insets:
            parent, rect = p.inset
            p.inset = (parent if parent < n_new else 0, rect)
        self.rows, self.cols = int(rows), int(cols)
        self.panels = grid + insets
        return self

    def inset(self, parent: int, rect=(0.55, 0.55, 0.4, 0.4)) -> Panel:
        """A small panel drawn inside panel ``parent``, at ``rect`` = (x, y,
        width, height) in fractions of that panel's axes from its lower
        left corner."""
        p = Panel(inset=(int(parent), tuple(float(v) for v in rect)))
        p.legend = "none"
        self.panels.append(p)
        return p

    # -- drawing -----------------------------------------------------------

    def _rc(self) -> dict:
        fs = self.fontsize
        serif = self.font == "serif"
        return {
            "font.family": "STIXGeneral" if serif else "DejaVu Sans",
            "mathtext.fontset": "stix" if serif else "dejavusans",
            "font.size": fs, "axes.labelsize": fs, "axes.titlesize": 1.1 * fs,
            "xtick.labelsize": 0.85 * fs, "ytick.labelsize": 0.85 * fs,
            "legend.fontsize": 0.85 * fs, "axes.formatter.use_mathtext": True,
            "axes.linewidth": 0.8, "xtick.major.width": 0.8,
            "ytick.major.width": 0.8, "xtick.major.size": 3.5,
            "ytick.major.size": 3.5, "legend.framealpha": 1.0,
            "legend.fancybox": False, "lines.scale_dashes": True,
        }

    def draw(self):
        """The figure as a matplotlib Figure."""
        import matplotlib.pyplot as plt
        from matplotlib.ticker import MultipleLocator
        with plt.rc_context(self._rc()):
            fig = plt.figure(figsize=(self.width / 25.4, self.height / 25.4),
                             layout="constrained")
            axes = []
            n = self.rows * self.cols
            for k, p in enumerate(self.panels):
                if k < n:
                    ax = fig.add_subplot(self.rows, self.cols, k + 1)
                else:
                    parent, rect = p.inset
                    ax = axes[parent].inset_axes(list(rect))
                    small = 0.8 * self.fontsize
                    ax.tick_params(labelsize=0.85 * small)
                axes.append(ax)
                self._draw_panel(ax, p, MultipleLocator, inset=k >= n)
        return fig

    def _draw_panel(self, ax, p: Panel, locator, inset: bool):
        fs = self.fontsize * (0.8 if inset else 1.0)
        for s in p.series:
            kw = dict(color=s.color, lw=s.width, linestyle=s.line)
            if s.line in _DASH:
                kw["dashes"] = _DASH[s.line]
            if s.marker:
                kw.update(marker=s.marker, ms=s.markersize, markevery=s.every,
                          mec=s.markercolor or s.color, mew=1.0,
                          mfc=s.color if s.markerfill else "none")
            ax.plot(s.x * s.scale[0], s.y * s.scale[1], label=s.label or None,
                    **kw)
        if p.logx:
            ax.set_xscale("log")
        if p.logy:
            ax.set_yscale("log")
        if p.xlim:
            ax.set_xlim(*p.xlim)
        if p.ylim:
            ax.set_ylim(*p.ylim)
        if p.xstep:
            ax.xaxis.set_major_locator(locator(p.xstep))
        if p.ystep:
            ax.yaxis.set_major_locator(locator(p.ystep))
        if p.grid:
            ax.grid(True, color="#b0b0b0", lw=0.6)
        if p.equal:
            ax.set_aspect("equal", adjustable="box")
        if p.title:
            ax.set_title(p.title, fontsize=1.1 * fs)
        if p.xlabel:
            ax.set_xlabel(p.xlabel, fontsize=fs)
        if p.ylabel:
            ax.set_ylabel(p.ylabel, fontsize=fs)
        if p.legend != "none" and any(s.label for s in p.series):
            kw = dict(fontsize=0.85 * fs, edgecolor="#cccccc")
            if p.legend == "outside":
                ax.legend(loc="upper left", bbox_to_anchor=(1.02, 1), **kw)
            else:
                ax.legend(loc=p.legend, **kw)

    def save(self, path, *, dpi: int = 300) -> None:
        """Write the figure: .pdf, .svg, .eps, or .png at ``dpi``."""
        import matplotlib.pyplot as plt
        fig = self.draw()
        with plt.rc_context(self._rc()):
            fig.savefig(path, dpi=dpi)
        plt.close(fig)

    def _repr_png_(self):
        import io
        import matplotlib.pyplot as plt
        fig = self.draw()
        buf = io.BytesIO()
        fig.savefig(buf, format="png", dpi=100)
        plt.close(fig)
        return buf.getvalue()

    def __repr__(self) -> str:
        return (f"<Figure {self.width:g} x {self.height:g} mm, {self.rows} x "
                f"{self.cols} panels, {len(self.panels) - self.rows * self.cols} insets>")


# ---- Data for the panels ------------------------------------------------------

_QUANTITY = re.compile(r"^(re|im)(\d+)$|^mu(\d+)_(re|im|abs)$")


def _sorted_eigenvalues(p) -> np.ndarray:
    with np.errstate(divide="ignore"):
        ev = np.log(np.asarray(p.multipliers, dtype=complex))
    return np.array(sorted(ev, key=lambda z: (-z.real, -z.imag)))


def _sorted_multipliers(p) -> np.ndarray:
    mu = np.asarray(p.multipliers, dtype=complex)
    return np.array(sorted(mu, key=lambda z: (-abs(z), -z.real, -z.imag)))


def _point_value(p, i, name: str, which: str, along) -> float:
    key = name.upper()
    if key == "PERIOD":
        return p.period
    if key == "NORM":
        return p.norm
    if p.parameter and key == p.parameter.upper():
        return p.value
    if p.parameter2 and key == p.parameter2.upper():
        return p.value2 if p.two_parameter and p.value2 is not None else math.nan
    m = _QUANTITY.match(name.lower())
    if m:
        if m.group(1):
            if p.periodic:
                return math.nan
            k = int(m.group(2)) - 1
            ev = _sorted_eigenvalues(p)
            if k >= len(ev):
                return math.nan
            return float(ev[k].real if m.group(1) == "re" else ev[k].imag)
        if not p.periodic:
            return math.nan
        k = int(m.group(3)) - 1
        mu = _sorted_multipliers(p)
        if k >= len(mu):
            return math.nan
        part = m.group(4)
        return float(mu[k].real if part == "re" else mu[k].imag if part == "im"
                     else abs(mu[k]))
    if along is not None:
        return float(along["min" if which == "min" else "max"][i])
    src = p.umin if which == "min" else p.umax
    if name not in src:  # the second parameter at a one-parameter point
        return math.nan
    return float(src[name])


_ALONG: dict = {}


def _along(model, column, pts):
    """model.auto.along(column, pts), remembered: a figure often draws the
    same quantity along the same diagram several times (max and min,
    stable and unstable), and each orbit has to be imported."""
    key = (id(model), column.upper(),
           tuple((p.index, p.branch, p.point, p.label, p.value) for p in pts))
    table = _ALONG.get(key)
    if table is None:
        if len(_ALONG) > 32:
            _ALONG.clear()
        table = _ALONG[key] = model.auto.along(column, pts)
    return table


def diagram_xy(d, y: str, *, x: str | None = None, which: str = "max",
               stable: bool | None = None, periodic: bool | None = None,
               branch: int | None = None, between=None, where=None,
               model=None, along=None):
    """Two arrays, one value per point of a diagram, with NaN between the
    pieces of a curve, as the app's Figure window draws a diagram.

    ``x`` is a quantity too (default: each point's continuation
    parameter). ``stable``, ``periodic``, ``branch`` and ``between`` (two
    labels: the points from one to the other, in the order computed) keep
    only the points that match; each kept piece starts at the point before
    it on the same branch, so that a stable and an unstable piece meet.
    Eigenvalues are sorted by real part, then imaginary part, largest
    first; multipliers by modulus, largest first. The first point of a
    branch of orbits has no multipliers (it is the Hopf point): NaN.

    ``where = (column, "min" or "max", low, high)`` keeps the points where
    that value of a column lies in [low, high]: ``("TENS", "min", 0,
    inf)`` keeps the orbits whose tension stays positive. Auxiliary
    columns, as ``y`` or in ``where``, need ``model`` (or ``along``, a
    table of ``model.auto.along(y, d)``).
    """
    pts = list(d)
    tables = {}

    def table(column):
        if column not in tables:
            if model is None:
                raise ValueError(f"{column!r} needs model= to be computed along "
                                 "the diagram")
            tables[column] = _along(model, column, pts)
        return tables[column]

    variables = set()
    if pts:
        variables = {k.upper() for k in pts[0].umax}
    named = {"PERIOD", "NORM"}
    if pts:
        named |= {pts[0].parameter.upper(), (pts[0].parameter2 or "").upper()}
    if (along is None and pts and y.upper() not in variables
            and not _QUANTITY.match(y.lower()) and y.upper() not in named):
        along = table(y)  # an auxiliary quantity: needs model=
    cond = None
    if where is not None:
        wcol, wwhich, wlo, whi = where
        if (wcol.upper() in variables or wcol.upper() in named
                or _QUANTITY.match(wcol.lower())):
            wv = np.array([_point_value(p, i, wcol, wwhich, None)
                           for i, p in enumerate(pts)])
        else:
            wv = np.asarray(table(wcol)["min" if wwhich == "min" else "max"])
        cond = (wv >= float(wlo)) & (wv <= float(whi))
    if which not in ("max", "min"):
        raise ValueError('which is "max" or "min"')
    lo, hi = 0, len(pts) - 1
    if between is not None:
        idx = [i for i, p in enumerate(pts) if p.label in between]
        if len(idx) < 2:
            raise LookupError(f"labels {between} are not both in the diagram")
        lo, hi = min(idx), max(idx)

    def keep(i):
        p = pts[i]
        return (lo <= i <= hi
                and (cond is None or bool(cond[i]))
                and (stable is None or p.stable == stable)
                and (periodic is None or p.periodic == periodic)
                and (branch is None or p.branch == branch))

    def follows(i):  # point i continues point i - 1 on the same branch
        return (i > 0 and pts[i].branch == pts[i - 1].branch
                and pts[i].periodic == pts[i - 1].periodic
                and abs(pts[i].point) == abs(pts[i - 1].point) + 1)

    def value(i, name, along_table):
        p = pts[i]
        if name is None:
            return p.value
        if (name.lower().startswith("mu") and _QUANTITY.match(name.lower())
                and p.periodic and abs(p.point) == 1):
            return math.nan
        return _point_value(p, i, name, which, along_table)

    xs, ys = [], []
    for i in range(len(pts)):
        if not keep(i):
            continue
        starts = not (i > 0 and keep(i - 1) and follows(i))
        if starts:
            if xs:
                xs.append(math.nan)
                ys.append(math.nan)
            if i > 0 and follows(i) and lo <= i - 1:
                xs.append(value(i - 1, x, None))
                ys.append(value(i - 1, y, along))
        xs.append(value(i, x, None))
        ys.append(value(i, y, along))
    return np.array(xs, dtype=float), np.array(ys, dtype=float)


_FUNCS = {"sqrt": np.sqrt, "exp": np.exp, "log": np.log, "ln": np.log,
          "log10": np.log10, "sin": np.sin, "cos": np.cos, "tan": np.tan,
          "asin": np.arcsin, "acos": np.arccos, "atan": np.arctan,
          "sinh": np.sinh, "cosh": np.cosh, "tanh": np.tanh, "abs": np.abs}
_TOKEN = re.compile(r"\s*(?:(\d+\.?\d*(?:[eE][-+]?\d+)?|\.\d+(?:[eE][-+]?\d+)?)|"
                    r"([A-Za-z_][A-Za-z_0-9]*)|(\*\*|[-+*/^(),]))")


def evaluate_expression(expression: str, x):
    """A formula in x evaluated at the array x, by the same small grammar
    the app's Figure window reads (no Python eval)."""
    toks = []
    pos = 0
    expression = expression.strip()
    while pos < len(expression):
        m = _TOKEN.match(expression, pos)
        if not m or m.end() == pos:
            raise ValueError(f"cannot read {expression[pos:]!r}")
        toks.append(m.group(1) or m.group(2) or m.group(3))
        pos = m.end()
    toks.append(None)
    x = np.asarray(x, dtype=float)
    k = 0

    def peek():
        return toks[k]

    def take():
        nonlocal k
        k += 1
        return toks[k - 1]

    def expr():
        v = term()
        while peek() in ("+", "-"):
            v = v + term() if take() == "+" else v - term()
        return v

    def term():
        v = unary()
        while peek() in ("*", "/"):
            v = v * unary() if take() == "*" else v / unary()
        return v

    def unary():
        if peek() == "-":
            take()
            return -unary()
        if peek() == "+":
            take()
            return unary()
        return power()

    def power():
        v = atom()
        if peek() in ("^", "**"):
            take()
            return v ** unary()
        return v

    def atom():
        t = take()
        if t is None:
            raise ValueError("the formula ends too soon")
        if t == "(":
            v = expr()
            if take() != ")":
                raise ValueError("a ( is not closed")
            return v
        if t[0].isdigit() or t[0] == ".":
            return np.full_like(x, float(t))
        if t == "x":
            return x.copy()
        if t == "pi":
            return np.full_like(x, math.pi)
        if t == "e":
            return np.full_like(x, math.e)
        if t in _FUNCS:
            if take() != "(":
                raise ValueError(f"{t} needs (...)")
            v = expr()
            if take() != ")":
                raise ValueError("a ( is not closed")
            with np.errstate(all="ignore"):
                return _FUNCS[t](v)
        raise ValueError(f"unknown name {t!r}: the formula's variable is x")

    with np.errstate(all="ignore"):
        v = expr()
    if peek() is not None:
        raise ValueError(f"unexpected {peek()!r}")
    return v


def model_curve(model, y: str, x: str, xlim, *, at: dict | None = None, n: int = 400):
    """x values and the right-hand side (``"v'"``) or an output column
    along them; see :meth:`Panel.curve`."""
    xv = np.linspace(float(xlim[0]), float(xlim[1]), int(n))
    state = {v: float(model.get(v)) for v in model.variables}
    for k, v in (at or {}).items():
        state[model.variables[model._var_index(k)]] = float(v)
    xname = model.variables[model._var_index(x)]
    out = np.empty_like(xv)
    if y.endswith("'"):
        j = model._var_index(y[:-1])
        for i, v in enumerate(xv):
            state[xname] = v
            out[i] = model.rhs(state)[j]
    else:
        for i, v in enumerate(xv):
            state[xname] = v
            out[i] = model.evaluate(state)[y]
    return xv, out
