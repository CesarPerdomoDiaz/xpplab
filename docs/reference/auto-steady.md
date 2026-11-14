# auto steady

Follow steady states as a parameter varies, and find where they fold or
lose stability.

## Syntax

```
auto steady
auto steady PAR
```

- **In the app:** Bifurcation window > choose the **Parameter** > **Follow
  steady states**.
- **In Python:** `d = m.auto.equilibria("PAR")`, with settings as keywords:
  `m.auto.equilibria("iapp", ds=-0.02, parmax=1)`.

## Description

`auto steady PAR` starts at the model's present steady state and follows it
as the parameter `PAR` changes. It draws the branch in the Bifurcation
window, solid where the steady state is stable and dashed where it is
unstable, and marks the special points: folds (LP), Hopf bifurcations (HB)
and branch points (BP). It is the first step of almost every bifurcation
analysis: the periodic orbits, the two-parameter curves and the other
branches all start from the special points it finds.

`auto steady` alone uses the parameter chosen in the Bifurcation window. At
first that is the model's first parameter.

The start point is the model's present state: the initial values, or the
end of the last run. AUTO first polishes it with Newton's method, so it
must be close to a steady state, not exactly on it. If the model has not
settled, run it for longer or use [eq](../manual/commands.md) first.

The branch is followed in the direction of the sign of **Step DS**: a
positive step increases the parameter at first, a negative one decreases it.
It stops at **Par min** or **Par max**, after **Max steps** points, or when
AUTO cannot continue.

## Examples

### Two Hopf bifurcations in a neuron model

The Morris-Lecar model with the parameter set `hopf` rests at low current
and at high current, and fires in between.

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Apply the set hopf](cmd:use hopf).
3. [Follow the steady states in iapp](cmd:auto steady iapp).

```
use hopf
auto steady iapp
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.apply_set("hopf")
d = m.auto.equilibria("iapp")
for p in d.special():
    print(p.type, round(p.value, 4), p.u0)
```

AUTO computes 49 points, from `iapp` = 0.05 to just past `Par max` = 0.5,
and labels two special points:

| Label | Type | iapp | V | W |
|---|---|---|---|---|
| 2 | HB | 0.2625 | −0.1989 | 0.2098 |
| 3 | HB | 0.4569 | 0.0641 | 0.6053 |

How to read it. The curve is the membrane potential `V` of the rest state
against the current. It is solid (stable) up to the first Hopf point, dashed
(unstable) between the two, and solid again after the second. Between
`iapp` = 0.2625 and 0.4569 the neuron cannot rest, so it fires: that is
where the periodic orbits live. To compute them, select the point HB and
choose *Periodic orbits from this Hopf point* (`auto from 2 periodic`).

### A fold, and the direction of the first step

`x' = a x + b − x³` has one steady state for large |b| and three for small
|b|. The two folds where the three merge into one are known exactly:
`b = ±2a^(3/2)/(3√3)`, which is ±0.3849 for `a` = 1, at `x = ∓1/√3`
= ∓0.5774.

Choose **File > New model**, replace the text with these three lines and
press **Apply**:

```ode
x'=a*x+b-x^3
par b=1,a=1
done
```

Then, in the command window:

```
auto set parmin=-2 parmax=2 ds=-0.02 dsmax=0.05 nmax=200
auto steady b
```

```python
m = xs.Model.from_text("x'=a*x+b-x^3\npar b=1,a=1\ndone\n")
d = m.auto.equilibria("b", parmin=-2, parmax=2, ds=-0.02, dsmax=0.05,
                      nmax=200)
```

The model starts at `b` = 1, on the upper steady state `x` = 1.3247. With
`ds` negative AUTO decreases `b`, and the branch draws an S:

| Label | Type | b | x |
|---|---|---|---|
| 2 | LP | −0.3849 | 0.5774 |
| 4 | LP | 0.3849 | −0.5774 |

How to read it. At the first fold the branch turns back: `b` now increases
while `x` keeps falling, and the steady state is unstable (dashed) until the
second fold, where the branch turns again and is stable down to `Par min`.
Between `b` = −0.3849 and 0.3849 there are three steady states for each `b`,
two stable and one unstable: the system is bistable there. AUTO finds the
folds to all four digits shown.

With the default positive step the same command would follow the upper
branch to the right only, where nothing happens. When a diagram looks
empty, try the other sign of **Step DS**.

## Settings

Set them in the Bifurcation window's **Numerics** section, with `auto set
NAME=VALUE ...`, or as keywords in Python. A model file can set its own with
`@` lines (`@ parmin=-0.2, parmax=0.5`).

| Setting | Command name | Meaning | Advice |
|---|---|---|---|
| **Parameter** | the argument | The parameter that varies. | Any parameter of the model. |
| **Par min**, **Par max** | `parmin`, `parmax` | The branch stops when the parameter leaves this range. | Set them first. The start value must lie inside, or AUTO computes nothing. |
| **Step DS** | `ds` | The first step along the branch. Its sign is the direction. | 0.01 to 0.02 of the parameter's range. Negative to go down first. |
| **DS min**, **DS max** | `dsmin`, `dsmax` | The smallest and largest step AUTO may take as it adapts the step. | A large `dsmax` gives few points and can step over a special point's neighbourhood; a small one gives a smooth curve and takes longer. |
| **Max steps** | `nmax` | The branch stops after this many points. | Raise it if the branch ends before the range does. |
| **Label every** | `npr` | A numbered label every this many points, besides the special points. | Labels are where later computations can start. |

## Results

- **The Bifurcation window** shows the branch: the parameter across, and up
  the variable chosen under **Plot** (its value at the steady state). Red is
  stable and black unstable, solid and dashed, as in XPPAUT.
- **The table of special points** lists each labelled point with its type,
  the parameter value and the state. Click a row, or the point on the plot,
  to select it. The actions that make sense from it appear beside the table.
- **The command window** prints the number of points and the special points.
- **In Python** the result is a `Diagram`: `d.special("HB")` gives the Hopf
  points, each `Point` has `value`, `u0` (the state), `stable`, `type` and
  `label`, and `d.plot("V")` draws it.
- **Export:** `export FILE.csv auto` writes every point, and **Save...**
  (`auto save FILE.auto`) keeps the diagram with what AUTO needs
  to continue from its labels later.

## More about

**Steady states and branches.** A steady state of `x' = f(x, p)` is a
solution of `f(x, p) = 0`. As `p` changes, the solution moves, and the set
of solutions is a curve in the space of `(x, p)`: a *branch*.

**Continuation.** AUTO does not solve `f = 0` for one value of `p` after
another. That fails at a fold, where the branch turns back and there is no
solution for the next `p`. Instead it takes steps of a given length *along
the curve* (pseudo-arclength continuation): it predicts the next point along
the tangent and corrects it with Newton's method, with `p` as one more
unknown. So it passes folds without trouble, and the step `DS` is a length
along the branch, not a change of `p`.

**Stability.** At each point AUTO computes the eigenvalues of the Jacobian
`∂f/∂x`. The steady state is stable when all of them have negative real
part.

**The special points** are where that changes:

- **LP**, a fold (limit point, saddle-node): one real eigenvalue passes
  through zero and the branch turns back. Two steady states meet and vanish.
- **HB**, a Hopf bifurcation: a pair of complex eigenvalues crosses the
  imaginary axis. A branch of periodic orbits starts there.
- **BP**, a branch point: one real eigenvalue passes through zero and a
  second branch of steady states crosses this one, as at a pitchfork or a
  transcritical bifurcation.
- **EP**, an end point: where the computation stopped. It is not a
  bifurcation.

## Tips

- Start from a steady state you understand. Run the model, look at where it
  settles, then follow that.
- Set **Par min** and **Par max** around the range you care about before the
  first run. The defaults suit few models.
- If the diagram shows only a short line, look at the last point: an EP at
  **Par min** or **Par max** means the range ended it, and an EP elsewhere
  means **Max steps** did.
- Follow the branch both ways: run once with a positive `ds` and once with a
  negative one. The second run adds to the diagram.
- Check a diagram against simulations where it matters: **Workspace >
  Range** runs the model over the same parameter and draws the result on the
  same axes.

## Algorithms

`auto steady` runs AUTO as embedded in XPPAUT 8.0, unchanged: pseudo-
arclength continuation with Newton correction and an adaptive step between
`dsmin` and `dsmax`, detection of folds, Hopf points and branch points from
the eigenvalues of the Jacobian, and their location to AUTO's tolerances
(`EPSL`, `EPSU`, `EPSS`: 1e-4 by default). The start point is
corrected by XPPAUT's Newton iteration before AUTO begins. After the run
the model's parameters are put back to what they were.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `AUTO computed no points` | The start is not near a steady state, or the parameter's value lies outside **Par min** to **Par max**. | Run the model until it settles, or press **Equilibrium**. Check the range. |
| `error: no parameter NAME` | `NAME` is not a parameter of the model. | `whos` lists the parameters. Only parameters can be followed, not variables. |
| The branch stops at once at an EP | The first step left the range, or `ds` is too large for the model's scale. | Reduce `ds`, or reverse its sign. |
| The curve has corners or misses a point you expect | `dsmax` is too large. | Reduce `dsmax` and run again. |
| `No model loaded.` | There is no model. | Open one first. |

## References

- Kuznetsov, Y. A. (2004). *Elements of Applied Bifurcation Theory*, 3rd
  edition. Springer. Chapters 3 (folds and Hopf bifurcations) and 10
  (continuation).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.
- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapters 3 and 8.

## See also

- [Bifurcation analysis with AUTO](../manual/auto.md): the chapter that
  walks through a whole analysis.
- [Codimension-two bifurcations](../manual/codim2.md): following the folds
  and Hopf points found here in two parameters.
- [Tutorial: why does a neuron fire?](../manual/neuron-tutorial.md), which
  uses this on the Morris-Lecar model.
- [Commands and shortcuts](../manual/commands.md): `auto from`, `auto set`,
  `auto save`, `eq`, `range`.

## Version history

Introduced in 0.1.0. Since 0.1.1 the model's parameters are restored after
the run.
