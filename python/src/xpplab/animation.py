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
    """One shape of an animation frame, in the animation's coordinates.
    [`Animation.frame`][xpplab.Animation.frame] returns them.

    Attributes:
        shape (str): ``"line"``, ``"rect"``, ``"ellipse"``, ``"text"``,
            ``"comet"`` or ``"nullcline"``.
        command (int): its command's place in the .ani file.
        x1 (float): a line's or a rectangle's first corner, an ellipse's
            centre, a text's or a comet's place.
        y1 (float): likewise.
        x2 (float): a line's or a rectangle's second corner, an ellipse's
            radii.
        y2 (float): likewise.
        color (str): a matplotlib colour.
        filled (bool): a filled rectangle or ellipse.
        thickness (int): the line's thickness; for a comet drawn as dots,
            minus their size.
        trail (int): a comet's length in frames; a nullcline's number of
            segments.
        font_size (int): a text's size, 0 to 4.
        text (str): a text's text.
        segments (numpy.ndarray | None): a nullcline's segments, one row of
            x1, y1, x2, y2 each.

    Examples:
        >>> m = xs.load(xs.example("pend.ode"))
        >>> sol = m.run()
        >>> [s.shape for s in m.animation("pend.ani").frame(0)]
        ['line', 'ellipse']

    See also:
        [ani](../reference/ani.md) in the command window.
    """
    shape: str
    command: int
    x1: float
    y1: float
    x2: float
    y2: float
    color: str
    filled: bool = False
    thickness: int = 1
    trail: int = 0
    font_size: int = 0
    text: str = ""
    segments: np.ndarray | None = None


class Animation:
    """An XPPAUT animation: an .ani file read by XPPAUT's own animator,
    applied to a model's last run.
    [`Model.animation`][xpplab.Model.animation] makes one.

    ``len(ani)`` is the number of frames, the rows of the run.

    Attributes:
        model (Model): the model.
        path (Path): the .ani file.
        commands (int): the number of the file's commands.
        window (tuple): ``(xlo, ylo, xhi, yhi)``, the animation's
            coordinates (``dimension`` in the file; by default 0 to 1).

    Examples:
        >>> m = xs.load(xs.example("pend.ode"))
        >>> sol = m.run()
        >>> ani = m.animation("pend.ani")
        >>> ani
        <Animation pend.ani: 2 commands, 401 frames>
        >>> ani.window
        (0.0, 0.0, 1.0, 1.0)

    See also:
        [ani](../reference/ani.md) in the command window.
    """

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
        """The shapes of one frame.

        Args:
            row: the row of the run. Row 0 also evaluates the file's
                permanent commands.

        Returns:
            A list of [`Shape`][xpplab.animation.Shape], in the file's
            order.

        Raises:
            EngineError: no such row.

        Examples:
            >>> m = xs.load(xs.example("pend.ode"))
            >>> sol = m.run()
            >>> ani = m.animation("pend.ani")
            >>> rod, bob = ani.frame(0)
            >>> rod.shape, (rod.x1, rod.y1), rod.thickness
            ('line', (0.5, 0.5), 3)
            >>> bob.shape, bob.color, bob.filled
            ('ellipse', '#ff0000', True)

        See also:
            [`plot`][xpplab.Animation.plot].
        """
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
        lines draw.

        They are computed in the plane of two variables, over the box of
        the first such line. With ``parameter`` and ``steps``, the frozen
        sets that ``xnull ...;k`` picks are made too: ``steps + 1`` of them,
        for the parameter from ``low`` to ``high``. The parameter is
        restored afterwards.

        Args:
            x: the variable along the box's horizontal side.
            y: the variable along its vertical side.
            grid: the number of cells along each side.
            parameter: the parameter of the frozen sets.
            steps: how many steps from ``low`` to ``high``.
            low: the parameter's first value.
            high: its last value.

        Returns:
            The number of frozen sets.

        Examples:
            >>> with open("small.ode", "w") as f:
            ...     _ = f.write("x'=x*(1-x)-y\\ny'=a*x-y\\npar a=0.5\\n@ total=5\\ndone\\n")
            >>> with open("small.ani", "w") as f:
            ...     _ = f.write("xnull -0.5;-0.5;1.5;1.5;$RED;-1\\nend\\n")
            >>> m = xs.load("small.ode")
            >>> sol = m.run()
            >>> ani = m.animation("small.ani")
            >>> ani.nullclines("x", "y", grid=60)
            0
            >>> [s.shape for s in ani.frame(0)]
            ['nullcline']

        See also:
            [ani](../reference/ani.md) in the command window;
            [`Model.nullclines`][xpplab.Model.nullclines].
        """
        m = self.model
        return self.model._engine.call(
            "xppe_ani_nullclines", m._var_index(x), m._var_index(y), int(grid),
            parameter.encode() if parameter else None, int(steps),
            float(low), float(high))

    def grab_points(self) -> list[tuple[float, float, float]]:
        """The file's grab points, where the initial values put them: the
        frame XPPAUT's Grab button shows.

        Returns:
            One ``(x, y, tolerance)`` per ``grab`` line.

        Examples:
            >>> m = xs.load(xs.example("itoy.ode"))
            >>> sol = m.run(total=1)
            >>> ani = m.animation("itoy.ani")
            >>> [tuple(round(v, 4) for v in g) for g in ani.grab_points()]
            [(0.9988, 0.05, 0.05), (3.0988, -0.05, 0.05)]

        See also:
            [`grab`][xpplab.Animation.grab].
        """
        eng = self.model._engine
        eng.call("xppe_ani_frame_initial")
        out = []
        for i in range(eng.call("xppe_ani_grab_count", check=False)):
            v = [ctypes.c_double() for _ in range(3)]
            eng.call("xppe_ani_grab_point", i, *[ctypes.byref(c) for c in v])
            out.append(tuple(c.value for c in v))
        return out

    def grab(self, x: float, y: float, x2: float, y2: float, dt: float = 0.1,
             run: bool = True):
        """Drag a grab point, as the animator's Grab mode does: press the
        mouse at a point of the first frame, drag, and release.

        The release sets the initial conditions or parameters the file's
        ``grab`` line names, and runs the model when the line says
        ``runnow=1``.

        Args:
            x: where the mouse is pressed, in the animation's coordinates.
            y: likewise.
            x2: where it is released.
            y2: likewise.
            dt: the drag's time, for the mouse's speed (``mouse_vx``,
                ``mouse_vy``).
            run: run the model if the file says ``runnow=1``.

        Returns:
            The index of the grab point taken, or ``None`` when no point is
            within its tolerance; nothing changes then.

        Examples:
            >>> m = xs.load(xs.example("itoy.ode"))
            >>> sol = m.run(total=1)
            >>> ani = m.animation("itoy.ani")
            >>> x, y, tol = ani.grab_points()[0]
            >>> ani.grab(x, y, -0.3, 0.9)
            0
            >>> ani.grab(x + 1, y, 0, 1) is None
            True

        See also:
            [ani](../reference/ani.md) in the command window.
        """
        eng = self.model._engine
        index, runnow = ctypes.c_int(), ctypes.c_int()
        eng.call("xppe_ani_frame_initial")
        eng.call("xppe_ani_grab_begin", float(x), float(y), ctypes.byref(index))
        if index.value < 0:
            return None
        eng.call("xppe_ani_grab_move", float(x2), float(y2), float(dt))
        eng.call("xppe_ani_grab_end", ctypes.byref(index), ctypes.byref(runnow))
        if runnow.value and run:
            self.model.run()
        return index.value

    def plot(self, row: int, ax=None, trails: dict | None = None):
        """Draw one frame with matplotlib.

        Args:
            row: the row of the run.
            ax: the matplotlib axes to draw in; by default the current ones,
                which are cleared.
            trails: a dictionary kept between calls, which collects the
                comets' tails; plot the frames in order to see them.

        Returns:
            The matplotlib axes.

        Examples:
            >>> m = xs.load(xs.example("pend.ode"))
            >>> sol = m.run()
            >>> ani = m.animation("pend.ani")
            >>> ax = ani.plot(100)
            >>> len(ax.lines), len(ax.patches)
            (1, 1)

        See also:
            [`save_gif`][xpplab.Animation.save_gif].
        """
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
        """Write the animation as an animated GIF. Needs matplotlib and
        Pillow.

        Args:
            path: the file to write.
            every: a frame every so many rows of the run.
            fps: frames per second.
            size: ``(width, height)`` in inches.

        Returns:
            The file's path.

        Examples:
            >>> m = xs.load(xs.example("pend.ode"))
            >>> sol = m.run()
            >>> ani = m.animation("pend.ani")
            >>> ani.save_gif("pend.gif", every=50).exists()
            True

        See also:
            [ani](../reference/ani.md) in the command window, whose window
            plays the animation.
        """
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
