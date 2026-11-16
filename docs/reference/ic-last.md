# ic last

Start the next run where the last one ended.

## Syntax

```
ic last
```

- **In the app:** Workspace > **Initial values** > **From end of last
  run**, or XPPAUT's keys `I L`.
- **In Python:** `m.ic_last()`.

## Description

`ic last` makes the final state of the last run the model's initial
values. The next [run](run.md) then starts where the last one stopped,
from `t` = `T0` as before.

Use it to drop a transient: run until the model has settled, then `ic last`
and run again, and the new run starts on the attractor. Repeated, it is a
quick way to see whether a model settles at all.

The time is not carried on. For a model whose equations contain `t`, such as
a periodic forcing, the new run sees the forcing start again from its value
at `T0`; use [ic shift](ic-shift.md) to let the time go on.

## Examples

### Starting on the oscillation

The Morris-Lecar model with `iapp` = 0.1 fires repetitively. Run it, take
the end as the new start, and run again:

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Set the current](cmd:set iapp=0.1) and [run](cmd:run).
3. [Start from the end](cmd:ic last) and [run again](cmd:run).

```
set iapp=0.1
run
ic last
run
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.set(iapp=0.1)
first = m.run()
m.ic_last()
second = m.run()
```

The first run starts at `V` = −0.144, the model's initial value, and ends
at `V` = −0.2384, `W` = 0.00706. Those become the initial values: `get v`
now answers −0.2384222746. The second run starts there, at `t` = 0.

How to read it. The first run spikes at `t` = 2.05 and 18.5, the second at
4.95 and 21.45: about 16.5 apart each time, the period of the oscillation.
The first run's first spike reaches only `V` = 0.2553, against 0.2615 for
the spikes after it, because that run starts off the cycle. The second
run's first spike already reaches 0.2613: it starts close to the cycle,
with the transient gone.

## Settings

`ic last` has no settings. It takes every variable of the model from the
last stored point of the run.

## Results

- **The initial values** are the last run's final state. The
  **Initial values** tab shows them, and the command window says "Initial
  values set to the end of the last run."
- **The start time** `T0` is unchanged.
- **In Python** `m.ic_last()` returns the model, so that
  `m.ic_last().run()` works.

## More about

**Precision.** The app takes the final state from the run as it is stored,
in single precision, so `get v` shows −0.2384222746. Python takes it from
the integrator, in full precision: −0.23842227676. The difference is in the
ninth digit. A session saved as a Python script writes the app's values
out, so the script repeats the app's run exactly.

**`ic last` and `continue`.** [continue](continue.md) extends the run
itself and leaves the initial values alone. `ic last` changes the initial
values and leaves the next run to replace the old one.

## Tips

- To drop a long transient once and for all, run with a long `total`,
  `ic last`, and then set `total` back.
- For a forced model, use [ic shift](ic-shift.md): otherwise the new run
  restarts the forcing.
- `ic last` changes the initial values for good. To go back, reopen the
  model, or keep them first with `ic write FILE`.

## Algorithms

XPPAUT's *Initial conds > Last*: every variable's value at the last stored
point of the run becomes its initial value.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `There is no run yet.` | The model's first run failed, so there is no end to take. | Fix what stopped the run, then `run`. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [ic shift](ic-shift.md): the same, and the time goes on.
- [continue](continue.md): extend the run instead.
- [run](run.md): the run that starts from the new values.

## Version history

Introduced in 0.1.0.
