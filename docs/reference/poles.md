# poles

The feedback gain K that gives the closed loop A − BK chosen eigenvalues
(pole placement).

## Syntax

```
poles P1 P2 ...
```

One pole per state; complex poles in conjugate pairs, written `-1+2i`
and `-1-2i`.

- **In the app:** Run > **Linearise...**, the pole placement part.
- **In Python:** `K = lin.place_poles([-1, -2])`.

## Description

After [linearize](linearize.md) with inputs, `poles` computes the gain K
for the feedback u = u₀ − K (x − x*) such that the eigenvalues of A − BK,
the poles of the closed loop, are the ones given. It needs a controllable
linearisation.

## Examples

### Hold the pendulum up

With the pendulum linearised at the top (see [linearize](linearize.md)),
place the poles at −1 and −2:

1. [Open the pendulum](example:classics/pendulum.ode),
   [start at the top](cmd:set th=3.14159265 w=0) and
   [linearise with u as input](cmd:linearize at ic u).
2. [Place the poles](cmd:poles -1 -2).

```
set th=3.14159265 w=0
linearize at ic u
poles -1 -2
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("classics/pendulum"))
lin = m.linearize(at={"th": np.pi, "w": 0}, inputs=["u"])
K = lin.place_poles([-1, -2])
print(K)
```

The command window answers `K[u]: TH 3 W 2.9`.

How to read it. The closed loop's characteristic polynomial is
λ² + (0.1 + k₂)λ + (k₁ − 1); for poles −1 and −2 it must be λ² + 3λ + 2,
so k₁ = 3 and k₂ = 2.9. The torque pushes back 3 units per radian of
lean and 2.9 per unit of angular velocity. [closedloop](closedloop.md)
writes this controller into the model to test it.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `P1 P2 ...` | the command; the list in Python | The wanted poles, one per state. | Further left is faster, but needs a larger gain. |

## Results

- **The command window** gives K, one row per input, one entry per state.
- **In Python** `lin.place_poles()` returns K as an array.

## More about

**Pole placement.** For a controllable single-input system any set of
poles can be reached, by exactly one K. Fast poles need large gains, and
large gains can saturate a real actuator or excite what the linear model
leaves out; [lqr](lqr.md) balances speed against effort instead.

## Tips

- Check the design on the nonlinear model with [closedloop](closedloop.md).
- See how the poles move as the gain is scaled with [locus](locus.md).

## Algorithms

Ackermann's formula for one input, and a robust placement for several,
on the A and B of the linearisation.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: linearise with inputs first, e.g. linearize inputs u` | No linearisation with inputs. | Run [linearize](linearize.md) with the inputs. |
| `cannot place poles: the pair (A, B) is not controllable ...` (Python) | The inputs cannot reach every state. | Choose other inputs in [linearize](linearize.md). |
| An error about the poles | Not one per state, not in conjugate pairs, or a pole repeated more often than there are inputs. | Give admissible poles. |

## References

- Åström, K. J. and Murray, R. M. (2021). *Feedback Systems*, 2nd
  edition. Princeton University Press. Chapter 7.

## See also

- [linearize](linearize.md), [lqr](lqr.md), [locus](locus.md),
  [closedloop](closedloop.md).

## Version history

Introduced in 0.1.0.
