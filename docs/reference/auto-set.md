# auto set

Set the numerical settings of AUTO: the range of the parameter, the step
along a branch, and how far a branch may go.

## Syntax

```
auto set NAME=VALUE ...
```

`NAME` is `parmin`, `parmax`, `ds`, `dsmin`, `dsmax`, `nmax`, `npr` or
`ntst`. Several settings can follow one another:
`auto set parmin=0 parmax=1 ds=-0.01`.

- **In the app:** Bifurcation window > **Numerics**.
- **In Python:** as keywords of the call they are for:
  `m.auto.equilibria("iapp", parmax=1, dsmax=0.05)`.
- **In a model file:** an options line, `@ parmin=-0.2, parmax=0.5`, sets
  them for everyone who opens the model.

## Description

`auto set` changes the settings that every later continuation uses:
[auto steady](auto-steady.md) and [auto from](auto-from.md). It computes
nothing itself. The settings stay until they are changed again or another
model is opened; a model's own `@` lines give their first values.

Two settings decide *where* AUTO looks, `parmin` and `parmax`. Three decide
*how carefully* it steps, `ds`, `dsmin` and `dsmax`. Two decide *how much*
it computes and keeps, `nmax` and `npr`. One, `ntst`, is the accuracy of
periodic orbits.

## Examples

### A range that hides a bifurcation

The Morris-Lecar model with the set `hopf` has Hopf points at
`iapp` = 0.2625 and 0.4569. With the range cut at 0.3, AUTO stops before
the second:

```
use hopf
auto set parmax=0.3
auto steady iapp
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.apply_set("hopf")
short = m.auto.equilibria("iapp", parmax=0.3)
```

AUTO computes 24 points and finds one Hopf point, at 0.2625. The branch
ends at an end point (EP) just past 0.3.

How to read it. Nothing in the diagram says that there is more beyond 0.3.
An EP at the edge of the range means "the range stopped me", not "the branch
ends here". When a diagram looks too simple, widen the range first.

### Large steps: fewer points, and a wrong verdict

With the full range again, and steps ten times larger:

```
auto reset
auto set parmax=0.5 dsmax=0.2
auto steady iapp
```

```python
m.auto.reset()
coarse = m.auto.equilibria("iapp", parmax=0.5, dsmax=0.2)
```

AUTO now computes 14 points where it took 49 with `dsmax` = 0.02. It still
finds both Hopf points, the second at 0.4568, but it reports that one as
*stable* where the careful run says *unstable*.

How to read it. The positions of the special points hardly move, because
AUTO locates each one precisely once it has stepped over it. What suffers
is everything between: the curve has corners, and the stability attached to
a point comes from a neighbour that is now far away. Use large steps to
find out where things are, then small ones for the diagram you keep.

## Settings

| Setting | Name | Meaning | Advice |
|---|---|---|---|
| **Par min**, **Par max** | `parmin`, `parmax` | The range of the first parameter. A branch stops when it leaves it. | Set them first. The parameter's present value must lie inside. |
| **Step DS** | `ds` | The first step along a branch. Its sign is the direction: positive increases the parameter at first. | About a hundredth of the range. Run once with each sign to see both sides of the start. |
| **DS min** | `dsmin` | The smallest step. AUTO gives up when it needs less. | Leave it small (1e-5). Raise it only if AUTO creeps. |
| **DS max** | `dsmax` | The largest step. | The main control of quality: a fiftieth of the range for a smooth curve, more for a quick look. |
| **Max steps** | `nmax` | The most points on one branch. | 200 is plenty for steady states. Keep it low (50 to 200) for periodic orbits, which often form closed loops that AUTO retraces. |
| **Label every** | `npr` | A numbered label every this many points, besides the special points. | Labels are where `auto from` and `auto orbit` can start. Lower it to sample a branch of orbits. |
| **Mesh NTST** | `ntst` | The number of mesh intervals for one periodic orbit. It does nothing for steady states. | 15 for smooth orbits; 50 or more for spikes, relaxation oscillations and long periods. |

`ds`, `dsmin` and `dsmax` are lengths along the branch in the space of the
parameter and the variables together, so their right size depends on the
scale of the model's variables as well as of the parameter.

The command and Python also take AUTO's other settings, which the
Numerics section does not show: `normmin` and `normmax` (a branch stops
when its norm leaves them: check them when a branch stops early), `ncol`
(collocation points per mesh interval), `epsl`, `epsu` and `epss`
(Newton's tolerances, and the one for locating special points), `iad` and
`iads` (how often the mesh and the step adapt), `mxbf` (how many branch
points one run follows), `itmx`, `itnw`, `nwtn`, `iid` and `suppbp`.

## Results

Nothing is drawn. The values appear in the **Numerics** section of the
Bifurcation window, and the next continuation uses them. An unknown name
gives an error and the other settings on the line are still applied.

## More about

**Why the step is adaptive.** AUTO takes the next step along the tangent to
the branch and corrects it with Newton's method. If the correction needs
many iterations, the next step is shorter; if it needs few, longer, up to
`dsmax`. On a straight stretch the steps therefore grow to `dsmax`, and
near a fold or a sharp turn they shrink.

**Why special points survive large steps.** After each step AUTO looks at
test functions that change sign at a bifurcation: the determinant of the
Jacobian for a fold, the real part of an eigenvalue for a Hopf point. When
one has changed sign across the last step, AUTO goes back and finds the
zero by a secant iteration to its own tolerance. A step can still be so
large that a function changes sign *twice* inside it, and then both
bifurcations are missed.

**The mesh of an orbit.** A periodic orbit is represented by a polynomial
on each of `NTST` intervals of its period. AUTO moves the intervals to
where the orbit changes fastest, but their number is fixed by `ntst`.

## Tips

- Change one thing at a time, and use **Clear diagram** (`auto reset`)
  before comparing two settings, or the runs pile up on one plot.
- A branch that stops after a handful of points with no EP at the edge of
  the range has usually failed to converge: reduce `ds` and `dsmax`.
- If two runs with different `dsmax` give different special points, trust
  neither until a third, smaller one agrees with one of them.
- Put the settings a model needs in its file, with `@` lines, so that the
  model opens ready to be analysed.

## Algorithms

The settings are AUTO's own constants: `ds`, `dsmin` and `dsmax` are DS,
DSMIN and DSMAX; `parmin` and `parmax` are RL0 and RL1; `nmax` is NMX,
`npr` is NPR and `ntst` is NTST. XPPAUT's defaults for a model that sets
none are NTST = 15, NMX = 200 and NPR = 50. The other constants keep
XPPAUT's values: 4 collocation points per mesh interval, and tolerances of
1e-4 for the parameter, the solution and the special points.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: unknown AUTO setting NAME` | `NAME` is not one of the eight. | Check the spelling; the names are in the table above. |
| A setting seems to be ignored | It was written without `=`, as `auto set nmax 50`. | Write `auto set nmax=50`. |
| `AUTO computed no points` on the next run | The parameter's value lies outside `parmin` to `parmax`. | Widen the range. |

## References

- Kuznetsov, Y. A. (2004). *Elements of Applied Bifurcation Theory*, 3rd
  edition. Springer. Chapter 10 (continuation, and locating bifurcations
  with test functions).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [auto steady](auto-steady.md) and [auto from](auto-from.md): the
  computations these settings control.
- [Bifurcation analysis with AUTO](../manual/auto.md): the chapter, with a
  section on the numerical settings.
- [The .ode file format](../manual/odefile.md): the `@` lines of a model.

## Version history

Introduced in 0.1.0.
