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
differences of the right-hand side, computed by the engine
(`xppe_linearize` in the C API), which the desktop app's *Linearise* also
calls: the app and Python give the same numbers, bit for bit. No step size
has to be chosen: the differences are taken at a sequence of shrinking
steps and extrapolated to zero step (Ridders' method), from three starting
step sizes, and the estimate with the smallest error wins. This keeps `A` and `B` accurate (typically 1e-9 or
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
[B, AB, ..., A^(n-1) B]), `singular_values` (of that matrix, largest first),
`rank`, `controllable` (the rank equals the number of states) and `residual`
(the largest |f| at the point, near zero at an equilibrium; for a map
|F(x) - x|). The rank counts the singular values above a cutoff,
max(n, n m) * 1e-8 times the largest, which you can change with `tol=`. For a
model with very many states A^(n-1) B can overflow; then `rank` is -1, with
a warning. At a point given in `at` the eigenvalues are those of `A`, from
XPPAUT's own eigenvalue routine (the one *Sing pts* uses).

```python
lin.plot()               # the eigenvalues in the complex plane
lin.to_csv("lin.csv")    # one row per state: the point, A, B, an eigenvalue
```

`plot()` draws what the app's *Linearise* view shows: the eigenvalues, with
the stable region shaded (the left half-plane; for a map, the inside of the
unit circle). `to_csv()` writes the file the app's `export lin.csv analysis`
writes: `state`, `at`, `A_<state>` ..., `B_<input>` ..., `eig_re`, `eig_im`,
and on the first row `rank` and `residual`.

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

## Pole placement

`lin.place_poles(poles)` returns the feedback gain `K` (one row per input,
one column per state) so that `A - B K` has the eigenvalues you ask for. The
control law is `u = u0 - K (x - x*)`: the nominal input minus the gain times
the distance from the linearisation point.

- With one input it uses Ackermann's formula and the controllability matrix.
- With several inputs it uses the method of Kautsky, Nichols and Van Dooren
  (the one behind `scipy.signal.place_poles`), which uses the spare inputs
  to keep the closed-loop eigenvectors well conditioned. Only NumPy is
  needed.
- `poles` has one number per state; complex poles must come in conjugate
  pairs. For a map (`meth=discrete`) the poles belong inside the unit circle.
- A pair that is not controllable, a model with no inputs, a wrong number of
  poles and a pole repeated more often than there are inputs raise
  `ValueError`. A warning is raised when the gain is very large, which
  means the pair is nearly uncontrollable.

`lin.closed_loop(K)` writes the closed-loop model as an `.ode` file and
loads it. Each input is taken off the model's `par` line and defined as a
fixed variable holding the feedback law; the rest of the model text is
unchanged, so the closed loop is the full nonlinear model. The file is in a
temporary directory unless you pass `path=`. The input must be declared on a
`par` or `number` line (not continued with a backslash).

### Example: balancing a pendulum upright

```python
import xpplab as xs

m = xs.Model.from_text("""
par g=9.8, l=1, u=0
th'=w
w'=g/l*sin(th)-0.2*w+u
init th=0, w=0
@ total=20, dt=0.01
""")

lin = m.linearize(at={"th": 0, "w": 0}, inputs=["u"])
lin.eigenvalues                       # +-3.0 : the upright position is unstable
K = lin.place_poles([-2 + 1j, -2 - 1j])
cl = lin.closed_loop(K)               # a new Model with u = -K (x - x*)
cl.ics["th"] = 0.3                    # start well away from upright
sol = cl.run()
sol["th"][-1]                         # ~ 0: the loop settles
```

The same start without the feedback (`m.ics["th"] = 0.3; m.run()`) falls
away. Check `lin.valid_radius()` to see how far from the point the linear
design can be trusted; the closed-loop model above is the nonlinear one, so
simulating it is the test.
