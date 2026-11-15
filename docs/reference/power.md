# power

The Fourier series of a column of the last run as amplitudes and phases:
how large each harmonic is.

## Syntax

```
power COLUMN
```

- **In the app:** Run > Analyse the run > **Fourier series**, shown as
  amplitude and phase (XPPAUT's *Numerics > stocHastic > Power*).
- **In Python:** `p = m.power("X")`.

## Description

`power COLUMN` is [fourier](fourier.md) written another way: for each
harmonic k, the amplitude √(c_k² + s_k²) and the phase of its cosine and
sine coefficients. The result is a table of `freq`, `amplitude` and
`phase`. As for `fourier`, the run is taken as one period.

The amplitudes do not depend on where in the cycle the run starts, so
they describe the shape of the oscillation itself.

## Examples

### The odd harmonics of the van der Pol cycle

As for [fourier](fourier.md), run one period of the van der Pol
oscillator from its cycle, then take the amplitudes:

1. [Open the van der Pol oscillator](example:classics/vanderpol.ode) and
   [run](cmd:run).
2. [Start from the end](cmd:ic last),
   [run one period](cmd:set total=6.66) and [run](cmd:run).
3. [Take the amplitudes of x](cmd:power X).

```
run
ic last
set total=6.66
run
power X
```

```python
import xpplab as xs

m = xs.load(xs.example("classics/vanderpol.ode"))
m.run()
m.ic_last()
m.set(total=6.66)
m.run()
p = m.power("X")
print(p["amplitude"][:8])
```

The amplitudes of the first eight harmonics, k = 0 to 7:

| k | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| amplitude | 0.0015 | 2.015 | 0.0025 | 0.2383 | 0.0006 | 0.048 | 0.0004 | 0.011 |

How to read it. The odd harmonics carry the oscillation, falling quickly
from 2.015 to 0.238, 0.048 and 0.011; the even ones are a hundred to a
thousand times smaller. That is the mark of a cycle with the symmetry
x(t + T/2) = −x(t). The `0` row is the mean of the run, 0.0015.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `COLUMN` | the command; the name in Python | The column to expand. | Any column of the run. |
| **The run's length** | `set total=...` | What counts as one period. | One period, or a whole number of them. |

## Results

- **The Analysis window** draws the amplitudes against the frequency.
- **The table** has the columns `freq`, `amplitude` and `phase`;
  `export FILE.csv analysis` writes it.
- **In Python** `m.power()` returns the table.

## More about

**Shape and harmonics.** A sine wave has one harmonic. The more a periodic
signal departs from one, the more harmonics it needs, and the slower
their amplitudes fall. With `mu` = 5 the van der Pol cycle becomes a
relaxation oscillation, with sudden jumps, a period of 11.61, and
amplitudes 2.106, 0.584, 0.307 and 0.192 for k = 1, 3, 5 and 7: they fall
much more slowly.

## Tips

- Compare amplitudes, not coefficients, between runs that start at
  different points of the cycle.
- Plot the amplitudes on a logarithmic scale to see how fast they fall.

## Algorithms

As [fourier](fourier.md), with the coefficients turned into amplitude and
phase.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: give a column, e.g. power V` | No column was named. | Name one; `whos` lists them. |
| Every harmonic is large | The run is not a whole number of periods. | Run exactly one period from the cycle. |

## References

- Strang, G. (2007). *Computational Science and Engineering*.
  Wellesley-Cambridge Press. Chapter 4.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [fourier](fourier.md): the cosine and sine coefficients.
- [spectrum](spectrum.md): the spectral density of a long run.

## Version history

Introduced in 0.1.0.
