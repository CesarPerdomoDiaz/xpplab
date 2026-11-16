# echo

Show each action as the Python that repeats it, in the command window.

## Syntax

```
echo on
echo off
```

- **In the app:** View > **Show actions as Python**.
- **In Python:** the lines shown are the package's calls; type them in a
  script or the console.

## Description

With `echo on`, each action, from a menu, a button or the command line, is
followed in the command window by its Python, after `>>>`: the same lines
that [script](script.md) saves. `echo off` stops showing them; they are
still recorded.

It is a way to learn the package: do something in the app, and read how to
do it in Python.

## Examples

### Watching a session become Python

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Show actions as Python](cmd:echo on), then
   [set the current](cmd:set iapp=0.1), [run](cmd:run) and
   [find the equilibrium](cmd:eq).

```
echo on
set iapp=0.1
run
eq
```

The command window shows:

```
>> echo on
Actions are shown as Python in the command window.
>> set iapp=0.1
>> run
>>> m.parameters["iapp"] = 0.1
>>> sol = m.run()
>> eq
Equilibrium (unstable focus): V=0.0442523 W=0.31671
  eigenvalues: 0.32827+1.0303i 0.32827-1.0303i
>>> eq = m.equilibrium()
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.parameters["iapp"] = 0.1
sol = m.run()
eq = m.equilibrium()
print(eq.kind, eq["V"], eq["W"])
```

How to read it. The parameter's line appears with the run, not with the
`set`: changes of values are written when the next computation uses
them, so `set iapp=0.1`, `set iapp=0.2` and `set iapp=0.15` before a run
give one line, `m.parameters["iapp"] = 0.15`. The Python block above is what those lines do, and it finds the
same equilibrium.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `on`, `off` | the command; View > Show actions as Python | Whether the lines are shown. | On while learning the package. |

## Results

- **The command window** shows the Python lines after `>>>`, or stops
  showing them, and says which.

## More about

**Shown and recorded.** The record that [script](script.md) saves is kept
either way; `echo` only decides whether you see it as it grows.

## Tips

- Copy lines from the command window into a script or the Python
  console.
- Turn it off for long sessions, to keep the command window short.

## Algorithms

The lines are those of the session's record, shown as they are added.

## When it fails

`echo` does not fail.

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [script](script.md): save the lines as a script.

## Version history

Introduced in 0.1.0.
