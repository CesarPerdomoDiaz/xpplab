# Mechanics: the pendulum and its relatives

This page studies mechanical systems with the tools of dynamical systems:
equilibria and their stability, bifurcations, limit cycles and chaos. It
follows the last three chapters of *Mecánica analítica* by F. J. Sanz Recio
and G. Sánchez Arriaga, cited as **[MA]** below. That book builds the theory
on a handful of mechanical examples, the pendulum above all, and this page
uses the same examples and notation. A reference such as [MA §8.2.3, p. 199]
points to the section of the book where the full derivations, figures and
more examples are. The book is in Spanish; the complete references are at
the [end of the page](#references).

The five examples:

1. the **pendulum**, without and with friction: energy, centres and saddles,
   separatrices, and a period that depends on the amplitude
   [MA §6.6.1, §8.2];
2. the **pendulum on a rotating hoop**: a pitchfork bifurcation
   [MA §6.6.1, §9.1.1];
3. the **pendulum with a vibrating support**: the upside-down position made
   stable, and parametric resonance [MA §9.2.1, §8.4.3];
4. the **van der Pol oscillator** as a model of flutter: a Hopf bifurcation
   and the method of averaging [MA §9.1.1, §9.2.3];
5. the **forced, damped pendulum**: period doubling and chaos
   [MA §8.1, §10.1, §10.3].

Save each model under the name in its first line, in the folder where you
run the Python. The snippets of a page run one after the other, in a single
script.

## Theory for mechanical systems

**From Lagrange's equations to a dynamical system.** Lagrange's equations
for a system with `n` degrees of freedom are `n` second order equations in
the generalised coordinates `q`. Take the coordinates and their velocities
together as the state, `x = (q, q')`, and they become `2n` first order
equations, `x' = f(x, t)`: the form that an ODE solver, and XPPAUT, needs.
The system is *autonomous* when `t` does not appear in `f` [MA §8.1,
p. 190]. Most of this page uses one equation, the pendulum with viscous
friction and a periodic force [MA eq. (8.2), p. 191]:

```
θ'' + 2γ θ' + ω0² sin θ = a0 cos Ωt
```

Here `θ` is the angle from the downward vertical, `ω0 = sqrt(g/ℓ)` the
frequency of small swings, `γ` the friction coefficient, and `a0` and `Ω`
the amplitude and frequency of the force. The state is `(θ, θ')`.

**First integrals and the energy function.** A function of the state that
keeps its value along every motion is a *first integral* [MA §1.4.1, p. 22].
When the Lagrangian does not depend on time explicitly, the *energy
function* `E = Σ q_i' ∂L/∂q_i' − L` is one [MA §1.4.2–1.4.3, pp. 22–23]. It
equals the familiar kinetic plus potential energy only when the kinetic
energy is a quadratic form in the velocities; Example 2 has a case where it
is not. With one degree of freedom a first integral decides the shape of
the orbits, which are its level curves in the phase plane, and the time
along them follows from one integral (a *quadrature*) [MA §6.6.1, p. 133].

**Volume in the state space.** Follow a small blob of initial states as the
flow carries it: the divergence `∇·f` is the rate at which its volume grows,
per unit volume [MA §8.1.3, p. 193]. Hamiltonian systems keep the volume
[MA §6.6.4, p. 139]; friction makes it shrink. A system whose volumes shrink
ends on *attractors*: equilibria, limit cycles, or strange attractors. For
the pendulum, `∇·f = −2γ`.

**Equilibria and their stability.** An equilibrium `x_e` solves
`f(x_e) = 0`. A small perturbation `y` of it obeys `y' = A y`, where `A` is
the Jacobian matrix of `f` at `x_e`, so it grows or decays like `exp(λt)`
with `λ` the eigenvalues of `A`. If all of them have negative real part the
equilibrium is asymptotically stable; if one has positive real part it is
unstable [MA §8.2.1, pp. 195–196]. In the plane the eigenvalues solve
`λ² − tr(A) λ + det(A) = 0`, and their signs and the sign of the
discriminant sort the equilibria into nodes, saddles, foci and centres
[MA §8.2.2 and Box 8.6, p. 197]. A Hamiltonian system with one degree of
freedom has only centres and saddles: nodes and foci need areas to shrink
or grow [MA §6.6.1, p. 134; §8.2.2, p. 198].

**When the eigenvalues are not enough: Lagrange's theorem.** At a centre the
eigenvalues are purely imaginary, and the linear analysis cannot tell
whether the neglected nonlinear terms make the point stable or unstable. A
first integral can. If it has a strict minimum (or maximum) at the
equilibrium, the orbits that start nearby stay on its small closed level
surfaces around the point, so the equilibrium is stable [MA §8.2.3,
p. 199]. For a Lagrangian `L = T − U(q)` this says that a strict minimum of
the potential is a stable equilibrium.

**Bifurcations.** As a parameter changes, an equilibrium can change its
stability when an eigenvalue crosses the imaginary axis. A real eigenvalue
crossing zero gives a saddle-node, a pitchfork or a transcritical
bifurcation; a pair of complex eigenvalues crossing gives a Hopf
bifurcation, where a branch of periodic orbits is born [MA §9.1.1,
pp. 223–224]. A periodic orbit changes its stability when one of its Floquet
multipliers (the eigenvalues of the map over one period) leaves the unit
circle [MA §8.4.2, §9.1.2]. AUTO follows these branches and labels the
special points (BP, LP, HB, PD, ...).

**Chaos.** A chaotic orbit stays in a bounded region, never repeats, and
separates from its neighbours exponentially fast:
`|δx(t)| ≈ |δx0| exp(λ_L t)`, with `λ_L > 0` the largest *Lyapunov
exponent* (called `μ` in [MA]). An initial error `δx0` then allows
predictions for about the *Lyapunov time* `ln(L/|δx0|) / λ_L`, with `L` the
size of the attractor [MA §10.1, pp. 248–249]. An autonomous system needs at
least three dimensions to be chaotic; a periodically forced one, such as the
pendulum above, needs only two [MA §10.1, p. 250].

## Example 1: the pendulum

Divide the Lagrangian of a pendulum of mass `m` and length `ℓ` by `mℓ²`:
`L = θ'²/2 + ω0² cos θ`. The momentum is `p = θ'`, and the energy function
is `E = p²/2 − ω0² cos θ` [MA §6.6.1, p. 134]. The equation of motion is
the one above with `a0 = 0`:

```ode
# pend.ode
th' = p
p' = -2*gam*p - w0^2*sin(th)
par gam=0, w0=1
init th=2, p=0
@ total=40, dt=0.01, xp=th, yp=p, xlo=-4, xhi=10, ylo=-3, yhi=3
done
```

In the file, `th` is `θ`, `p` is `θ'`, `gam` is `γ` and `w0` is `ω0`. With
`w0 = 1` the time is measured in units of `1/ω0`.

**Without friction** (`γ = 0`) the energy function is conserved. Its
smallest value, `E0 = −ω0²`, is the pendulum at rest hanging down. The
equilibria are `θ = 0` (hanging) and `θ = π` (upright), and their copies a
whole turn away. At the bottom the Jacobian is `[[0, 1], [−ω0², 0]]`, with
eigenvalues `±iω0`: a centre. At the top it is `[[0, 1], [ω0², 0]]`, with
eigenvalues `±ω0`: a saddle. Small orbits around the bottom are ellipses;
as `E0` grows they stretch, and at `E0 = ω0²` they reach the saddles. Above
that energy the pendulum no longer swings but goes round and round. The
orbits with `E0 = ω0²`, the *separatrices*, divide swinging from rotating,
and take an infinite time to reach the saddle [MA §6.6.1, pp. 134–135 and
Fig. 6.5; §7.6.1].

```python
import numpy as np
import xpplab as xs

pend = xs.load("pend.ode")
bottom = pend.equilibrium({"th": 0.1, "p": 0})
top = pend.equilibrium({"th": 3.0, "p": 0})
bottom.eigenvalues                     # +1j and -1j
top.kind, top.eigenvalues              # 'saddle', +1 and -1

sol = pend.run()
E = sol["p"] ** 2 / 2 - np.cos(sol["th"])
E[0], np.ptp(E)                        # 0.416, and a spread below 1e-6
```

`bottom.kind` prints `'stable focus'`: the program sorts equilibria by the
sign of the real parts of the eigenvalues, and here they are zero up to
rounding. The linear analysis cannot tell a centre from a weakly stable or
unstable focus. Lagrange's theorem can: `E` has a strict minimum at the
bottom, so the bottom is stable, though not asymptotically stable, since
nearby orbits circle it for ever [MA §8.2.3, p. 199].

**The period depends on the amplitude.** Only small swings take the same
time whatever their size [MA Box 8.7, p. 198]. A swing of amplitude `a`
takes four times the time from `θ = 0` to `θ = a`, which is the quadrature
of [MA §6.6.1, eq. (6.63)] with `p = sqrt(2ω0² (cos θ − cos a))`. The
change of variable `sin(θ/2) = k sin φ`, with `k = sin(a/2)`, removes the
infinity of the integrand at the turning point and gives

```
T(a) = (4/ω0) ∫ dφ / sqrt(1 − k² sin² φ),   φ from 0 to π/2
```

(a complete elliptic integral of the first kind). `T` grows without bound as
`a → π`, the separatrix.

```python
def period(s):
    p = s["p"]
    down = s.t[1:][(p[:-1] > 0) & (p[1:] <= 0)]    # p crosses 0 going down
    return np.diff(down).mean()

def quadrature(a, n=100000):
    phi = (np.arange(n) + 0.5) * (np.pi / 2) / n   # midpoint rule
    k = np.sin(a / 2)
    return 4 * np.sum(1 / np.sqrt(1 - (k * np.sin(phi)) ** 2)) * (np.pi / 2) / n

pend.set(total=200)
for a in (0.5, 2, 3, 3.1):
    print(a, round(period(pend.run(th=a, p=0)), 2), round(quadrature(a), 2))
# 0.5  6.38   6.38
# 2    8.35   8.35
# 3    16.16  16.16
# 3.1  21.04  21.04
```

**With friction** (`γ > 0`) the divergence is `−2γ`, and along the motion
`E' = −2γ p²`, which is never positive. At the bottom the Jacobian is
`[[0, 1], [−ω0², −2γ]]`, with eigenvalues `−γ ± sqrt(γ² − ω0²)`: a stable
focus when `γ < ω0` (the swings die out slowly) and a stable node when
`γ > ω0` (the pendulum creeps back without swinging) [MA §8.2.2, Example 1,
p. 198]. At the top the eigenvalues are `−γ ± sqrt(γ² + ω0²)`, real and of
opposite signs: a saddle, whatever the friction.

```python
pend.parameters["gam"] = 0.1
bottom = pend.equilibrium({"th": 0.1, "p": 0})
bottom.kind, bottom.eigenvalues        # 'stable focus', -0.1 +/- 0.995i

pend.set(total=40)
sol = pend.run(th=2, p=0)
E = sol["p"] ** 2 / 2 - np.cos(sol["th"])
E[-1], np.diff(E).max()                # -0.9994 (the bottom is -1), below 1e-8
```

Every orbit now ends hanging down, but at which copy `θ = 2πk` depends on
where it started. Push the pendulum from the bottom with a speed `p0`:
without friction it goes over the top when `E0 = p0²/2 − 1` reaches `1`,
that is for `p0 = 2`. With friction it loses energy on the way up, and
needs more:

```python
pend.set(total=100)
[round(pend.run(th=0, p=p0)["th"][-1] / (2 * np.pi)) for p0 in (2.0, 2.5, 4.5)]
# [0, 1, 2]: no turn, one turn, two turns before it settles
```

The boundaries between these outcomes are the stable manifolds of the
saddles at `θ = π + 2πk`: the curves of initial states that end exactly on
the upright position [MA §8.2.1, p. 196]. The app's **Phase Plane** tab
draws them; in Python, `pend.manifolds({"th": 3.14, "p": 0})` returns the
saddle and its four branches, two stable and two unstable.

!!! note "Exercises"
    1. Show that `E' = −2γ p²` along the motion.
    2. At which friction does the hanging equilibrium change from a focus
       to a node? Check with `pend.equilibrium` for `gam` = 0.9 and 1.1.
    3. For small swings, the Lindstedt-Poincaré method [MA §9.2.2,
       pp. 235–236] gives the frequency of `x'' + ω0² x = −αx² − βx³` as
       `ω ≈ ω0 + (9βω0² − 10α²) a² / (24ω0³)`. Expand `sin θ` to third
       order to find `α` and `β` for the pendulum, and show that
       `ω ≈ ω0 (1 − a²/16)`. Compare the period it predicts with the
       measured ones for `a` = 0.5, 1 and 2.
    4. With `gam = 0.1`, find by bisection the smallest `p0` that takes the
       pendulum over the top.

??? success "Solutions"
    1. `E' = p p' + ω0² sin θ θ' = p (−2γp − ω0² sin θ) + ω0² sin θ p
       = −2γ p²`.
    2. The discriminant `γ² − ω0²` changes sign at `γ = ω0 = 1`. For
       `gam = 0.9` the program prints `'stable focus'` (eigenvalues
       `−0.9 ± 0.436i`); for `gam = 1.1`, `'stable node'` (`−0.642` and
       `−1.558`).
    3. `sin θ ≈ θ − θ³/6`, so the equation is
       `θ'' + ω0² θ = (ω0²/6) θ³`: `α = 0` and `β = −ω0²/6`. Then
       `ω ≈ ω0 − 9 ω0⁴ a² / (6 · 24 ω0³) = ω0 (1 − a²/16)`. The predicted
       periods `2π / (1 − a²/16)` are 6.383, 6.702 and 8.378; the measured
       ones are 6.38, 6.70 and 8.35. The approximation is good while
       `a²/16` is small, and is off by 0.3% at `a = 2`.
    4. Bisect on `p0` between 1 and 3, with the criterion
       `abs(th[-1]) > pi` after a long run: it converges to 2.4149.

## Example 2: the pendulum on a rotating hoop

A bead slides without friction on a vertical hoop of radius `ℓ` that turns
about its vertical diameter at a constant rate `Ω0`. This is the same as a
pendulum whose plane of swing turns about the vertical [MA §6.6.1, p. 137;
§9.1.1, Example 2, p. 224]. With `θ` measured from the bottom, the
Lagrangian divided by `mℓ²` is

```
L = θ'²/2 + (Ω0²/2) sin² θ + ω0² cos θ
```

and the equation of motion is [MA eq. (9.2)]

```
θ'' = Ω0² sin θ (cos θ − ω0²/Ω0²)
```

The term `(Ω0²/2) sin² θ` is kinetic energy of the rotation, but it does
not contain `θ'`, so the kinetic energy is not a quadratic form in the
velocity. The energy function, `E = θ'²/2 − (Ω0²/2) sin² θ − ω0² cos θ`, is
still conserved, because `L` does not depend on time, but it is not the
kinetic plus potential energy: that one changes, since the motor that keeps
the hoop turning does work on the bead [MA §1.4.2–1.4.3]. `E` is the energy
of a particle moving in the *effective potential*
`U_ef = −ω0² cos θ − (Ω0²/2) sin² θ`.

**Equilibria.** `θ' = 0` and `sin θ (cos θ − ω0²/Ω0²) = 0`:

- `θ = 0`, the bottom. Linearising gives `θ'' = (Ω0² − ω0²) θ`: a centre
  while `Ω0 < ω0` (and a minimum of `U_ef`, so stable by Lagrange's
  theorem), a saddle when `Ω0 > ω0`.
- `θ = π`, the top: `θ'' = (Ω0² + ω0²)(θ − π)`, a saddle at every speed.
- `θ = ±θ*` with `cos θ* = ω0²/Ω0²`, which exist only when `Ω0 > ω0`. They
  are minima of `U_ef`, so stable, and the bead swings about them with the
  frequency `Ω0 sin θ*` [MA §9.1.1, Example 2].

At `Ω0 = ω0` the bottom loses its stability and hands it to two new
equilibria, mirror images of each other: a **supercritical pitchfork**
[MA §9.1.1, p. 223]. Spin the hoop faster than the pendulum's own frequency
and the bead climbs to one side. Which side is decided by the smallest
asymmetry in the start; the equations themselves do not prefer one.

The book's example has no friction. Here we add viscous friction `2γθ'`,
so that the centres become stable foci and the runs settle:

```ode
# hoop.ode
th' = p
p' = sin(th)*(Om^2*cos(th) - w0^2) - 2*gam*p
par Om=0.5, w0=1, gam=0.1
init th=0.5, p=0
@ total=100, dt=0.02, xp=th, yp=p, xlo=-3.5, xhi=3.5, ylo=-2, yhi=2
done
```

```python
hoop = xs.load("hoop.ode")
hoop.parameters["Om"] = 1.5            # faster than w0 = 1
straight = hoop.equilibrium({"th": 0, "p": 0})
bent = hoop.equilibrium({"th": 1, "p": 0})
straight.kind, bent.kind               # 'saddle', 'stable focus'
bent["th"], np.arccos(1 / 1.5 ** 2)    # 1.1102 and 1.1102

hoop.run(th=0.01, p=0)["th"][-1], hoop.run(th=-0.01, p=0)["th"][-1]
# 1.1102 and -1.1102: the start chooses the side
```

Now let AUTO follow the equilibria as the hoop speeds up, first from the
bottom, then from a bent state:

```python
hoop.parameters["Om"] = 0.5
hoop.initial_conditions.update(th=0, p=0)
d = hoop.auto.equilibria("Om", parmin=0, parmax=2.5)
d.special("BP")[0].value               # 1.0: a branch point at Om = w0

hoop.parameters["Om"] = 2
hoop.equilibrium({"th": 1.3, "p": 0}, set_initial=True)
side = hoop.auto.equilibria("Om", parmin=0.5, parmax=2.5, ds=-0.02)
side.special("LP")[0].value            # 1.0 again
```

On the branch of the bottom, AUTO marks the pitchfork as a branch point
(BP), where other branches cross. Followed from a bent state, the branch
turns back at the same point: as `θ*` shrinks to zero, `Ω0` reaches its
smallest value `ω0` and grows again on the mirror state `−θ*`. AUTO marks
that turn as a limit point (LP). Both labels are the same pitchfork, seen
from different branches.

Last, the energy function. Take the friction away and compare `E` with the
kinetic plus potential energy along a run:

```python
hoop.parameters.update(Om=1.5, gam=0)
s = hoop.run(th=0.5, p=0)
th, p = s["th"], s["p"]
E = p ** 2 / 2 - 1.5 ** 2 / 2 * np.sin(th) ** 2 - np.cos(th)    # energy function
TU = p ** 2 / 2 + 1.5 ** 2 / 2 * np.sin(th) ** 2 - np.cos(th)   # T + U
np.ptp(E), np.ptp(TU)                  # below 1e-6, and 1.73
```

!!! note "Exercises"
    1. With friction, show that the eigenvalues at the bottom are
       `−γ ± sqrt(γ² + Ω0² − ω0²)`. For `γ = 0.1`, at which speed does the
       bottom change from a focus to a node, and at which from a node to a
       saddle?
    2. Without friction and with `Om = 1.5`, start the bead at `θ* + 0.01`
       and measure the period of its small swings. Compare with
       `2π / (Ω0 sin θ*)`.
    3. Expand the equation of motion to third order in `θ` and show that
       near `Ω0 = ω0` it is `θ'' ≈ (Ω0² − ω0²) θ − (ω0²/2) θ³`. Which
       `θ*` does this predict for `Om = 1.05`? Compare with
       `arccos(ω0²/Ω0²)`.
    4. Use `hoop.equilibrium_sweep` to follow the bent state for `Om` from
       1.1 to 2.5 (with `gam = 0.1`) and compare with `arccos(1/Om²)`.

??? success "Solutions"
    1. The Jacobian at `θ = 0` is `[[0, 1], [Ω0² − ω0², −2γ]]`: trace
       `−2γ`, determinant `ω0² − Ω0²`. The eigenvalues are real when
       `γ² + Ω0² − ω0² > 0`, that is for `Ω0 > sqrt(ω0² − γ²) = 0.995`,
       and one of them is positive when `Ω0 > ω0 = 1`. So the bottom is a
       stable focus below 0.995, a stable node between 0.995 and 1, and a
       saddle above 1.
    2. `θ* = 1.1102`, `Ω0 sin θ* = 1.3437`, so the predicted period is
       4.676; the run gives 4.677.
    3. `sin θ cos θ ≈ θ − 2θ³/3` and `sin θ ≈ θ − θ³/6`, so
       `θ'' ≈ (Ω0² − ω0²) θ − (2Ω0²/3 − ω0²/6) θ³`, and at `Ω0 = ω0` the
       cubic coefficient is `ω0²/2`. The equilibria are then
       `θ*² ≈ 2 (Ω0² − ω0²)/ω0²`: 0.453 for `Om = 1.05`, against the exact
       0.435. The error shrinks as `Ω0 → ω0`. This cubic equation is the
       normal form of the pitchfork.
    4. `Oms = np.linspace(1.1, 2.5, 8)`,
       `tab = hoop.equilibrium_sweep("Om", Oms, guess={"th": 1, "p": 0})`;
       `np.abs(tab["th"] - np.arccos(1 / Oms**2)).max()` is below 1e-5.

## Example 3: the pendulum with a vibrating support

Move the point of support up and down as `Δ cos ωt`, fast (`ω ≫ ω0`) and by
a small amount (`Δ ≪ ℓ`). In the frame of the support, the pendulum feels
gravity plus the inertial force of the moving support, and its equation
becomes

```
θ'' + 2γ θ' + (ω0² − (Δ/ℓ) ω² cos ωt) sin θ = 0
```

The motion is then the sum of a slow part and a small, fast jiggle at the
frequency `ω`. [MA §9.2.1] shows how to average the jiggle out: the slow
part moves as if in an *effective potential*, which is the old one plus the
mean kinetic energy of the jiggle. For this pendulum, per unit `mℓ²`,
[MA §9.2.1, Example 10, pp. 234–235]

```
U_ef(θ) = −ω0² cos θ + (Δ²ω² / 4ℓ²) sin² θ
```

The new term has its minima at `θ = 0` and `θ = π`. The bottom stays a
minimum of `U_ef`, and so stable. The top, a maximum of `−ω0² cos θ`,
becomes a minimum of `U_ef` when the new term wins, that is when
`Δ²ω² > 2gℓ`: then the upside-down pendulum is stable. Near the top, the
slow motion is a swing with the frequency `sqrt(Δ²ω²/2ℓ² − ω0²)`. This
system is often called Kapitza's pendulum.

```ode
# vibpend.ode
th' = p
p' = -2*gam*p - (w0^2 - d*w^2*cos(w*t))*sin(th)
par d=0.1, w=30, w0=1, gam=0.02
init th=2.9, p=0
@ total=60, dt=0.002, xp=t, yp=th, xlo=0, xhi=60, ylo=2, yhi=4.3
done
```

Here `d` is `Δ/ℓ`, and the time step is small because the run has to
follow the fast jiggle (60 steps per period of the support).

```python
vib = xs.load("vibpend.ode")
d, w = 0.1, 30
d * w, np.sqrt(2)                      # 3.0 > 1.41: the top should be stable
s = vib.run()                          # starts 0.24 rad from the top
s["th"].min(), s["th"].max()           # 2.90 and 3.37: it stays up

vib.parameters["gam"] = 0
s = vib.run(th=np.pi - 0.1, p=0)
th = s["th"]
up = s.t[1:][(th[:-1] < np.pi) & (th[1:] >= np.pi)]
np.diff(up).mean(), 2 * np.pi / np.sqrt(d ** 2 * w ** 2 / 2 - 1)
# 3.343 and 3.359: the period of the slow swing about the top
```

The measured period of the slow swing is within 0.5% of the averaged
theory; the difference shrinks as `ω/ω0` grows.

**Parametric resonance.** Near the bottom, `sin θ ≈ θ`, and the same
equation without friction is Mathieu's equation,
`θ'' + (ω0² − ε cos ωt) θ = 0` with `ε = Δω²/ℓ`. Its rest state becomes
unstable when the driving frequency is close to `2ω0/n` for a whole number
`n`, most strongly for `ω = 2ω0`: a small swing grows, fed by the moving
support. This is *parametric resonance*, which pumps a swing by changing a
parameter instead of pushing [MA §8.4.3, pp. 207–208, and Box 8.14].

!!! note "Exercises"
    1. Show that `U_ef'' (π) = Δ²ω²/2ℓ² − ω0²`, which gives the condition
       `Δ²ω² > 2gℓ` and the frequency of the slow swing.
    2. Keep `w = 30` and `gam = 0.02` and lower `d`. Starting at
       `th = pi - 0.1`, for which `d` does the pendulum still stay up?
       Compare `d*w` at the threshold with `sqrt(2)`.
    3. Show parametric resonance: with `d = 0.1`, `gam = 0`, `dt = 0.01`
       and `total = 100`, start at `th = 0.01` and compare the largest
       `|th|` for `w = 2`, `w = 1.5` and `w = 1`.

??? success "Solutions"
    1. `U_ef' = ω0² sin θ + (Δ²ω²/4ℓ²) sin 2θ` and
       `U_ef'' = ω0² cos θ + (Δ²ω²/2ℓ²) cos 2θ`. At `θ = π` this is
       `−ω0² + Δ²ω²/2ℓ²`, positive when `Δ²ω² > 2ω0²ℓ² = 2gℓ`. The slow
       swing about a minimum of `U_ef` has the frequency `sqrt(U_ef'')`.
    2. The pendulum stays up for `d = 0.0475` (`d*w = 1.425`) and falls for
       `d = 0.045` (`d*w = 1.35`). The theory predicts the threshold
       `d*w = sqrt(2) = 1.414`.
    3. For `w = 2 = 2ω0` the swing grows from 0.01 to 1.73. For `w = 1.5`
       it stays at 0.01. For `w = 1` (the resonance with `n = 2`) it grows
       only very slowly: the higher resonances are much weaker for a small
       `ε`, here 0.1.

## Example 4: the van der Pol oscillator and flutter

A wing in an airstream can start to vibrate by itself, taking energy from
the flow: this is *flutter*. [MA ch. 9, exercise E2, p. 245] gives the van
der Pol oscillator as a simple model of it:

```
u' = v,    v' = (μ − u²) v − u
```

For small `u` the term `μv` is a damping of the wrong sign when `μ > 0`: it
feeds energy in. The term `−u²v` takes energy out at large amplitude. The
balance of the two is a self-sustained oscillation, a limit cycle.

**The Hopf bifurcation.** The only equilibrium is the origin. Its Jacobian
`[[0, 1], [−1, μ]]` has trace `μ` and determinant 1, so the eigenvalues are
`μ/2 ± i sqrt(1 − μ²/4)`: a stable focus for `μ < 0`, an unstable one for
`μ > 0`. A complex pair crosses the imaginary axis at `μ = 0`: a Hopf
bifurcation [MA §9.1.1, p. 224].

**Averaging.** For small `μ` the orbit is nearly a circle of slowly
changing radius. Write `u = a cos(t + φ)` and `v = −a sin(t + φ)` with `a`
and `φ` slowly varying, and replace the equations for `a` and `φ` by their
averages over one turn: the method of averaging, which [MA §9.2.3,
pp. 237–238] applies to the Rayleigh oscillator. Here it gives

```
a' = (a/2) (μ − a²/4),    φ' = 0
```

For `μ < 0` every amplitude decays. For `μ > 0` the amplitude grows if
`a < 2 sqrt(μ)` and decays if `a > 2 sqrt(μ)`: there is a stable limit
cycle of amplitude `2 sqrt(μ)` and period close to `2π`. Because the cycle
exists on the side where the equilibrium is unstable, and is stable, the
Hopf bifurcation is *supercritical* [MA §9.1.1, pp. 223–224].

```ode
# vdp.ode
u' = v
v' = (mu - u^2)*v - u
par mu=-0.5
init u=0.1, v=0
@ total=200, dt=0.02, bound=1e4, xp=u, yp=v, xlo=-4, xhi=4, ylo=-4, yhi=4
done
```

```python
vdp = xs.load("vdp.ode")
vdp.equilibrium({"u": 0, "v": 0}).eigenvalues     # -0.25 +/- 0.968i

d = vdp.auto.equilibria("mu", parmin=-0.5, parmax=1)
hb = d.special("HB")[0]
hb.value                               # 0.0 (to 1e-7): the Hopf point
cycles = vdp.auto.continue_from(hb, "periodic").to_pandas()
cycles = cycles[cycles["periodic"] & (cycles["mu"] > 0)]
small = cycles[cycles["mu"] < 0.2]
np.abs(small["U_max"] - 2 * np.sqrt(small["mu"])).max()    # below 1e-3
cycles["stable"].all()                 # True: a stable cycle, supercritical
```

AUTO's branch of cycles leaves the Hopf point with an amplitude that agrees
with `2 sqrt(μ)` to three decimals while `μ` is small. Further out the
agreement degrades slowly: the cycle is no longer a circle.

For large `μ` the cycle changes its character. It creeps slowly along the
curve where `v' = 0` and jumps quickly between its branches: a *relaxation
oscillation*, whose period grows in proportion to `μ`. Its analysis needs
other methods than the weakly nonlinear ones above (see Strogatz, ch. 7).
With the substitution `u = sqrt(μ) x`, the model becomes the more common
form `x'' − μ (1 − x²) x' + x = 0`, whose cycle has amplitude about 2 for
every `μ`.

!!! note "Exercises"
    1. Derive the averaged equation for `a`. Hint: substitute `u` and `v`
       into `v' = ...`, and use `⟨sin² θ⟩ = 1/2` and
       `⟨sin² θ cos² θ⟩ = 1/8`.
    2. Run `mu = 0.1` from `u = 0.1` and from `u = 3`. Where do both runs
       end?
    3. [MA §9.2.3] finds that the Rayleigh oscillator
       `x'' + ω0² x = ε (x' − x'³)` has a limit cycle of amplitude
       `2 / (sqrt(3) ω0)`. Write it as an `.ode` file and check the
       amplitude for `ω0 = 1` and `ε = 0.1`.
    4. Measure the period of the cycle for `mu = 10` (use `meth="stiff"`
       and a long run). How does it compare with `2π`?

??? success "Solutions"
    1. With `u = a cos θ`, `v = −a sin θ` and `θ = t + φ`, the condition
       `a' cos θ − a φ' sin θ = 0` and the equation for `v` give
       `a' = −(μ − a² cos² θ)(−a sin θ) sin θ`, averaged over `θ`:
       `a' = a (μ/2 − a²/8)`.
    2. Both end on the same cycle, of amplitude 0.632 = `2 sqrt(0.1)`: the
       limit cycle attracts orbits from inside and from outside.
    3. With `x' = y`, `y' = -w0^2*x + eps*(y - y^3)`, a run of 300 time
       units from `x = 0.1` ends on a cycle of amplitude 1.156, and
       `2/sqrt(3) = 1.1547`.
    4. About 19.08, three times `2π`: the relaxation oscillation is much
       slower than the small cycle near the Hopf point.

## Example 5: the forced, damped pendulum and chaos

Now switch on the force in the pendulum equation,
`θ'' + 2γ θ' + ω0² sin θ = a0 cos Ωt`. The system is no longer autonomous:
its state is `(θ, θ')` together with the phase of the force, which leaves
room for chaos [MA §10.1, p. 250]. With a weak force the pendulum settles
on an oscillation that repeats with the period of the force
[MA Box 8.8, p. 199].

A clear way to see what the motion does is to look at the state once per
period `T = 2π/Ω` of the force. This *stroboscopic map* is a Poincaré
section [MA §6.6.2, p. 138; §8.4, p. 203]. An orbit of period `T` shows up
as one point, an orbit of period `2T` as two, a chaotic orbit as a cloud of
points that never repeats.

```ode
# forced.ode
th' = p
p' = -2*gam*p - w0^2*sin(th) + a0*cos(Om*t)
par gam=0.25, w0=1, a0=0.5, Om=0.6666667
init th=0, p=0
@ total=600, dt=0.01, bound=1e6, xp=th, yp=p, xlo=-4, xhi=4, ylo=-3, yhi=3
done
```

When the pendulum goes round and round, `θ` grows without limit:
`bound=1e6` keeps XPPAUT from stopping the run when `|θ|` passes its
default bound of 100.

The Lyapunov exponent measures the same thing in a number. Follow two
states a small distance `d0` apart for one period of the force, see by
which factor their distance has grown, put the second state back at the
distance `d0` in the direction it has moved to, and repeat. The average of
the logarithms of the growth factors, per unit time, is the largest
Lyapunov exponent: this is the definition of [MA §10.1, p. 249] turned into
a computation.

```python
forced = xs.load("forced.ode")
T = 2 * np.pi / forced.parameters["Om"]    # the period of the force
forced.set(dt=T / 500)                     # 500 steps per period

def strobe(a0, periods=150):
    """p once per period of the force, after 50 periods of transient."""
    forced.parameters["a0"] = a0
    s = forced.run(th=0, p=0, total=periods * T)
    return s["p"][::500][50:]

def one_period(th, p):
    """The state one period of the force later, in full precision."""
    forced.run(th=th, p=p, total=T)
    forced.ic_last()
    return forced.get("th"), forced.get("p")

def lyapunov(a0, periods=500, d0=1e-8):
    forced.parameters["a0"] = a0
    th, p = 0.0, 0.0
    for _ in range(100):                   # settle on the attractor
        th, p = one_period(th, p)
    th2, p2, logs = th + d0, p, 0.0
    for _ in range(periods):
        th, p = one_period(th, p)
        th2, p2 = one_period(th2, p2)
        gap = np.hypot(th2 - th, p2 - p)
        logs += np.log(gap / d0)
        th2, p2 = th + (th2 - th) * d0 / gap, p + (p2 - p) * d0 / gap
    return logs / (periods * T)

for a0 in (0.5, 1.07, 1.15, 1.5):
    points = strobe(a0)
    print(a0, len(np.unique(np.round(points, 3))), round(lyapunov(a0), 3))
# 0.5   1   -0.25    period T
# 1.07  2   -0.04    period 2T
# 1.15  93  0.094    chaos
# 1.5   98  0.117    chaos
```

As the force grows the periodic orbit of period `T` loses its stability to
one of period `2T`: a period doubling, where a Floquet multiplier passes
through −1 [MA §9.1.2, p. 227]. More doublings follow at shorter and shorter
intervals, and the orbit becomes chaotic. This is the *Feigenbaum cascade*
[MA §10.3.1, pp. 266–267]. On the periodic orbits the exponent is negative
(nearby states are drawn in); on the chaotic ones it is positive.

The periodic lines of that table come out the same on every computer. The
chaotic lines do not. The pendulum's equation calls the sine function, and
different mathematical libraries round its last binary digit differently;
on a chaotic orbit that tiny difference grows like any other perturbation,
until the orbit is a different one. The numbers above come from the
computer these pages were written on. On another one the count of points
will differ (it stays a cloud of 80 to 100) and so will the second digit
of the exponent (it stays positive, about 0.1). Single chaotic orbits
cannot be reproduced; their statistics can, which is why the study of
chaos turns to averaged quantities such as the Lyapunov exponent
[MA §10.1, p. 250].

XPPAUT has its own estimate, `forced.lyapunov()`, which follows a small
perturbation along the stored run. It is not used here: XPPAUT stores runs
in single precision, and when an angle winds up to thousands of radians, as
`th` does when the pendulum goes round, the rounding of the stored values
is as large as the perturbation, and the estimate goes wrong. For variables
that stay of moderate size, as in `lorenz.ode`, it works.

**Sensitive dependence.** Start on the attractor and run the chaotic case
twice, with angles that differ by `10⁻⁶`:

```python
forced.parameters["a0"] = 1.5
th, p = 0.0, 0.0
for _ in range(100):                       # start on the attractor
    th, p = one_period(th, p)
a = forced.run(th=th, p=p, total=300)
b = forced.run(th=th + 1e-6, p=p, total=300)
gap = np.abs(a["th"] - b["th"])
a.t[np.argmax(gap > 1)], np.log(1e6) / 0.117    # 114 here, and 118
```

The gap reaches a whole radian at `t = 114`, close to the Lyapunov time
`ln(1/10⁻⁶) / λ_L = 118`. (In runs that differ only in rounding, as on
another computer, this time ranges from about 100 to 170: one pair of
orbits does not separate exactly at the average rate.) After that the two
runs have nothing in common,
although the equations are deterministic and the starts differ by a
millionth of a radian [MA §10.1, p. 249].

!!! note "Exercises"
    1. Draw the bifurcation diagram, as an experimenter would, turning the
       force up slowly: for `a0` from 1.0 to 1.5 in steps of 0.01, run 200
       periods starting where the previous run ended, and plot the
       stroboscopic values of `p` from the last 100 periods against `a0`.
       Find the period doublings and the windows where the motion is
       periodic again.
    2. Look at `a0 = 1.35`. How many stroboscopic points are there, what is
       the Lyapunov exponent, and what does `th` do?
    3. By the Lyapunov time, how much later should the gap reach a radian
       if the initial difference is ten times smaller? Check with
       `b = forced.run(th=th + 1e-7, p=p, total=300)`.

??? success "Solutions"
    1. Each run starts where the previous one ended:

        ```python
        import matplotlib.pyplot as plt
        th, p = 0.0, 0.0
        for a0 in np.arange(1.0, 1.5001, 0.01):
            forced.parameters["a0"] = a0
            s = forced.run(th=th, p=p, total=200 * T)
            th, p = s["th"][-1] % (2 * np.pi), s["p"][-1]
            pts = s["p"][::500][100:]
            plt.plot([a0] * len(pts), pts, ",k")
        ```

        One point up to 1.06, two at 1.07 and four at 1.08: two period
        doublings. From 1.09 the points fill a cloud (chaos), with a window
        of period 3 from 1.12 to 1.14 and chaos again from 1.15 to about
        1.28. From 1.30 to 1.42 there is a single point again, then period 2
        from 1.43 to 1.45, and chaos near 1.5. Several attractors coexist for
        some forces, so the picture depends a little on where each run
        starts.
    2. One point, and a Lyapunov exponent of −0.25: a stable periodic
       orbit. `th` decreases by `2π` in each period of the force: the
       pendulum goes round once per period, in step with the force.
    3. `ln(10) / 0.117 ≈ 20` time units later. Here the run gives 147
       instead of 114, 33 later; in runs that differ only in rounding the
       delay ranges from about 5 to 50. The growth rate varies along the
       orbit, and only its long-time average is `λ_L`.

## Where to go next

- [MA] continues with the double pendulum, a Hamiltonian system whose
  Poincaré sections fill with chaos as the energy grows [MA §10.2.4,
  pp. 258–265]; the examples folder has `doubpend.ode`. It also treats the
  Lorenz equations, the classic dissipative chaos [MA Box 8.2, p. 191;
  §10.1, p. 252]: open `lorenz.ode` and `waterwheel.ode`, a water wheel
  with leaking buckets that obeys the same equations (Strogatz, ch. 9).
- The examples folder also has `pend.ode`, `invpend.ode` (the pendulum with
  a vibrating support, with an animation) and `forcpend.ode`. Open them in
  the app: the `.ani` files make the motion visible.
- [MA] comes with MATLAB programs for chapters 8 to 10, available from the
  publisher, that explore the same examples.
- [Bifurcation analysis with AUTO](../manual/auto.md) for branch points,
  Hopf points and the continuation of periodic orbits.
- [Data analysis](../python/analysis.md) for spectra, the Lyapunov exponent
  and the period of a limit cycle.

## References

- **[MA]** Sanz Recio, F. J. and Sánchez Arriaga, G. (2019). *Mecánica
  analítica: lagrangiana, hamiltoniana y sistemas dinámicos*. Madrid:
  McGraw-Hill/Interamericana de España. ISBN 978-84-486-1539-0 (print),
  978-84-486-1541-3 (electronic). In Spanish. Chapters 8 (regular
  solutions), 9 (bifurcations and approximate methods) and 10
  (deterministic chaos) are the basis of this page.
- Landau, L. D. and Lifshitz, E. M. (1976). *Mechanics*, 3rd edition
  (Course of Theoretical Physics, vol. 1). Oxford: Butterworth-Heinemann.
  ISBN 978-0-7506-2896-9. Chapter V, on small oscillations, includes
  parametric resonance, nonlinear oscillations and motion in a rapidly
  oscillating field, the topics of Examples 1 and 3.
- Strogatz, S. H. (2024). *Nonlinear Dynamics and Chaos: With Applications
  to Physics, Biology, Chemistry, and Engineering*, 3rd edition. Boca
  Raton: CRC Press (Chapman & Hall). ISBN 978-0-367-02650-9. Chapter 3
  (bifurcations) treats the bead on a rotating hoop in the overdamped
  limit, chapter 7 (limit cycles) relaxation oscillations, and chapter 9
  the Lorenz equations and the chaotic water wheel.
