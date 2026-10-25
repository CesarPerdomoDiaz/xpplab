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
are green (stable) and blue (unstable).

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

Try it: [open the Morris-Lecar model](example:lecar.ode), apply the `hopf`
set (Workspace > Sets), then [follow the steady states in iapp](cmd:auto steady iapp).

## Check the diagram with simulations

**Workspace > Range** (XPPAUT's *Initial conds > Range*) runs the model
once for each value of a parameter, and in *Max / min* mode records the
maximum and minimum of a variable after a transient. When the parameter and
the plotted variable match the Bifurcation window's, the results appear as
grey dots on AUTO's diagram. They should sit on the stable branches: the
solid red lines and the solid green ones. Where the system is bistable,
different initial values land on different branches; tick *Start each run
where the last ended* to follow one branch as the parameter moves.

## The numerical settings

| Setting | Meaning | Advice |
|---|---|---|
| Step DS | Size of the first step. **Its sign is the direction.** | Use a negative DS to explore towards smaller parameter values. |
| DS min, DS max | Bounds on the adaptive step. | A smaller DS max gives smoother curves and fewer missed points. |
| Par min, Par max | The parameter range. | AUTO stops at the edges. |
| Max steps (NMAX) | Most steps per run. | A closed loop of orbits runs until NMAX: lower it to stop sooner. |
| Label every (NPR) | Also label a point every N steps. | Labelled points can be continued from. |
| Mesh NTST | Mesh intervals for periodic orbits. | Increase it (to 50 or 100) for spiky orbits such as action potentials. |

## Saving a diagram and coming back to it

**Save...** under the buttons saves the diagram as an `.auto` file, the
format of XPPAUT's *AUTO > File > Save diagram*. The file holds AUTO's
settings, every point, and AUTO's restart data. **Open...** brings it back
in place of the diagram shown, so the next day you can pick a labelled
point and carry on where you stopped. XPPAUT opens these files too, and
XPP Lab opens XPPAUT's. (XPPAUT wrote the numbers with 6 digits; XPP Lab
writes them in full, so a diagram comes back exactly as saved.)

To see one of the periodic orbits, select a point on a periodic branch and
press **Show this orbit as the run** (XPPAUT's *Import orbit*). One period
of the orbit, from AUTO's own solution, becomes the current run: it shows
in the phase plane and the time series, and the Analysis tools work on it.

Commands: `auto save fold.auto`, `auto load fold.auto`, `auto orbit 6`.

## When things go wrong

- **"AUTO computed no points"**: the start point is probably not an
  equilibrium. Run the model to steady state first, or press Equilibrium.
- **The branch stops early**: check Par min and Par max, and Norm max.
  Then try a smaller DS max.
- **Periodic orbits fail from a Hopf point**: increase NTST, or reduce DS.
- **Points come out jagged**: reduce DS max.

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
