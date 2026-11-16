# ic read

Read the initial values from a text file.

## Syntax

```
ic read FILE
```

- **In the app:** Workspace > **Initial values** > **From file...**
  (XPPAUT's *Initial conds > File*).
- **In Python:** `m.read_ics("state.txt")`.

## Description

`ic read FILE` reads numbers from `FILE` and makes them the initial values,
the first number for the first variable, the second for the second, and so
on, in the order `whos` lists the variables. The numbers may be separated by
spaces, new lines, commas or semicolons; a file that [ic write](ic-write.md)
wrote is read back exactly.

If the file holds fewer numbers than the model has variables, nothing
changes and the command says so. Numbers after the last variable's are
ignored.

## Examples

### Back to a saved state

Write the initial values of a firing neuron to a file, change them, and
read them back:

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Set the current](cmd:set iapp=0.1), [run](cmd:run) and
   [start from the end](cmd:ic last), then `ic write firing.txt`.
3. Change the values, [set v=0 w=0](cmd:set v=0 w=0), and read them back
   with `ic read firing.txt`.

```
set iapp=0.1
run
ic last
ic write firing.txt
set v=0 w=0
ic read firing.txt
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.set(iapp=0.1)
m.run()
m.ic_last()
m.write_ics("firing.txt")
m.set(V=0, W=0)
m.read_ics("firing.txt")
```

After `ic read`, `get v` answers −0.2384222746 again, and `get w`
0.007057134062. In Python `m.ics["V"]` is −0.23842227676 again: Python's
`ic_last()` keeps the full precision (see [ic last](ic-last.md)), and the
file keeps whichever it was given.

How to read it. The values are those of the file to the last digit: a
write and a read change nothing, so a saved state can be come back to at
any time, in this session or another.

### A file written by hand

A file need not come from `ic write`. One line, with a comma:

```
-0.3, 0.01
```

```python
from pathlib import Path

Path("rest.txt").write_text("-0.3, 0.01\n")
m.read_ics("rest.txt")
print(dict(m.ics))
```

Saved as `rest.txt` and read with `ic read rest.txt`, it sets `V` to −0.3
and `W` to 0.01.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `FILE` | the command; the path in Python | The file to read. | In the app a relative name is relative to the model's folder; in Python, to the working folder. |

## Results

- **The initial values** are the file's numbers. The **Initial values** tab
  shows them, and the command window says "Initial values read from FILE."
- **In Python** `m.read_ics()` returns the model.

## More about

**What is read.** The numbers are read as XPPAUT reads them, with C's
`%lg`: `0.1`, `-2`, `1e-3` and `1.5E+2` all work. Reading stops at the
first word that is not a number, so a file must not begin with a heading.

**The order.** The numbers go to the variables in the order of the model
file. A file written for another model fits only if that model has the
same variables in the same order.

## Tips

- Keep the files of a project next to its model file, named after what
  they hold: `rest.txt`, `firing.txt`, `threshold.txt`.
- To read a state into a model with more variables, write the extra
  numbers into the file by hand, in the order `whos` shows.

## Algorithms

XPPAUT's *Initial conds > File*: the numbers are read in order with
`fscanf`, one per variable.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: FILE holds 1 number; the model has 2 variables` | The file has fewer numbers than the model has variables, or something that is not a number comes first. | Check the file: one number per variable, and no heading. Nothing was changed. |
| `error: cannot open FILE` | There is no such file, or it cannot be read. | Check the name and the folder. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [ic write](ic-write.md): write the file.
- [ic last](ic-last.md): take the end of the last run as the initial
  values.
- [whos](whos.md): the order of the variables.

## Version history

Introduced in 0.1.0.
