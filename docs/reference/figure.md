# figure

Build a figure for a paper in the Figure window: a page in millimetres,
panels, curves from runs and diagrams, labels with mathematics, and PDF,
SVG, EPS or PNG files.

## Syntax

```
figure
figure new W H
figure new W H ROWS COLS
figure panel K
figure grid ROWS COLS
figure inset
figure run X Y
figure many X Y
figure many X Y split
figure diagram Y OPTIONS
figure range
figure range min
figure function FORMULA LO HI
figure curve Y X LO HI
figure eq
figure from FIGURE
figure title TEXT
figure xlabel TEXT
figure ylabel TEXT
figure xlim LO HI
figure xlim auto
figure style K OPTIONS
figure export FILE
figure export FILE.png DPI
```

`fig` is another name for `figure`. `FIGURE` is `time`, `phase`, `auto`
or `auto2`; `ylim` works as `xlim`.

- **In the app:** View > **Figure**, or **Add to the Figure window** on a
  plot's Export menu.
- **In Python:** `xs.Figure`, whose calls the app writes for each change:
  `fig = xs.Figure(86, 65, rows=1, cols=2)`, `fig[0].run(sol, "T", "V")`,
  `fig.save("figure.pdf")`.

## Description

`figure` alone brings the Figure window forward. `figure new` starts a
page `W` by `H` millimetres (86 by 65 when not given; more than 5 each)
with `ROWS` by `COLS` panels, at most 12. The commands after it act on the
selected panel, which `figure panel K` chooses, counting from 1 row by
row. `figure grid` changes the grid; `figure inset` adds a small panel
inside the selected one.

Series:

- `figure run X Y`: two columns of the last run;
- `figure many X Y`: the runs of [many](many.md), with `split` a series
  for each way the runs ended;
- `figure diagram Y`: the AUTO diagram, with the options `x X`, `max` or
  `min`, `stable` or `unstable`, `steady` or `orbits`, `branch B`,
  `between L1 L2` (the points between two labels) and
  `where COLUMN max|min LO HI`;
- `figure range`: the [range](range.md) diagram's maxima, or minima;
- `figure function FORMULA LO HI`: a formula in `x`, written without
  spaces;
- `figure curve Y X LO HI`: a formula of the model's variable `X`, such
  as a nullcline written by hand;
- `figure eq`: the equilibria marked on the Phase Plane;
- `figure from FIGURE`: a plot window's curves as a new panel, with its
  axis labels and annotations: the columns the Time Series shows; the
  run, the many runs and the marked equilibria of the Phase Plane; the
  bifurcation diagram's branches.

`figure title`, `xlabel` and `ylabel` take the rest of the line, with
mathtext between dollar signs (`$V_W$`). `figure xlim LO HI` fixes the
axis and `figure xlim auto` lets it follow the data. `figure style K`
changes the `K`-th series of the panel: `color #rrggbb`, `line -`, `--`,
`:`, `-.` or `none`, `width W` (points), `marker o`, `s`, `^`, `D`, `x`,
`+`, `*`, `.` or `none`, `scale SX SY` (shown as `x*SX`, `y*SY`) and
`label TEXT`, which takes the rest of the line.

`figure export` writes the figure: PDF, SVG and EPS as vector files with
the text as outlines, PNG at `DPI` dots per inch (50 to 2400). Relative
paths are relative to the model's folder.

## Examples

### Two panels for a paper

A journal column, 86 by 65 mm, with the voltage over time beside the
orbit:

1. [Open the Morris-Lecar model](example:lecar.ode),
   [set the current](cmd:set iapp=0.1 total=100) and [run](cmd:run).
2. [Start the figure](cmd:figure new 86 65 1 2), add
   [the time series](cmd:figure run T V) to the first panel, then
   [choose the second](cmd:figure panel 2) and add
   [the orbit](cmd:figure run V W).
3. Label the axes and write the files.

```
set iapp=0.1 total=100
run
figure new 86 65 1 2
figure run T V
figure xlabel time $t$
figure ylabel $V$
figure panel 2
figure run V W
figure xlabel $V$
figure ylabel $W$
figure export lecar.pdf
figure export lecar.png 600
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
sol = m.run(iapp=0.1, total=100)

fig = xs.Figure(86, 65, rows=1, cols=2)
fig[0].run(sol, "T", "V")
fig[0].set(xlabel="time $t$", ylabel="$V$")
fig[1].run(sol, "V", "W")
fig[1].set(xlabel="$V$", ylabel="$W$")
fig.save("lecar.pdf")
fig.save("lecar.png", dpi=600)
```

The command window answers "Saved the figure: lecar.pdf" and "Saved the
figure: lecar.png". The PNG is 2031 by 1535 pixels, with 600 dpi recorded
in it: 86 by 65 mm at 600 dots per inch, so it is placed at its size.
The script's PNG has the same size.

How to read it. The page is set in millimetres and the text in points,
so the figure keeps the journal's sizes whatever the screen. With
[echo](echo.md) on, each command is followed by the `xs.Figure` call that
makes the same change; the Python block above is those calls, without the
legend labels and the full paths the app adds.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `W`, `H` | `figure new` | The page in millimetres. | 86 or 178 mm wide for most journals' one or two columns. |
| `ROWS`, `COLS` | `figure new`, `figure grid` | The grid of panels. | At most 12 panels. |
| `K` | `figure panel`, `figure style` | The panel, or the panel's series, from 1. | |
| `LO`, `HI` | `figure xlim`, `figure ylim` | The axis' ends. | Two different numbers; `HI` below `LO` turns the axis round. |
| `DPI` | `figure export FILE.png DPI` | Dots per inch of a PNG. | 300 to 600 for print. |

## Results

- **The Figure window** shows the page as it will be written.
- **The files** written by `figure export`, and the command window's
  "Saved the figure: FILE".
- **The session's Python script** holds the `xs.Figure` calls that build
  the same figure.

## More about

**Vector and pixels.** PDF, SVG and EPS keep lines sharp at any size and
need no fonts, since the text is drawn as outlines; they go into LaTeX,
Word, Inkscape or Illustrator as they are. A PNG records its resolution,
so 86 mm at 600 dpi is placed 86 mm wide.

**Annotations.** Text, arrows and markers go on the selected panel with
`annotate figure ...`; see [annotate](annotate.md).

## Tips

- Make the figure with the mouse in the Figure window, then save the
  session's script: the figure can be remade from Python, with other data.
- `figure from auto` is a quick start for a bifurcation diagram: each
  kind of branch becomes a series with its legend entry.

## Algorithms

The app draws the page with its own code, using the fonts of
matplotlib (DejaVu Sans, or STIX for serif) and its mathtext rules for
the labels; `xs.Figure` draws the same page with matplotlib itself.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: figure new W H [ROWS COLS]: the page in millimetres, more than 5 each, and at most 12 panels` | A size that is not a number or is too small, or too many panels. | Give the page in millimetres. |
| `error: figure panel K, from 1 to N` | No such panel. | Count from 1, row by row. |
| `error: figure X Y: two columns` (with `run` or `many`) | A name that is not a column. | `whos` lists them. |
| `error: figure xlim LO HI, two different numbers, or figure xlim auto` | A word, or the same number twice. | Two numbers. |
| `error: figure style: the panel has no series yet` | `style` before any series. | Add one first. |
| `error: figure style: color is #rrggbb (six hex digits), not 'red'` | A colour by name. | `#d62728` and the like. |
| `error: figure style takes color, line, width, marker, scale and label, not 'WORD'` | A misspelt option. | Nothing is changed; correct it. |
| `error: figure diagram takes x, min, max, stable, unstable, steady, orbits, branch, where and between, not 'WORD'` | A misspelt option. | Correct it. |
| `error: figure export FILE.pdf\|svg\|eps\|png [DPI], DPI a whole number from 50 to 2400, as in the Figure window` | No file, or a wrong `DPI`. | |
| `error: give the file a .pdf, .svg, .eps or .png name` | Another extension. | |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [annotate](annotate.md): text, arrows and markers on a panel.
- [export](export.md): one plot window as SVG or PNG, or the data.
- [kinescope capture](kinescope-capture.md): pictures of the window as a
  film.

## Version history

Introduced in 0.1.0. Since 0.1.4 a wrong number or word is an error and
changes nothing; before, numbers that were not numbers counted as 0 and
unknown words were skipped.
