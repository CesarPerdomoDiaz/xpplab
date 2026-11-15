# ic shift

Start the next run where the last one ended, and let the time go on from
there.

## Syntax

```
ic shift
```

- **In the app:** Workspace > **Initial values** > **From end, time goes
  on**, or XPPAUT's keys `I S`.
- **In Python:** `m.ic_last(shift=True)`.

## Description

`ic shift` does what [ic last](ic-last.md) does, making the final state of
the last run the initial values, and also sets the start time `T0` to the
time the run ended. The next [run](run.md) then goes on from that time, as
if the integration had never stopped.

It matters for a model whose equations contain `t`, such as a periodic
forcing. For a model without `t` it gives the same run as `ic last`, only
with the times shifted.

## Examples

### A forced pendulum, run in two parts

The forced pendulum `x'' + 0.1 x' + sin x = cos t` depends on the time
through its forcing. Run it to `t` ≈ 10, then go on with each command:

1. [Open the forced pendulum](example:forcpend.ode).
2. [Set the run length](cmd:set total=10 trans=0) and [run](cmd:run).
3. [Start from the end, with the time](cmd:ic shift) and
   [run again](cmd:run).

```
set total=10 trans=0
run
ic shift
run
```

```python
import xpplab as xs

m = xs.load(xs.example("forcpend.ode"))
m.set(total=10, trans=0)
first = m.run()
m.ic_last(shift=True)
second = m.run()
```

The first run goes from `t` = 0 to 9.9852 (the step is 0.0628), and `ic
shift` sets `T0` to 9.9852. The second run goes from 9.9852 to 19.9704 and
ends at `x` = −0.8317.

How to read it. That is exactly where a single run of 20 ends: in Python
the two parts and the single run agree to the last digit. With `ic last`
instead, the second run starts at `t` = 0 and the forcing starts again from
`cos 0` = 1, not from `cos 9.9852`: the second part then ends at
`x` = −7.546, and differs from the single run by up to 6.7. That is more
than 2π in the angle: a whole extra turn over the top.

## Settings

`ic shift` has no settings. It takes every variable from the last stored
point of the run, and the time from that point too.

## Results

- **The initial values** are the last run's final state, and **`T0`** the
  time it ended. The command window says "Initial values set to the end of
  the last run, and the start time T0 to 9.9852."
- **In Python** `m.ic_last(shift=True)` returns the model.

## More about

**Autonomous and forced models.** A model whose right-hand sides do not
contain `t` is *autonomous*: its future depends only on where it is, not on
when. For it, `ic last` and `ic shift` give the same trajectory. A forced
model is not autonomous: the same state at another time leads somewhere
else, and only `ic shift` carries on the original run.

**`T0` stays shifted.** Every later run starts at the new `T0`. To start
again at 0, `set t0=0`.

## Tips

- After several `ic shift` and runs, `T0` is the total time so far:
  `get t0` shows it.
- [continue](continue.md) gives the same continuation without changing
  `T0` or the initial values, and keeps it all in one run.

## Algorithms

XPPAUT's *Initial conds > Shift*: as *Last*, and the time of the last stored
point becomes `T0`.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `There is no run yet.` | The model's first run failed, so there is no end to take. | Fix what stopped the run, then `run`. |

## References

- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapter 8 (the forced pendulum).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [ic last](ic-last.md): the same without the time.
- [continue](continue.md): one longer run instead of a second one.

## Version history

Introduced in 0.1.0.
