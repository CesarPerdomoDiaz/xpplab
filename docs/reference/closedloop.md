# closedloop

Write the model with the designed feedback built in, and open it.

## Syntax

```
closedloop
closedloop FILE.ode
```

- **In the app:** Run > **Linearise...**, Closed loop.
- **In Python:** `held = lin.closed_loop(K, "held.ode")`.

## Description

`closedloop` takes the last gain K designed by [poles](poles.md) or
[lqr](lqr.md) (right after [linearize](linearize.md), the gain that
reflects each unstable pole into the left half-plane) and writes a new
model in which each input parameter is
replaced by the feedback law

u = u₀ − K (x − x*),

with u₀ the input's nominal value and x* the point of the linearisation.
The rest of the model file is kept as it is. The new model is saved next
to the old one (or as `FILE.ode`) and opened, so it can be run like any
other: the test of a linear design on the nonlinear model.

## Examples

### Does the gain hold the real pendulum?

1. [Open the pendulum](example:classics/pendulum.ode),
   [start at the top](cmd:set th=3.14159265 w=0),
   [linearise](cmd:linearize at ic u) and [place the poles](cmd:poles -1 -2).
2. Write and open the closed loop with `closedloop held.ode`, then start
   half a radian off the top and run.

```
set th=3.14159265 w=0
linearize at ic u
poles -1 -2
closedloop held.ode
set th=3.64159265 total=20
run
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("classics/pendulum"))
lin = m.linearize(at={"th": np.pi, "w": 0}, inputs=["u"])
K = lin.place_poles([-1, -2])
held = lin.closed_loop(K, "held.ode")
sol = held.run(th=np.pi + 0.5, w=0, total=20)
free = m.run(th=np.pi + 0.01, w=0, total=20)
print(sol["TH"][-1], free["TH"][-1])
```

The command window says "Wrote the closed loop to held.ode", and `whos`
shows that `u` is no longer a parameter: only `c` is. From θ = π + 0.5 the
held pendulum returns to π, to 1e-5, within 20 time units. Without the
feedback, a start only 0.01 off the top falls: after 20 time units θ is
5.24.

How to read it. The gain was designed for small deviations, yet it brings
the nonlinear pendulum back from half a radian. Starting further off shows
where the linear design stops working.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `FILE.ode` | the command; `path` in Python | Where to write the closed loop. | Next to the model by default. |

## Results

- **A new model file**, opened in the app; in Python the new `Model`
  (its file in `.path`).

## More about

**What the file holds.** The input parameter becomes a fixed quantity
defined by the feedback law, written at the top of the model, so every
equation that used the input now sees the feedback. A long law is split
into partial sums to fit XPPAUT's line length.

## Tips

- Try starts further from the point to find how far the design holds.
- Compare designs by writing each closed loop to its own file.

## Algorithms

The model's text is copied with the inputs' parameter entries replaced by
fixed definitions of the feedback law; nothing is computed until it runs.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: linearise with inputs and place the poles first` | No linearisation with inputs. | Run [linearize](linearize.md) with the inputs, then design a gain. |
| `error: no gain yet: ...` | The gain could not be designed. | Check the message; run [poles](poles.md) or [lqr](lqr.md). |
| `error: cannot write FILE` | The folder cannot be written. | Give another path. |

## References

- Åström, K. J. and Murray, R. M. (2021). *Feedback Systems*, 2nd
  edition. Princeton University Press. Chapter 7.

## See also

- [linearize](linearize.md), [poles](poles.md), [lqr](lqr.md).

## Version history

Introduced in 0.1.0.
