# Figures for papers

The **Figure** window (**View > Figure**, or **Export... > Add to the
Figure window** in any plot) makes a figure ready for a report, a thesis or
a journal: a page of the size you need, one or more panels, labels with
mathematics, your own line styles, and files in PDF, SVG, EPS or PNG. What
the preview shows is what the files hold, point for point.

Every change is also written to the session as Python (the
[echo](../python/app.md)), with the `xpplab.figure` calls that make the
same figure, so it can be made again or changed in a script
([Python figures](../python/figures.md)).

## Start from a plot

The quickest start is a plot you already have. In the Time Series, Phase
Plane or Bifurcation window, open **Export...** and choose **Add to the
Figure window**. The plot becomes a panel: the time series' variables, the
phase plane's trajectory with the many runs and equilibria on it, or the
bifurcation diagram with its stable and unstable branches in XPPAUT's
colours (orbits as their maximum and minimum).

## The page

- **Size**: a journal column (86 x 65 mm), 1.5 columns, two columns
  (178 mm wide), a slide, or any width and height in millimetres. Fonts and
  lines keep their size in points whatever the page, as they will be
  printed.
- **Font**: sans (DejaVu Sans) or serif (STIX, close to the Times of most
  LaTeX papers), and its size in points. Tick labels and the legend are
  0.85 of it, titles 1.1.
- **Rows** and **columns** of panels, and **Inset**: a small panel inside
  the selected one, for a close-up (its position and size are fractions of
  the panel's axes).

## A panel

Choose the panel in the list (or click it in the preview), then:

- **title**, **x label**, **y label**. Text between dollar signs is
  mathematics, as in matplotlib and LaTeX: `$V_W$ [m/s]`,
  `Elevation $\Gamma$ [deg]`, `$\tilde{V}_{W,H}$`, `$T/(mg)$`,
  `$\lambda_+ > 0$`, `$1.109724/\sqrt{\Lambda}$`. Greek letters,
  subscripts and superscripts, `\frac`, `\sqrt`, accents (`\tilde`,
  `\hat`, `\bar`, `\dot`, `\vec`), symbols (`\times`, `\pm`, `\leq`,
  `\approx`, `\infty`, ...) and `\mathrm{...}` work; a mistake is shown in
  red under the field.
- **x** and **y** limits: from the data (widened by 5%), or typed. Drag a
  rectangle on the preview to zoom into a region, and double-click to go
  back to all of the data. **log** makes the axis logarithmic, **step**
  fixes the spacing of the ticks.
- **grid**, **equal scales** (a unit as long on both axes, for a complex
  plane or a map), and the **legend**'s place: the best corner, a corner of
  your choice, outside on the right, or none.

## Series

The panel's series are listed with their colours. Select one to change its
**label** (in the legend; empty leaves it out), **colour**, **line**
(solid, dashed, dotted, dash-dot, none), **width** in points, **marker**
(circle, square, triangle, diamond, x, plus, star, point) with its size,
whether it is filled and how often it is drawn, and its **x scale** and
**y scale**: the numbers are shown multiplied by them. **y rad->deg** shows
an angle computed in radians in degrees; a factor such as 1/31.32 turns a
wind speed into the dimensionless one of a model.

**Add to this panel** brings in:

- **Columns of the run**: any two columns of the last run, `aux` quantities
  included.
- **Many runs / shooting**: the runs of Workspace > Many runs (see
  [Many runs](tools.md#many-runs-and-shooting)), as one series or one per
  way the runs ended.
- **Bifurcation diagram**: along the diagram, any of a variable or an `aux`
  quantity (the value at each steady state; the maximum or minimum over
  each orbit), the period, the norm, the eigenvalues `re1`, `im1`, ... of
  the steady states (sorted by real part, largest first), or the Floquet
  multipliers `mu1_abs`, `mu1_re`, `mu1_im`, ... of the orbits (by size).
  The horizontal axis is the parameter, or another of these quantities
  (`mu1_re` against `mu1_im` gives the multipliers in the complex plane).
  **XPPAUT's styles** adds one series per kind of branch; otherwise choose
  stable or not, steady states or orbits, a branch, or the points between
  two labels (the upper branch of a fold, say). An `aux` quantity over the
  orbits needs their whole solution, which AUTO keeps at labelled points
  only: set **NPR** to 1 before following the orbits to have them all.
- **Range (max/min)**: the result of a Range that recorded a variable's
  maximum and minimum, a brute-force diagram to lay over AUTO's.
- **Formula y = f(x)**: a curve such as `1.109724/sqrt(x)` over a range of
  x, to compare with a law.
- **Model function of a variable**: a right-hand side (`V'` is dV/dt) or an
  output column as one variable sweeps a range and the others keep their
  initial values: the residual whose zeros are the equilibria, for example.
- **Equilibria**: markers at the equilibria found in the phase plane.

## Export

**PDF...**, **SVG...** and **EPS...** write vector figures: lines stay sharp
at any size, and the text is drawn as outlines, so the files need no fonts
and look the same everywhere (LaTeX's `\includegraphics`, Word, Inkscape,
Illustrator). **PNG...** writes an image at the **dpi** you choose, with
that resolution recorded in the file, so that it is placed at its size:
86 mm at 600 dpi is 2031 pixels wide.

## Commands

The same from the command window or an `xpplab-batch` script:

```
figure new 86 65 1 2           a page of 86 x 65 mm, one row of two panels
figure panel 2                 work on panel 2
figure run T V                 two columns of the run
figure many V W split          the many runs, a series per ending
figure diagram V stable steady   (also: unstable, orbits, min, x X,
                                 branch B, between L1 L2, where COL min LO HI)
figure function 1/sqrt(x) 0.1 10
figure curve V' V -1 1
figure xlabel Wind velocity $V_W$ [m/s]
figure xlim 2.1 2.3            (ylim, or xlim auto)
figure style 1 color #d62728 line -- width 1 marker o scale 1 57.2958 label Stable
figure export figure.pdf       (or .svg, .eps, .png 600)
```
