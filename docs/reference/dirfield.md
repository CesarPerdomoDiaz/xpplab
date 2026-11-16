# dirfield

Draw the direction field in the Phase Plane: an arrow at each point of a
grid, pointing where the flow goes.

## Syntax

```
dirfield
dirfield off
```

`dd` is another name for `dirfield`.

- **In the app:** Phase Plane > tick **Flow**, Run > **Direction field**,
  or XPPAUT's keys `D D`.
- **In Python:** `X, Y, U, V = m.vector_field("V", "W", xlim, ylim)` for
  the numbers, or `m.phase_plane("V", "W")` to draw them with the rest of
  the phase plane.

## Description

`dirfield` draws, on a grid of 21 by 21 points over the Phase Plane's
window, a short arrow in the direction the model moves from that point:
the direction of (x', y') for the two plotted variables. Every arrow has
the same length, so the field shows directions, not speeds. A dot marks
each arrow's head.

`dirfield` alone switches the field on or off; `dirfield off` hides it.
The field is redrawn whenever the window moves or a parameter changes.

The other variables, if the model has more than two, are held at their
initial values. The field is computed at `t` = 0, so for a model whose
equations contain `t` it is the field at that one time.

## Examples

### The flow of the neuron

1. [Open the Morris-Lecar model](example:lecar.ode).
2. Open the **Phase Plane** tab, [draw the direction field](cmd:dirfield)
   and [the nullclines](cmd:nullclines).

```
dirfield
nullclines
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
X, Y, U, V = m.vector_field("V", "W", (-0.6, 1.2), (-0.25, 1.2), n=21)
print(m.rhs({"V": 1.0, "W": 0.2}))
print(m.rhs({"V": -0.5, "W": 0.5}))
```

The arrows sweep round the plane anticlockwise. At `V` = 1, `W` = 0.2, to
the right of the `V` nullcline, the derivatives are (−1.38, 2.97): the
arrow points up and to the left. At `V` = −0.5, `W` = 0.5, on the left,
they are (−0.148, −0.669): down, and a little left.

How to read it. Where an arrow crosses the red `V` nullcline it is
vertical, and where it crosses the green `W` nullcline horizontal. Between
the curves the arrows keep one of four directions, so the field and the
nullclines together show a spike's path before any run: out to the right,
up, back left along the top, and down to rest.

### Directions, not speeds

At the stable node, `V` = −0.3825, `W` = 0.0013, both derivatives are 0.
On the grid the largest are 5.56 for `V`' and 9.24 for `W`', both at the
right-hand edge, `V` = 1.2:

```python
import numpy as np

print(m.rhs({"V": -0.3825, "W": 0.0013}))
print(np.abs(U).max(), np.abs(V).max())
```

How to read it. Arrows of equal length hide that difference: near an
equilibrium the flow is slow, far from it fast. To see the speed, run from
a few points with `click X Y`, and compare how far each run gets in the
same time.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| **The plotted variables** | Phase Plane, x and y | Which two variables the field is drawn for. | Any two ODE variables. |
| **The window** | the Phase Plane's view; `xlim`, `ylim` in Python | The region the grid covers. | Zoom in for detail: the grid follows the window. |
| **Grid** | `n` in Python (20 by default; 21 when drawn) | The number of points on each side. | The app always uses 21. |

## Results

- **The Phase Plane** draws the arrows in grey.
- **In Python** `m.vector_field()` returns four `n` × `n` arrays: the grid
  `X`, `Y` and the derivatives `U`, `V` there, ready for matplotlib's
  `quiver`. `m.phase_plane()` draws them as arrows of one length.

## More about

**The field and the trajectories.** A trajectory is tangent to the field at
every point it passes. Sketching curves that follow the arrows is the
classical way to see a phase portrait without integrating.

**Maps.** For a model that is a map (`meth=discrete`), the arrow at a
point goes towards its image: it shows the displacement x(n+1) − x(n).

**Forced models.** For x' = f(x, t) the field changes with time, and an
arrow drawn at `t` = 0 is not what a trajectory sees later. Trajectories of
a forced model can cross each other in the plane.

## Tips

- Draw the field with the nullclines: each explains the other.
- Zoom in on an equilibrium to see how the flow turns round it: spiralling
  for a focus, straight in or out for a node, in and out for a saddle.

## Algorithms

The right-hand side is evaluated with XPPAUT's own routine at each grid
point, with the other variables at their initial values and `t` = 0, and
each arrow is scaled to a fixed length on the screen.

## When it fails

| Symptom | Why | What to do |
|---|---|---|
| No arrows | The model has fewer than two variables, or the right-hand side cannot be evaluated over the window. | Choose two variables, or move the window. |
| The arrows do not match the runs | The model has more than two variables, or depends on `t`. | The field is a slice: see Description. |

## References

- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapter 6.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [nullclines](nullclines.md): the curves where the arrows are vertical or
  horizontal.
- [eq](eq.md): where the arrows vanish.

## Version history

Introduced in 0.1.0.
