# annotate

Add text, an arrow or a marker to a plot, or clear them.

## Syntax

```
annotate WHERE text X Y [size PT] [color #rrggbb] TEXT
annotate WHERE arrow X1 Y1 X2 Y2 [color #rrggbb]
annotate WHERE marker X Y [o|s|^|D|x|+|*|.] [color #rrggbb]
annotate WHERE clear
annotate WHERE clear K
```

`WHERE` is `time`, `phase`, `auto`, `auto2` (the two-parameter diagram)
or `figure` (the Figure window's selected panel). Positions are in the
plot's own units.

- **In the app:** right-click a plot > **Text, etc.**
- **In Python:** `notes = xs.Annotations()`, then
  `notes["phase"].text(...)` and `notes["phase"].draw(ax)`.

## Description

Annotations label what a plot shows: an equilibrium, a branch, a moment
in time. Each is placed in the plot's own coordinates, so it stays with
the curves when the plot is zoomed or moved, and is exported with the
figure.

- `text` writes `TEXT` with the left end of its baseline at (X, Y);
  `size` is in points.
- `arrow` draws an arrow from (X1, Y1) to (X2, Y2), its head at the second
  point.
- `marker` draws a marker at (X, Y): a circle `o`, a square `s`, a
  triangle `^`, a diamond `D`, a cross `x` or `+`, a star `*` or a dot `.`.
- `clear` removes them all, `clear K` the `K`-th.

## Examples

### Labelling the phase plane

1. [Open the Morris-Lecar model](example:lecar.ode),
   [draw the nullclines](cmd:nullclines) and [show the Phase Plane](cmd:show phase).
2. Label the rest state, point at the unstable focus, and mark the saddle:

```
annotate phase text -0.38 0.12 size 14 rest
annotate phase arrow 0.3 0.6 0.03 0.3 color #d62728
annotate phase marker -0.1425 0.034 x
```

```python
import matplotlib
matplotlib.use("Agg")
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
ax = m.phase_plane("V", "W")
notes = xs.Annotations()
notes["phase"].text(-0.38, 0.12, "rest", size=14)
notes["phase"].arrow(0.3, 0.6, 0.03, 0.3, color="#d62728")
notes["phase"].marker(-0.1425, 0.034, "x")
notes["phase"].draw(ax)
```

The Phase Plane shows "rest" by the stable node at `V` = −0.38, a red
arrow ending at the unstable focus near (0.03, 0.27), and a cross on the
saddle at (−0.1425, 0.034).

How to read it. The labels are part of the plot: exported with
`export FILE.svg phase`, they appear in the figure; saved as a script,
they come back as the `notes[...]` lines of the Python block.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `WHERE` | the command; `notes["..."]` in Python | Which plot. | `figure` for the Figure window's panel. |
| `size PT` | the command; `size=` in Python | The text's size in points. | Leave it out for the size of the axis labels. |
| `color #rrggbb` | the command; `color=` in Python | The colour. | Black by default. |

## Results

- **The plot** shows the annotation; `clear` removes them.
- **In Python** `notes[...].draw(ax)` draws them on a matplotlib axes.

## More about

**Text and options.** The text is whatever follows the options, so a text
that begins with the word `size` or `color` needs another word before it.
In Python the text may use matplotlib's mathtext, for subscripts and
Greek letters.

## Tips

- Place labels after zooming to the final view.
- Use the Figure window ([Figures](../manual/figures.md)) for a figure
  with several panels.

## Algorithms

The annotations are stored with the plot and drawn after its curves;
nothing is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: annotate KIND: N numbers (the position in the plot's own units)` | A coordinate is missing. | Give every coordinate. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [Figures](../manual/figures.md): figures for papers.
- [export](export.md): save a plot as SVG or PNG.

## Version history

Introduced in 0.1.0.
