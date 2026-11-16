# many

Run from many initial values drawn at random in a box, and keep every
run.

## Syntax

```
many N VAR LO HI ...
many N seed S VAR LO HI ...
many N VAR LO HI ... keep COLUMN LO HI
```

- **In the app:** Workspace > **Many runs** > *Random initial values*:
  tick the variables to draw and give each its two values, set the number
  of **runs** and the **seed**, optionally a **keep where** condition, and
  press **Run them**.
- **In Python:** `starts = m.sample_initial(50, {"V": (lo, hi), ...})`,
  then `runs = m.run_many(starts)`.

## Description

`many N` draws `N` initial values uniformly at random in the box given by
the `VAR LO HI` triples (the other variables keep their initial values),
runs from each, and keeps all the runs, drawn together on the plots.

The random numbers come from a generator started at `seed` (1 unless
given), the same in the app and in Python, so the same command gives the
same starts in both. `keep COLUMN LO HI` keeps only draws at which a
column, such as an auxiliary quantity, lies between `LO` and `HI`.

The command window says how many runs ran to the end and how many a
global flag stopped, for models with `global` events.

## Examples

### Two behaviours at one current

With the set `hopf` and `iapp` = 0.255 the Morris-Lecar model is
bistable: a stable rest state at `V` = −0.2072, `W` = 0.2008, and a stable
oscillation around it ([range](range.md) found the strip, between the
fold at 0.2484 and the Hopf point at 0.2625). Start fifty runs near the
rest state:

1. [Open the Morris-Lecar model](example:lecar.ode) and
   [apply the set hopf](cmd:use hopf).
2. [Set the current and the run length](cmd:set iapp=0.255 total=200).
3. [Run from fifty starts](cmd:many 50 V -0.26 -0.16 W 0.175 0.225).

```
use hopf
set iapp=0.255 total=200
many 50 V -0.26 -0.16 W 0.175 0.225
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.apply_set("hopf")
m.set(iapp=0.255, total=200)
starts = m.sample_initial(50, {"V": (-0.26, -0.16), "W": (0.175, 0.225)})
runs = m.run_many(starts)
swing = np.array([np.ptp(r["V"][r.t > 100]) for r in runs])
print((swing > 0.3).sum(), (swing < 0.05).sum())
```

The command window says "Many runs: 50 from random initial values (seed
1): 50 ran to the end." Over the last 100 time units, 16 runs swing by
0.648 in `V`, firing, and 34 by less than 0.005, settling to rest.

How to read it. Starts close to the rest state stay there; starts further
out are thrown onto the oscillation. Between the two sets of starts runs
an unstable periodic orbit, the boundary of the rest state's basin of
attraction. Many runs from a box are the quickest way to see that a model
has more than one attractor, and roughly where their basins meet.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `N` | the command; **runs** in the tab (up to 5000); the first argument in Python | The number of runs. | Tens for a picture, hundreds for a fraction. |
| `VAR LO HI` | the command; a ticked variable and its two values in the tab; the box in Python | The variables to draw and their ranges. | Variables not named keep their initial values. |
| `seed` | the command; **seed** in the tab; `seed=` in Python | Where the random numbers start. | Change it for another sample. |
| `keep COLUMN LO HI` | the command; **keep where** in the tab; `require=` in Python | Keep only draws where a column, a variable or an `aux` quantity, lies in a range. | For a constraint such as an energy. `inf` is allowed: `keep tens 0 inf` keeps the states where a tether is taut. |

## Results

- **The plots** draw all the runs in the Phase Plane and the Time Series,
  each coloured by the way it ended: by the flag that stopped it, or as
  having run to the end. In the tab, **Show on the plots** hides or shows
  them and **Clear** deletes them.
- **The command window** and the status bar say how many ran to the end
  and which flags stopped the others.
- **`export FILE.csv many`** writes every run with its ending: the columns
  `run` and `event`, then the run's own. The
  [Figure window](../manual/figures.md) draws the runs for a paper, as one
  series or one per ending.
- **In Python** `m.run_many()` returns a `RunList` of the runs, with
  `.events` and `.counts()` for the flags.

## More about

**Basins of attraction.** The set of starts that lead to an attractor is
its basin. With two attractors the basins share a boundary, often the
stable manifold of a saddle or, as here, an unstable cycle. The fraction
of random starts that reach each attractor estimates the size of its
basin within the box.

**Flags that stop a run.** A run ends early when a global flag of the
model stops it, such as

```
global -1 gam {arret=1}      # gam crosses zero going down: the ground
global -1 tn {arret=stop}    # a tension through zero; stop=0 turns it off
```

and the runs are counted by the flag that stopped them, or as having run
to the end.

## Tips

- Make the run long enough for every run to settle, then judge where each
  ended over the last part only.
- Use the same `seed` to compare parameter values on the same starts.

## Algorithms

The starts are drawn with a SplitMix64 generator, uniformly in the box;
each run is an ordinary [run](run.md), and the initial values are put
back afterwards.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `Write: many N [seed S] VAR LO HI ... [keep COLUMN LO HI]` | `N` is missing. | Give the number of runs. |
| `error: NAME is not a variable, or its LO HI are missing` | A name is not a variable, or a range is incomplete. | Check the names with `whos`. |
| `error: keep COLUMN LO HI` | The `keep` clause is incomplete. | Give the column and both bounds. |

## References

- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapter 6.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [range](range.md): many runs over a parameter instead.
- [click](click.md): one run from a chosen point.

## Version history

Introduced in 0.1.0.
