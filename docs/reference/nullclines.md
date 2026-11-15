# nullclines

Draw the nullclines in the Phase Plane: the curves where one variable stands
still.

## Syntax

```
nullclines
nullclines off
```

- **In the app:** Phase Plane > tick **Nullclines**, the **Nullclines**
  button in the toolbar, or XPPAUT's keys `N N`.
- **In Python:** `xn, yn = m.nullclines("V", "W", xlim, ylim)` for the
  curves, or `m.phase_plane("V", "W")` to draw them with the rest of the
  phase plane.

## Description

`nullclines` draws, in the Phase Plane, the two curves of the plotted
variables: on the first, the first variable's derivative is zero; on the
second, the second variable's is. They are drawn in red and green and
redrawn whenever the window moves or a parameter changes. `nullclines off` hides them.

The other variables, if the model has more than two, are held at their
initial values while the curves are computed.

Nullclines are the quickest way to understand a model in the plane:

- every crossing of the two curves is an equilibrium;
- on each side of a nullcline its variable increases on one side and
  decreases on the other, so the curves divide the plane into regions where
  the flow points in one of four directions;
- as a parameter changes the curves move, and equilibria appear or
  disappear where they stop crossing.

## Examples

### Three crossings, three equilibria

1. [Open the Morris-Lecar model](example:lecar.ode).
2. Open the **Phase Plane** tab and
   [draw the nullclines](cmd:nullclines).

```
nullclines
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
vn, wn = m.nullclines("V", "W", (-0.6, 1.2), (-0.25, 1.2))
```

The `V` nullcline (red) is an N-shaped curve, and the `W` nullcline (green)
an S-shaped one that rises across it. They cross three times, at the three
equilibria that [eq all](eq-all.md) lists: the stable node at
`V` = −0.3825, the saddle at −0.1425 and the unstable focus at 0.0276.

How to read it. Each crossing lies on both curves, so both derivatives
vanish there. Between the curves the flow has a direction you can read off:
above the `V` nullcline `V` decreases, below it `V` increases. A spike is a
trajectory that is carried round the right-hand branch of the N.

### A parameter that removes two equilibria

Raise the current and draw them again:

```
set iapp=0.1
nullclines
```

```python
m.set(iapp=0.1)
vn, wn = m.nullclines("V", "W", (-0.6, 1.2), (-0.25, 1.2))
```

The `V` nullcline moves up, the lower bend of the N lifts off the green
curve, and only one crossing is left: the unstable focus, now at
`V` = 0.0443. The rest state and the threshold have met and vanished.

How to read it. Where two crossings merge and disappear is a fold of
equilibria. [auto steady](auto-steady.md) finds it at `iapp` = 0.0833, and
with `iapp` = 0.08 the three crossings are still there. With no stable
equilibrium left, the neuron fires repetitively.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| **The plotted variables** | Phase Plane, x and y | Which two variables the nullclines are drawn for. | Any two ODE variables. |
| **The window** | the Phase Plane's view; `xlim`, `ylim` in Python | The region the curves are computed in. | Zoom to the region of interest: the curves are computed on a grid over what is shown. |
| **Grid** | `grid` in Python (the app uses 100) | The number of cells on each side of the grid the curves are traced on. | Raise it if a curve has gaps or misses a sharp bend. |

## Results

- **The Phase Plane** draws the first variable's nullcline in red and the
  second's in green, and lists them in its legend.
- **In Python** `m.nullclines()` returns two arrays of line segments, one
  for each nullcline, with columns x1, y1, x2, y2, for plotting or for
  finding crossings.
- `export FILE.csv nullclines` writes the nullclines shown.

## More about

**Definition.** For a model x' = f(x, y), y' = g(x, y), the x-nullcline is
the set where f(x, y) = 0 and the y-nullcline the set where g(x, y) = 0. A
trajectory crosses the x-nullcline vertically (x stands still there) and
the y-nullcline horizontally.

**Equilibria.** A point on both nullclines has f = g = 0: it is an
equilibrium. So the number of crossings in a region is the number of
equilibria in it.

**Folds.** When a parameter moves a nullcline until it only touches the
other, two equilibria meet; moving it further, they disappear. That is a
saddle-node bifurcation, a fold, and it is the commonest way equilibria
are created and destroyed.

## Tips

- Draw the nullclines before looking for equilibria: they show how many
  there are and where to start [eq](eq.md).
- Drag a parameter's slider with the nullclines on and watch the crossings:
  it shows a bifurcation before you compute it.
- `nullclines freeze` keeps the present curves on the plot, so that the
  ones for another parameter value can be drawn beside them.
- For a model with more than two variables the nullclines are a slice at
  the other variables' initial values, and the crossings are not
  equilibria of the whole model unless those values are too.

## Algorithms

XPPAUT's own nullcline routine: the right-hand side is evaluated on a grid
over the window, and in each cell the zero curve of each component is
traced by linear interpolation along the cell's edges. The result is a set
of short line segments.

## When it fails

| Symptom | Why | What to do |
|---|---|---|
| A nullcline has gaps, or a sharp bend is cut off | The grid is coarse for the curve. | Zoom in, or raise `grid` in Python. |
| No nullcline appears | It lies outside the window, or the model has fewer than two variables. | Zoom out, or choose the plotted variables. |
| The crossings do not match `eq` | The model has more than two variables, and the others are not at an equilibrium. | Use [eq](eq.md) for the whole model. |

## References

- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapters 5 to 7.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [eq](eq.md) and [eq all](eq-all.md): the crossings, computed.
- [auto steady](auto-steady.md): following them as a parameter changes.
- [Tutorial: why does a neuron fire?](../manual/neuron-tutorial.md).

## Version history

Introduced in 0.1.0.
