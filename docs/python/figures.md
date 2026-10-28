# Figures for papers

`xs.Figure` makes a figure for a report or a journal in a few calls: a page
in millimetres, panels, curves from runs and diagrams, labels with
mathematics, and PDF, SVG, EPS or PNG files. It is the vocabulary of the
desktop app's [Figure window](../manual/figures.md): every change made
there is echoed as one of these calls, so a figure made with the mouse can
be made again in a script, and the other way round. It draws with
matplotlib, with the fonts the app uses (DejaVu Sans, or STIX for
`font="serif"`).

```python
import math
import xpplab as xs

m = xs.load("lecar.ode")
sol = m.run(total=200)

fig = xs.Figure(86, 65)                 # a journal column, millimetres
p = fig[0]
p.run(sol, "T", "V", label="$V$", color="k")
p.run(sol, "T", "W", label="$W$", line="--")
p.set(xlabel=r"time $t$ [ms]", ylabel="voltage", legend="upper right")
fig.save("lecar.pdf")                   # .pdf, .svg, .eps, or .png
fig.save("lecar.png", dpi=600)
```

## The page and its panels

```python
fig = xs.Figure(178, 70, rows=1, cols=3, font="serif", fontsize=8)
fig[0], fig[1], fig[2]                  # the panels, row by row
fig.grid(2, 2)                          # another grid; panels beyond it go
small = fig.inset(0, (0.55, 0.55, 0.4, 0.4))   # inside panel 0: x, y, w, h
fig.set(width=120, height=80)
```

`width` and `height` are in millimetres; text and lines keep their size in
points. `fontsize` is the labels'; ticks and legends are 0.85 of it.

## Panels

```python
p.set(title="", xlabel=r"$V_W$ [m/s]", ylabel=r"$\Gamma$ [deg]",
      xlim=(2.1, 2.3), ylim=None,       # None: from the data, plus 5%
      logx=False, logy=True, grid=True,
      xstep=0.05, ystep=None,           # tick spacing; None: chosen
      legend="lower right",             # best, upper right, upper left,
                                        # lower left, outside, none
      equal=False)                      # the same scale on both axes
```

Labels take mathtext, matplotlib's subset of TeX: use raw strings
(`r"..."`) so that the backslashes stay.

## Series

```python
s = p.line(x, y, label="data", color="#d62728", line="--", width=1.0,
           marker="o", markersize=4, markerfill=False, every=10,
           scale=(1, 180 / math.pi))    # shown as x*1, y*180/pi
s.set(color="k")
p[0].set(label="first")                 # a panel's series by number
p.remove(1)
```

NaN in the data breaks a line. Besides `line`, a panel adds what the
desktop app adds:

| Call | What it draws |
|------|---------------|
| `p.run(sol, x, y)` | two columns of a run |
| `p.runs(runs, x, y, event=1)` | the runs of a [`RunList`](simulation.md#many-initial-values-and-how-each-run-ended), all or those a flag stopped |
| `p.diagram(d, y, ...)` | a quantity along a bifurcation diagram (below) |
| `p.function("1.109724/sqrt(x)", (30, 10000))` | a formula in x |
| `p.curve(m, "om'", "gam", (-1.6, 1.6), at={"om": 0})` | a right-hand side or an output column as one variable sweeps a range |

## Diagrams

`p.diagram(d, y)` draws one value per point of a diagram `d`
(`m.auto.diagram`, or what a continuation returned):

- a variable: its maximum over each orbit (`which="min"` for the minimum),
  and its value at each steady state;
- an `aux` quantity, the same, with `model=m` to compute it
  ([`Auto.along`](auto.md#auxiliary-quantities-along-a-diagram));
- `"period"`, `"norm"`, a parameter;
- `"re1"`, `"im1"`, `"re2"`, ...: the eigenvalues of the steady states,
  sorted by real part, largest first;
- `"mu1_abs"`, `"mu1_re"`, `"mu1_im"`, ...: the Floquet multipliers of the
  orbits, sorted by size.

`x=` takes the same names (the parameter by default), so
`p.diagram(d, "mu1_im", x="mu1_re")` puts the multipliers in the complex
plane. The points can be chosen with `stable=True/False`,
`periodic=True/False`, `branch=`, `between=(label1, label2)` and
`where=(column, "min" or "max", low, high)`:

```python
inf = float("inf")
taut = ("tens", "min", 0, inf)        # orbits whose tension stays positive
p.diagram(d, "V", stable=True, periodic=False, color="#d62728")
p.diagram(d, "V", stable=False, periodic=False, color="k", line="--")
p.diagram(d, "period", periodic=True, where=taut, model=m)
```

Each kept piece starts at the point before it on its branch, so a stable
and an unstable piece meet. The function `xs.figure.diagram_xy` returns the
same arrays for plotting them some other way.

## Drawing

`fig.draw()` returns the matplotlib figure, to change anything else before
saving it; in a Jupyter notebook a `Figure` shows itself.
