"""XPPAUT animations (.ani files), evaluated by XPPAUT's own animator code.

    m.run(total=20)
    ani = m.animation("pend.ani")
    shapes = ani.frame(100)        # what to draw at row 100 of the run
    ani.plot(100)                  # draw it with matplotlib
    ani.save_gif("pend.gif")       # every few rows, as an animated GIF
    ani.nullclines("u", "v")       # what xnull and ynull lines draw
    ani.grab(0.5, 0.5, 0.2, 0.8)   # drag a grab point, as the app does
"""
from __future__ import annotations

import ctypes
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from . import _engine as E

SHAPES = {0: "line", 1: "rect", 2: "ellipse", 3: "text", 4: "comet",
          5: "nullcline"}

# XPPAUT's colour indices for $BLACK, $RED, ... $PURPLE.
NAMED = {0: "#000000", 20: "#ff0000", 21: "#ff4500", 22: "#ff8c00",
         23: "#ffb000", 24: "#e8d800", 25: "#9acd32", 26: "#00a000",
         27: "#00a0a0", 28: "#0000ff", 29: "#8a2be2"}


def colour_of(color: int, level: float) -> str:
    """An XPPAUT colour as a matplotlib colour."""
    if color >= 0:
        return NAMED.get(color, "#000000")
    import colorsys
    z = min(max(level, 0.0), 1.0)
    r, g, b = colorsys.hsv_to_rgb(0.83 * (1 - z), 1.0, 0.9)
    return "#%02x%02x%02x" % (int(r * 255), int(g * 255), int(b * 255))


@dataclass
class Shape:
    """One shape of an animation frame, in the animation's coordinates."""
    shape: str        # "line", "rect", "ellipse", "text", "comet" or "nullcline"
    command: int      # its command's position in the .ani file
    x1: float
    y1: float
    x2: float
    y2: float
    color: str        # a matplotlib colour
    filled: bool = False
    thickness: int = 1
    trail: int = 0
    font_size: int = 0
    text: str = ""
    segments: np.ndarray | None = None  # nullclines: (n, 4) x1, y1, x2, y2


class Animation:
    """An .ani file parsed by XPPAUT, applied to a model's last run."""

    def __init__(self, model, path: Path):
        self.model = model
        self.path = Path(path)
        self.commands = model._engine.call("xppe_ani_load", str(self.path).encode())
        w = [ctypes.c_double() for _ in range(4)]
        model._engine.call("xppe_ani_window", *[ctypes.byref(x) for x in w])
        self.window = tuple(x.value for x in w)  # xlo, ylo, xhi, yhi

    def __len__(self) -> int:
        """The number of frames: the rows of the model's last run."""
        return self.model._engine.call("xppe_rows", check=False)

    def frame(self, row: int) -> list[Shape]:
        """The shapes at a row of the run. Row 0 also evaluates the file's
        permanent commands."""
        eng = self.model._engine
        n = eng.call("xppe_ani_frame", int(row))
        item = E.AniItem()
        out = []
        for i in range(n):
            eng.call("xppe_ani_get", i, ctypes.byref(item))
            seg = None
            if item.shape == 5:
                seg = np.zeros((item.trail, 4))
                eng.call("xppe_ani_segments", i,
                         seg.ctypes.data_as(ctypes.POINTER(ctypes.c_double)),
                         item.trail)
            out.append(Shape(SHAPES.get(item.shape, "line"), item.command,
                             item.x1, item.y1, item.x2, item.y2,
                             colour_of(item.color, item.level),
                             bool(item.filled), item.thickness, item.trail,
                             item.font_size, E._text(item.text), seg))
        return out

    def nullclines(self, x: str, y: str, grid: int = 100, parameter: str | None = None,
                   steps: int = 0, low: float = 0.0, high: float = 1.0) -> int:
        """Compute the nullclines that the file's ``xnull`` and ``ynull``
        lines draw, in the plane of variables ``x`` and ``y`` over the box
        of the first such line. With ``parameter`` and ``steps`` the frozen
        sets, the ones ``xnull ...;k`` picks, are made too: ``steps + 1``
        of them, for the parameter from ``low`` to ``high`` (it is put
        back). Returns the number of frozen sets."""
        m = self.model
        return self.model._engine.call(
            "xppe_ani_nullclines", m._var_index(x), m._var_index(y), int(grid),
            parameter.encode() if parameter else None, int(steps),
            float(low), float(high))

    def grab_points(self) -> list[tuple[float, float, float]]:
        """The grab points of the file as ``(x, y, tolerance)``, where the
        state of the last frame puts them."""
        eng = self.model._engine
        out = []
        for i in range(eng.call("xppe_ani_grab_count", check=False)):
            v = [ctypes.c_double() for _ in range(3)]
            eng.call("xppe_ani_grab_point", i, *[ctypes.byref(c) for c in v])
            out.append(tuple(c.value for c in v))
        return out

    def grab(self, x: float, y: float, x2: float, y2: float, dt: float = 0.1,
             run: bool = True):
        """The animator's Grab mode: press the mouse at ``(x, y)`` of the
        first frame, drag to ``(x2, y2)`` and release. Returns the index of
        the grab point taken, or ``None`` when no point is within its
        tolerance (nothing changes then). The release makes the values the
        file sets initial conditions or parameters, and runs the model when
        the file says ``runnow=1`` (unless ``run`` is false)."""
        eng = self.model._engine
        index, runnow = ctypes.c_int(), ctypes.c_int()
        eng.call("xppe_ani_frame", 0, check=False)
        eng.call("xppe_ani_grab_begin", float(x), float(y), ctypes.byref(index))
        if index.value < 0:
            return None
        eng.call("xppe_ani_grab_move", float(x2), float(y2), float(dt))
        eng.call("xppe_ani_grab_end", ctypes.byref(index), ctypes.byref(runnow))
        if runnow.value and run:
            self.model.run()
        return index.value

    def plot(self, row: int, ax=None, trails: dict | None = None):
        """Draw one frame with matplotlib. ``trails`` (a dict kept between
        calls) collects comet tails; plot frames in order to see them."""
        import matplotlib.pyplot as plt
        from matplotlib import patches
        from matplotlib.collections import LineCollection
        ax = ax or plt.gca()
        xlo, ylo, xhi, yhi = self.window
        ax.clear()
        ax.set_xlim(xlo, xhi)
        ax.set_ylim(ylo, yhi)
        ax.set_aspect("equal")
        ax.set_xticks([])
        ax.set_yticks([])
        for s in self.frame(row):
            lw = max(s.thickness, 1)
            if s.shape == "nullcline":
                ax.add_collection(LineCollection(
                    s.segments.reshape(-1, 2, 2), colors=s.color, linewidths=lw))
            elif s.shape == "line":
                ax.plot([s.x1, s.x2], [s.y1, s.y2], color=s.color, lw=lw)
            elif s.shape == "rect":
                ax.add_patch(patches.Rectangle(
                    (min(s.x1, s.x2), min(s.y1, s.y2)), abs(s.x2 - s.x1),
                    abs(s.y2 - s.y1), fill=s.filled, color=s.color, lw=lw))
            elif s.shape == "ellipse":
                ax.add_patch(patches.Ellipse((s.x1, s.y1), 2 * s.x2, 2 * s.y2,
                                             fill=s.filled, color=s.color, lw=lw))
            elif s.shape == "text":
                ax.text(s.x1, s.y1, s.text, color=s.color,
                        fontsize=8 + 3 * s.font_size)
            elif s.shape == "comet":
                pts = (trails.setdefault(s.command, []) if trails is not None
                       else [])
                pts.append((s.x1, s.y1))
                del pts[:-max(s.trail, 1)]
                xs, ys = zip(*pts)
                if s.thickness < 0:
                    ax.scatter(xs, ys, s=4 * s.thickness ** 2, color=s.color)
                else:
                    ax.plot(xs, ys, color=s.color, lw=lw)
        return ax

    def save_gif(self, path, *, every: int = 5, fps: int = 20, size=(4, 4)):
        """Write the animation as a GIF, one frame every ``every`` rows.
        Needs matplotlib and Pillow."""
        import matplotlib.pyplot as plt
        from matplotlib.animation import FuncAnimation, PillowWriter
        fig, ax = plt.subplots(figsize=size)
        trails: dict = {}
        rows = list(range(0, len(self), max(1, every)))
        anim = FuncAnimation(fig, lambda r: self.plot(r, ax, trails), frames=rows)
        anim.save(str(path), writer=PillowWriter(fps=fps))
        plt.close(fig)
        return Path(path)

    def __repr__(self) -> str:
        return f"<Animation {self.path.name}: {self.commands} commands, {len(self)} frames>"
