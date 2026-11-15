# continue

Integrate further from the end of the last run, and add the new part to it.

## Syntax

```
continue
continue TIME
```

`cc` is another name for `continue`.

- **In the app:** XPPAUT's keys `C C`.
- **In Python:** `sol = m.continue_run(sol)`, or `m.continue_run(sol, 170)`.

## Description

`continue TIME` takes the state at the end of the last run and integrates
for `TIME` more, with the same method and step. The new points are added to
the run, so the Time Series shows one longer run, not two. Without `TIME`,
or with a time that is not positive, it integrates for the total time
(`total`) again.

The model's initial values, its start time and its total time are not
changed: the next [run](run.md) starts from the beginning as before.

Use it when a run was too short: to see whether the model settles, to let a
slow transient die away, or to add periods to an oscillation, without
starting again from the beginning.

## Examples

### A short run, made longer

The Morris-Lecar model with `iapp` = 0.1 fires repetitively, but the first
30 time units show only two spikes. Continue for 170 more:

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Set the current](cmd:set iapp=0.1) and [run](cmd:run).
3. [Continue for 170](cmd:continue 170).

```
set iapp=0.1
run
continue 170
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
sol = m.run(iapp=0.1)
sol = m.continue_run(sol, 170)
```

The run grows from 601 points ending at `t` = 30 to 4001 points ending at
`t` = 200, and the status line says "Continued to t = 200".

How to read it. The result is the run you would have had with `total=200`
from the start. Comparing the two, `V` differs by at most 6e-8, at any
time: the only difference is that the continuation starts from the stored
end point, which is kept in single precision.

### The model is left as it was

After the continuation:

```
get total
get v
```

```python
print(m.get("total"), m.get("v"))
```

`total` is still 30 and `v` still −0.144, the model's own initial value.
So `run` starts again from the beginning; to start the next run from where
this one ended instead, use `ic last`.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `TIME` | the command; `time` in Python | How long to integrate more. | Leave it out to add the total time again. |
| The method, the step and the other options | as for [run](run.md) | The continuation uses them as they are now. | Changing the step between a run and its continuation is allowed, and the run then has two step sizes. |

## Results

- **The run** is longer, by `TIME`, and every window that shows it is
  redrawn.
- **The status line** shows the new final time.
- **In Python** `m.continue_run()` returns the whole run, the old points
  and the new.

## More about

**Why the result matches one long run.** A fixed-step method takes the same
steps whether the integration stops at `t` = 30 and goes on, or runs
through. The only change is the starting point of the second part: the
stored end point, in single precision, instead of the full-precision
value the integrator had. That changes the result in the seventh digit,
and in a chaotic model the difference then grows as any small difference
does.

**Adaptive methods** choose their steps as they go, and the continuation
starts that choice again from its first point, so its steps need not be
those of one long run. The result still agrees closely: in the example,
`V` differs from one long run by at most 1.6e-7 with `meth=qualrk` and
4e-8 with `meth=stiff`.

## Tips

- To see whether a model settles, continue a few times and watch the last
  values: if they still change, keep going.
- For a model with a long transient, set `trans` instead, so that the
  transient is integrated but not stored.
- Opening a model runs it once, so `continue` works straight away and
  extends that first run.

## Algorithms

The state at the last stored point becomes the initial value, the start
time becomes the last stored time, and XPPAUT's integration routine runs
for `TIME`. The new points are added to the stored run, and the initial
values, start time and total time are then put back. This is XPPAUT's
*Continue*.

## When it fails

| Message | Why | What to do |
|---|---|---|
| A new run instead of a continuation | There was no run to extend (the model's first run failed), so `continue` runs from the initial values. | Fix what stopped the first run; then `continue` extends it. |
| The continuation stops early | A variable passed `bound`, or the storage is full. | As for [run](run.md): raise `bound` or `maxstor`. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [run](run.md): the run that this extends.
- [Commands and shortcuts](../manual/commands.md): `ic last` and
  `ic shift`, to start the next run where this one ended.

## Version history

Introduced in 0.1.0.
