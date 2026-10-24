"""XPPAUT animations (.ani files), evaluated by XPPAUT's own animator code.

    m.run(total=20)
    ani = m.animation("pend.ani")
    shapes = ani.frame(100)        # what to draw at row 100 of the run
    ani.plot(100)                  # draw it with matplotlib
    ani.save_gif("pend.gif")       # every few rows, as an animated GIF
"""
from __future__ import annotations

import ctypes
from dataclasses import dataclass
from pathlib import Path

from . import _engine as E

SHAPES = {0: "line", 1: "rect", 2: "ellipse", 3: "text", 4: "comet"}

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
    shape: str        # "line", "rect", "ellipse", "text" or "comet"
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
            out.append(Shape(SHAPES.get(item.shape, "line"), item.command,
                             item.x1, item.y1, item.x2, item.y2,
                             colour_of(item.color, item.level),
                             bool(item.filled), item.thickness, item.trail,
                             item.font_size, E._text(item.text)))
        return out

    def plot(self, row: int, ax=None, trails: dict | None = None):
        """Draw one frame with matplotlib. ``trails`` (a dict kept between
        calls) collects comet tails; plot frames in order to see them."""
        import matplotlib.pyplot as plt
        from matplotlib import patches
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
            if s.shape == "line":
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
