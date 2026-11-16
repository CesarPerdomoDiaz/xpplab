# hfun

The interaction function H of two weakly coupled copies of an oscillator:
which phase differences they lock at.

## Syntax

```
hfun F1; F2; ...
```

- **In the app:** Run > Analyse the run > **H function** (XPPAUT's
  *Numerics > Averaging > Make H*).
- **In Python:** after `m.phase_response()`,
  `h = m.h_function({"V": "V'-V", "W": "0"})`.

## Description

Two identical oscillators, each receiving a weak coupling term from the
other, keep their shape and change only their timing. `hfun` gives the
averaged effect: H(φ), the rate at which one oscillator's phase is pushed
when the other is φ ahead of it.

The formulas `F1; F2; ...`, one per variable in the order of `whos`, are
the coupling into each variable; a primed name is the other oscillator's
variable, so `V'-V` is diffusive coupling through `V`, as by a gap
junction. A variable that receives nothing gets `0`.

`hfun` needs the phase response of the cycle: it computes it first, as
[prc](prc.md) does. The result is a table with `phi` (the phase
difference, in time units, over one period), `H`, and its odd and even
parts `Hodd` = ½(H(φ) − H(−φ)) and `Heven` = ½(H(φ) + H(−φ)), which
XPPAUT forms by pairing each stored point with its mirror image in the
period.

## Examples

### Gap junctions synchronise the neurons

1. [Open the Morris-Lecar model](example:lecar.ode) and
   [set iapp = 0.1](cmd:set iapp=0.1).
2. [Compute H for coupling through V](cmd:hfun V'-V; 0).

```
set iapp=0.1
hfun V'-V; 0
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.set(iapp=0.1)
m.phase_response()
h = m.h_function({"V": "V'-V", "W": "0"})
crossings = np.where(np.diff(np.sign(h["Hodd"])) != 0)[0]
print(h["phi"][crossings])
```

The command window says "H function: 330 points over one period
(16.4745)." `Hodd` is zero at φ = 0, rising there with slope 0.699, and
crosses zero again, falling, at φ = 8.2, half the period of 16.47.

How to read it. For two coupled cells the phase difference φ changes as
φ′ = −2ε Hodd(φ), with ε the coupling strength. The zeros of `Hodd` are the
locked states: in phase (φ = 0) and in antiphase (half a period). Where
`Hodd` rises through zero the difference is pulled back to the zero,
which is stable; where it falls, pushed away. So weak gap-junction
coupling brings two of these neurons into synchrony, and antiphase is
unstable.

### The other kind of neuron

With the set `hopf` at `iapp` = 0.3 the same coupling gives a `Hodd` that
rises through zero at φ = 0 with slope 0.378 and falls through zero near
7.8, half its period of 15.64:

```python
n = xs.load(xs.example("lecar.ode"))
n.apply_set("hopf")
n.set(iapp=0.3)
n.phase_response()
hh = n.h_function({"V": "V'-V", "W": "0"})
```

How to read it. Synchrony is stable here too, but the slope at φ = 0 is
about half as steep: the same weak coupling pulls these neurons together
about half as fast.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `F1; F2; ...` | the command; a dict or a list in Python | The coupling into each variable; primed names are the other cell's. | `V'-V` for diffusive coupling; `0` for a variable that receives nothing. |
| The transient of [prc](prc.md) | as for `prc` | The settling onto the cycle. | As for `prc`. |

## Results

- **The Analysis window** draws H, Hodd and Heven against φ.
- **The table** has `phi`, `H`, `Hodd` and `Heven`; `export FILE.csv
  analysis` writes it.
- **In Python** `m.h_function()` returns the table.

## More about

**Averaging.** For weak coupling ε, each oscillator's phase θ obeys
θ′ = 1 + ε H(θ_other − θ), where H(φ) = (1/T) ∫ Z(t) · G(X(t), X(t + φ)) dt
averages the coupling G against the phase response Z over one period T.
Two cells give φ′ = ε (H(−φ) − H(φ)) = −2ε Hodd(φ).

**Stability.** A locked state φ* is stable when Hodd′(φ*) > 0; the larger
the slope, the faster the pair locks.

## Tips

- Check the phase response first with [prc](prc.md): H is only as good as
  the cycle and its adjoint.
- Try other couplings, such as a synapse that acts only while the other
  cell fires, and see which locked states become stable: the coupling, as
  much as the cell, decides.

## Algorithms

XPPAUT's Make H: the integral of the adjoint against the coupling, taken
over the stored period for each phase difference on the grid of stored
points.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: no steady oscillation of V found. ...` | There is no stable cycle to average over. | As for [prc](prc.md). |
| `note: N of M coupling formulas given; the rest are kept` | Fewer formulas than variables. | Give one per variable. |

## References

- Ermentrout, B. and Terman, D. H. (2010). *Mathematical Foundations of
  Neuroscience*. Springer.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [prc](prc.md): the phase response the averaging uses.

## Version history

Introduced in 0.1.0.
