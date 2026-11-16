# view3d

Show three columns of the last run as a curve in three dimensions, which
can be turned and spun.

## Syntax

```
view3d X Y Z
view3d spin
view3d spin SPEED
view3d spin off
view3d angles THETA PHI
```

- **In the app:** Run > **3D view**, or View > **3D View**; drag in the
  view to turn it.
- **In Python:** `sol.plot3d("X", "Y", "Z")`, a matplotlib 3-D plot.

## Description

`view3d X Y Z` opens the 3-D view with the columns `X`, `Y` and `Z` of the
last run as its three axes. Any column can be an axis: variables,
auxiliary quantities or `t`.

`view3d spin` turns the view continuously, 24 degrees a second, or `SPEED`
degrees a second; `view3d spin off` stops it. `view3d angles THETA PHI`
sets the angles of the view, in degrees. Without columns, `view3d` opens
the view with the columns it had.

## Examples

### The Lorenz attractor

1. [Open the Lorenz system](example:lorenz.ode).
2. [Show it in three dimensions](cmd:view3d X Y Z) and
   [let it spin](cmd:view3d spin).

```
view3d X Y Z
view3d spin
```

```python
import numpy as np
import xpplab as xs

lorenz = xs.load(xs.example("lorenz.ode"))
sol = lorenz.run()
sol.plot3d("X", "Y", "Z")
switches = np.count_nonzero(np.diff(np.sign(sol["X"])))
print(switches, np.mean(sol["X"] > 0))
```

The run, 1601 points to `t` = 40, draws the two-lobed butterfly, with `X`
between −16.6 and 17.6 and `Z` between 5.4 and 42.7. The curve circles one
lobe, then the other: `X` changes sign 20 times, the first time at
`t` = 3.78, and the run spends 34% of its time on the side `X` > 0.

How to read it. Each lobe winds round an equilibrium, C± = (±8.3267,
±8.3267, 26), both of them saddle-foci: the trajectory spirals outwards
round one until it is thrown across to the other. The order of the visits
looks random and depends sensitively on the start; that is the chaos the
positive Liapunov exponent measures ([mean](mean.md)). Turning the view
shows what no single projection does: the two lobes are nearly flat
sheets, joined where the curve crosses over.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `X`, `Y`, `Z` | the command; **x**, **y**, **z** in the view; the names in Python | The three columns. | Any columns of the run. |
| `SPEED` | `view3d spin SPEED`; **Spin** in the view | Degrees a second. | 24 by default. |
| `THETA`, `PHI` | `view3d angles`; **theta**, **phi** in the view | The view's angles, in degrees, as XPPAUT's: `THETA` turns the box about the vertical axis, and `PHI` tilts it, with 90 for a side view and 0 looking straight down. | Turn by dragging, and read them off for a figure you will remake. |

## Results

- **The 3-D view** draws the run as a curve in a box, with any kept
  curves, and its axes with their names. Drag to turn it, scroll to zoom,
  double-click to reset the zoom.
- **In Python** `sol.plot3d()` returns matplotlib's 3-D axes.

## More about

**Three dimensions on a screen.** A still 3-D picture is a projection, and
a curve can seem to cross itself where it only passes behind. Turning the
view, or letting it spin, separates what is in front from what is behind.

**A model's own view.** A model that asks for a 3-D plot, as
`@ xplot=x, yplot=y, zplot=z, axes=3d, phi=60` does (with `xmin=` ...
`zmax=` for the box), opens with its view, as in XPPAUT. **Reset** returns
to it.

**Equilibria of the Lorenz system.** With `r` = 27, `s` = 10 and
`b` = 8/3, the equilibria are the origin, a saddle, and C± = (±√(b(r−1)),
±√(b(r−1)), r − 1), as [eq all](eq-all.md) finds them.

## Tips

- Use the 3-D view for models with three or more variables, after the
  Time Series and the Phase Plane have shown the projections.
- `t` as one of the axes unrolls a run along time.

## Algorithms

The stored run is drawn; nothing is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: no column 'NAME'; type 'whos' for the names` | A column does not exist. | Check the names with `whos`. |

## References

- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapter 9 (the Lorenz equations).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [mean](mean.md): the Liapunov exponent of the run.
- [run](run.md): the run that is drawn.

## Version history

Introduced in 0.1.0.
