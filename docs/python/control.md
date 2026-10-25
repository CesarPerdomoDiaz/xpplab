# Control design

## Linearisation and controllability

`m.linearize(at=None, inputs=[...])` linearises the model around a point and
tests whether the chosen parameters can steer it.

- `at` is a dict of state values (the other variables keep their initial
  values). By default the point is the equilibrium that `m.equilibrium()`
  finds from the current initial conditions, and the eigenvalues are the
  ones the engine reports there.
- `inputs` lists the parameters that act as controls.

`A` is the Jacobian of the right-hand side and `B` holds the derivative with
respect to each input, one column per input. Both come from central
differences of `m.rhs()`. No step size has to be chosen: the differences are
taken at a sequence of shrinking steps and extrapolated to zero step
(Ridders' method), from three starting step sizes, and the estimate with the
smallest error wins. This keeps `A` and `B` accurate (typically 1e-9 or
better) for models with fast oscillations, steep exponentials, or states
that are very large or very small. `lin.A_error` and `lin.B_error` hold the
error estimate for each entry; look at them if a model is not smooth.
`step=1e-6` switches to a single central difference with that relative step.
The parameters are restored afterwards. For a map (`meth=discrete`) `A` and
`B` are the derivatives of the map itself.

A linearisation is only valid near its point, and a controller designed on
`A` and `B` is only as good as that approximation. So the result can measure
it instead of leaving you to guess:

```python
lin.valid_radius(tol=0.05)    # size of a perturbation the linear model holds for
lin.error(dx=[0.2, 0.0], du=[1.0])   # relative error for one perturbation
```

`error` compares the model's right-hand side at the perturbed state and
input with the prediction `A dx + B du`. `valid_radius` scans outwards over
32 fixed random directions in (state, input) space and returns the largest
distance at which the error stays below `tol` in all of them (`inf` for a
linear system). Design inside that distance, or check closed-loop
simulations against the nonlinear model beyond it. When the derivatives
themselves are uncertain (a kink, a step in the right-hand side) a warning
is raised, and `A_error` shows which entries.

The linearisation is about an equilibrium or any point you give in `at`. It
is not a linearisation about a periodic orbit, which is time-varying; use
the Floquet tools for that.

The result has `A`, `B`, `eigenvalues`, `controllability` (the matrix
[B, AB, ..., A^(n-1) B]), `rank`, `controllable` (the rank equals the number
of states) and `residual` (the largest |f| at the point, near zero at an
equilibrium). The rank comes from the singular values of the controllability
matrix, with a cutoff you can change with `tol=`.

### Example: a damped oscillator pushed by a force

```python
import xpplab as xs

m = xs.Model.from_text("""
par c=0.5, k=4, u=0
x'=v
v'=-k*x-c*v+u
init x=0.1, v=0
""")

lin = m.linearize(inputs=["u"])
lin.A              # [[ 0. ,  1. ], [-4. , -0.5]]
lin.B              # [[0.], [1.]]
lin.eigenvalues    # -0.25 +- 1.98j
lin.rank           # 2
lin.controllable   # True
```

If the force cannot reach a state, the rank drops. In
`x' = -x + u, y' = -2y` the second state is independent of `u`:
`rank` is 1 and `controllable` is `False`.
