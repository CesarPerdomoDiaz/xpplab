# ensemble

Run a noisy model many times and keep the mean and the variance of every
column at each time.

## Syntax

```
ensemble
ensemble N
```

- **In the app:** Run > Analyse the run > **Ensemble of noisy runs** (XPPAUT's
  *Numerics > stocHastic > Compute*).
- **In Python:** `ens = m.ensemble(400)`; `ens.mean`, `ens.variance` and
  `ens.std` are runs.

## Description

A model with noise (`wiener`, `ran()`) gives a different run each time.
`ensemble N` runs it `N` times from the same initial values, the random
numbers going on from one run to the next, and keeps the mean and the
variance of every column at each output time, as XPPAUT's batch mode does
with `@ stoch=1`. The last of the runs stays as the current run.

In the app the result is a table of `T`, then `X_mean` and `X_var` for
each column `X`; the Analysis window draws a mean with a band of one
standard deviation.

## Examples

### The Ornstein-Uhlenbeck process

The Ornstein-Uhlenbeck process, dx = −θx dt + σ dW with θ = 1 and
σ = 0.5, starts at x = 2. Its mean decays as 2e^(−t), and its variance
grows to σ²/(2θ) = 0.125.

1. [Open the Ornstein-Uhlenbeck process](example:classics/ornstein_uhlenbeck.ode).
2. [Run it 400 times](cmd:ensemble 400), and write the table with
   `export ens.csv analysis`.

```
ensemble 400
export ens.csv analysis
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("classics/ornstein_uhlenbeck"))
m.run()                       # as the app does on opening the model
ens = m.ensemble(400)
t, mean, var = ens.mean.t, ens.mean["X"], ens.variance["X"]
```

The command window says "Ensemble of 400 runs: 501 times." Against the
formulas:

| t | mean | 2e^(−t) | variance | 0.125 (1 − e^(−2t)) |
|---|---|---|---|---|
| 1 | 0.753 | 0.736 | 0.1204 | 0.1081 |
| 2 | 0.288 | 0.271 | 0.1224 | 0.1227 |
| 5 | 0.026 | 0.013 | 0.1387 | 0.125 |

How to read it. The averages follow the formulas within what 400 runs can
show: the standard error of the mean is √(0.125/400) ≈ 0.018, and the
largest difference in the mean, 0.045, is about two and a half of those.
The variance after t = 3 averages 0.131, 5% above 0.125. More runs bring
both closer, as one over the square root of their number.

The Python block gives the app's table exactly, number for number: it
runs the model once first, as the app does when it opens a model, so the
random numbers are at the same place when the ensemble starts.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `N` | the command; `trials` in Python | The number of runs (at least 2). | 50 in the app at first, then the number given last. Hundreds for a smooth mean, thousands for a variance to a few per cent. |
| **The method and step** | `set meth=... dt=...` | How each run is integrated. | Euler with a small step, as XPPAUT's noise needs. |

## Results

- **The Analysis window** draws each column's mean with a band of one
  standard deviation.
- **The table** has `T`, `X_mean` and `X_var` for each column;
  `export FILE.csv analysis` writes it.
- **In Python** `m.ensemble()` returns an `Ensemble`: `mean` and
  `variance` as runs, and `std`.

## More about

**Noise in XPPAUT.** A `wiener w` statement makes `w` a normal random
number divided by √dt at each step, so that `sigma*w` in an equation with
Euler's method gives the increments of a Wiener process. Each run draws
new numbers; the sequence starts from the same seed when the model is
opened, so a session repeats exactly.

**Single precision.** XPPAUT accumulates the sums in single precision, as
in its batch mode; the mean and the variance are exact to about seven
digits.

## Tips

- Check one run first, to see the noise is of the size you meant.
- Compare the mean with the model without noise (`sigma=0`): for a linear
  model they agree; for a nonlinear one the noise can shift the mean.

## Algorithms

XPPAUT's stocHastic > Compute: a range over the first parameter from its
value to the same value, `N` runs, each added to running sums, and the
mean and the variance taken from them.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `an ensemble needs at least 2 runs` | `N` is less than 2. | Give 2 or more. |

## References

- Gardiner, C. (2009). *Stochastic Methods*, 4th edition. Springer.
  Chapter 3 (the Ornstein-Uhlenbeck process).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [many](many.md): many runs from different starts.
- [mean](mean.md): the mean of one run over time.

## Version history

Introduced in 0.1.0.
