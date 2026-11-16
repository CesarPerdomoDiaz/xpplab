# py

Run Python in the app's command window, with the model open there already
defined.

## Syntax

```
py CODE
py
```

- **In the app:** click the command window's `>>` prompt, which becomes
  `py>`; click it again, or type `:cmd`, to go back to commands.
- **In Python:** the same lines run in a notebook or a script after
  `m = xs.app.connect().model()`, or on a model of your own after
  `m = xs.load(...)`.

## Description

`py CODE` runs one line of Python in the app's console. `py` alone turns
the command window into a Python prompt, `py>`, until `:cmd` or a click on
the prompt turns it back.

The Python runs in a process of its own, started by the first line, with
these names defined:

- `m`, the model open in the app. `m.parameters`, `m.initial_conditions`,
  `m.set()`, `m.get()` and `m.run()` act in the app, so its sliders and
  plots follow, and the lines the app writes for each action run there
  unchanged. Other methods (`m.equilibrium()`, `m.sweep()`, `m.auto`, ...)
  compute with a private copy of the model at the app's current values.
- `app`, the app itself: `app.command("auto steady iapp")` runs any
  command.
- `xs` and `np`: the xpplab package and NumPy.

A statement over several lines (`for`, `if`, `def`) shows `...` until an
empty line, or `py` alone, ends it. The console's Python is chosen with
[python](python.md).

## Examples

### Compute in the app's console

1. [Open the Morris-Lecar model](example:lecar.ode).
2. Set the current and run longer, from Python:
   `py m.parameters["iapp"] = 0.1`, then `py sol = m.run(total=200)`.
3. Ask for the largest voltage and the equilibrium's kind:
   `py np.max(sol["V"])` and `py eq = m.equilibrium(); eq.kind`.

```
py m.parameters["iapp"] = 0.1
py sol = m.run(total=200)
py np.max(sol["V"])
py eq = m.equilibrium(); eq.kind
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.parameters["iapp"] = 0.1
sol = m.run(total=200)
print(np.max(sol["V"]))
eq = m.equilibrium()
print(eq.kind)
```

The command window shows:

```
py> m.parameters["iapp"] = 0.1
py> sol = m.run(total=200)
py> np.max(sol["V"])
np.float64(0.26150020956993103)
py> eq = m.equilibrium(); eq.kind
'unstable focus'
```

and the app's Workspace shows `iapp` = 0.1 and a total time of 200: the
console's `m` is the model on the screen.

How to read it. The peak of the spikes is 0.2615, and the equilibrium
inside the cycle is an unstable focus: the oscillation surrounds a
repelling rest state. The same lines in a script on a model of its own
give the same numbers, bit for bit.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `CODE` | the command | One line of Python. | Several statements go on one line with `;`. |

## Results

- **The command window** shows each line after `py>`, and its value or
  output; errors in red.
- **The Workspace > Python tab** lists the console's variables with their
  types and values.
- **The app** follows what `m` does: its values, its runs and its plots.

## More about

**A process of its own.** An endless loop or a crash in your code never
stops the app: **Stop** beside the command window ends the Python, whose
variables are then lost, and the next line starts a fresh one.

**The session's script.** What `m` does in the app is written into the
session's Python script (see [script](script.md)), as the app's own
actions are.

## Tips

- Use `py` alone for several lines; Tab completes names.
- Draw with matplotlib from the console: `import matplotlib.pyplot as
  plt`, then `plt.plot(sol.t, sol["V"]); plt.show()`.

## Algorithms

Each line is sent to a Python process started with the app's copy of the
xpplab package; `m` and `app` talk to the app through the same channel as
[serve](serve.md).

## When it fails

| Message | Why | What to do |
|---|---|---|
| An error in red, with Python's traceback | The line raised. | Read the last line of the traceback. |
| The command window offers **Install Python** | No Python with NumPy was found. | `python fetch`, or `python use PATH`; see [python](python.md). |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [python](python.md): choose, stop or install the console's Python.
- [serve](serve.md): let a notebook or a script drive the app.
- [echo](echo.md): see each action as Python.

## Version history

Introduced in 0.1.0.
