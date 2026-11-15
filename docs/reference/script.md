# script

Save the session as a Python script that repeats it.

## Syntax

```
script FILE.py
```

- **In the app:** File > **Save session as Python script...**.
- **In Python:** the script is Python: run it with `python FILE.py`.

## Description

Everything done in the app since the model was opened, from the menus,
the buttons or the command line, is recorded as calls to the `xpplab`
package. `script FILE.py` writes that record as a script. Run with Python,
it repeats the session's computations, number for number: the same runs,
the same equilibria, the same diagrams, and the same exported files.

The script starts with a comment saying when it was saved, then loads the
model by its full path and follows the session's actions in order.

## Examples

### From a session to a script

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Set the current](cmd:set iapp=0.1), [run](cmd:run) and
   [find the equilibrium](cmd:eq), then write the run with
   `export run.csv`.
3. Save the session with `script session.py`.

```
set iapp=0.1
run
eq
export run.csv
script session.py
```

The command window says "Saved the session as a Python script:
session.py", and the file reads:

```python
# An XPP Lab session, saved 2026-11-15 15:20.
# Running this script with Python repeats the session's computations,
# number for number. It needs the xpplab package: pip install xpplab

import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
sol = m.run()
m.parameters["iapp"] = 0.1
sol = m.run()
eq = m.equilibrium()
sol.to_csv("run.csv")
```

(The app writes the full paths of the model and of `run.csv`; they are
shortened here.)

How to read it. Each line is one action of the session: opening the model
ran it once (`sol = m.run()`), then the parameter, the second run, the
equilibrium and the export. Run as a script, it finds the same unstable
focus, `V` = 0.0442523, `W` = 0.31671, with eigenvalues
0.32827 ± 1.0303i, and writes the same `run.csv`.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `FILE.py` | the command; the file dialog | Where to write the script. | It is replaced if it exists. |

## Results

- **The script**, and the command window says where it went.

## More about

**Why it repeats exactly.** The app and the package call the same engine
with the same arguments, so they compute the same numbers. Where the app
uses values the package would compute in another precision, such as the
end of a stored run for `ic last`, the script writes the values
themselves. XPP Lab's tests replay sessions this way and compare the
files they export to the last bit.

**What is recorded.** Actions that compute or change something: runs,
parameter and option changes, equilibria, continuation, analyses,
exports. Looking (zooming, moving windows, switching tabs) is not.

## Tips

- Save a script at the end of an exploration: it documents exactly what
  was done, and makes it reproducible.
- Edit the script to sweep a parameter or to make figures in a loop: it
  is ordinary Python.
- [echo](echo.md) shows each line as it is recorded.

## Algorithms

The recorded lines are written in order, after a header; nothing is
computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: cannot write FILE` | The folder does not exist, or cannot be written. | Choose another place. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [echo](echo.md): show the Python as it is recorded.
- [export](export.md): write the numbers themselves.

## Version history

Introduced in 0.1.0.
