# auto from

Continue from a labelled point of the bifurcation diagram: periodic orbits
from a Hopf point, a curve of bifurcations in two parameters, the rest of a
branch.

## Syntax

```
auto from LABEL ACTION
auto from LABEL two PAR2
```

`ACTION` is `periodic`, `two`, `extend`, `switch`, `doubling` or `fixed`.

- **In the app:** Bifurcation window > select a point in the table of
  special points, or click it on the plot > choose an action under
  **Continue from the selected point**.
- **In Python:** `m.auto.continue_from(point, "periodic")`, where `point` is
  a point of a diagram or a label. The actions are `"periodic"`,
  `"two-parameter"` (with `second="PAR2"`), `"extend"`, `"switch"`,
  `"doubling"` and `"fixed-period"`.

## Description

[auto steady](auto-steady.md) draws a branch of steady states and labels its
special points. `auto from` starts a new computation at one of those labels
and adds the result to the diagram. `LABEL` is the number shown in the
table and on the plot. What can be done depends on the kind of point:

| From | Action | What it computes |
|---|---|---|
| HB, a Hopf point | `periodic` | The branch of periodic orbits that starts there. |
| HB, LP, BP, PD or TR | `two PAR2` | The curve of that bifurcation as a second parameter `PAR2` varies too. It appears in the **Two parameters** tab. |
| Any labelled point of a branch | `extend` | More of the same branch, from that point. |
| BP on a branch of periodic orbits | `switch` | The other branch that crosses there. |
| PD, a period doubling | `doubling` | The branch of orbits of twice the period. |
| PD, TR or a periodic orbit | `fixed` | Orbits of that same period, in two parameters. |

`auto from LABEL` alone extends the branch. From a fold (LP) the only
action is `two`.

The settings of [auto steady](auto-steady.md#settings) apply: the new
branch stops at **Par min** or **Par max**, or after **Max steps** points.

## Examples

### Periodic orbits from a Hopf point

The Morris-Lecar model with the set `hopf` has two Hopf points, at
`iapp` = 0.2625 and 0.4569 ([auto steady](auto-steady.md) finds them, with
the labels 2 and 3). The neuron fires between them. This follows the
firing.

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Apply the set hopf](cmd:use hopf) and
   [follow the steady states](cmd:auto steady iapp).
3. [Allow 50 steps](cmd:auto set nmax=50), then
   [follow the periodic orbits from the first Hopf point](cmd:auto from 2 periodic).

```
use hopf
auto steady iapp
auto set nmax=50
auto from 2 periodic
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.apply_set("hopf")
d = m.auto.equilibria("iapp")
orbits = m.auto.continue_from(d.first("HB"), "periodic", nmax=50)
for p in orbits.special():
    print(p.label, p.type, round(p.value, 4), round(p.period, 2))
```

AUTO labels two folds of periodic orbits:

| Label | Type | iapp | Period |
|---|---|---|---|
| 5 | LP | 0.2484 | 25.76 |
| 6 | LP | 0.4657 | 19.06 |

How to read it. For a periodic orbit the plot shows the largest and the
smallest `V` over one period, so the branch is a pair of curves that open
out from the Hopf point. The orbits that leave the first Hopf point are
unstable (blue) and exist *below* it, down to the fold at `iapp` = 0.2484.
There the branch turns and the orbits are stable (green) all the way to the
second fold at 0.4657, where it turns again and returns, unstable, to the
second Hopf point. So the neuron fires for `iapp` between 0.2484 and
0.4657, a little wider than the range between the Hopf points. In the two
narrow strips outside them, 0.2484 to 0.2625 and 0.4569 to 0.4657, both the
rest state and the firing are stable: which one the neuron shows depends on
where it starts. The period of the stable orbits runs from 14.1 to 24.3.

Why 50 steps. This branch is a closed loop from one Hopf point to the
other. When AUTO reaches the far end it turns round and follows the same
orbits back, again and again, until **Max steps** is used up. With the
model's own setting of 2000 that draws the loop dozens of times and fills
the table with repeated folds. Fifty steps cover it once.

### A curve of Hopf points in two parameters

How do the Hopf points move when `phi`, the rate of the recovery variable,
changes? From the same Hopf point:

```
auto set nmax=120 dsmax=0.05
auto from 2 two phi
```

```python
curve = m.auto.continue_from(d.first("HB"), "two-parameter", second="phi",
                             nmax=120, dsmax=0.05)
```

The **Two parameters** tab shows `phi` against `iapp`. The curve leaves the
Hopf point (0.2625, 0.2), rises to `phi` = 1.85 at `iapp` = 0.3435, and
comes down through `iapp` = 0.4568 at `phi` = 0.2, which is the second Hopf
point.

How to read it. The two Hopf points of the first diagram are two points of
one curve. Under the curve the rest state is unstable and the neuron fires;
above it, it rests. So a fast enough recovery (`phi` above 1.85) removes
the firing whatever the current.

### A branch point: the other branch is already there

`x' = r x − x³` has the steady state `x` = 0, and for `r` > 0 also
`x` = ±√r: a pitchfork at `r` = 0. Choose **File > New model**, replace the
text with

```ode
x'=r*x-x^3
par r=-1
init x=0
@ parmin=-2,parmax=2,dsmax=0.1
done
```

and [follow the steady states](cmd:auto steady r).

```python
m = xs.Model.from_text("x'=r*x-x^3\npar r=-1\ninit x=0\n"
                       "@ parmin=-2,parmax=2,dsmax=0.1\ndone\n")
d = m.auto.equilibria("r")
```

One run draws both branches: `x` = 0 from `r` = −1 to 2, with a branch
point (BP, label 2) at `r` = 0 where it loses stability, and the parabola
`x` = ±√r out to ±1.42 at `r` = 2.02, stable.

How to read it. For steady states AUTO switches branch by itself: when a
branch ends it goes back to each branch point it found and follows the
branch that crosses there. Nothing more is needed, and `auto from 2 switch`
would only draw `x` = 0 again. `switch` is for branch points on branches of
*periodic orbits*, where AUTO does not do it by itself.

## Settings

The numerical settings are those of [auto steady](auto-steady.md#settings).
Two matter more here:

| Setting | Command name | Meaning | Advice |
|---|---|---|---|
| **Max steps** | `nmax` | The new branch stops after this many points. | Branches of periodic orbits are often closed loops that AUTO retraces: start with 50 to 200 and raise it if the branch stops short. |
| **Mesh NTST** | `ntst` | The number of mesh intervals that describe one periodic orbit. | The default of 15 suits smooth orbits. Raise it to 50 or more for orbits with sharp spikes or a long period. |
| **2nd param.** | the `PAR2` argument | The second parameter of a two-parameter curve. | A parameter that the bifurcation depends on. Its range is not limited by **Par min** and **Par max**, which bound the first parameter. |

## Results

- **Periodic orbits** are drawn in the Bifurcation window as the largest and
  smallest value of the plotted variable over a period: green where the
  orbit is stable, blue where it is not. Their special points are folds of
  orbits (LP), period doublings (PD), torus bifurcations (TR) and branch
  points (BP).
- **Two-parameter curves** are drawn in the **Two parameters** tab, the
  second parameter against the first.
- **The table of special points** gains the new labels. Each can be the
  start of a further `auto from`.
- **A periodic orbit** at a labelled point can be shown as a run with
  `auto orbit LABEL`: the time series and the phase plane then show it.
- **The model's parameter** is left at the value of the point continued
  from, not where it was before.
- **In Python** the call returns the new branch as a `Diagram`; a periodic
  `Point` has `period`, `umax` and `umin`, and a point of a two-parameter
  curve has `value` and `value2`. `m.auto.diagram` holds everything computed
  so far.

## More about

**Periodic orbits.** A periodic orbit of period `T` is a solution with
`x(t + T) = x(t)`. AUTO finds it as the solution of a boundary value
problem on one period, with `T` as an unknown, and follows it as the
parameter changes, as it follows a steady state.

**Stability of an orbit.** A small disturbance of the orbit is multiplied,
after one period, by the *Floquet multipliers*. One of them is always 1.
The orbit is stable when all the others lie inside the unit circle.

**The special points of orbits** are where a multiplier leaves the unit
circle:

- **LP**, a fold of periodic orbits: a multiplier passes through 1. A
  stable and an unstable orbit meet and vanish.
- **PD**, a period doubling: a multiplier passes through −1. An orbit of
  twice the period appears.
- **TR**, a torus bifurcation: a complex pair crosses the circle. The
  motion gains a second frequency.

**A Hopf point** is *supercritical* when the orbits that leave it are
stable, and *subcritical* when they are unstable, as in the first example.
A subcritical Hopf point usually comes with a fold of orbits nearby, and
with a range where rest and oscillation are both stable.

**Two-parameter curves.** A bifurcation such as a Hopf point happens at one
value of a parameter. Let a second parameter vary and that value moves: the
bifurcation traces a curve in the plane of the two parameters, which
divides the plane into regions where the system behaves differently.

## Tips

- Compute the steady states first, then the orbits from each Hopf point,
  then the two-parameter curves. Each step tells you which next step is
  worth taking.
- If the table fills with the same folds over and over, the branch is a
  loop: use **Clear diagram**, lower **Max steps** and run again.
- If a branch of orbits ends at a period that grows without bound, it is
  approaching a homoclinic orbit. Raise **Mesh NTST**, and expect the last
  points to be less accurate.
- Check the stable orbits by simulation: set the parameter inside the
  range, run, and compare the period.

## Algorithms

`auto from` restarts AUTO from the labelled solution it saved. Periodic
orbits are computed by orthogonal collocation, with `NTST` mesh intervals
and 4 collocation points in each, and an adaptive mesh; their stability
comes from the Floquet multipliers of the linearised problem. A
two-parameter curve is the continuation of the extended system that defines
the bifurcation.

Starting from a label first sets the model's parameters and variables to
those of that point, as XPPAUT's *Grab* does, and they stay there after the
run: after `auto from 2 periodic` in the first example, `iapp` is 0.2625,
the value at the Hopf point.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: no point labelled N` | The diagram has no label `N`. | Read the labels in the table of special points. After **Clear diagram** the labels start again. |
| `error: from a fold (LP) AUTO can only follow the fold in two parameters: write auto from N two` | From a fold there is one action. | `auto from N two PAR2`. |
| `error: unknown action NAME` | The action is not one of the six. | See the table under Description. |
| `error: no parameter NAME` | `PAR2` is not a parameter of the model. | `whos` lists the parameters. |
| The same special points repeat | The branch is a closed loop and **Max steps** is large. | Lower **Max steps**. |
| `switch` draws the branch it started from | The point is a branch point of steady states, whose other branch AUTO has drawn already. | Nothing: look at the diagram of `auto steady`. |

## References

- Kuznetsov, Y. A. (2004). *Elements of Applied Bifurcation Theory*, 3rd
  edition. Springer. Chapters 3 to 5 (bifurcations of equilibria and of
  periodic orbits) and 10 (continuation).
- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapter 8 (Hopf bifurcations and bifurcations of cycles).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [auto steady](auto-steady.md): the branch of steady states this starts
  from.
- [Bifurcation analysis with AUTO](../manual/auto.md): the chapter that
  walks through a whole analysis.
- [Codimension-two bifurcations](../manual/codim2.md): two-parameter curves
  and the points where they meet.
- [Tutorial: why does a neuron fire?](../manual/neuron-tutorial.md), which
  follows these orbits on the Morris-Lecar model.

## Version history

Introduced in 0.1.0.
