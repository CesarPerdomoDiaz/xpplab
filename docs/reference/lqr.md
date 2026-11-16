# lqr

The optimal feedback gain for weights on the states and the inputs (the
linear-quadratic regulator).

## Syntax

```
lqr
lqr Q q1 q2 ... R r1 ...
```

`Q` gives the weights on the states and `R` those on the inputs, as their
diagonals; one number stands for all alike.

- **In the app:** Run > **Linearise...**, the LQR part.
- **In Python:** `res = lin.lqr(Q=[10, 1], R=1)`; `res.K`, `res.poles`.

## Description

After [linearize](linearize.md) with inputs, `lqr` finds the gain K of
the feedback u = u₀ − K (x − x*) that minimises

∫ (δxᵀ Q δx + δuᵀ R δu) dt,

the deviation of the states weighed against the effort of the inputs,
and reports the closed loop's poles. `linearize` sets every weight to 1;
weights not given keep their last values.

## Examples

### Two ways to hold the pendulum up

With the pendulum linearised at the top (see [linearize](linearize.md)):

1. [Open the pendulum](example:classics/pendulum.ode),
   [start at the top](cmd:set th=3.14159265 w=0) and
   [linearise with u as input](cmd:linearize at ic u).
2. [Equal weights](cmd:lqr), then
   [ten times more weight on the angle](cmd:lqr Q 10 1 R 1).

```
set th=3.14159265 w=0
linearize at ic u
lqr
lqr Q 10 1 R 1
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("classics/pendulum"))
lin = m.linearize(at={"th": np.pi, "w": 0}, inputs=["u"])
equal = lin.lqr()
angle = lin.lqr(Q=[10, 1], R=1)
print(equal.K, equal.poles, angle.K, angle.poles)
```

| Weights | K (θ, ω) | Closed-loop poles |
|---|---|---|
| Q = 1, 1; R = 1 | 2.41421, 2.31628 | −0.99509, −1.4212 |
| Q = 10, 1; R = 1 | 4.31662, 3.00536 | −1.5527 ± 0.95174i |

How to read it. With equal weights the gain on the angle is 1 + √2 =
2.41421, the textbook value for this system. Weighing the angle ten times
more buys a faster return (poles further left), at the price of a larger
gain, and the poles become a complex pair: the pendulum overshoots a
little on its way back.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `Q q1 ...` | the command; `Q=` in Python | The weights on the states (the diagonal of Q). | Larger for the states that must stay close. |
| `R r1 ...` | the command; `R=` in Python | The weights on the inputs. | Larger for an input that is costly or limited. |

## Results

- **The command window** gives K and the closed-loop poles.
- **The Analysis window**, after *Linearise* with inputs, has an *Optimal
  control (LQR)* section under the gain: one weight per state (Q) and one
  per input (R), all 1 to start with. *Compute the LQR gain* puts the gain
  in *Gain K* and the closed loop's poles in the pole list, so that
  *Simulate the closed loop* and the root locus over the gain use it.
  Moving a pole afterwards places the poles instead.
- **In Python** an `LQR` result: `K`, `poles`, and the Riccati solution.

## More about

**Why LQR.** Pole placement asks where the poles should be; LQR asks what
matters, deviations or effort, and finds the best compromise. The closed
loop it gives is always stable for a controllable system with positive
weights.

## Tips

- Start with equal weights, then raise the weight of the state that
  should settle faster.
- Check the gain on the nonlinear model with [closedloop](closedloop.md).

## Algorithms

The continuous algebraic Riccati equation AᵀP + PA − PBR⁻¹BᵀP + Q = 0 is
solved for P by the matrix sign function (the doubling algorithm for a
map), and K = R⁻¹BᵀP.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: linearise with inputs first, e.g. linearize inputs u` | No linearisation with inputs. | Run [linearize](linearize.md) with the inputs. |
| `Write: lqr [Q q1 q2 ...] [R r1 ...], ...` | The weights were not understood. | One weight per state after `Q`, per input after `R`, or one for all. |
| An error that no gain stabilises the loop | An unstable mode the inputs cannot move, or one `Q` does not see. | Choose other inputs, or give that state a weight. |

## References

- Åström, K. J. and Murray, R. M. (2021). *Feedback Systems*, 2nd
  edition. Princeton University Press. Chapter 7.
- Anderson, B. D. O. and Moore, J. B. (1990). *Optimal Control: Linear
  Quadratic Methods*. Prentice Hall.

## See also

- [linearize](linearize.md), [poles](poles.md), [locus](locus.md),
  [closedloop](closedloop.md).

## Version history

Introduced in 0.1.0.
