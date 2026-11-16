# Bifurcation analysis with AUTO

A **bifurcation diagram** shows how the long-term behaviour of a system
changes as one parameter varies: where equilibria appear, vanish or lose
stability, and where oscillations are born. XPP Lab computes these
diagrams with **AUTO** (E. Doedel), the same continuation code that XPPAUT
embeds.

## The idea in brief

Pick a parameter, say a current `I`. AUTO starts from an equilibrium and
follows it step by step as `I` changes. At each step it solves for the
equilibrium and checks its stability. When something qualitative happens it
marks a **special point**:

| Label | Name | What happens |
|---|---|---|
| LP | limit point (fold, saddle-node) | Two equilibria meet and vanish; the branch turns back. |
| HB | Hopf bifurcation | A pair of eigenvalues crosses the imaginary axis; periodic orbits are born. |
| BP | branch point | Two branches cross (common with symmetry). |
| PD | period doubling | An orbit loses stability to one of twice the period. |
| TR | torus (Neimark-Sacker) | An orbit loses stability to quasi-periodic motion. |
| LP (on orbits) | fold of limit cycles | Two periodic orbits meet and vanish. |
| EP | end point | Where the computation stopped. |

Stable parts are drawn solid, unstable parts dashed. The colours follow
XPPAUT: steady states are red (stable) and black (unstable); periodic orbits
are green (stable) and blue (unstable). The reference pages say how to
read every part of the window: [auto steady](../reference/auto-steady.md)
for steady states, [auto from](../reference/auto-from.md) for what can be
continued from each kind of point.

## A typical session

1. **Start at an equilibrium.** Run the model until it settles, or press
   **Equilibrium**. **Follow steady states** also polishes the start point
   with Newton's method, but AUTO needs to begin close to an equilibrium.
2. **Choose the parameter** and its range (**Par min** and **Par max**).
3. **Follow steady states.** The special points appear in the table and on
   the plot.
4. **Select a special point** (click it in the table or the plot) and choose
   what to do from there:
   - from **HB**: *Periodic orbits from this Hopf point*, or a two-parameter
     curve of Hopf points;
   - from **LP**: a two-parameter curve of folds;
   - from **BP**: *Switch to the other branch*;
   - from **PD**: the period-doubled branch.
5. For two-parameter curves, choose the **2nd param.** first. The result
   appears in the **Two parameters** tab.
   [Codimension-two bifurcations](codim2.md) works an example, with the cusp
   and Bogdanov-Takens points where the curves meet.

Try it: [open the Morris-Lecar model](example:lecar.ode), apply the `hopf`
set (Workspace > Sets), then [follow the steady states in iapp](cmd:auto steady iapp).

## Check the diagram with simulations

A diagram is a computation, and worth checking against runs.
**Workspace > Range** in *Max / min* mode runs the model once per value of
the parameter and records the largest and smallest value of a variable;
when the parameter and the variable match the Bifurcation window's, they
appear as grey dots on AUTO's diagram, and should sit on the stable
branches. See [range](../reference/range.md).

## The numerical settings

Three settings matter most. **Par min** and **Par max** are where the
branch stops; set them first. The sign of **Step DS** is the direction
the branch is followed in: negative to go towards smaller values first.
**DS max** controls the quality: smaller gives a smoother curve and fewer
missed points. For periodic orbits, raise **Mesh NTST** (to 50 or 100)
for spiky orbits such as action potentials, and keep **Max steps** low,
since a branch of orbits is often a closed loop that AUTO retraces. Each
setting is on [auto set](../reference/auto-set.md).

## Saving a diagram and coming back to it

**Save...** under the buttons saves the diagram as an `.auto` file, the
format of XPPAUT's *AUTO > File > Save diagram*, and **Open...** brings it
back, so the next day you can pick a labelled point and carry on.
**Show this orbit as the run** makes one period of a periodic orbit the
current run, for the phase plane, the time series and the Analysis tools.
See [auto save](../reference/auto-save.md),
[auto load](../reference/auto-load.md) and
[auto orbit](../reference/auto-orbit.md).

## Boundary value problems, homoclinic and heteroclinic orbits

AUTO's boundary-value, homoclinic and heteroclinic starts are available
from Python, `m.auto.start("bvp", ...)`, `"homoclinic"` and
`"heteroclinic"`; the desktop app does not offer them yet. The Python
guide's [Bifurcations with AUTO](https://cesarperdomodiaz.github.io/xpplab/python/auto/#boundary-value-problems-homoclinic-and-heteroclinic-orbits)
shows how to start each, with the tested examples and their limits.

## When things go wrong

- **"AUTO computed no points"**: the start point is probably not an
  equilibrium. Run the model to steady state first, or press Equilibrium.
- **The branch stops early**: check Par min and Par max, then try a
  smaller DS max.
- **Periodic orbits fail from a Hopf point**: increase NTST, or reduce DS.
- **Points come out jagged**: reduce DS max.

Each reference page ends with a *When it fails* table of every message
its command can give, why, and what to do.

## Scripting the same analysis

Every step is also available from the command window:

```
use hopf
auto set ds=0.02 dsmax=0.05 parmax=0.6
auto steady iapp
auto from 2 periodic
```

and from Python:

```
import xpplab as xs
m = xs.load("lecar.ode")
m.apply_set("hopf")
d = m.auto.equilibria("iapp")
orbits = m.auto.continue_from(d.first("HB"), "periodic")
m.auto.diagram.plot()
```
