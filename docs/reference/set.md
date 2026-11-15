# set

Give parameters, initial values or numerical options new values.

## Syntax

```
set NAME=VALUE
set NAME=VALUE NAME=VALUE ...
```

The pairs may be separated by spaces, commas or semicolons; there is no
space around `=`.

- **In the app:** the Workspace's **Parameters**, **Initial values** and
  **Numerics** tabs, and the sliders.
- **In Python:** `m.set(iapp=0.1, total=100, meth="stiff")`.

## Description

`set` changes each `NAME` to its `VALUE`, in order:

- a **parameter** gets the value;
- a **variable** gets it as its initial value;
- a **numerical option**, such as `total`, `dt`, `meth`, `trans`, `bound`
  or `maxstor`, is set as by an `@` line in the model file.

Names ignore case. The values of parameters, variables and numerical
options must be numbers, such as `0.1`, `-2` or `1e-3`, not formulas: a
value that is not a number is refused and the old one kept. `meth` takes a
method's name: `rungekutta`, `stiff`, `cvode`, `5dp` and so on.

A changed value is used by the next computation; with **Live** on in the
app, the model also runs again at once.

## Examples

### A longer run with another method

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Set the current, the run length and the method](cmd:set iapp=0.1 total=100 meth=stiff),
   then [run](cmd:run).

```
set iapp=0.1 total=100 meth=stiff
run
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.set(iapp=0.1, total=100, meth="stiff")
sol = m.run()
print(len(sol), sol.t[-1], m.get("METH"))
```

The run has 2001 points to `t` = 100, one every `dt` = 0.05, and `whos`
now ends "Method stiff, total 100, dt 0.05."

How to read it. One `set` changed a parameter and two options; they all
apply to the next run. The method changes how the run is computed, not
how often it is stored: the points still come every `dt`.

### Initial values

```
set v=-0.2 w=0.01
run
```

```python
m.set(V=-0.2, W=0.01)
sol = m.run()
print(sol["V"][0], sol["W"][0])
```

The run starts at `V` = −0.2, `W` = 0.01, stored in single precision as
−0.2 and 0.0099999998.

How to read it. A variable's name sets its initial value; the run's first
point is that value. Names ignore case: `v` and `V` are the same.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `NAME` | the command; keyword arguments in Python | A parameter, a variable or a numerical option. | `whos` lists the parameters and variables. |
| `VALUE` | the command; the value in Python | A number, or for `meth` a method's name. | No formulas: compute the number first. |

## Results

- **The values** change; the Workspace shows them.
- **In Python** `m.set()` returns the model, so `m.set(iapp=0.1).run()`
  works.

## More about

**Methods.** XPPAUT recognises a method by the first letter of its name,
so `meth=s` is `stiff`; a value whose first letter names no method is
refused. The methods are `discrete`, `euler`, `modeuler`, `rungekutta`,
`adams`, `gear`, `volterra`, `backeuler`, `qualrk`, `stiff`, `cvode`,
`5dp`, `83dp`, `2rb` and `ymp`.

**Other options.** Names that are not parameters, variables or options
the app knows are passed to XPPAUT, which takes options of its own (plot
settings, AUTO's settings) and ignores the rest; the command window notes
when a name is not one the app knows.

## Tips

- Check a value with [get](get.md) after setting it.
- Use [use](use.md) for a whole named set of values at once.
- For a sweep over a parameter, [range](range.md) does the setting and
  running for you.

## Algorithms

Parameters and initial values are set in the engine; options go through
XPPAUT's own reading of `@` lines.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: NAME=VALUE: the value must be a number (write 0.1, 1e-3, -2; not a formula)` | A parameter's or variable's value is not a number. | Write the number. |
| `error: total=abc: the value must be a number` | An option's value is not a number. | Write the number. |
| `error: no method called NAME; the methods are ...` | No method starts with that letter. | Use a method's name. |
| `error: write set NAME=VALUE (got 'NAME')` | The `=` or the value is missing. | Write `NAME=VALUE`, without spaces. |
| `note: NAME is not a parameter, variable or option the app knows; XPPAUT may ignore it` | A misspelt name, or an option the app does not list. | Check the name with `whos`. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [get](get.md): read a value.
- [whos](whos.md): every parameter and variable.
- [use](use.md): a named set of values.

## Version history

Introduced in 0.1.0. Values that are not numbers are refused since 0.1.4.
