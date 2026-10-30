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

To look closely at part of the diagram, scroll to zoom and drag to pan
(right-drag zooms to a box). The view then stays where you put it while
AUTO adds points; double-click the plot or press **Fit** to see the whole
diagram again.

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

## Boundary value problems and homoclinic orbits (Python)

AUTO's boundary value and homoclinic starts are available from Python as
`start("bvp", ...)` and `start("homoclinic", ...)`, and tested against known
answers (`python/tests/test_auto_bvp_homoclinic.py`). The desktop app does
not offer them yet.

**Boundary value problems.** Rescale the interval to `[0, 1]`, give one `b`
line per equation, and run the model so that AUTO starts from a solution
that satisfies the conditions. For `x'' = l x`, `x(0) = 0`, `x(1) = 1`:

```python
m.run()                      # the exact solution at l = 1
d = m.auto.start("bvp", "l", nmax=40, ds=0.05)
```

Each point agrees with `x'(0) = sqrt(l) / sinh(sqrt(l))`.

**Homoclinic orbits.** The start continues a long periodic orbit in two
parameters. Find the orbit first: continue periodic orbits from a Hopf point
until the period is large, import one with `auto.orbit(point)`, and put the
model's parameters at that point's values. Then give the saddle:

```python
m.auto.orbit(orbit_point)
m.set(a=orbit_point.value)
h = m.auto.start("homoclinic", "a", second="b", left=[0, 0],
                 unstable=1, stable=1)
```

`left=` is the saddle (and `right=` the second equilibrium of a
heteroclinic start); `unstable=` and `stable=` are the dimensions of its
manifolds. They fill the "Homoclinic info" box XPPAUT shows. The test uses
`x' = y`, `y' = x(1-x) - a y + b x y`: the curve starts where the period of
the orbits diverges and follows `a = 6b/7` for small `b`.

Limits:

- The starting orbit must be close to the homoclinic but not too long. A
  period of about 150 worked. With a period of 12 000 or more in the same
  model, and with a period of 370 in another, the first steps left the
  curve. Start from the first labelled orbit with a long period, not the
  longest.
- The first steps after the start can wobble before AUTO settles on the
  curve (a couple of per cent in the test); the test allows 1% on the
  settled points.
- Only 1-dimensional unstable and stable manifolds in the plane have been
  tested. Heteroclinic starts use the same code but have no test.

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
