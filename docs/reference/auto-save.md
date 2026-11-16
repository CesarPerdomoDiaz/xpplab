# auto save

Save the bifurcation diagram to a file, with what AUTO needs to continue
from its labelled points later.

## Syntax

```
auto save FILE.auto
```

- **In the app:** Bifurcation window > **Save...**.
- **In Python:** `m.auto.save("FILE.auto")`.

## Description

`auto save FILE.auto` writes everything in the Bifurcation window to one
file: every point of every branch, AUTO's settings, and the solution AUTO
stored at each labelled point. [auto load](auto-load.md) brings it back in
a later session, and the labels can then be continued from as if the
diagram had just been computed.

A diagram can take minutes to compute and is lost when the model is closed
or changed. Save it when it is worth keeping.

To take the *numbers* of a diagram to another program, use
`export FILE.csv auto` instead: that writes a table of the points, which
cannot be loaded back.

## Examples

### Save a diagram, and go on from it another day

The steady states of the Morris-Lecar model with the set `hopf`:

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Apply the set hopf](cmd:use hopf) and
   [follow the steady states](cmd:auto steady iapp).
3. Save the diagram: `auto save hopf.auto`.

```
use hopf
auto steady iapp
auto save hopf.auto
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.apply_set("hopf")
d = m.auto.equilibria("iapp")
m.auto.save("hopf.auto")
```

The command window answers "Saved the bifurcation diagram (49 points) to"
and the file's full path.

How to read it. In the app a name without a folder is saved in the folder
of the model; in Python, in the working folder. For a model that came with
XPP Lab, give a folder of your own, since the examples' folder may not be
writable. The file holds the 49 points, the two Hopf points with their labels
2 and 3, and the settings in use. [auto load](auto-load.md) shows the other
half: loading it and computing the periodic orbits from label 2.

## Settings

There are none. The file records AUTO's settings as they are when it is
saved.

## Results

- **One file**, `FILE.auto`. For the example above it is about 25 kB.
- **The command window** prints the number of points and the full path.
- The diagram on screen is not changed.

## More about

**What is in the file.** Four things, as in the files XPPAUT's own *AUTO >
File > Save diagram* writes: the points of the diagram as drawn, the axes
of the plot, AUTO's numerical settings, and AUTO's restart data, which is
the complete solution at each labelled point (for a periodic orbit, the
whole orbit on its mesh). Without the restart data a diagram could be drawn but not
continued.

**Which model.** The last line of the file names the model's variables
and the parameters AUTO used:
`# XPP Lab model: variables V W; parameters iapp phi v1 v2 v3 v4 gca vk`.
[auto load](auto-load.md) refuses a file whose line does not match the
open model. The model itself is not in the file, nor the values of the
parameters that were not varied.

## Tips

- Save the model's file next to the diagram, under a matching name. The
  diagram is only meaningful with the model and the parameter set it came
  from.
- Save before an experiment that may clutter the diagram, such as a branch
  of orbits with **Max steps** too high. Loading the file again is quicker
  than clearing and recomputing.
- Files written here open in XPPAUT, and XPPAUT's open here.

## Algorithms

The file is XPPAUT's diagram file, written by XPPAUT's own routine, with
AUTO's restart file appended and then the line that names the model.
XPPAUT reads the file as before: it copies the end of the file into its
restart file, whose reader stops at the first line that is not a record.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: the diagram is empty` | Nothing has been computed yet. | Compute a branch first. |
| `Write: auto save FILE.auto` | No file name was given. | Add the name. |
| `error: cannot write FILE` | The folder does not exist or cannot be written. | Give a path in a folder you can write to. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [auto load](auto-load.md): bringing a saved diagram back.
- [auto steady](auto-steady.md) and [auto from](auto-from.md): computing
  what is saved.
- [Commands and shortcuts](../manual/commands.md): `export FILE.csv auto`,
  for a table of the points.

## Version history

Introduced in 0.1.2.

0.1.4: the file ends with a line naming the model's variables and AUTO's
parameters, which `auto load` checks.
