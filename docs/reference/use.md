# use

Apply one of the model's named parameter sets.

## Syntax

```
use SET
```

- **In the app:** Workspace > **Sets**, and click the set.
- **In Python:** `m.apply_set("hopf")`.

## Description

A model file can name groups of values with `set NAME {...}`: parameters,
initial values and options such as `total`. `use SET` gives the model all
the values of `SET` at once. Values the set does not name stay as they are.

Sets keep a model's interesting regimes together in its file: the values
of a figure in a paper, a bistable case, a chaotic one. [sets](sets.md)
lists the sets a model has.

## Examples

### Two ways for a neuron to start firing

The Morris-Lecar model comes with the sets `snic` and `hopf`, which change
four parameters (`v3`, `v4`, `gca`, `phi`). Its own values are those of
`snic`. Compare the equilibria with each:

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Apply the set snic](cmd:use snic) and [find every equilibrium](cmd:eq all).
3. [Apply the set hopf](cmd:use hopf) and [find them again](cmd:eq all).

```
use snic
eq all
use hopf
eq all
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
box = {"V": (-0.6, 1.2), "W": (-0.25, 1.2)}
m.apply_set("snic")
snic = m.find_equilibria(box=box)
m.apply_set("hopf")
hopf = m.find_equilibria(box=box)
```

With `snic`, at `iapp` = 0.05, there are three equilibria: a stable node
at `V` = −0.3825, a saddle at −0.1425 and an unstable focus at 0.0276. With
`hopf` there is one, a stable focus at `V` = −0.4417.

How to read it. The two sets are the two textbook routes to repetitive
firing. With `snic` the rest state meets the saddle and both vanish as the
current rises (a saddle-node on an invariant circle), so firing starts at
an arbitrarily low rate. With `hopf` the single rest state loses its
stability in a Hopf bifurcation, and firing starts at a definite rate.
[auto steady](auto-steady.md) follows each as `iapp` changes.

### A set with options

The set `vvst` holds `total=100` and `iapp=0.1` as well as plot settings:

```
use vvst
get total
get iapp
```

```python
m.apply_set("vvst")
print(m.get("total"), m.get("iapp"))
```

After it `total` is 100 and `iapp` 0.1: a set changes options as readily
as parameters.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `SET` | the command; the name in Python | The set to apply, as the model file names it. | `sets` lists the names. |

## Results

- **The values** of the set are the model's. The Workspace shows them, and
  the command window says "Applied set SET."
- **In Python** `m.apply_set()` returns the model.

## More about

**Writing a set.** In the model file:

```
set hopf {v3=0,v4=.3,gca=1.1,phi=.2}
```

Any parameter, variable (its initial value) or option can appear, separated
by commas. [The ODE file](../manual/odefile.md) describes the syntax.

**Sets do not undo.** A set only gives values; it does not remember the
ones it replaced. To go back, apply a set that holds the old values, as
`snic` does for the Morris-Lecar model, or reopen the model.

## Tips

- Name a set after what it shows, not after a figure number: `bistable`,
  `chaos`, `hopf`.
- Put the model's own values in a set too, so that you can return to them.

## Algorithms

XPPAUT's own reading of a set's values, as when the model file is read.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: no set named NAME` | The model has no set of that name. | `sets` lists them. |
| `error: write use SET, with one of: ...` | `use` without a name. | Add the set's name. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.
- Izhikevich, E. M. (2007). *Dynamical Systems in Neuroscience*. MIT Press.

## See also

- [sets](sets.md): the sets a model has.
- [eq all](eq-all.md): the equilibria in the example.
- [whos](whos.md): the values after a set.

## Version history

Introduced in 0.1.0.
