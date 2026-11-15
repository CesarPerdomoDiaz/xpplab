# spectrum

The power spectral density of a column of the last run, or the cross
spectrum or coherence of two: which frequencies the run contains.

## Syntax

```
spectrum COLUMN
spectrum COLUMN LENGTH
spectrum COLUMN COLUMN2 cross LENGTH
spectrum COLUMN COLUMN2 coherence LENGTH
```

`psd` is another name for `spectrum`.

- **In the app:** Run > Analyse the run > **Spectrum** (XPPAUT's
  *Numerics > stocHastic > Spec.dns*), where the window can be chosen too.
- **In Python:** `m.spectrum("V", length=8192)`, or
  `m.spectrum("V", "W", kind="coherence", length=8192)`.

## Description

`spectrum COLUMN LENGTH` estimates the power spectral density of `COLUMN`
by Welch's method: the run is cut into windows of `LENGTH` points that
overlap by half, each window is tapered (a Hamming window by default) and
transformed, and the squared transforms are averaged. The result is a table
of `freq`, in cycles per unit time, and `power`, with `LENGTH`/2 rows.

`LENGTH` is 512 points at first; without it the app uses the length given
last, and Python 512. Either way it is halved until the run holds at
least two half-overlapping windows. A longer window resolves finer frequencies; more windows give a
smoother estimate.

With `COLUMN2`, `cross` gives the cross spectrum of the two columns and
`coherence` how strongly they are related at each frequency, from 0 (not
at all) to 1 (exactly, up to a fixed gain and delay).

## Examples

### The firing frequency and its harmonics

The Morris-Lecar model with `iapp` = 0.1 fires with a period of 16.45, a
frequency of 0.0608. Run long, then take the spectrum with windows long
enough to resolve it:

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Make it fire for a long time](cmd:set iapp=0.1 total=2000 maxstor=50000)
   and [run](cmd:run).
3. [Take the spectrum of V](cmd:spectrum V 8192).

```
set iapp=0.1 total=2000 maxstor=50000
run
spectrum V 8192
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.run(iapp=0.1, total=2000, maxstor=50000)
s = m.spectrum("V", length=8192)
k = np.argmax(s["power"][1:]) + 1
print(s["freq"][k])
```

The run has 40001 points, so it holds several windows of 8192 points
(409.6 time units each), and the frequencies are 1/409.6 = 0.00244 apart.
The highest peak is at 0.0610, and smaller peaks follow at 0.1221, 0.1831
and 0.2417, with 63%, 30% and 11% of its power.

How to read it. The first peak is the firing frequency, 0.0608, to within
the spacing of the frequencies. The others are its multiples, the
harmonics: a train of sharp spikes is far from a sine wave, and its shape
is made of many harmonics. A sinusoidal oscillation would have one peak
only.

### Too short a window

With the default window of 512 points (25.6 time units) the frequencies
are 0.039 apart. The firing frequency, 0.0608, falls between the
frequencies 0.039 and 0.078, and the highest value is at 0.039:

```python
coarse = m.spectrum("V")
print(len(coarse), coarse["freq"][1])
```

How to read it. A window must hold several periods for its spectrum to
resolve them: here 25.6 time units hold 1.6 periods. Choose `LENGTH` so
that `LENGTH` × the step is many times the longest period of interest.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `LENGTH` | the command; `length=` in Python | The points in each window. | A power of two. Many periods long, and short enough that the run holds several windows. |
| `cross`, `coherence` | the command; `kind=` in Python | What to compute from two columns. | `coherence` to see at which frequencies two columns move together. |
| **Window** | the Spectrum tool; `window=` in Python | The taper: `square`, `parzen`, `hamming`, `bartlett` or `hann`. | Hamming, the default, suits most runs. |
| **Total time**, **step** | `set total=... dt=...` | The run's length and its sampling. | The highest frequency shown is 1/(2 × step); the finest spacing 1/(`LENGTH` × step). |

## Results

- **The Analysis window** draws the spectrum, on a logarithmic scale if
  chosen.
- **The table** has the columns `freq` and `power` (or `cross`, or
  `coherence`); `export FILE.csv analysis` writes it.
- **In Python** `m.spectrum()` returns the table.

## More about

**Resolution and noise.** The spacing of the frequencies is one over the
length of a window in time; the scatter of the estimate falls as more
windows are averaged. A fixed run trades one against the other: longer
windows, fewer of them.

**Coherence.** For two columns of a deterministic oscillation the
coherence is 1 at the oscillation's frequencies: in the example `V` and
`W` have coherence 1.0 at all four peaks. Noise lowers it.

**Harmonics.** A periodic signal of frequency f has power only at f, 2f,
3f, and so on. The more abruptly it changes, the further up the harmonics
reach.

## Tips

- Leave out the transient (`set trans=...`) so that the approach to the
  oscillation does not blur the peaks.
- For a noisy model, use many windows: the average is what makes the
  estimate reliable.

## Algorithms

XPPAUT's Spec.dns: Welch's method, with windows of `LENGTH` points
overlapping by half, the chosen taper, and the FFT of each window; the
squared magnitudes (or the products of the two columns' transforms) are
averaged over the windows.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: give a column, e.g. spectrum V` | No column was named. | Name one; `whos` lists them. |
| `note: the run has N points, so the window is M points instead of L` | The run is shorter than a window. | Run longer, or give a smaller `LENGTH`. |
| One broad hump instead of peaks | The window is shorter than a few periods. | Raise `LENGTH`, and the total time with it. |

## References

- Press, W. H., Teukolsky, S. A., Vetterling, W. T., and Flannery, B. P.
  (2007). *Numerical Recipes*, 3rd edition. Cambridge University Press.
  Chapter 13 (spectral analysis).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [xcorr](xcorr.md): the same information in time: the autocorrelation is
  the transform of the spectrum.
- [mean](mean.md): the mean and spread of a column.

## Version history

Introduced in 0.1.0.
