# xcorr

The autocorrelation of a column of the last run, or the cross-correlation
of two: how much the run resembles itself, or another column, shifted in
time.

## Syntax

```
xcorr COLUMN
xcorr COLUMN LAGS
xcorr COLUMN COLUMN2 LAGS
```

- **In the app:** Run > Analyse the run > **Correlation** (XPPAUT's
  *Numerics > stocHastic > Xcorrel*), where it can also be computed by FFT.
- **In Python:** `m.correlation("V", lags=800)`, or
  `m.correlation("V", "W", lags=800)`, with `fft=True` for the FFT.

## Description

`xcorr COLUMN LAGS` subtracts the mean of `COLUMN` and compares the run
with itself shifted by every whole number of output steps from −`LAGS`/2
to `LAGS`/2 (100 lags by default, so ±50 steps). At a shift of zero the
result is the column's variance; at other shifts it is large when the run
repeats itself after that time and negative when it is opposite.

With `COLUMN2` it compares the two columns instead: a peak at a lag τ means
that `COLUMN2` follows `COLUMN` by about τ.

The number of lags is made odd, so that zero is among them, and at most
about half the run. The result is a table of `lag`, in time units, and
`correlation`.

## Examples

### The period of a firing neuron

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Make it fire for longer](cmd:set iapp=0.1 total=200) and [run](cmd:run).
3. [Take the autocorrelation of V over ±20](cmd:xcorr V 800).

```
set iapp=0.1 total=200
run
xcorr V 800
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
run = m.run(iapp=0.1, total=200)
c = m.correlation("V", lags=800)
later = c["lag"] > 5
print(c["lag"][later][np.argmax(c["correlation"][later])])
```

The step is 0.05, so 800 lags cover −20 to 20 and the table has 802 rows.
At zero lag the value is 0.03308, the variance of `V`. The largest value
after that is at lag 16.45, where it is 0.0311, 94% of the variance; the
lowest is at 8.51, −0.0142.

How to read it. The run repeats itself after one period, so the
autocorrelation peaks at the period: 16.45, as the spike times of
[ic last](ic-last.md) gave. Half a period later the run is at its most
unlike itself, so the trough sits near half the period.

### What follows what

```
xcorr V W 800
```

```python
vw = m.correlation("V", "W", lags=800)
near = np.abs(vw["lag"]) < 8
print(vw["lag"][near][np.argmax(vw["correlation"][near])])
```

The cross-correlation of `V` and `W` is largest at a lag of 1.32.

How to read it. A positive lag means that the second column follows the
first: `W` at time t + 1.32 is most like `V` at time t. The recovery
variable rises about 1.3 time units after each spike of `V`: the delay
between excitation and recovery.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `LAGS` | the command; `lags=` in Python | The number of shifts, in output steps, half each way. | Make `LAGS` × the step more than twice the time you look for: 800 × 0.05 = 40 covers a period of 16.45. |
| `COLUMN2` | the command; the second name in Python | The column to compare with. | Leave it out for the autocorrelation. |
| **FFT** | the Correlation tool; `fft=True` in Python | Compute through the fast Fourier transform. | Faster for long runs and many lags. In the example it agrees with the direct sums to 7e-8, except in the table's last two rows. |

## Results

- **The Analysis window** draws the correlation against the lag.
- **The table** has the columns `lag` and `correlation`;
  `export FILE.csv analysis` writes it.
- **In Python** `m.correlation()` returns the table.

## More about

**The lag column.** As in XPPAUT, the `LAGS` + 1 shifts are labelled
evenly from −`LAGS`/2 × step to `LAGS`/2 × step over `LAGS` + 2 rows, so
the labels are spaced slightly less than one step (0.04994 instead of 0.05
here) and the zero shift is labelled −0.025. Read peaks to within a step.

**Autocorrelation and periodicity.** For a periodic run the
autocorrelation is periodic too, with the same period; for a chaotic or
noisy run it decays with the lag, and how fast it decays is the run's
correlation time.

## Tips

- Leave out the transient (`set trans=...`) so that the approach to the
  attractor does not lower the peaks.
- For a noisy model, average over a long run: the peaks stand out of the
  noise as the run grows.

## Algorithms

XPPAUT's Xcorrel: the means are subtracted and the products of the column
with the other one, shifted by each lag, are averaged; with FFT the same
sums come from the transform of the two columns.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: give a column, e.g. xcorr V` | No column was named. | Name one; `whos` lists them. |
| `the run is too short for a correlation` | The run has too few points. | Run longer, or with a smaller step. |
| No peak at the period | `LAGS` × the step is shorter than the period. | Raise `LAGS`. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [hist](hist.md): the distribution of a column.
- [mean](mean.md): its mean and variance.

## Version history

Introduced in 0.1.0.
