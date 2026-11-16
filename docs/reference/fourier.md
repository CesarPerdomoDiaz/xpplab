# fourier

The Fourier series of a column of the last run, taken as one period: the
cosine and sine coefficient of each harmonic.

## Syntax

```
fourier COLUMN
```

`fft` is another name for `fourier`.

- **In the app:** Run > Analyse the run > **Fourier series** (XPPAUT's
  *Numerics > stocHastic > Fourier*).
- **In Python:** `f = m.fourier("X")`.

## Description

`fourier COLUMN` treats the whole run as one period of a periodic signal
and writes it as a sum of harmonics,

x(t) ≈ c₀ + Σ (c_k cos(2πkt/L) + s_k sin(2πkt/L)),

where L is the length of the run. The result is a table of `freq` (k/L,
in cycles per unit time), `cos` (c_k) and `sin` (s_k), one row per
harmonic k = 0, 1, 2, ..., up to half the number of points.

The series is exact only when the run is exactly one period, or a whole
number of periods; otherwise the jump between its end and its start
spreads into every harmonic. [power](power.md) gives the same series as
amplitudes and phases.

## Examples

### One period of the van der Pol oscillator

The van der Pol oscillator settles onto a limit cycle of period 6.663.
Settle, start on the cycle, and run one period:

1. [Open the van der Pol oscillator](example:classics/vanderpol.ode) and
   [run](cmd:run) to settle.
2. [Start from the end](cmd:ic last),
   [run one period](cmd:set total=6.66) and [run](cmd:run).
3. [Take the Fourier series of x](cmd:fourier X).

```
run
ic last
set total=6.66
run
fourier X
```

```python
import xpplab as xs

m = xs.load(xs.example("classics/vanderpol.ode"))
m.run()
m.ic_last()
m.set(total=6.66)
m.run()
f = m.fourier("X")
print(f["freq"][1], f["cos"][1], f["sin"][1])
```

The run has 667 points, so the table has 332 rows, the harmonics k = 0
to 331. The first harmonic is at 0.15015 cycles per unit time (one over
the run's length, 6.66), with coefficients `cos` = 1.7014 and `sin` =
−1.0795. The second harmonic's are both below 0.002 in size; the third's
are −0.2098 and 0.1131.

How to read it. The fundamental carries almost all of the oscillation.
The even harmonics nearly vanish because the van der Pol cycle is
symmetric: x(t + T/2) = −x(t), and a signal with that symmetry has only
odd harmonics. What remains of the even ones comes from the run being
6.66 rather than exactly one period.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `COLUMN` | the command; the name in Python | The column to expand. | Any column of the run. |
| **The run's length** | `set total=...` | What counts as one period. | Make it one period, or a whole number of them. |

## Results

- **The Analysis window** draws the coefficients against the frequency.
- **The table** has the columns `freq`, `cos` and `sin`;
  `export FILE.csv analysis` writes it.
- **In Python** `m.fourier()` returns the table.

## More about

**Coefficients and phase.** The pair (c_k, s_k) depends on where in the
cycle the run starts; the amplitude √(c_k² + s_k²) does not. For the
example, √(1.7014² + 1.0795²) = 2.015, as [power](power.md) reports.

**Fourier series and spectra.** The series describes one period exactly.
For a long run that is not periodic, or not an exact number of periods, a
spectral density estimate such as [spectrum](spectrum.md) is the better
tool.

## Tips

- Measure the period first (with [prc](prc.md) or from the Time Series),
  and run exactly that long.
- Start on the cycle (`ic last` after a long run), so that the transient is
  not in the series.

## Algorithms

XPPAUT's Fourier: the discrete Fourier transform of the stored points of
the column, scaled to the coefficients of the series.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: give a column, e.g. fourier V` | No column was named. | Name one; `whos` lists them. |
| Every harmonic is large | The run is not a whole number of periods, or not periodic at all. | Run exactly one period from the cycle. |

## References

- Strang, G. (2007). *Computational Science and Engineering*.
  Wellesley-Cambridge Press. Chapter 4 (Fourier series).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [power](power.md): amplitudes and phases.
- [spectrum](spectrum.md): the spectral density of a long run.

## Version history

Introduced in 0.1.0.
