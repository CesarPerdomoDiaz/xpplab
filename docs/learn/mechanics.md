# Mechanics: pendulums, buckling beams, oscillators and chaos

Newton's law makes a second order differential equation, `m x'' = F`. To
give it to an ODE solver, write it as two first order equations: the
position `x` and the velocity `v`, with `x' = v` and `v' = F/m`. The phase
plane `(x, v)` is then the natural picture, and the questions of mechanics
become the questions of this site: where are the equilibria, are they
stable, and what happens when a parameter changes?

This page has four examples:

1. the **damped pendulum**: energy, a stable and an unstable equilibrium,
   and the escape over the top;
2. the **buckling beam** (a double-well oscillator): a pitchfork
   bifurcation, where symmetry breaks;
3. the **van der Pol oscillator**: self-sustained oscillations that turn
   into relaxation oscillations;
4. the **Lorenz equations**, a model of a fluid in a loop: chaos and
   sensitive dependence on initial conditions.

Save each model under the name in its first line, in the folder where you
run the Python. The snippets of a page run one after the other, in a single
script. The ideas used (equilibria, eigenvalues, bifurcations) are
summarised at the start of the
[neuroscience page](neuroscience.md#theory-in-five-paragraphs).

## Theory for mechanical systems

**Conservative and dissipative systems.** If the force comes from a
potential `U(x)`, the energy `E = v^2/2 + U(x)` is constant along the
motion. Orbits are the level curves of `E`: closed curves around the minima
of `U` (the stable equilibria) and curves that touch the maxima (the
unstable ones). Friction takes energy away, `E' = -c v^2` for a linear
damping `c`, and the closed orbits become spirals that end in a minimum.

**Equilibria of `x'' = F(x)`.** At an equilibrium `v = 0` and `F(x*) = 0`.
The Jacobian is `[[0, 1], [F'(x*), -c]]`. If `F' < 0` (a minimum of the
potential) the equilibrium is a stable node or focus when `c > 0`. If
`F' > 0` (a maximum) it is a saddle for every `c`.

**Symmetry breaking.** When a system has a symmetry (`x -> -x`), an
equilibrium on the symmetry axis can lose stability to a pair of
equilibria that the symmetry exchanges. This is the **pitchfork
bifurcation**; in a structure it is **buckling**.

**Self-sustained oscillations.** A damped oscillator comes to rest. To
oscillate for ever the system needs an energy source, with negative damping
at small amplitude and positive damping at large amplitude: a **limit
cycle** that attracts nearby orbits. Clocks, bowed strings, and the heart
are such oscillators.

**Chaos.** A dissipative system with three or more variables can have an
attractor on which nearby orbits separate exponentially fast, so that the
long term behaviour cannot be predicted, although the equations are
deterministic.

## Example 1: the damped pendulum

A pendulum of unit length and unit mass, with friction proportional to the
angular velocity `c`:

```ode
# pend.ode
x' = v
v' = -sin(x) - c*v
par c=0.2
init x=2, v=0
@ total=60, dt=0.02, xp=x, yp=v, xlo=-4, xhi=8, ylo=-3, yhi=3
done
```

The angle `x` is measured from the hanging position, so the equilibria are
`x = 0` (hanging down) and `x = pi` (balanced upright), and also all
`2 pi k` and `pi + 2 pi k`, which are the same positions after full turns.

**Hanging down.** The Jacobian is `[[0, 1], [-1, -c]]`, with eigenvalues
`-c/2 ± i sqrt(1 - c^2/4)` = `-0.1 ± 0.995i` for `c = 0.2`. A **stable
focus**: the pendulum swings back and forth with a slowly decaying
amplitude, with frequency near 1.

**Upright.** The Jacobian is `[[0, 1], [1, -c]]`, with real eigenvalues of
opposite signs, 0.905 and -1.105: a **saddle**. A perfectly balanced
pendulum is an equilibrium; any push, however small, takes it away.

**Energy.** `E = v^2/2 + 1 - cos(x)` is zero at the bottom and 2 at the
top. Along the motion `E' = -c v^2`, which is never positive.

```python
import numpy as np
import xpplab as xs

pend = xs.load("pend.ode")
down = pend.equilibrium({"x": 0.1, "v": 0})
up = pend.equilibrium({"x": 3.0, "v": 0})
down.kind, down.eigenvalues            # 'stable focus', -0.1 +/- 0.995i
up.kind, up.eigenvalues                # 'saddle', 0.905 and -1.105

sol = pend.run()
E = 0.5 * sol["v"] ** 2 + 1 - np.cos(sol["x"])
E[0], E[-1]                            # 1.416, 1e-5
np.all(np.diff(E) <= 1e-9)             # True: energy never increases
```

**Escaping over the top.** From the bottom (`x = 0`) with an initial
velocity `v0`, the pendulum needs enough energy to reach the top. Without
friction that is `v0 = 2`. With friction some energy is lost on the way,
and the threshold is higher; for `c = 0.2` it is `v0 = 2.415`.

```python
pend.set(total=100)
slow = pend.run(x=0, v=1.0)            # swings and settles at x = 0
fast = pend.run(x=0, v=2.5)            # goes over the top once
slow["x"][-1] / np.pi, fast["x"][-1] / np.pi    # 0.0 and 2.0: one turn more
```

The phase plane (`pend.phase_plane("x", "v")`, or the app's **Phase Plane**
tab) shows the picture: spirals around the points `x = 2 pi k`, and the
stable manifolds of the saddles, which separate the starting points that
end in one well from those that end in the next. Use
`pend.manifolds({"x": 3.1, "v": 0})` to compute those manifolds: it returns
the saddle and four branches, two unstable and two stable.

!!! note "Exercises"
    1. Show that `E' = -c v^2`. With the run above, check that the energy
       never increases.
    2. Without friction (`c = 0`) the period of oscillation depends on the
       amplitude. Measure it from `x = 2` and from `x = 3`. The exact
       period for amplitude `a` is `4 K(sin(a/2))`, with `K` the complete
       elliptic integral; it is `2 pi` for small `a` and tends to infinity
       as `a -> pi`.
    3. Find the threshold velocity 2.415 with a bisection, using `pend.run`
       and the final value of `x`.
    4. With `c = 0.2` the swings of a pendulum released from `x = 2` shrink.
       Find the successive maxima of `x`, and fit a line to their logarithm
       against time. What rate do you expect from the eigenvalues?

??? success "Solutions"
    1. `E' = v v' + sin(x) x' = v(-sin(x) - c v) + sin(x) v = -c v^2`.
       The last line of the code checks the same on the run.
    2. Set `c = 0` and `total=100`, and take the time between the maxima of
       `x` (the points where `v` crosses zero downwards). The period is 8.34
       for `x = 2` (the exact value is 8.350) and 16.16 for `x = 3` (the
       exact value 16.156). The small oscillation value is 6.283.
    3. The bisection on `v0` between 1 and 3, with the criterion
       `abs(x[-1]) > pi`, converges to 2.4149.
    4. The maxima of `x` are 0.887 at `t = 7.32`, 0.456 at `t = 13.82`,
       0.240 at `t = 20.18`, and so on. The fitted line has slope -0.1003.
       The eigenvalues of the hanging equilibrium have real part
       `-c/2 = -0.1`: the amplitude decays at that rate.

## Example 2: the buckling beam and the pitchfork

A thin elastic beam is compressed by a force `mu`. A simple model of its
lateral displacement `x` has a double-well potential:

```
x'' = mu x - x^3 - c x'
```

For `mu < 0` the potential `U = -mu x^2/2 + x^4/4` has one well at
`x = 0`: the beam is straight. For `mu > 0` the point `x = 0` is a maximum
and there are two wells at `x = ±sqrt(mu)`: the beam is bent to the left or
to the right.

```ode
# buckle.ode
x' = v
v' = mu*x - x^3 - c*v
par mu=-0.5, c=0.5
init x=0.1, v=0
@ total=100, dt=0.05, xp=x, yp=v, xlo=-2, xhi=2, ylo=-2, yhi=2
done
```

The equilibria are `v = 0` and `x (mu - x^2) = 0`. For `mu < 0` the only
equilibrium is `x = 0`, a stable focus or node. At `mu = 0` it loses
stability by a real eigenvalue crossing zero, and for `mu > 0` there are
three equilibria: the straight beam `x = 0`, now a saddle, and the two bent
states `x = ±sqrt(mu)`, both stable. This is the **supercritical
pitchfork bifurcation**: the symmetric state gives way to two states that
are mirror images.

```python
beam = xs.load("buckle.ode")
beam.parameters["mu"] = 1.0
found = beam.find_equilibria(100, box={"x": (-3, 3), "v": (-3, 3)})
sorted((round(e["x"], 3), e.kind) for e in found)
# [(-1.0, 'stable focus'), (0.0, 'saddle'), (1.0, 'stable focus')]

beam.run(x=0.05, v=0)["x"][-1]         # +1.0
beam.run(x=-0.05, v=0)["x"][-1]        # -1.0
```

The tiniest initial asymmetry decides which way the beam bends: that is the
symmetry breaking. Now follow the equilibria, with AUTO from the straight
state and with the equilibrium sweep from a bent one:

```python
beam.parameters["mu"] = -0.5
beam.initial_conditions.update(x=0, v=0)
d = beam.auto.equilibria("mu", parmin=-1, parmax=2)
d.special("BP")[0].value               # 0.0: the branch point

mus = np.linspace(0.25, 2, 8)
tab = beam.equilibrium_sweep("mu", mus, guess={"x": 1, "v": 0})
np.abs(tab["x"] - np.sqrt(mus)).max()  # below 1e-5: x = sqrt(mu)
```

AUTO marks the straight branch with a branch point (BP) at `mu = 0`, where
the two bent branches leave it.

!!! note "Exercises"
    1. Show from the Jacobian at `x = 0` that the eigenvalues are
       `(-c ± sqrt(c^2 + 4 mu))/2`, and that one crosses zero at `mu = 0`.
       For `c = 0.5`, is the straight beam a focus or a node for
       `mu = -0.5`? And at `mu = -0.01`?
    2. For `mu = 1` the bent states have the eigenvalues
       `(-c ± sqrt(c^2 - 8 mu))/2`. Are they a focus or a node? Check with
       `beam.equilibrium`.
    3. Change the cubic to `+ x^3` in `v'` with `mu` of opposite sign (a
       **subcritical** pitchfork): `v' = mu*x + x^3 - c*v`. Where are the
       equilibria for `mu < 0`? What happens to a trajectory started near
       `x = 0` for `mu > 0`?
    4. Add a small constant force `f` to the right side, `v' = ... + f`.
       The pitchfork is broken (an **imperfect bifurcation**). For `f = 0.1`,
       where is the fold, and which states does it create? Start AUTO from
       the bent state `x = -1` at `mu = 1` and decrease `mu` (give
       `ds=-0.02` to `auto.equilibria`).

??? success "Solutions"
    1. `J = [[0, 1], [mu, -c]]`: trace `-c`, determinant `-mu`. The
       eigenvalues are `(-c ± sqrt(c^2 + 4 mu))/2`. One is zero when
       `mu = 0`. For `c = 0.5` and `mu = -0.5` the square root is of
       `0.25 - 2 < 0`: a stable focus. At `mu = -0.01` it is of `0.21 > 0`:
       a stable node.
    2. The square root is of `0.25 - 8 < 0`, so complex: a stable focus,
       as in the run (it prints `'stable focus'`).
    3. With `v' = mu*x + x^3 - c*v` the equilibria for `mu < 0` are
       `x = 0` and `x = ±sqrt(-mu)`; the outer ones are saddles and the
       straight one is stable. For `mu > 0` only `x = 0` is left, a saddle,
       and trajectories run off to infinity: nothing in the model limits the
       bending. A subcritical pitchfork is a sudden, dangerous buckling.
    4. With `f > 0` the straight state is no longer an equilibrium: the
       branch that comes from `mu < 0` bends smoothly into the right-hand
       bent state, with no bifurcation. The other bent state and the saddle
       are born together, in a saddle-node at `mu = 3 (f/2)^(2/3)` = 0.4072
       for `f = 0.1`. AUTO, started from `x = -1` at `mu = 1` and
       decreasing `mu`, reports a limit point (LP) at 0.4072.

## Example 3: the van der Pol oscillator

The van der Pol oscillator is a spring with a damping that changes sign:

```
x'' - mu (1 - x^2) x' + x = 0
```

For `|x| < 1` the damping is negative and pumps energy in; for `|x| > 1`
it is positive and removes it. The balance is a limit cycle.

```ode
# vdp.ode
x' = y
y' = mu*(1-x^2)*y - x
par mu=1
init x=2, y=0
@ total=100, dt=0.01
done
```

**Small `mu`.** The origin is the only equilibrium. Its Jacobian
`[[0, 1], [-1, mu]]` has trace `mu` and determinant 1: the eigenvalues are
`mu/2 ± i sqrt(1 - mu^2/4)`. For `mu < 0` the origin is stable; for
`mu > 0` it is unstable, and every orbit goes to one limit cycle, of
amplitude about 2 and period close to `2 pi` for small `mu`.
(At `mu = 0` the system is a linear center, a degenerate Hopf point.
AUTO's branch of periodic orbits from it stays at `mu = 0` while the
amplitude grows, which tells you nothing about the cycle; simulate
instead.)

**Large `mu`.** The motion is a **relaxation oscillation**. In Liénard's
coordinates, `x' = mu (w - (x^3/3 - x))` and `w' = -x/mu`, the state stays
close to the cubic `w = x^3/3 - x` for a long time, creeping along it, then
jumps horizontally to the other branch. The period grows in proportion to
`mu`:

```
T = (3 - 2 ln 2) mu + 3 a mu^(-1/3) + ...,    a = 2.338
```

where `a` is the first zero of the Airy function (negated), and
`3 - 2 ln 2 = 1.614`.

```python
vdp = xs.load("vdp.ode")

def cycle(mu, total):
    vdp.set(total=total, meth="stiff")
    s = vdp.run(mu=mu)
    x = s["x"]
    up = s.t[1:][(x[:-1] < 0) & (x[1:] >= 0)]      # upward zero crossings
    return np.diff(up)[-1], x[len(x) // 2:].max()

for mu in (0.1, 1, 10, 50):
    period, amplitude = cycle(mu, total=40 * max(mu, 2))
    print(mu, round(period, 2), round(amplitude, 3))
# 0.1  6.29  2.000
# 1    6.66  2.009
# 10   19.08 2.014
# 50   82.50 2.003
```

The amplitude barely changes (it stays near 2), but the period goes from
6.29 to 82.5. The formula above gives 19.39 for `mu = 10` and 82.59 for
`mu = 50`; for `mu = 50` it agrees to within 0.2%.
For large `mu` use `meth="stiff"`: the fast jumps and the slow creeping
need very different step sizes.

!!! note "Exercises"
    1. Show that for `mu = 0.1` the origin is an unstable focus with a
       growth rate `mu/2 = 0.05`, and that the amplitude of the limit cycle
       is 2 to three digits.
    2. Plot `x(t)` for `mu = 0.1`, 1, and 10 on the same axes. How does the
       shape change?
    3. The period 6.66 for `mu = 1` is not `2 pi`. Why is the period of a
       nonlinear oscillator not determined by the linear part?
    4. Measure the period for `mu = 20` and compare it with the formula.

??? success "Solutions"
    1. The eigenvalues are `0.05 ± 0.9987i`. The limit cycle amplitude in
       the run is 2.0001. Averaging theory gives exactly 2 at leading
       order.
    2. For `mu = 0.1` it is a sine wave; for `mu = 1` it is slightly
       distorted; for `mu = 10` it is a sawtooth-like shape with long slow
       parts and fast jumps.
    3. On the cycle the amplitude is 2, not small, so the nonlinear term is
       not negligible: the period is a property of the whole cycle, and
       changes with `mu` (6.29, 6.66, 11.6, 19.08, 82.5 for `mu` = 0.1, 1,
       5, 10, 50).
    4. The measured period is 34.68; the formula gives 34.86. The relative
       difference, 0.5%, is smaller than for `mu = 10` (1.6%) and shrinks
       as `mu` grows.

## Example 4: the Lorenz equations and chaos

Lorenz derived three equations from a model of convection in a fluid
layer, heated from below. The same equations describe a water wheel with
leaking buckets (see `waterwheel.ode` in the examples):

```ode
# lorenz.ode
x' = s*(y-x)
y' = r*x - y - x*z
z' = x*y - b*z
par s=10, r=28, b=2.6666667
init x=1, y=1, z=1
@ total=50, dt=0.005, xp=x, yp=z, xlo=-25, xhi=25, ylo=0, yhi=50
done
```

Write `b` as a decimal: in a `par` line XPPAUT reads `b=8/3` as `b=8`,
because the value there must be a number, not an expression.

The origin is an equilibrium for every `r`. It is stable for `r < 1`. At
`r = 1` it loses stability in a pitchfork and two new equilibria,
`C+` and `C-` at `x = y = ±sqrt(b (r - 1))`, `z = r - 1`, appear. They are
the convection rolls turning one way or the other. For `s = 10`,
`b = 8/3` they lose stability in a Hopf bifurcation at

```
r = s (s + b + 3)/(s - b - 1) = 24.74
```

and for `r = 28` they are unstable and the origin too, but the orbits stay
bounded: they wind around `C+` for a while, switch to `C-`, and so on, in
an irregular pattern that never repeats. The set they fill is the
**strange attractor**, the butterfly.

```python
lor = xs.load("lorenz.ode")
c_plus = lor.equilibrium({"x": 8, "y": 8, "z": 27})
c_plus.kind                            # 'saddle-focus'
c_plus["x"], c_plus["z"]               # 8.485, 27.0
c_plus.eigenvalues                     # -13.85 and 0.094 +/- 10.19i

lor.parameters["r"] = 20               # below the Hopf value
lor.equilibrium({"x": 7, "y": 7, "z": 19}).kind     # 'stable focus' (r = 20)
```

**Sensitive dependence.** Run the system twice, with initial values of `x`
that differ by 1e-6:

```python
lor.parameters["r"] = 28
a = lor.run()
lor.initial_conditions["x"] = 1.000001
b = lor.run()
lor.initial_conditions["x"] = 1.0
gap = abs(a["x"] - b["x"])
a.t[np.argmax(gap > 1)]                # 27.3: the two runs are no longer alike
gap.max()                              # 30: as far apart as the attractor is wide
```

At first the two runs are indistinguishable. The gap grows
exponentially, a factor of about e every two time units, until at
`t = 27.3` it is of order one, and then the two runs have nothing to do
with each other. Halving the initial difference buys only about one more
time unit of prediction. This is **sensitive dependence on initial conditions**,
the property that makes long term weather forecasts impossible, and the
reason that the numerical method and the step size matter for the details
of a chaotic orbit but not for its statistics.

!!! note "Exercises"
    1. Compute the Hopf value `r = 24.74` from the formula and confirm it:
       find the eigenvalues of `C+` at `r = 24` and at `r = 25`.
    2. Plot `z` against `x` (the butterfly): `a.plot("x", "z")`, or the
       app's Phase Plane tab with `xp=x, yp=z`. Does the orbit switch from
       one wing to the other at regular intervals?
    3. For `r = 10` and `r = 20` run from `x = y = z = 1`. Where do the
       orbits end? Compare with `x = ±sqrt(b (r - 1))`.

??? success "Solutions"
    1. `10 (10 + 2.6667 + 3)/(10 - 2.6667 - 1) = 24.737`. At `r = 24` the
       equilibrium `C+` has the eigenvalues `-13.62` and `-0.0225 ± 9.49i`:
       stable. At `r = 25` they are `-13.68` and `+0.0079 ± 9.67i`: unstable.
    2. No: the number of turns around one wing before the orbit crosses to
       the other is irregular, and never settles into a pattern.
    3. For `r = 10` the orbit ends at `C+`, `x = 4.899`; for `r = 20` at
       `C-`, `x = -7.118`; both agree with `±sqrt(b (r - 1))`. Which of the
       two is reached depends on the start.

## Where to go next

- The examples folder has `pend.ode` and `invpend.ode` (the inverted
  pendulum with an animation), `doubpend.ode` (a double pendulum),
  `waterwheel.ode` and `lorenz.ode`. Open them in the app: the `.ani`
  animation files make the motion visible.
- [Bifurcation analysis with AUTO](../manual/auto.md) for branch points and
  their continuation.
- [Data analysis](../python/analysis.md) for spectra, the Lyapunov exponent
  and the period of a limit cycle.
