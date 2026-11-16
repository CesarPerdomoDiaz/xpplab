# mean

The mean and standard deviation of a column of the last run, and the
largest Liapunov exponent of the run.

## Syntax

```
mean COLUMN
```

- **In the app:** Run > Analyse the run > **Statistics** (XPPAUT's
  *Numerics > stocHastic > Mean* and *Liapunov*).
- **In Python:** `m.mean("V")` for the mean and standard deviation, and
  `m.lyapunov()` for the exponent.

## Description

`mean COLUMN` prints three numbers:

- the **mean** of `COLUMN` over the stored points of the last run;
- its **standard deviation** about that mean;
- the **largest Liapunov exponent** of the run: the rate at which two
  nearby trajectories separate, which tells rest, oscillation and chaos
  apart.

`COLUMN` is any column of the run: a variable, an auxiliary quantity or
`t`.

## Examples

### Rest, oscillation and chaos

The Morris-Lecar model at rest, the same model firing, and the Lorenz
system:

1. [Open the Morris-Lecar model](example:lecar.ode) and
   [take the statistics of V](cmd:mean V).
2. [Raise the current and run longer](cmd:set iapp=0.1 total=200),
   [run](cmd:run) and [take them again](cmd:mean V).

```
mean V
set iapp=0.1 total=200
run
mean V
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.run()
print(m.mean("V"), m.lyapunov())
m.set(iapp=0.1, total=200)
m.run()
print(m.mean("V"), m.lyapunov())
```

The command window answers:

```
V: mean -0.299913, standard deviation 0.172776.   Largest Liapunov exponent of the run: -0.1755
V: mean -0.184298, standard deviation 0.181866.   Largest Liapunov exponent of the run: 0.009812
```

Then the Lorenz system, `x' = 10(y − x)`, `y' = 27x − y − xz`,
`z' = xy − 8z/3`:

```
mean X
```

```python
lorenz = xs.load(xs.example("lorenz.ode"))
lorenz.run()
print(lorenz.mean("X"), lorenz.lyapunov())
```

The command window answers
`X: mean -2.29813, standard deviation 7.41067.   Largest Liapunov exponent of the run: 0.8667`.

How to read it. A negative exponent means that nearby trajectories come
together: the neuron settles to rest (one spike from its start, then
nothing). An exponent near zero means they keep their distance along an
orbit: the neuron fires periodically. Over 2000 time units instead of 200
the exponent falls from 0.0098 to 0.0022, as it should for a limit cycle,
whose exponent is exactly zero. A positive exponent means they separate
exponentially: the Lorenz system is chaotic, and its exponent stays near
0.87 however long the run (0.8707 over 400 time units, 0.8777 over 2000).

The means and deviations describe the run as a sample: the firing neuron
spends most of its time near rest, so its mean `V`, −0.184, is far below
the middle of its spikes.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `COLUMN` | the command; the name in Python | The column for the mean and deviation. | The exponent does not depend on it. |
| **Total time** | `set total=...` | The length of the run, for all three numbers. | An exponent needs a long run: hundreds of periods for a limit cycle. |
| **Transient** | `set trans=...` | The part of the run left out. | Leave out the approach to the attractor. |
| `jac_eps` | `set jac_eps=...` | The distance between the two trajectories of the exponent (0.001). | Smaller, such as 1e-6, comes closer to the exponent of the linearised equations. |

## Results

- **The command window** prints the three numbers.
- **In Python** `m.mean()` returns the mean and the standard deviation as
  a pair, and `m.lyapunov()` the exponent.

## More about

**The standard deviation** is that of the stored points as a population:
the square root of the mean squared difference from the mean, divided by
the number of points, not by one less (`numpy.std` with `ddof=0`).

**The Liapunov exponent.** Two trajectories that start a distance δ apart
are, after a time t, about δ·e^(λt) apart; λ is the largest Liapunov
exponent. The sign is what matters: λ < 0 for a stable equilibrium, λ = 0
for a stable limit cycle (a shift along the orbit neither grows nor
shrinks), λ > 0 for chaos. A finite run only estimates it, so a value
near zero must be judged by whether it shrinks as the run grows.

**XPP Lab's exponent and XPPAUT's.** XPPAUT estimates the exponent from the
stored run, which it keeps in single precision; for long runs or large
values the rounding spoils that estimate. XPP Lab integrates a second,
nearby trajectory alongside the run in double precision instead, with the
model's own method, and renormalises the distance after each output step.
In Python `m.lyapunov(stored=True)` gives XPPAUT's estimate.

## Tips

- Judge an exponent near zero by running longer: for a cycle it shrinks
  towards zero, for weak chaos it stays positive.
- Leave out the transient (`trans`) so that the approach to an attractor
  does not count.
- With an adaptive method, keep `tol` well below `jac_eps`, or the error
  control moves the two trajectories apart by itself.

## Algorithms

The mean and deviation are computed over the stored points. The exponent:
the run is integrated from the initial values for the total time with a
copy moved by `jac_eps`; after each output step the logarithm of the
factor by which their distance grew is added up, and the copy is put back
at the distance `jac_eps` in the direction it moved to. The exponent is the
sum divided by the time (Benettin's method).

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: give a column, e.g. mean V` | No column was named. | Name one; `whos` lists them. |
| `error: no column 'NAME'; type 'whos' for the names` | There is no such column. | Check the name. |
| Only the mean and deviation, no exponent | The model has delays, integrals or noise, for which the exponent is not computed. | The mean and deviation are still right. |

## References

- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapters 9 and 10 (the Lorenz system and Liapunov
  exponents).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [range](range.md): `range ... lyap` takes the exponent at each value of a
  parameter.
- [run](run.md): the run the numbers describe.

## Version history

Introduced in 0.1.0.
