# prc

The phase response curves of a stable limit cycle: how much a small kick
at each moment of the cycle advances or delays it.

## Syntax

```
prc
prc TRANSIENT
```

- **In the app:** Run > Analyse the run > **Phase response** (XPPAUT's
  *Numerics > Averaging > Adjoint*).
- **In Python:** `p = m.phase_response()`, with `transient=`; `p.period`
  is the period.

## Description

`prc` settles the model onto its limit cycle (for `TRANSIENT` time units,
by default the total time), measures the period, integrates exactly one
period, and computes XPPAUT's *adjoint* along it. The adjoint, one curve
per variable, is the infinitesimal phase response curve: for a small kick
δ to variable `x` at a point of the cycle, the oscillation is advanced by
about δ times the adjoint's `x` value there, in time units. A negative
value means a delay.

The result is a table with the `phase` (0 to 1 over the period), the time
`T`, and one column per variable. Phase 0 is where the settled run
happened to be, not a particular event such as a spike.

## Examples

### Two neurons, two kinds of response

The Morris-Lecar model with its own parameters (the set `snic`) and
`iapp` = 0.1 fires just past a saddle-node on its cycle; with the set
`hopf` and `iapp` = 0.3 it fires past a Hopf bifurcation. Take both
responses:

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Set the current](cmd:set iapp=0.1) and
   [take the phase response](cmd:prc).
3. [Apply the set hopf](cmd:use hopf), [set iapp = 0.3](cmd:set iapp=0.3)
   and [take it again](cmd:prc).

```
set iapp=0.1
prc
use hopf
set iapp=0.3
prc
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.set(iapp=0.1)
snic = m.phase_response()
h = xs.load(xs.example("lecar.ode"))
h.apply_set("hopf")
h.set(iapp=0.3)
hopf = h.phase_response()
print(snic.period, hopf.period)
```

The command window says "Phase response: 330 points over one period
(16.4745)." Measured from the spike, the response to a kick in `V`:

| | Period | Largest | Smallest | Negative over |
|---|---|---|---|---|
| `snic`, `iapp` = 0.1 | 16.47 | 58.7, at phase 0.66 | −7.2, at 0.10 | 16% of the cycle |
| `hopf`, `iapp` = 0.3 | 15.64 | 25.3, at 0.79 | −12.3, at 0.21 | 50% of the cycle |

How to read it. The first curve is almost everywhere positive: a kick that
depolarises the neuron can only bring the next spike forward, most of all
late in the cycle, as the neuron creeps towards threshold. This is a
*type I* response, typical of firing that begins at a saddle-node. The
second is negative for half the cycle: early in the cycle a kick delays
the next spike. This is a *type II* response, typical of firing that
begins at a Hopf bifurcation. The kind of response decides how such
neurons synchronise when coupled.

### What the numbers mean

The largest value of the first curve is 58.69. A kick of 0.001 to `V` at
that point should advance the next spike by 0.001 × 58.69 = 0.0587 time
units. Simulating the kick gives an advance of 0.0584, and a kick of
−0.001 a delay of 0.0587.

How to read it. The phase response curve is the linear part of the true
response: right for small kicks, a guide for larger ones.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `TRANSIENT` | the command; `transient=` in Python | How long to run before measuring, to settle on the cycle. | The total time by default. Longer for a slowly attracting cycle. |

## Results

- **The Analysis window** draws the curves against the phase.
- **The table** has `phase`, `T` and one column per variable;
  `export FILE.csv analysis` writes it, and `export FILE.csv` the period
  run.
- **In Python** `m.phase_response()` returns the table, with the period in
  `.period`; the period run becomes the last run.

## More about

**Normalisation.** The adjoint Z is normalised so that Z · f = 1 along
the cycle, where f is the right-hand side: then the phase, measured in
time, advances at rate 1, and a kick δx changes it by Z · δx. Here
Z · f stays between 0.9986 and 1.0022 along the cycle.

**Type I and type II.** Near a saddle-node on the cycle the response is
close to 1 − cos of the phase measured from the spike, never negative;
near a Hopf bifurcation it is close to −sin, half negative. These are
the two classes of neural excitability in their phase form.

**Weak coupling.** For two oscillators weakly coupled, the phase response
curve, averaged against the coupling, gives the interaction function H
whose zeros are the phase-locked states: see `hfun`.

## Tips

- Check that the run settles: a period that is not steady gives a wrong
  adjoint.
- For a model with many variables, look at the variable the kicks act on:
  a current acts on the voltage.

## Algorithms

The cycle's period is measured from the upward crossings of the first
variable through the middle of its range, one period is integrated from a
point on the cycle, and XPPAUT's adjoint routine computes the periodic
solution of the adjoint linear equations along it, normalised so that
Z · f = 1.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: no steady oscillation of V found. Check that the model oscillates, or give a longer transient.` | The model rests at these parameters (as Morris-Lecar does at its own `iapp`, 0.05), or the run has not settled. | Check that it oscillates; raise `TRANSIENT`. In Python the messages are `no oscillation found` and `the oscillation is not periodic yet`. |

## References

- Ermentrout, B. and Terman, D. H. (2010). *Mathematical Foundations of
  Neuroscience*. Springer.
- Izhikevich, E. M. (2007). *Dynamical Systems in Neuroscience*. MIT Press.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [mean](mean.md): whether the run is periodic.
- [auto from](auto-from.md): the cycle's branch, with its stability.

## Version history

Introduced in 0.1.0.
