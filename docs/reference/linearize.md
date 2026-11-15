# linearize

Linearise the model at an equilibrium or a point: the matrices A and B, the
eigenvalues, and whether chosen parameters can steer every state.

## Syntax

```
linearize
linearize P1 P2 ...
linearize at eq P1 P2 ...
linearize at ic P1 P2 ...
```

`linearise` is another name for `linearize`.

- **In the app:** Run > **Linearise...**.
- **In Python:** `lin = m.linearize(at={...}, inputs=["u"])`.

## Description

Near a point x*, the model x′ = f(x, p) behaves like the linear system

δx′ = A δx + B δu,

where A is the Jacobian ∂f/∂x and B the derivatives ∂f/∂p with respect to
the parameters `P1 P2 ...` chosen as **inputs**. `linearize` computes A
and B, the eigenvalues of A, and the rank of the controllability matrix
[B, AB, ..., Aⁿ⁻¹B]: when it equals the number of states, the inputs can
steer every state, and a feedback can place the eigenvalues anywhere.

`at eq` (the default) linearises at the equilibrium that Newton's method
finds from the initial values; `at ic` at the initial values themselves.
The derivatives come from central differences extrapolated to zero step,
accurate to about 1e-9. The result is the starting point of
[poles](poles.md), [lqr](lqr.md), [locus](locus.md) and
[closedloop](closedloop.md).

## Examples

### The pendulum held upside down

The classic pendulum, θ″ + 0.1 θ′ + sin θ = u, has a torque input `u`.
Linearise it at the top, θ = π:

1. [Open the pendulum](example:classics/pendulum.ode) and
   [start at the top](cmd:set th=3.14159265 w=0).
2. [Linearise with u as input](cmd:linearize at ic u).

```
set th=3.14159265 w=0
linearize at ic u
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("classics/pendulum"))
lin = m.linearize(at={"th": np.pi, "w": 0}, inputs=["u"])
print(lin.A, lin.B, lin.eigenvalues, lin.controllable)
```

The command window answers:

```
Linearised at the initial values: TH=3.14159 W=0
  eigenvalues: 0.95125+0i -1.0512+0i
  controllability matrix: rank 2 of 2 (controllable)
```

and A = [[0, 1], [1, −0.1]], B = [0, 1].

How to read it. At the top, −sin θ ≈ +(θ − π): gravity pushes away, so A
has +1 where the hanging pendulum has −1. The eigenvalues,
(−0.1 ± √4.01)/2, are a positive one, 0.951, the fall, and a negative one.
The torque reaches the angle through the velocity, so the rank is 2: one
input is enough to hold both states.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `P1 P2 ...` | the command; `inputs=` in Python | The parameters that act as controls. | Parameters that enter the equations; without any, only A and its eigenvalues. |
| `at eq`, `at ic` | the command; `at=` in Python | The point: the equilibrium from the initial values, or the initial values. | `at ic` for a point you set exactly, as here. |
| `step` | `step=` in Python | A single central difference with this relative step instead of the extrapolation. | Rarely needed. |

## Results

- **The command window** gives the point, the eigenvalues and the rank.
- **The Analysis window** shows A, B and the eigenvalues in the complex
  plane; `export FILE.csv analysis` writes them.
- **In Python** a `Linearization`: `A`, `B`, `eigenvalues`,
  `controllability`, `rank`, `controllable`, `singular_values`,
  `residual`, and the error estimates `A_error`, `B_error`.

## More about

**Where a linearisation holds.** Only near its point. In Python,
`lin.valid_radius(tol=0.05)` gives the size of perturbation within which
the linear model's error stays below 5%, and `lin.error(dx, du)` the error
for one perturbation.

**At a point that is not an equilibrium**, f(x*) is not zero and the
linear model has a constant term; `residual` reports its size.

## Tips

- Check `residual`: near 0 at an equilibrium.
- Choose as inputs only parameters that a controller could really change.

## Algorithms

Central differences of the right-hand side, extrapolated to zero step by
Ridders' method from three starting steps, the estimate with the smallest
error kept; the eigenvalues by XPPAUT's own eigenvalue routine; the rank
from the singular values of the controllability matrix.

## When it fails

| Message | Why | What to do |
|---|---|---|
| An error that no equilibrium was found | `at eq`, and Newton's method failed from the initial values. | Start nearer the equilibrium, or use `at ic`. |
| `rank` below the number of states | The inputs cannot reach some states. | Choose other or more inputs. |

## References

- Åström, K. J. and Murray, R. M. (2021). *Feedback Systems*, 2nd
  edition. Princeton University Press. Chapters 6 and 7.
- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapter 6.

## See also

- [poles](poles.md), [lqr](lqr.md): design a feedback from A and B.
- [eq](eq.md): the equilibrium and its eigenvalues.

## Version history

Introduced in 0.1.0.
