# Control: holding an unstable state

How do I hold an inverted pendulum upright? How do I keep a chemical
reactor at a steady state that it would leave on its own? Both questions
have the same shape. The model has an equilibrium you want, the equilibrium
is unstable, and you can push on the system through one quantity (a torque,
a coolant temperature). The task is to choose that push, from the
measured state, so that the equilibrium becomes stable.

The standard answer is **state feedback** on the linearised model
[AM ch. 7 and 8; FPE ch. 7]. This page follows it in five steps, first for
the pendulum and then for the reactor: linearise, test controllability,
place the poles, compute the optimal gain (LQR), and run the closed loop on
the full nonlinear model. Each step is a Python call and a command of the
desktop app, and both give the same numbers.

Save each model under the name in its first line, in the folder where you
run the Python. The snippets of a page run one after the other, in a single
script. The numbers in the comments are what the code printed when the page
was written; the test suite computes them again.

## Theory for state feedback

**The linear model.** Near an equilibrium `x*` of `x' = f(x, u)`, with the
input `u` at its nominal value `u0`, write `x = x* + dx` and `u = u0 + du`.
Then `dx' = A dx + B du`, with `A` the Jacobian of `f` with respect to the
states and `B` its derivative with respect to the inputs. In XPP Lab the
inputs are parameters of the model, chosen by name.

**Controllability.** The inputs can steer every state when the matrix
`[B, AB, ..., A^(n-1) B]` has rank `n`, the number of states [AM ch. 7].
If it does not, some poles (eigenvalues of `A`) cannot be moved by any
feedback.

**Pole placement.** With the feedback `du = -K dx` the closed loop is
`dx' = (A - B K) dx`. When the pair `(A, B)` is controllable, `K` can be
chosen so that `A - B K` has any eigenvalues you ask for, complex ones in
conjugate pairs. The loop is stable when all of them have a negative real
part [AM ch. 7].

**LQR.** Instead of choosing poles, choose what the motion costs: the
feedback that minimises the integral of `dx' Q dx + du' R du` is
`du = -K dx`, with `K` from the algebraic Riccati equation [AM ch. 8]. A
large `Q` asks for a small error, a large `R` asks for a gentle input.

**Where the linear model is valid.** The feedback is designed on the linear
model but applied to the real one. It is sure to work only close to `x*`,
and `valid_radius` estimates how close that is. Farther away, the
simulation of the full model decides.

## Example 1: an inverted pendulum

A pendulum of unit length and unit mass, with friction `c` and a motor
torque `u` at the pivot. The angle `th` is zero when the pendulum is
upright, so gravity pushes it away from that position:

```ode
# pend.ode
th' = w
w' = sin(th) - c*w + u
par c=0.2, u=0
init th=0.5, w=0
@ total=20, dt=0.01, meth=rungekutta
done
```

Without torque, the upright position is a saddle. Run from `th = 0.5` it
falls, and ends near the hanging position (`th` close to pi):

```python
import numpy as np
import xpplab as xs

pend = xs.load("pend.ode")
sol = pend.run()
sol["th"][-1]                          # 3.0638: it has fallen
```

**Linearise.** Linearise at the upright position, with the torque as the
input:

```python
lin = pend.linearize(at={"th": 0, "w": 0}, inputs="u")
lin.A                                  # [[0, 1], [1, -0.2]]
lin.B                                  # [[0], [1]]
lin.eigenvalues                        # 0.9050 and -1.1050: a saddle
```

The positive eigenvalue is the rate at which a small tilt grows: about
`exp(0.905 t)`. In the app, open `pend.ode`, set the initial values to the upright position
and run `linearize at ic inputs u` in the command line. It shows `A`, `B`,
the eigenvalues in the complex plane and the controllability result.

**Controllability.** The matrix `[B, AB]` and its rank:

```python
lin.controllability                    # [[0, 1], [1, -0.2]]
lin.rank, lin.controllable             # 2, True
```

The torque can steer both states, so the poles can be placed anywhere.

**Place the poles.** Ask for the pair `-2 + i` and `-2 - i`: a decay of
`exp(-2 t)` with a little oscillation.

```python
K = lin.place_poles([-2 + 1j, -2 - 1j])
K                                      # [[6.0, 3.8]]
np.linalg.eigvals(lin.A - lin.B @ K)   # -2 + 1i and -2 - 1i
```

This gain can be checked by hand. With `du = -(k1 dx1 + k2 dx2)` the
characteristic polynomial of `A - B K` is `s^2 + (c + k2) s + (k1 - 1)`.
The requested poles give `s^2 + 4 s + 5`, so `k2 = 4 - c = 3.8` and
`k1 = 6`. In the app: `poles -2+1i -2-1i`.

**LQR.** Now weigh the angle ten times more than the angular velocity, and
the torque by one:

```python
lqr = lin.lqr(Q=[10, 1], R=1)
lqr.K                                  # [[4.3166, 2.9102]]
lqr.poles                              # -1.5551 +- 0.9478i
```

In the app: `lqr Q 10 1 R 1`. LQR gives a gentler gain than the poles
above, and finds poles by itself.

**Close the loop.** `closed_loop` writes the model with the torque replaced
by the feedback law `u = u0 - K (x - x*)`, loads it, and keeps the initial
values and settings. It is the full nonlinear model:

```python
closed = lin.closed_loop(K)
sol = closed.run(th=0.5, w=0)
sol["th"][-1]                          # 0, to 1e-12: it is held upright
```

In the app, `closedloop` does the same and runs it: the angle, drawn as a
solid line, decays to zero. The simulation checks what the linear model
only promises. The feedback built for small tilts also holds a pendulum
that starts at `th = 3`, close to hanging, because the torque it asks for
is large enough to swing it up.

```python
far = closed.run(th=3.0, w=0)
far["th"][-1]                          # 0, to 1e-12
peak = abs(K[0, 0] * far["th"] + K[0, 1] * far["w"]).max()
peak                                   # 18.0: the largest torque used
```

That torque is `k1 * 3` at the start. A real motor has a limit, and the
linear design does not know it. The exercises compare gains.

!!! note "Exercises"
    1. Place the poles at `-1`, `-1.5` (real) and then at `-4 + 4i`,
       `-4 - 4i`. How do the gains and the peak torque for `th = 0.5`
       change? What does faster mean for the motor?
    2. Compute the LQR gain for `R = 0.01`, `1` and `100` with
       `Q = [10, 1]`. Which is the gentle one?
    3. Set `u = 0`, `th = 0.001`, `w = 0` and run the open loop. How many
       time units does the angle take to grow from 0.01 to 0.1? Compare with
       `ln(10)` over the positive eigenvalue.

??? success "Solutions"
    1. The first-order gain needs `k1 = 1 + 1.5 = 2.5` and
       `k2 = 2.5 - c = 2.3` (the polynomial is `s^2 + 2.5 s + 1.5`). The
       faster pair needs `k1 = 33` and `k2 = 7.8`. The peak torque from
       `th = 0.5` is 1.25 for the first pair, 3.0 for the pole pair of the
       text and 16.5 for the fast one: the faster the poles, the larger the
       gain and the torque.
    2. `R = 0.01` gives `K = [32.64, 12.66]`, `R = 1` gives
       `[4.317, 2.910]`, `R = 100` gives `[2.049, 1.837]`. A large `R` is
       the gentle controller. Its poles are slower.
    3. 2.55 time units, against `ln(10)/0.905 = 2.54`.

## Example 2: an unstable reactor

A continuous stirred-tank reactor with an exothermic first order reaction
(the dimensionless model of Uppal, Ray and Poore [UR], textbook form [FPE
ch. 7]). The state is the conversion `x1` and the temperature `x2`
(relative to the feed), and the input `uc` is the temperature of the
coolant, relative to its nominal value:

```ode
# cstr.ode
x1' = -x1 + da*(1-x1)*exp(x2/(1+x2/gam))
x2' = -x2 + b*da*(1-x1)*exp(x2/(1+x2/gam)) - beta*(x2 - uc)
par da=0.072, b=8, beta=0.3, gam=20, uc=0
init x1=0.5, x2=3
@ total=30, dt=0.05, meth=rungekutta
done
```

With these parameters the reactor has three steady states. The lowest and
the highest are stable. The middle one is a saddle: the operating point
you may want, with a conversion near 45%, between a cold and little
converted state and a hot, highly converted one. Start Newton's method
near it:

```python
cstr = xs.load("cstr.ode")
cstr.set(x1=0.45, x2=2.7)
eq = cstr.equilibrium()
eq.kind                                # saddle
eq["x1"], eq["x2"]                     # 0.4472, 2.7517
eq.eigenvalues                         # -0.8378 and 0.4958
```

Without control, the reactor runs away from that state, to the hot one
(`x2` near 4.7):

```python
cstr.set(x1=0.5, x2=3.0)
cstr.run()["x2"][-1]                   # 4.705
```

Linearise with the coolant temperature as input, and test controllability:

```python
lin = cstr.linearize(inputs="uc")
lin.A                                  # [[-1.809, 0.346], [-6.471, 1.464]]
lin.B                                  # [[0], [0.3]]
lin.rank, lin.controllable             # 2, True
lin.valid_radius()                     # 0.060
```

The coolant acts on the temperature only, but the temperature acts on the
conversion, so both are reachable. The linear model is valid in a small
region around the point (a radius of about 0.06), much smaller than the
distance to the other steady states.

Place the poles at `-1` and `-1.5`, and compute an LQR gain with the same
weights as before. Then run the closed loop from a start away from the
steady state:

```python
K = lin.place_poles([-1, -1.5])
K                                      # [[-19.16, 7.185]]
lqr = lin.lqr(Q=[10, 1], R=1)
lqr.K                                  # [[-10.56, 4.172]]
lqr.poles                              # -0.798 +- 0.346i

closed = lin.closed_loop(lqr.K)
closed.set(x1=0.5, x2=3.0)
sol = closed.run()
sol["x1"][-1], sol["x2"][-1]           # 0.4472, 2.7517
```

The open loop from `(0.5, 3.0)` went to the hot state. The same start with
feedback goes to the middle one and stays. In the app, load `cstr.ode`,
run `eq` after setting the initial values to 0.45 and 2.7, then
`linearize at eq inputs uc`, `lqr Q 10 1 R 1` and `closedloop`.

The closed-loop model is also a file, at `closed.path`. The input is gone
from its parameters and a line defines it from the states:

```python
law = next(s for s in open(closed.path) if s.startswith("uc="))
law                                    # shortened: uc=0-(-10.56)*(X1-(0.4472))-(4.172)*(X2-(2.7517))
```

The file has the numbers at full precision; the comment shows them to four
digits. This is `uc = 0 - K (x - x*)`, written out.

Read the gain sign: when the reactor is too hot, `uc` falls, the coolant
is colder, and the temperature is pulled back.

!!! note "Exercises"
    1. Change the start to `(0.2, 1.0)`, in the basin of the cold state,
       and then to `(0.9, 8.0)`, near the hot state. Does the closed loop
       still reach the middle state?
    2. Make the weights `Q = [1000, 1]`. What happens to the gain, and
       to the poles?
    3. Remove the input: `linearize` without `inputs`. What does
       `place_poles` say?

??? success "Solutions"
    1. Both starts reach it, as do the other starts we tried. The radius
       of 0.06 is where the linear model is accurate, not where the
       feedback works, which is much larger here. Only the simulation of
       the full model tells.
    2. The gain on the conversion falls to -6.22 and the gain on the
       temperature grows to 8.12. The poles become `-1.39 +- 1.19i`: faster
       and more oscillatory than with `Q = [10, 1]`.
    3. It raises a `ValueError`: no inputs were chosen, and there is
       nothing to place the poles with.

## Where to go next

- [Command reference](../manual/commands.md): `linearize`, `poles`, `lqr`,
  `locus gain` (how the poles move as the gain grows) and `closedloop`.
- [Python API reference](../python/reference.md): `Linearization`,
  `place_poles`, `lqr`, `closed_loop` and `root_locus`.
- The `notebooks/` folder has a control notebook with the same calls on
  the Morris-Lecar model.
- [Chemistry](chemistry.md) and [Mechanics](mechanics.md), for bistable
  reactions and the pendulum.

## References

- **[AM]** Åström, K. J. and Murray, R. M. (2021). *Feedback Systems: An
  Introduction for Scientists and Engineers*, 2nd edition. Princeton:
  Princeton University Press. Chapter 7 (state feedback, reachability and
  pole placement) and chapter 8 (linear quadratic regulator).
- **[FPE]** Franklin, G. F., Powell, J. D. and Emami-Naeini, A. (2019).
  *Feedback Control of Dynamic Systems*, 8th edition. Pearson. Chapter 7
  (state-space design).
- **[UR]** Uppal, A., Ray, W. H. and Poore, A. B. (1974). On the dynamic
  behavior of continuous stirred tank reactors. *Chemical Engineering
  Science* 29, 967–985.
