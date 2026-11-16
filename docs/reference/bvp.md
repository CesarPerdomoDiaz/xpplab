# bvp

Solve the model's boundary-value problem: find the initial values for
which the run meets conditions at both ends.

## Syntax

```
bvp
bvp tol=T maxit=N eps=E
```

- **In the app:** Run > **Boundary-value problem**, or the **Solve BVP**
  toolbar button, which a model with `bndry` statements shows (XPPAUT's
  *Bndryval > Show*).
- **In Python:** `sol = m.bvp()`, or `m.bvp(tolerance=1e-8)`.

## Description

An ordinary run starts from known initial values. A boundary-value problem
knows only some values at the start and some at the end, and asks for the
run that satisfies both. The conditions are the model file's `bndry`
statements: each is an expression that must be zero, at the start for
plain names and at the end for primed names (`bndry v-1`, `bndry v'`).
XPPAUT expects one condition per variable.

`bvp` solves it by *shooting*: from the present initial values as a first
guess, it runs over [`T0`, `T0` + `total`], measures how far the conditions
are from zero, and corrects the initial values by Newton's method until
they are within the tolerance. On success the initial values hold the
solution's, so the next solve starts from it, and the solution is the
current run.

`tol`, `maxit` and `eps` change XPPAUT's settings for the shooting: the
tolerance (1e-5 at first), the most Newton iterations (20) and the step of
the finite differences for its Jacobian (1e-5). They stay until changed.

## Examples

### A cable with its end held at zero

`cable.ode` is the linear cable equation, `v'' = v/λ²` with λ² = `lam2`,
written as two first-order equations for `v` and `vx` = `v'`. Its
conditions are `v(0) = v0` and `a v(L) + b v'(L) = 0` at the far end. With
`a` = 1, `b` = 0 the far end is held at zero (a *killed* end). On a cable
of length 1, with λ = 1, the exact solution is v(x) = sinh(1 − x)/sinh(1).

1. [Open the cable model](example:cable.ode).
2. [Make the cable of length 1](cmd:set total=1 dt=0.01).
3. [Solve it](cmd:bvp).

```
set total=1 dt=0.01
bvp
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("cable.ode"))
m.set(total=1, dt=0.01)
sol = m.bvp()
exact = np.sinh(1 - sol.t) / np.sinh(1)
print(m.get("VX"), np.abs(sol["V"] - exact).max())
```

The command window says "Solved the boundary-value problem by shooting.
Initial values: V=1 VX=-1.31304". The slope at the start is
−1.31303528556; the exact value, −coth 1, is −1.31303528550. Along the
cable `V` differs from the exact solution by at most 4e-8.

How to read it. The unknown was the slope at the start, `vx`(0): the
shooting found the one that brings `v` to zero at the far end. The
remaining difference from the exact solution is the precision in which
the run is stored.

### A sealed end, and the tolerance

With `a` = 0, `b` = 1 the condition is `v'(1)` = 0: no current leaves the
far end (a *sealed* end). The exact solution is cosh(1 − x)/cosh(1), with
slope −tanh 1 = −0.7615941560 at the start.

```
set a=0 b=1 v=1 vx=0
bvp
get vx
bvp tol=1e-8
get vx
```

```python
m.set(a=0, b=1, V=1, VX=0)
m.bvp()
loose = m.get("VX")
m.bvp(tolerance=1e-8)
print(loose, m.get("VX"))
```

With XPPAUT's tolerance the slope is −0.7615917, 2.5e-6 from the exact
value; with `tol=1e-8` it is −0.76159415592, within 4e-11.

How to read it. The shooting stops as soon as the conditions are within
the tolerance, and 1e-5 is generous: the slope is then right to about five
digits, and how much better depends on the path Newton's method took.
Started from the first example's solution instead of `vx` = 0, the same
tolerance happens to give −0.76159415594. When the answer matters to more
digits, tighten `tol`, and the result no longer depends on the guess.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `tol` | `bvp tol=...`; `tolerance=` in Python | How close to zero the conditions must come. | 1e-5 at first. Tighten it for more digits. |
| `maxit` | `bvp maxit=...`; `max_iterations=` in Python | The most Newton iterations. | 20 at first. Raise it if a good guess still fails. |
| `eps` | `bvp eps=...`; `epsilon=` in Python | The step of the finite differences for the Jacobian. | 1e-5 at first; rarely needs changing. |
| **The interval** | `set t0=... total=...` | Where the conditions apply: at `T0` and `T0` + `total`. | Set the length of the domain with `total`. |
| **The first guess** | the initial values | Where the shooting starts. | Close to the solution for a nonlinear problem; any for a linear one. |

## Results

- **The initial values** are the solution's, and the command window lists
  them.
- **The run** is the solution, over the interval.
- **In Python** `m.bvp()` returns the solution as a run, and the initial
  values hold the solution's.

## More about

**Shooting.** The conditions at the end are functions of the unknown
initial values. Shooting solves those equations by Newton's method, each
evaluation being a run. It works well when the runs depend smoothly and
not too violently on the initial values; for a problem whose solutions
grow very fast along the interval, a small change at the start makes a
huge one at the end, and the shooting needs a good first guess.

**The cable.** For v'' = v/λ², the solutions are combinations of e^(x/λ)
and e^(−x/λ). The two conditions pick one: sinh for a killed end, cosh for
a sealed one. A longer cable or a smaller λ makes them nearly the decaying
exponential alone.

## Tips

- Check the number of `bndry` statements: one per variable.
- For a nonlinear problem, start from a guess near the solution, for
  example from a run that nearly meets the conditions.
- Tighten `tol` when the solution's digits matter, and check the result
  against a known case, as here.

## Algorithms

XPPAUT's Bndryval: Newton's method on the boundary conditions as functions
of the initial values, with the Jacobian from finite differences of step
`eps`, each evaluation an ordinary run with the model's method.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `This model has no boundary conditions. Add bndry statements, ...` | The model has no `bndry` lines. | Add one per variable. |
| `error: ...` with the shooting's reason | Newton's method did not converge in `maxit` iterations, or a run failed. | A better first guess, a larger `maxit`, or a shorter interval. |
| `error: write bvp [tol=T] [maxit=N] [eps=E], ...` | A setting is not a positive number. | Write `tol=1e-8`, `maxit=40`. |

## References

- Ascher, U. M., Mattheij, R. M. M., and Russell, R. D. (1995). *Numerical
  Solution of Boundary Value Problems for Ordinary Differential
  Equations*. SIAM. Chapter 4 (shooting).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [run](run.md): each evaluation of the shooting.
- [The ODE file](../manual/odefile.md): `bndry` statements.

## Version history

Introduced in 0.1.0. `tol`, `maxit` and `eps` in the app since 0.1.4.
