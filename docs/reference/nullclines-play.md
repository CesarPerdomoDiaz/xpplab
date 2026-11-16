# nullclines play

Show the frozen nullclines one at a time, in turn, as a film.

## Syntax

```
nullclines play
nullclines play off
nullclines show K
```

- **In the app:** Phase Plane > **Freeze...** > the **Show** slider and
  the **Play** box (XPPAUT's *Freeze > Animate*).
- **In Python:** draw each pair of `m.nullcline_sweep()` in turn, for
  example as frames of a matplotlib animation.

## Description

`nullclines play` cycles through the frozen nullclines, showing one set
at a time, so that a family frozen over a parameter range moves on the
Phase Plane like a film. `nullclines play off` stops it.

`nullclines show K` shows only the `K`-th frozen set, and `nullclines show
0` all of them again. Either stops the playing.

Playing needs at least two frozen sets; see
[nullclines freeze](nullclines-freeze.md).

## Examples

### The fold, as a film

1. [Open the Morris-Lecar model](example:lecar.ode) and open the **Phase
   Plane** tab.
2. [Freeze twenty-one currents](cmd:nullclines freeze iapp 0 0.1 21).
3. [Play them](cmd:nullclines play), then
   [show the last one](cmd:nullclines show 21).

```
nullclines freeze iapp 0 0.1 21
nullclines play
nullclines show 21
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
values = np.linspace(0, 0.1, 21)
frozen = m.nullcline_sweep("iapp", values, "V", "W", (-0.6, 1.2), (-0.25, 1.2))
print(len(frozen))
```

The `V` nullcline rises step by step as `iapp` goes from 0 to 0.1 in steps
of 0.005; its lower knee lifts off the `W` nullcline between the
seventeenth value, 0.08, and the eighteenth, 0.085. The last set, at 0.1,
crosses once.

How to read it. The film shows the fold as motion: two crossings approach,
touch and vanish. The values on each side of the touching bracket the
fold, at 0.0833.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `K` | `nullclines show K` | Which frozen set to show; 0 for all. | The legend gives the shown set's parameter value. |

## Results

- **The Phase Plane** shows one frozen set at a time, with its parameter
  value in the legend.

## More about

**Families and films.** A family drawn all at once shows the range a curve
sweeps; one at a time, it shows the order. Both are useful: freeze, look at
them all, then play.

## Tips

- Freeze enough values for a smooth film: twenty or more.
- Stop on the set just before and just after an event with
  `nullclines show K`.

## Algorithms

The frozen sets are drawn in turn; nothing is computed.

## When it fails

| Symptom | Why | What to do |
|---|---|---|
| Nothing plays | Fewer than two sets are frozen. | Freeze a range first. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [nullclines freeze](nullclines-freeze.md): make the family.
- [nullclines unfreeze](nullclines-unfreeze.md): remove it.

## Version history

Introduced in 0.1.0.
