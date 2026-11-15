# eq

Find an equilibrium near the initial values, with its eigenvalues and its
stability.

## Syntax

```
eq
```

- **In the app:** the **Equilibrium** button, or XPPAUT's keys `S G`.
- **In Python:** `e = m.equilibrium()`, or from a guess:
  `m.equilibrium({"V": -0.4, "W": 0})`.

## Description

`eq` looks for a point where the model stands still, where every derivative
is zero. It starts from the initial values shown in the Workspace and
improves them with Newton's method until the point stops moving. It prints
the point, the eigenvalues of the model's Jacobian there, and the kind of
equilibrium they make it: a stable or unstable node, a stable or unstable
focus, or a saddle.

For a saddle in the plane `eq` also draws its four *manifolds* in the Phase
Plane: the two curves that lead into it and the two that lead away. They
divide the plane into the regions that end in different places.

Which equilibrium `eq` finds depends on where it starts. A model may have
several, and `eq` finds the one Newton's method reaches from the initial
values, usually the nearest. To find all of them, use
[eq all](eq-all.md).

## Examples

### Three equilibria, three starts

The Morris-Lecar model, as it opens, has three equilibria. From its own
initial values:

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Find the equilibrium](cmd:eq).

```
eq
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
e = m.equilibrium()
print(e.kind, e["V"], e["W"], e.eigenvalues)
```

The command window answers:

```
Equilibrium (saddle): V=-0.142535 W=0.034049
  eigenvalues: 1.03+0i -0.30101+0i
  4 branches of its stable and unstable manifolds drawn in the phase plane.
```

Now start from two other points:

```
set v=-0.4 w=0
eq
set v=0.1 w=0.3
eq
```

```python
low = m.equilibrium({"V": -0.4, "W": 0})
high = m.equilibrium({"V": 0.1, "W": 0.3})
```

| Start | Kind | V | W | Eigenvalues |
|---|---|---|---|---|
| the initial values (−0.144, 0.03) | saddle | −0.142535 | 0.034049 | 1.03, −0.30101 |
| (−0.4, 0) | stable node | −0.382505 | 0.001286 | −0.40276, −0.89036 |
| (0.1, 0.3) | unstable focus | 0.027610 | 0.269240 | 0.52042 ± 0.7811 i |

How to read it. The stable node is the neuron at rest: two negative real
eigenvalues, so every small disturbance dies away without oscillating. The
saddle is the threshold: one positive and one negative eigenvalue, and its
stable manifold is the line in the phase plane that separates a disturbance
that dies away from one that fires a spike. The unstable focus sits inside
the loop that a spike traces: complex eigenvalues with positive real part,
so trajectories spiral away from it.

### Where the starting point ends up depends on the start

The [run](run.md) page shows the same model firing one spike and coming to
rest at `V` = −0.3825. That is the stable node in the table above: a run
from the initial values ends there, and `eq` from the same initial values
finds the saddle instead. Newton's method looks for the nearest point where
the derivatives vanish, whether or not trajectories go there; a run follows
the trajectories.

## Settings

| Setting | Name | Meaning | Advice |
|---|---|---|---|
| **Initial values** | the variables' names | Where Newton's method starts. | Set them near the equilibrium you want: `set v=-0.4 w=0`. |
| **Newton tolerance** | `newt_tol` | The search stops when its step is smaller than this. | The default 0.001 is enough to classify; lower it (1e-8) when the digits matter. |
| **Newton iterations** | `newt_iter` | The search gives up after this many steps. | The default is 100. Raise it only if the search fails close to an equilibrium. |
| **Jacobian step** | `jac_eps` | The step of the finite differences that form the Jacobian. | The default 0.001 suits variables of order one; scale it with the model's variables. |
| **Bound** | `bound` | The search gives up if a value grows past this. | As for a run. |

## Results

- **The command window** prints the kind, the point and the eigenvalues,
  and for a saddle how many manifold branches were drawn.
- **The Phase Plane** marks the equilibrium, and for a saddle draws its
  manifolds.
- **In Python** `m.equilibrium()` returns an `Equilibrium`: `e["V"]` is a
  value, `e.eigenvalues` the eigenvalues, `e.stable` whether it is stable,
  `e.kind` the words above. With `set_initial=True` the equilibrium becomes
  the initial values, which is where [auto steady](auto-steady.md) starts.
  `m.manifolds()` returns the saddle's manifolds as trajectories.

## More about

**Equilibria.** A point x* where f(x*) = 0, so that the model x' = f(x)
does not move. Near it the model behaves like its linearisation,
d' = J d, where J is the Jacobian of f at x* (the matrix of the partial
derivatives of each component of f with respect to each variable) and
d = x - x* is the distance from the equilibrium.

**Stability from the eigenvalues of J.** Each eigenvalue is a rate: a
disturbance along its eigenvector grows or decays like exp(eigenvalue × t).
The equilibrium is stable when every eigenvalue has a negative real part.
In the plane that gives five kinds:

| Eigenvalues | Kind | Trajectories nearby |
|---|---|---|
| both real and negative | stable node | come straight in |
| both real and positive | unstable node | go straight out |
| real, of opposite signs | saddle | come in along one direction, leave along the other |
| complex, negative real part | stable focus | spiral in |
| complex, positive real part | unstable focus | spiral out |

A pair of complex eigenvalues on the imaginary axis is a *centre* in the
linearisation; the nonlinear terms decide whether it is stable.

**Manifolds of a saddle.** The *stable manifold* is the set of points whose
trajectories end at the saddle, the *unstable manifold* the set of points
whose trajectories started there. In the plane each is a pair of curves
leaving the saddle along the eigenvectors. They organise the whole phase
plane.

**Newton's method** solves f(x) = 0 by repeating the step: replace x by
the point where the linearisation of f at x is zero. It converges fast once
close, but from far away it can go to any root, or to none.

**Maps.** For a map, x(n+1) = F(x(n)), an equilibrium is a fixed point,
and it is stable when every eigenvalue of the Jacobian of F lies inside the
unit circle. `eq` works for maps too, and reports them as stable or
unstable.

## Tips

- If `eq` finds an equilibrium you did not expect, set the initial values
  close to the one you want and try again.
- To see all the equilibria, draw the nullclines (`nullclines`): every
  crossing of two nullclines is an equilibrium.
- Before [auto steady](auto-steady.md), use `eq` to put the model on the
  equilibrium you want to follow.

## Algorithms

`eq` calls XPPAUT's own routine for *Sing pts > Go*: Newton's method with
a Jacobian formed by finite differences, then the eigenvalues of that
Jacobian, and for a saddle the integration of its manifolds from points a
small distance along the eigenvectors. The settings are XPPAUT's own:
`NEWT_TOL` (0.001), `NEWT_ITER` (100) and `JAC_EPS` (0.001) by default.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `No equilibrium found near that point (Newton's method did not converge).` | Newton's method did not converge from the initial values, or the model has no equilibrium. | Start closer: draw the nullclines and set the initial values near a crossing. A model such as x' = x² + 1 has none. |
| An equilibrium far outside the region of interest | Newton's method went to another root. | Start closer, or use [eq all](eq-all.md) with the Phase Plane's window around the region. |
| `No model loaded.` | There is no model. | Open one first. |

## References

- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapters 5 and 6 (linear systems and the phase plane).
- Kuznetsov, Y. A. (2004). *Elements of Applied Bifurcation Theory*, 3rd
  edition. Springer. Chapter 2.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [eq all](eq-all.md): every equilibrium in a region.
- [auto steady](auto-steady.md): following an equilibrium as a parameter
  changes.
- [Tutorial: why does a neuron fire?](../manual/neuron-tutorial.md), which
  uses these three equilibria.

## Version history

Introduced in 0.1.0.
