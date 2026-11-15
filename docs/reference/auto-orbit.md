# auto orbit

Show the periodic orbit at a labelled point of the bifurcation diagram as
the run: its time series, its curve in the phase plane.

## Syntax

```
auto orbit LABEL
```

- **In the app:** Bifurcation window > select a labelled point of a branch
  of periodic orbits > **Show this orbit as the run**.
- **In Python:** `sol = m.auto.orbit(point)`, where `point` is a point of a
  diagram or a label.

## Description

In the diagram a periodic orbit is two numbers: the largest and smallest
value of one variable. `auto orbit LABEL` brings back the whole orbit that
AUTO computed at that point, one period of it, and makes it the model's
run. The Time Series and Phase Plane windows then show it, and every tool
that works on the last run can use it: the phase response curve, the
spectrum, the animation, the export to a file.

AUTO keeps the whole orbit only at labelled points: the special points, the
ends of a branch, and one point every **Label every** (`npr`) steps. To
look at an orbit somewhere else on a branch, lower `npr` and compute the
branch again.

The orbit is AUTO's own solution, not a new simulation, so it is there for
unstable orbits too, which no simulation would settle on.

## Examples

### An orbit of the firing neuron, checked by simulation

The Morris-Lecar model with the set `hopf`, its steady states, and the
orbits from the first Hopf point, with a label every 10 points
([auto from](auto-from.md) explains the 50 steps):

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Apply the set hopf](cmd:use hopf) and
   [follow the steady states](cmd:auto steady iapp).
3. [Label every 10 points](cmd:auto set nmax=50 npr=10) and
   [follow the orbits](cmd:auto from 2 periodic).
4. [Show the orbit at label 8](cmd:auto orbit 8).

```
use hopf
auto steady iapp
auto set nmax=50 npr=10
auto from 2 periodic
auto orbit 8
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.apply_set("hopf")
d = m.auto.equilibria("iapp")
orbits = m.auto.continue_from(d.first("HB"), "periodic", nmax=50, npr=10)
point = orbits.label(8)
sol = m.auto.orbit(point)
```

Label 8 is a stable orbit at `iapp` = 0.352. The run is now one period of
it: 61 points from `t` = 0 to 14.375, with `V` between −0.3434 and 0.2760.

How to read it, and how to check it. The Time Series window shows one
spike, and the Phase Plane a closed curve. Now simulate the model at the
same current and compare:

```python
run = m.run(iapp=point.value, total=600, dt=0.05)
```

After the transient the simulation repeats every 14.3745 time units, with
`V` between −0.3434 and 0.2761: the orbit AUTO computed, to four digits.
The agreement is the reason to trust the rest of the branch.

### An orbit no simulation would show

Label 5 of the same branch is an *unstable* orbit, at `iapp` = 0.2489,
between the Hopf point and the fold:

```
auto orbit 5
```

```python
unstable = m.auto.orbit(5)
```

Its period is 29.03, twice that of the stable orbit above.

How to read it. A simulation started near this orbit leaves it, towards
either the rest state or the large stable orbit. The unstable orbit is the
boundary between the two: starts inside it go to rest, starts outside it go
to firing. AUTO is the only way to see it.

## Settings

| Setting | Command name | Meaning | Advice |
|---|---|---|---|
| **Label every** | `npr` | How often a point of a branch gets a label, and so keeps its orbit. | Set it before computing the branch. 10 gives a handful of orbits along a branch of 50 points. |
| **Mesh NTST** | `ntst` | The number of mesh intervals of the orbit. The run has 4 × NTST + 1 points. | With the default 15 the run has 61 points. Raise it before computing the branch for a smoother curve. |

## Results

- **The run** is replaced by the orbit: one period, starting at `t` = 0,
  with as many points as AUTO's mesh has. The points are not evenly spaced
  in time: AUTO puts more where the orbit changes fast.
- **The command window** prints the label and the number of points.
- **The model's parameters** are not changed by showing an orbit.
- **In Python** `m.auto.orbit(point)` returns the run, as `m.run()` does,
  with `sol.t` and one column for each variable.
- With **Keep curves** on, the previous run stays on the plots, so several
  orbits of a branch can be drawn together.

## More about

**What AUTO stores.** For a periodic orbit AUTO solves a boundary value
problem on the interval from 0 to 1, a scaled time, with the period as an
unknown. Its solution is the orbit on a mesh of `NTST` intervals with 4
points in each. `auto orbit` reads that solution back and multiplies the
scaled time by the period.

**Where the orbit starts.** A periodic orbit has no first point. AUTO fixes
one with a *phase condition*, chosen so that the orbit shifts as little as
possible from one step of the branch to the next. So `t` = 0 is not a peak
or a crossing of zero, and two orbits of a branch start at about the same
phase.

## Tips

- To make a figure of how an orbit changes along a branch, turn **Keep
  curves** on and show the orbits of several labels one after another.
- For the phase response curve of an orbit, show it with `auto orbit` and
  then open the Analysis window: the adjoint needs a run of exactly one
  period, which is what this gives.
- The 61 points of the default mesh draw a spike with corners. Compute the
  branch with `ntst=50` for a smooth one.
- If the simulation and the orbit disagree, the simulation is usually the
  one at fault: check that it has passed its transient, and that the model
  is not in a range with two stable states.

## Algorithms

`auto orbit` reads the labelled solution that AUTO saved for restarts: the
values at the `NTST × 4 + 1` mesh points and the period. It writes them as
the model's stored run, in single precision, as XPPAUT's *File > Import
orbit* does. Nothing is integrated.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: no point labelled N` | The diagram has no label `N`, or no label was given. | Read the labels in the table of special points. |
| `error: a steady state has no orbit; grab it to make it the initial values` | The label is a point of a branch of steady states. | Choose a label on a branch of periodic orbits. |
| The label you want does not exist | AUTO labels one point in every `npr`. | Lower **Label every**, use **Clear diagram**, and compute the branch again. |

## References

- Kuznetsov, Y. A. (2004). *Elements of Applied Bifurcation Theory*, 3rd
  edition. Springer. Chapter 10 (continuation of periodic orbits as
  boundary value problems).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [auto from](auto-from.md): computing the branch of periodic orbits.
- [auto set](auto-set.md): `npr` and `ntst`.
- [Data analysis](../manual/analysis.md): the phase response curve and the
  other tools that work on the run.

## Version history

Introduced in 0.1.0.
