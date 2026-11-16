# auto load

Bring back a bifurcation diagram saved with `auto save`, ready to be
continued from its labelled points.

## Syntax

```
auto load FILE.auto
```

- **In the app:** Bifurcation window > **Open...**.
- **In Python:** `d = m.auto.load("FILE.auto")`.

## Description

`auto load FILE.auto` replaces the diagram in the Bifurcation window with
the one in the file, and sets AUTO's settings to those the file was saved
with. The labels of the loaded diagram work as if it had just been
computed: [auto from](auto-from.md) continues from them, and
[auto orbit](auto-orbit.md) shows their orbits.

Open the model the diagram was computed from first, with the same
parameter set. The file holds the diagram, not the model, but it names the
model's variables and AUTO's parameters, and a file whose names do not
match the open model is refused: the diagram shown stays.

Files saved by XPPAUT's *AUTO > File > Save diagram*, and by XPP Lab
before 0.1.4, load too. They do not name the model, so they are not
checked.

## Examples

### Load a diagram and compute the orbits from its Hopf point

[auto save](auto-save.md) saved the steady states of the Morris-Lecar model
with the set `hopf` as `hopf.auto`. In a new session:

```
use hopf
auto load hopf.auto
auto set nmax=50
auto from 2 periodic
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.apply_set("hopf")
d = m.auto.load("hopf.auto")
orbits = m.auto.continue_from(d.first("HB"), "periodic", nmax=50)
```

The command window answers "Loaded a bifurcation diagram of 49 points from"
the file, "its labelled points can be continued", and the continuation then
finds the folds of periodic orbits at `iapp` = 0.2484 and 0.4657.

How to read it. Those are the folds that [auto from](auto-from.md) finds
when the steady states are computed in the same session: nothing was lost
in the file. The labels are the same too, 2 and 3 for the Hopf points and 5
and 6 for the new folds.

## Settings

There are none. Loading *sets* AUTO's settings (the range, the steps,
**Max steps** and the rest) to the values in the file.

## Results

- **The diagram** in the Bifurcation window is replaced, and the window
  comes forward.
- **AUTO's settings** take the values of the file.
- **The command window** prints the number of points loaded.
- **In Python** the call returns the loaded `Diagram`.

## More about

**Why the model must match.** A labelled point in the file is a solution
of one model: the values of its variables, and for an orbit the whole curve
on its mesh. AUTO restarts from those numbers with the equations of the
model that is open *now*. If that is another model, or the same model with
other parameter values, the numbers are not a solution of it, and the
continuation fails or follows something meaningless.

**What is checked.** The file's last line names the variables and the
parameters AUTO used (see [auto save](auto-save.md#more-about)). The
variables must be the open model's, in the same order, and each of AUTO's
parameters must be at the same place in the model's list of parameters.
Two models with the variables `V` and `W`, such as `lecar.ode` and
`fhn.ode`, differ in their parameters. The values are not checked: the
same model with another parameter set loads, and is the reader's to get
right.

## Tips

- Keep the model's file and its diagram together, and note the parameter
  set in the name: `lecar-hopf.auto`.
- After loading, check one thing you know, such as the position of a
  special point, before building on the diagram.
- To combine two saved diagrams, load one and compute the other again:
  loading always replaces what is there.

## Algorithms

XPPAUT's own routine for loading a diagram reads the file: the diagram so
far is forgotten, as by [auto reset](auto-reset.md), then the file's
settings, points and restart data take its place.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: cannot read FILE` | There is no such file, or it cannot be opened. | Check the name and the folder. In the app a name without a folder is looked for in the folder of the model; in Python, in the working folder. |
| `Write: auto load FILE.auto` | No file name was given. | Add the name. |
| `error: FILE was not loaded: it is a diagram of a model with the variables V W and the parameters iapp ...; this model has the variables ...` | The file was saved from another model. | Open the model the line names, and load again. |
| The continuation from a loaded label stops after a few points, marked MX | The model is the right one with other parameter values, or the file was saved by XPPAUT or an older XPP Lab, which do not name the model. | Open the right model, with the right parameter set, and load again. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [auto save](auto-save.md): writing the file.
- [auto from](auto-from.md) and [auto orbit](auto-orbit.md): what to do
  with a loaded diagram.
- [auto reset](auto-reset.md): clearing the diagram without loading one.

## Version history

Introduced in 0.1.2.

0.1.4: a file saved from another model is refused, with its variables and
parameters named.
