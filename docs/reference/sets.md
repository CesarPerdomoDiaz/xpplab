# sets

List the model's named parameter sets.

## Syntax

```
sets
```

- **In the app:** Workspace > **Sets**.
- **In Python:** `m.sets`.

## Description

`sets` prints the name of every `set NAME {...}` in the model file, one per
line, in the file's order. [use](use.md) applies one.

## Examples

### The sets of the Morris-Lecar model

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [List its sets](cmd:sets).

```
sets
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
print(m.sets)
```

The command window answers:

```
  vvst
  pp
  snic
  hopf
  hom
  cool
```

How to read it. Six sets. `vvst` and `pp` set up plots (voltage against
time, and the phase plane); `snic`, `hopf` and `hom` are three regimes of
the neuron; `cool` is `hom` with a particular current, `iapp` = 0.080918.
The model file shows what each holds.

## Settings

`sets` has no settings.

## Results

- **The command window** lists the names, or says "No parameter sets in this
  model."
- **In Python** `m.sets` is a list of the names.

## More about

**Where sets come from.** Only from the model file: a line
`set NAME {name=value, ...}`. [use](use.md) shows one being written.

## Tips

- Read the model file next to the list: the sets' names rarely say all they
  hold.

## Algorithms

`sets` lists the names XPPAUT read from the model file; it computes
nothing.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `No parameter sets in this model.` | The model file has no `set` lines. | Add one if you need it; see [use](use.md). |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [use](use.md): apply a set.

## Version history

Introduced in 0.1.0.
