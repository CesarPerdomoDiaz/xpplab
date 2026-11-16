# whos

List the model's variables, parameters and main options, with their values.

## Syntax

```
whos
```

`who` and `list` are other names for `whos`.

- **In the app:** the Workspace shows the same in its **Parameters**,
  **Initial values** and **Numerics** tabs.
- **In Python:** `m.variables`, `m.initial_conditions`, `m.parameters` and
  `m.options`.

## Description

`whos` prints three lines in the command window:

- the variables, with their initial values;
- the parameters, with their values;
- the number of output columns, the method, the total time and the step.

The values are the present ones, after any `set`, `use` or `ic last`, not
the ones in the model file.

## Examples

### What is in the Morris-Lecar model

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [List what it has](cmd:whos).

```
whos
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
print(m.variables)
print(dict(m.initial_conditions))
print(dict(m.parameters))
print(m.options["METH"], m.options["TOTAL"], m.options["DT"])
```

The command window answers:

```
Variables (initial value): V=-0.144 W=0.03
Parameters: iapp=0.05 phi=0.333 v1=-0.01 v2=0.15 v3=0.1 v4=0.145 gca=1 vk=-0.7 vl=-0.5 gk=2 gl=0.5 om=1
Output columns: 7. Method rungekutta, total 30, dt 0.05.
```

How to read it. Two variables, `V` and `W`, and twelve parameters. The
seven output columns are the time, the two variables and the model's four
auxiliary quantities (`cur`, `ica`, `ik`, `rate`), each of which a plot or
an export can show.

### After a change

```
set iapp=0.1
use hopf
whos
```

```python
m.set(iapp=0.1)
m.apply_set("hopf")
print(dict(m.parameters))
```

The parameters line now shows `iapp=0.1` and the four values the set
`hopf` changes: `phi=0.2`, `v3=0`, `v4=0.3` and `gca=1.1`. The set does not
touch `iapp`, so the value just set stays.

## Settings

`whos` has no settings.

## Results

- **The command window** prints the three lines.
- **In Python** `m.variables` is a list of names; `m.initial_conditions`
  and `m.parameters` are live dictionaries (`m.parameters["iapp"] = 0.1`
  sets the value); `m.options` is a dictionary of every numerical option.

## More about

**Names and case.** XPPAUT does not tell capitals from small letters:
`V` and `v` are the same variable, and `set V=-0.2` and `set v=-0.2` do the
same. XPPAUT keeps the variables' names in capitals, which is why `whos`
shows `V` for the `v` of the model file.

**What is not listed.** Fixed quantities and functions of the model file
(`fixed`, `function`) and auxiliary quantities are part of the model's
formulas, not values you set, so `whos` does not list them; the auxiliary
quantities appear among the output columns.

## Tips

- Use `whos` before `set`, to check the exact names.
- `get NAME` shows one value, including every option (`get dt`,
  `get meth`).

## Algorithms

`whos` reads the names and values the engine holds; it computes nothing.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `No model loaded.` | There is no model. | Open one first. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [Commands and shortcuts](../manual/commands.md): `set` and `get`.
- [The ODE file](../manual/odefile.md): where the names come from.

## Version history

Introduced in 0.1.0.
