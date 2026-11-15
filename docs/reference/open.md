# open

Open a model file.

## Syntax

```
open FILE.ode
```

- **In the app:** File > **Open...** (Ctrl+O; ⌘O on a Mac), the Models
  window, or a file dropped on the window.
- **In Python:** `m = xs.load("FILE.ode")`.

## Description

`open FILE.ode` reads the model, replaces the one that was open, and runs
it once, so that the plots show it at once. The app's working folder
becomes the model's folder: files the model reads (tables, data) and
relative names in later commands (`export run.csv`, `ic write`) are found
there.

A relative name is looked for first in the present working folder, which
is the folder of the model already open.

## Examples

### From one model to another

With the Morris-Lecar model open (it ships in the same folder as the
Lorenz system):

1. [Open the Morris-Lecar model](example:lecar.ode).
2. Open the Lorenz system with `open lorenz.ode`, and list it with `whos`.

```
open lorenz.ode
whos
```

```python
import xpplab as xs

m = xs.load(xs.example("lorenz.ode"))
print(m.variables, dict(m.parameters))
```

The command window answers:

```
Loaded lorenz.ode: 3 variables, 3 parameters.
Variables (initial value): X=-7.5 Y=-3.6 Z=30
Parameters: r=27 s=10 b=2.66666
Output columns: 4. Method rungekutta, total 40, dt 0.025.
```

How to read it. The name was found because the folder of the model open
before, XPPAUT's examples, holds `lorenz.ode` too. The parameters and
options are the new model's own, from its file.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `FILE.ode` | the command; the path in Python | The model file. | A full path, or a name relative to the open model's folder. |

## Results

- **The model** is loaded and run once; every window shows it.
- **The command window** gives the numbers of variables and parameters.
- **In Python** `xs.load()` returns a `Model`; it does not run it.

## More about

**What is kept.** Nothing of the old model: its runs, kept curves,
equilibria and diagram go with it. The Python record that
[script](script.md) saves starts again with the new `xs.load`.

## Tips

- The Models window lists XPPAUT's examples and the classics, with their
  descriptions.
- Errors in a model file are reported with the line; open it in the
  editor to fix them.

## Algorithms

XPPAUT's own reader parses the file, in a fresh copy of the engine.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: cannot read FILE` | No such file, or it cannot be read. | Check the name, and the folder it is relative to. |
| An error with a line number | The file has a mistake. | Fix that line; [The ODE file](../manual/odefile.md) describes the format. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [new](new.md): a new model from a template.
- [save](save.md): save the model being edited.

## Version history

Introduced in 0.1.0.
