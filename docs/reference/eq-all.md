# eq all

Find every equilibrium in a region, from many starting points.

## Syntax

```
eq all
eq all GUESSES
```

- **In the app:** Phase Plane > **All equilibria**.
- **In Python:** `found = m.find_equilibria()`, or with a box:
  `m.find_equilibria(box={"V": (-0.6, 1.2), "W": (-0.25, 1.2)})`.

## Description

[eq](eq.md) finds one equilibrium, the one Newton's method reaches from the
initial values. `eq all` starts Newton's method from `GUESSES` random
points (100 by default) spread over a box, and lists every different
equilibrium it reaches, with its kind. It is XPPAUT's *Sing pts > Monte
Carlo*.

In the app the box is what the Phase Plane shows for its two variables.
For any other variable it is the range the last run covered, widened by a
quarter on each side, or −10 to 10 when there is no run. In Python the box
is −10 to 10 for every variable unless `box` says otherwise.

Two equilibria closer than 0.001 (the sum of the differences of their
values) count as one.

## Examples

### All three equilibria of the neuron

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Find every equilibrium](cmd:eq all).

```
eq all
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
found = m.find_equilibria(box={"V": (-0.6, 1.2), "W": (-0.25, 1.2)})
for e in found:
    print(e.kind, round(e["V"], 4), round(e["W"], 4))
```

The command window answers:

```
Found 3 equilibria from 100 random guesses in the box:
  unstable focus: V=0.0276103 W=0.26924
  stable node: V=-0.382505 W=0.0012856
  saddle: V=-0.142535 W=0.034049
```

How to read it. These are the three that [eq](eq.md) found one at a time
from three starts. Here they come in the order Newton's method reached
them, which carries no meaning. With **Nullclines** on, each is a crossing
of the two nullclines.

### Fewer starting points

```
set iapp=0.3
use hopf
eq all 50
```

```python
m.apply_set("hopf")
m.set(iapp=0.3)
one = m.find_equilibria(guesses=50, box={"V": (-0.6, 1.2), "W": (-0.25, 1.2)})
```

With the set `hopf` and `iapp` = 0.3 the model has a single equilibrium,
an unstable focus at `V` = −0.152093, and fifty starts find it. Fewer
starts are quicker; more make it less likely that an equilibrium with a
small basin of Newton's method is missed.

## Settings

| Setting | Name | Meaning | Advice |
|---|---|---|---|
| **Guesses** | the `GUESSES` argument; `guesses=` in Python | How many random starting points. | 100 suits a model with a few variables. Raise it for many variables or a large box. |
| **The box** | the Phase Plane's window; `box=` in Python | Where the starting points are drawn. | Make it cover the region you care about, and not much more. |
| The settings of [eq](eq.md) | `newt_tol`, `newt_iter`, `jac_eps` | Newton's method from each start. | As for `eq`. |

## Results

- **The command window** lists each equilibrium with its kind.
- **The Phase Plane** marks them.
- **In Python** a list of `Equilibrium`, each with its values, `kind`,
  `stable` and `eigenvalues`.

## More about

**Why random starts.** Newton's method from one point reaches one
equilibrium, and which one depends on the start in a complicated way. With
many starts spread over a region, every equilibrium whose basin of
attraction (for Newton's method) covers a fair part of the region is
reached at least once. Nothing guarantees that all are found: an
equilibrium with a tiny basin can be missed, and one outside the box
usually is.

**The random numbers repeat.** XPPAUT's random numbers start from the same
seed each time, so the same command finds the same list in the same order.

## Tips

- Draw the nullclines first. Their crossings show how many equilibria to
  expect, and where to put the box.
- If an equilibrium you expect is missing, zoom the Phase Plane onto it and
  run `eq all` again, or use [eq](eq.md) from a start near it.
- For a model with many variables, Python's `box=` limits each one; the
  default of −10 to 10 is often far too wide.

## Algorithms

XPPAUT's Monte Carlo search: the starting points are drawn uniformly in the
box with XPPAUT's random numbers, Newton's method runs from each with the
settings of [eq](eq.md), and an equilibrium is kept when it differs from
every one found before by more than the tolerance.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `Found 0 equilibria` | No start converged inside the box. | Widen the box, raise `GUESSES`, or check with the nullclines that there is an equilibrium at all. |
| `No model loaded.` | There is no model. | Open one first. |

## References

- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapter 6.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [eq](eq.md): one equilibrium, with its manifolds.
- [auto steady](auto-steady.md): how the equilibria move with a parameter.

## Version history

Introduced in 0.1.0.
