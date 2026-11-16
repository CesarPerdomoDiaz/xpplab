# locus

The root locus: how the eigenvalues move as a feedback gain, a single
feedback, or a parameter changes.

## Syntax

```
locus gain LO HI N
locus feedback INPUT STATE LO HI N
locus par PARAMETER LO HI N
```

- **In the app:** Run > **Linearise...**, the root locus part.
- **In Python:** `lin.root_locus(K, gain=(0, 1), steps=301)`, or
  `m.root_locus("c", (-1, 1), steps=21)`.

## Description

- `locus gain LO HI N` scales the last gain designed by [poles](poles.md)
  or [lqr](lqr.md) by g from `LO` to `HI` in `N` steps, and follows the
  eigenvalues of A − gBK. Right after [linearize](linearize.md) the gain
  is the one that reflects each unstable pole into the left half-plane.
- `locus feedback INPUT STATE LO HI N` feeds one state back through one
  input with a gain from `LO` to `HI`.
- `locus par PARAMETER LO HI N` follows the equilibrium as a parameter
  changes, and its eigenvalues.

The command window reports where a pole crosses the edge of stability
(the imaginary axis; the unit circle for a map). `export FILE.csv locus`
writes the whole locus.

## Examples

### How much of the gain is needed

With the pendulum held at the top by the gain K = [3, 2.9] of
[poles](poles.md), scale it from 0 to 1:

1. [Open the pendulum](example:classics/pendulum.ode),
   [start at the top](cmd:set th=3.14159265 w=0),
   [linearise](cmd:linearize at ic u) and [place the poles](cmd:poles -1 -2).
2. [Follow the poles over the gain](cmd:locus gain 0 1 301).

```
set th=3.14159265 w=0
linearize at ic u
poles -1 -2
locus gain 0 1 301
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("classics/pendulum"))
lin = m.linearize(at={"th": np.pi, "w": 0}, inputs=["u"])
K = lin.place_poles([-1, -2])
rl = lin.root_locus(K, gain=(0, 1), steps=301)
print(rl.crossings())
```

The command window says "gain = 0.333333: a pole becomes stable, at 0".

How to read it. With gK the constant term of the characteristic
polynomial is 3g − 1, which turns positive at g = 1/3: below a third of
the gain the pendulum still falls, above it it is held. The margin
between 1/3 and 1 tells how much the gain may drop before control is lost.

### Friction as a parameter

The hanging pendulum, θ = 0, has eigenvalues solving λ² + cλ + 1 = 0.
Follow them over the friction `c`:

```
set th=0 w=0
locus par c -1 1 21
```

```python
m.set(th=0, w=0)
loc = m.root_locus("c", (-1, 1), steps=21)
print(loc.crossings())
```

The command window says "c = 0: a pair of poles becomes stable, at
0 +/- 1i".

How to read it. With negative friction the swings grow; with positive
they decay. At c = 0 the pair crosses the imaginary axis at ±i: the
frictionless pendulum, swinging for ever at angular frequency 1.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `LO HI` | the command; `gain=` or the range in Python | The range of the gain or the parameter. | Wide first, then narrow around a crossing. |
| `N` | the command; `steps=` in Python | The number of steps. | Enough to place a crossing: it is interpolated between steps. |

## Results

- **The Analysis window** draws the poles' paths in the complex plane,
  each from an x at the first value to an o at the last, in the *Root
  locus* section under the eigenvalues after *Linearise*.
- **The command window** lists the crossings of the edge of stability.
- **In Python** a `RootLocus`: `values`, `poles`, `crossings()`, `plot()`,
  `to_csv()`.

## More about

**Branches and crossings.** From one step to the next each eigenvalue is
matched with the nearest one, so a branch follows one eigenvalue. A
crossing is found between the two steps around it. Over a parameter, a
complex pair crossing the imaginary axis is a Hopf bifurcation, and a real
eigenvalue crossing zero a fold or a branch point.

**Gain margin.** The smallest scale of the gain that keeps the loop
stable, here 1/3, is a measure of robustness: a real controller whose
gain falls by more than that factor loses the pendulum.

## Tips

- Use enough steps: a crossing is interpolated between two steps, so a
  coarse locus places it only roughly.

## Algorithms

The eigenvalues of A − gBK, or of the Jacobian at the equilibrium
followed by Newton's method from one step to the next, with XPPAUT's
eigenvalue routine; crossings interpolated linearly between steps.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: linearise with inputs first, e.g. linearize inputs u` | No linearisation with inputs. | Run [linearize](linearize.md) with the inputs. |
| `error: no gain K yet: place the poles first (poles ...)` | No gain to scale. | Run [poles](poles.md) or [lqr](lqr.md). |
| `no pole crosses the edge of stability` | Nothing changes stability in the range. | Widen the range. |

## References

- Åström, K. J. and Murray, R. M. (2021). *Feedback Systems*, 2nd
  edition. Princeton University Press. Chapter 12.

## See also

- [poles](poles.md), [lqr](lqr.md): the gain that is scaled.
- [auto steady](auto-steady.md): following an equilibrium with AUTO.

## Version history

Introduced in 0.1.0.
