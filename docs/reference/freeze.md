# freeze

Keep the current run on the plots, so that the next one is drawn beside
it.

## Syntax

```
freeze
```

- **In the app:** XPPAUT's *Graphic stuff > Freeze*.
- **In Python:** keep the runs: `runs.append(m.run())`, and plot them
  together.

## Description

Each run replaces the one before on the plots. `freeze` keeps a copy of
the current run on them, as a fixed curve, so that a run with another
parameter or start can be compared with it. Up to 16 curves are kept; a
seventeenth replaces the oldest.

The current run itself stays the run: [ic last](ic-last.md),
[continue](continue.md), `export` and the analyses use it as before.
[erase](erase.md) removes the kept curves.

## Examples

### Two currents, one plot

Compare the neuron at rest and firing:

1. [Open the Morris-Lecar model](example:lecar.ode).
2. Run with [iapp = 0.05](cmd:set iapp=0.05) and [keep it](cmd:freeze).
3. [Raise the current](cmd:set iapp=0.1) and [run](cmd:run).

```
set iapp=0.05
run
freeze
set iapp=0.1
run
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
runs = [m.run(iapp=0.05), m.run(iapp=0.1)]
for r in runs:
    print(round(r["V"].min(), 4), round(r["V"].max(), 4))
```

The Time Series shows both runs. With `iapp` = 0.05 the neuron fires a
single spike, to `V` = 0.2283, and comes to rest at −0.3825; its lowest
value, −0.4306, is the dip after the spike. With `iapp` = 0.1 it fires
twice in the 30 time units, to 0.2615, and keeps firing.

How to read it. The kept curve is the reference the new run is read
against: here, that the larger current turns one spike into repetitive
firing. The single spike at 0.05 comes from the start: `V` = −0.144 is
above the threshold (see [click](click.md)). Freeze a
run before each change to build up a family of curves.

## Settings

`freeze` has no settings.

## Results

- **The plots** show the kept curve with the runs that follow.
- **The status line** says how many curves are kept.
- **In Python** the runs are yours to keep in a list, and to plot together
  with matplotlib.

## More about

**Kept curves and the current run.** The current run is what every
command that needs a run uses. A kept curve is only drawn: it cannot be
continued or exported. To export several runs, export each while it is
the current one, or keep them in Python.

**Keeping every run.** `click keep` keeps each run started by a click, and
[range](range.md) without a measure draws all the runs of a sweep, without
`freeze`.

## Tips

- Freeze before changing a parameter, then compare by eye.
- `erase` before starting a new comparison.

## Algorithms

The current run's columns are copied into the list of kept curves; nothing
is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `There is no run yet.` | The model's first run failed. | Fix what stopped it, and `run`. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [erase](erase.md): remove the kept curves.
- [click](click.md): `click keep` keeps clicked runs.
- [run](run.md): the run that is kept.

## Version history

Introduced in 0.1.0. Since 0.1.4 the run stays the current run after
`freeze`.
