# auto reset

Clear the bifurcation diagram.

## Syntax

```
auto reset
```

- **In the app:** Bifurcation window > **Clear diagram**.
- **In Python:** `m.auto.reset()`.

## Description

`auto reset` removes every branch from the Bifurcation window and forgets
the labelled points. AUTO's settings stay as they are, and so do the model
and its run.

Each continuation *adds* to the diagram. That is what builds a diagram
step by step, steady states and then orbits and then curves in two
parameters. It also means that a run made with the wrong settings stays on
the plot until the diagram is cleared.

## Examples

### Try a setting again from a clean diagram

The steady states of the Morris-Lecar model with the set `hopf`, first with
the range cut short, then with the whole range:

```
use hopf
auto set parmax=0.3
auto steady iapp
auto reset
auto set parmax=0.5
auto steady iapp
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.apply_set("hopf")
m.auto.equilibria("iapp", parmax=0.3)
m.auto.reset()
d = m.auto.equilibria("iapp", parmax=0.5)
```

After `auto reset` the command window says "AUTO: diagram cleared." The
second run then draws 49 points, with the Hopf points labelled 2 and 3.

How to read it. Without the reset the diagram would hold both runs, 73
points, the second drawn over the first. The labels would repeat too: each
`auto steady` numbers its points from 1, so there would be two points
labelled 2, the Hopf point of each run. [auto from](auto-from.md) takes a
label, and with two of the same it uses the later one. A clean diagram has
one point for each label.

## Settings

There are none.

## Results

- **The Bifurcation window** is empty, in both its tabs.
- **The labels are forgotten**: `auto from` and `auto orbit` have nothing
  to start from until a branch is computed again.
- **AUTO's settings, the model's parameters and the run** are not changed.

## More about

**How labels are numbered.** A run of [auto steady](auto-steady.md) uses 1
for its start, the next numbers for its special points and the last for its
end. A continuation with [auto from](auto-from.md) carries on from the
highest label so far. A second `auto steady` starts again at 1. So a
diagram built in a different order has different labels for the same
points, and one with two runs of `auto steady` has labels that repeat.

## Tips

- Save a diagram worth keeping with [auto save](auto-save.md) before
  clearing it. There is no undo.
- Clear before comparing two settings, and before a figure for a paper, so
  that the diagram holds one run of each branch.
- Changing the model or opening another one clears the diagram too.

## Algorithms

XPPAUT's own routine for resetting AUTO: the stored points are freed and
AUTO's restart data is discarded.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `No model loaded.` | There is no model. | Open one first. |
| `error: no point labelled N`, on a later `auto from` | The label belonged to the diagram that was cleared. | Compute the branch again and read the new label. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [auto steady](auto-steady.md) and [auto from](auto-from.md): building the
  diagram.
- [auto save](auto-save.md) and [auto load](auto-load.md): keeping one.

## Version history

Introduced in 0.1.0.
