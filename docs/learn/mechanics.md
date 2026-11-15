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
for a system with $n$ degrees of freedom are $n$ second order equations in
the generalised coordinates $q$. Take the coordinates and their velocities
together as the state, $x = (q, \dot{q})$, and they become $2n$ first order
equations, $\dot{x} = f(x, t)$: the form that an ODE solver, and XPPAUT, needs.
The system is *autonomous* when $t$ does not appear in $f$ [MA §8.1,
p. 190]. Most of this page uses one equation, the pendulum with viscous
friction and a periodic force [MA eq. (8.2), p. 191]:

$$
\ddot{\theta} + 2\gamma \dot{\theta} + \omega_0^2 \sin\theta = a_0 \cos\Omega t
$$

Here $\theta$ is the angle from the downward vertical, $\omega_0 = \sqrt{g/\ell}$ the
frequency of small swings, $\gamma$ the friction coefficient, and $a_0$ and $\Omega$
the amplitude and frequency of the force. The state is $(\theta, \dot{\theta})$.

**First integrals and the energy function.** A function of the state that
keeps its value along every motion is a *first integral* [MA §1.4.1, p. 22].
When the Lagrangian does not depend on time explicitly, the *energy
function* $E = \sum_i \dot{q}_i \, \partial L/\partial \dot{q}_i - L$ is one [MA §1.4.2–1.4.3, pp. 22–23]. It
equals the familiar kinetic plus potential energy only when the kinetic
energy is a quadratic form in the velocities; Example 2 has a case where it
is not. With one degree of freedom a first integral decides the shape of
the orbits, which are its level curves in the phase plane, and the time
along them follows from one integral (a *quadrature*) [MA §6.6.1, p. 133].

**Volume in the state space.** Follow a small blob of initial states as the
flow carries it: the divergence $\nabla \cdot f$ is the rate at which its volume grows,
per unit volume [MA §8.1.3, p. 193]. Hamiltonian systems keep the volume
[MA §6.6.4, p. 139]; friction makes it shrink. A system whose volumes shrink
ends on *attractors*: equilibria, limit cycles, or strange attractors. For
the pendulum, $\nabla \cdot f = -2\gamma$.

**Equilibria and their stability.** An equilibrium $x_e$ solves
$f(x_e) = 0$. A small perturbation $y$ of it obeys $\dot{y} = A y$, where $A$ is
the Jacobian matrix of $f$ at $x_e$, so it grows or decays like $e^{\lambda t}$
with $\lambda$ the eigenvalues of $A$. If all of them have negative real part the
equilibrium is asymptotically stable; if one has positive real part it is
unstable [MA §8.2.1, pp. 195–196]. In the plane the eigenvalues solve
$\lambda^2 - \mathrm{tr}(A)\,\lambda + \det(A) = 0$, and their signs and the sign of the
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
p. 199]. For a Lagrangian $L = T - U(q)$ this says that a strict minimum of
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
$\lvert\delta x(t)\rvert \approx \lvert\delta x_0\rvert\, e^{\lambda_L t}$, with $\lambda_L > 0$ the largest *Lyapunov
exponent* (called $\mu$ in [MA]). An initial error $\delta x_0$ then allows
predictions for about the *Lyapunov time* $\ln(L/\lvert\delta x_0\rvert) / \lambda_L$, with $L$ the
size of the attractor [MA §10.1, pp. 248–249]. An autonomous system needs at
least three dimensions to be chaotic; a periodically forced one, such as the
pendulum above, needs only two [MA §10.1, p. 250].

## Example 1: the pendulum

Divide the Lagrangian of a pendulum of mass $m$ and length $\ell$ by $m\ell^2$:
$L = \dot{\theta}^2/2 + \omega_0^2 \cos\theta$. The momentum is $p = \dot{\theta}$, and the energy function
is $E = p^2/2 - \omega_0^2 \cos\theta$ [MA §6.6.1, p. 134]. The equation of motion is
the one above with $a_0 = 0$:

```ode
# pend.ode
th' = p
p' = -2*gam*p - w0^2*sin(th)
par gam=0, w0=1
init th=2, p=0
@ total=40, dt=0.01, xp=th, yp=p, xlo=-4, xhi=10, ylo=-3, yhi=3
done
```

In the file, `th` is $\theta$, `p` is $\dot{\theta}$, `gam` is $\gamma$ and `w0` is $\omega_0$. With
`w0 = 1` the time is measured in units of $1/\omega_0$.

**Without friction** ($\gamma = 0$) the energy function is conserved. Its
smallest value, $E_0 = -\omega_0^2$, is the pendulum at rest hanging down. The
equilibria are $\theta = 0$ (hanging) and $\theta = \pi$ (upright), and their copies a
whole turn away. At the bottom the Jacobian is $\begin{pmatrix} 0 & 1 \\ -\omega_0^2 & 0 \end{pmatrix}$, with
eigenvalues $\pm i\omega_0$: a centre. At the top it is $\begin{pmatrix} 0 & 1 \\ \omega_0^2 & 0 \end{pmatrix}$, with
eigenvalues $\pm\omega_0$: a saddle. Small orbits around the bottom are ellipses;
as $E_0$ grows they stretch, and at $E_0 = \omega_0^2$ they reach the saddles. Above
that energy the pendulum no longer swings but goes round and round. The
orbits with $E_0 = \omega_0^2$, the *separatrices*, divide swinging from rotating,
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
unstable focus. Lagrange's theorem can: $E$ has a strict minimum at the
bottom, so the bottom is stable, though not asymptotically stable, since
nearby orbits circle it for ever [MA §8.2.3, p. 199].

**The period depends on the amplitude.** Only small swings take the same
time whatever their size [MA Box 8.7, p. 198]. A swing of amplitude $a$
takes four times the time from $\theta = 0$ to $\theta = a$, which is the quadrature
of [MA §6.6.1, eq. (6.63)] with $p = \sqrt{2\omega_0^2 (\cos\theta - \cos a)}$. The
change of variable $\sin(\theta/2) = k \sin\varphi$, with $k = \sin(a/2)$, removes the
infinity of the integrand at the turning point and gives

$$
T(a) = \frac{4}{\omega_0} \int_0^{\pi/2} \frac{d\varphi}{\sqrt{1 - k^2 \sin^2\varphi}}
$$

(a complete elliptic integral of the first kind). $T$ grows without bound as
$a \to \pi$, the separatrix.

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

**With friction** ($\gamma > 0$) the divergence is $-2\gamma$, and along the motion
$\dot{E} = -2\gamma p^2$, which is never positive. At the bottom the Jacobian is
$\begin{pmatrix} 0 & 1 \\ -\omega_0^2 & -2\gamma \end{pmatrix}$, with eigenvalues $-\gamma \pm \sqrt{\gamma^2 - \omega_0^2}$: a stable
focus when $\gamma < \omega_0$ (the swings die out slowly) and a stable node when
$\gamma > \omega_0$ (the pendulum creeps back without swinging) [MA §8.2.2, Example 1,
p. 198]. At the top the eigenvalues are $-\gamma \pm \sqrt{\gamma^2 + \omega_0^2}$, real and of
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

Every orbit now ends hanging down, but at which copy $\theta = 2\pi k$ depends on
where it started. Push the pendulum from the bottom with a speed $p_0$:
without friction it goes over the top when $E_0 = p_0^2/2 - 1$ reaches $1$,
that is for $p_0 = 2$. With friction it loses energy on the way up, and
needs more:

```python
pend.set(total=100)
[round(pend.run(th=0, p=p0)["th"][-1] / (2 * np.pi)) for p0 in (2.0, 2.5, 4.5)]
# [0, 1, 2]: no turn, one turn, two turns before it settles
```

The boundaries between these outcomes are the stable manifolds of the
saddles at $\theta = \pi + 2\pi k$: the curves of initial states that end exactly on
the upright position [MA §8.2.1, p. 196]. The app's **Phase Plane** tab
draws them; in Python, `pend.manifolds({"th": 3.14, "p": 0})` returns the
saddle and its four branches, two stable and two unstable.

!!! note "Exercises"
    1. Show that $\dot{E} = -2\gamma p^2$ along the motion.
    2. At which friction does the hanging equilibrium change from a focus
       to a node? Check with `pend.equilibrium` for `gam` = 0.9 and 1.1.
    3. For small swings, the Lindstedt-Poincaré method [MA §9.2.2,
       pp. 235–236] gives the frequency of $\ddot{x} + \omega_0^2 x = -\alpha x^2 - \beta x^3$ as
       $\omega \approx \omega_0 + (9\beta\omega_0^2 - 10\alpha^2)\, a^2 / (24\omega_0^3)$. Expand $\sin\theta$ to third
       order to find $\alpha$ and $\beta$ for the pendulum, and show that
       $\omega \approx \omega_0 (1 - a^2/16)$. Compare the period it predicts with the
       measured ones for $a$ = 0.5, 1 and 2.
    4. With `gam = 0.1`, find by bisection the smallest $p_0$ that takes the
       pendulum over the top.

??? success "Solutions"
    1. $\dot{E} = p \dot{p} + \omega_0^2 \sin\theta\, \dot{\theta} = p\,(-2\gamma p - \omega_0^2 \sin\theta) + \omega_0^2 \sin\theta\, p = -2\gamma p^2$.
    2. The discriminant $\gamma^2 - \omega_0^2$ changes sign at $\gamma = \omega_0 = 1$. For
       `gam = 0.9` the program prints `'stable focus'` (eigenvalues
       $-0.9 \pm 0.436i$); for `gam = 1.1`, `'stable node'` ($-0.642$ and
       $-1.558$).
    3. $\sin\theta \approx \theta - \theta^3/6$, so the equation is
       $\ddot{\theta} + \omega_0^2 \theta = (\omega_0^2/6)\, \theta^3$: $\alpha = 0$ and $\beta = -\omega_0^2/6$. Then
       $\omega \approx \omega_0 - 9 \omega_0^4 a^2 / (6 \cdot 24\, \omega_0^3) = \omega_0 (1 - a^2/16)$. The predicted
       periods $2\pi / (1 - a^2/16)$ are 6.383, 6.702 and 8.378; the measured
       ones are 6.38, 6.70 and 8.35. The approximation is good while
       $a^2/16$ is small, and is off by 0.3% at $a = 2$.
    4. Bisect on $p_0$ between 1 and 3, with the criterion
       `abs(th[-1]) > pi` after a long run: it converges to 2.4149.

## Example 2: the pendulum on a rotating hoop

A bead slides without friction on a vertical hoop of radius $\ell$ that turns
about its vertical diameter at a constant rate $\Omega_0$. This is the same as a
pendulum whose plane of swing turns about the vertical [MA §6.6.1, p. 137;
§9.1.1, Example 2, p. 224]. With $\theta$ measured from the bottom, the
Lagrangian divided by $m\ell^2$ is

$$
L = \frac{\dot{\theta}^2}{2} + \frac{\Omega_0^2}{2} \sin^2\theta + \omega_0^2 \cos\theta
$$

and the equation of motion is [MA eq. (9.2)]

$$
\ddot{\theta} = \Omega_0^2 \sin\theta \left( \cos\theta - \frac{\omega_0^2}{\Omega_0^2} \right)
$$

The term $(\Omega_0^2/2) \sin^2\theta$ is kinetic energy of the rotation, but it does
not contain $\dot{\theta}$, so the kinetic energy is not a quadratic form in the
velocity. The energy function, $E = \dot{\theta}^2/2 - (\Omega_0^2/2) \sin^2\theta - \omega_0^2 \cos\theta$, is
still conserved, because $L$ does not depend on time, but it is not the
kinetic plus potential energy: that one changes, since the motor that keeps
the hoop turning does work on the bead [MA §1.4.2–1.4.3]. $E$ is the energy
of a particle moving in the *effective potential*
$U_{\mathrm{ef}} = -\omega_0^2 \cos\theta - (\Omega_0^2/2) \sin^2\theta$.

**Equilibria.** $\dot{\theta} = 0$ and $\sin\theta\, (\cos\theta - \omega_0^2/\Omega_0^2) = 0$:

- $\theta = 0$, the bottom. Linearising gives $\ddot{\theta} = (\Omega_0^2 - \omega_0^2)\, \theta$: a centre
  while $\Omega_0 < \omega_0$ (and a minimum of $U_{\mathrm{ef}}$, so stable by Lagrange's
  theorem), a saddle when $\Omega_0 > \omega_0$.
- $\theta = \pi$, the top: $\ddot{\theta} = (\Omega_0^2 + \omega_0^2)(\theta - \pi)$, a saddle at every speed.
- $\theta = \pm\theta^*$ with $\cos\theta^* = \omega_0^2/\Omega_0^2$, which exist only when $\Omega_0 > \omega_0$. They
  are minima of $U_{\mathrm{ef}}$, so stable, and the bead swings about them with the
  frequency $\Omega_0 \sin\theta^*$ [MA §9.1.1, Example 2].

At $\Omega_0 = \omega_0$ the bottom loses its stability and hands it to two new
equilibria, mirror images of each other: a **supercritical pitchfork**
[MA §9.1.1, p. 223]. Spin the hoop faster than the pendulum's own frequency
and the bead climbs to one side. Which side is decided by the smallest
asymmetry in the start; the equations themselves do not prefer one.

The book's example has no friction. Here we add viscous friction $2\gamma\dot{\theta}$,
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
turns back at the same point: as $\theta^*$ shrinks to zero, $\Omega_0$ reaches its
smallest value $\omega_0$ and grows again on the mirror state $-\theta^*$. AUTO marks
that turn as a limit point (LP). Both labels are the same pitchfork, seen
from different branches.

Last, the energy function. Take the friction away and compare $E$ with the
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
       $-\gamma \pm \sqrt{\gamma^2 + \Omega_0^2 - \omega_0^2}$. For $\gamma = 0.1$, at which speed does the
       bottom change from a focus to a node, and at which from a node to a
       saddle?
    2. Without friction and with `Om = 1.5`, start the bead at $\theta^* + 0.01$
       and measure the period of its small swings. Compare with
       $2\pi / (\Omega_0 \sin\theta^*)$.
    3. Expand the equation of motion to third order in $\theta$ and show that
       near $\Omega_0 = \omega_0$ it is $\ddot{\theta} \approx (\Omega_0^2 - \omega_0^2)\, \theta - (\omega_0^2/2)\, \theta^3$. Which
       $\theta^*$ does this predict for `Om = 1.05`? Compare with
       $\arccos(\omega_0^2/\Omega_0^2)$.
    4. Use `hoop.equilibrium_sweep` to follow the bent state for `Om` from
       1.1 to 2.5 (with `gam = 0.1`) and compare with $\arccos(1/\mathrm{Om}^2)$.

??? success "Solutions"
    1. The Jacobian at $\theta = 0$ is $\begin{pmatrix} 0 & 1 \\ \Omega_0^2 - \omega_0^2 & -2\gamma \end{pmatrix}$: trace
       $-2\gamma$, determinant $\omega_0^2 - \Omega_0^2$. The eigenvalues are real when
       $\gamma^2 + \Omega_0^2 - \omega_0^2 > 0$, that is for $\Omega_0 > \sqrt{\omega_0^2 - \gamma^2} = 0.995$,
       and one of them is positive when $\Omega_0 > \omega_0 = 1$. So the bottom is a
       stable focus below 0.995, a stable node between 0.995 and 1, and a
       saddle above 1.
    2. $\theta^* = 1.1102$, $\Omega_0 \sin\theta^* = 1.3437$, so the predicted period is
       4.676; the run gives 4.677.
    3. $\sin\theta \cos\theta \approx \theta - 2\theta^3/3$ and $\sin\theta \approx \theta - \theta^3/6$, so
       $\ddot{\theta} \approx (\Omega_0^2 - \omega_0^2)\, \theta - (2\Omega_0^2/3 - \omega_0^2/6)\, \theta^3$, and at $\Omega_0 = \omega_0$ the
       cubic coefficient is $\omega_0^2/2$. The equilibria are then
       ${\theta^*}^2 \approx 2\, (\Omega_0^2 - \omega_0^2)/\omega_0^2$: 0.453 for `Om = 1.05`, against the exact
       0.435. The error shrinks as $\Omega_0 \to \omega_0$. This cubic equation is the
       normal form of the pitchfork.
    4. `Oms = np.linspace(1.1, 2.5, 8)`,
       `tab = hoop.equilibrium_sweep("Om", Oms, guess={"th": 1, "p": 0})`;
       `np.abs(tab["th"] - np.arccos(1 / Oms**2)).max()` is below 1e-5.

## Example 3: the pendulum with a vibrating support

Move the point of support up and down as $\Delta \cos\omega t$, fast ($\omega \gg \omega_0$) and by
a small amount ($\Delta \ll \ell$). In the frame of the support, the pendulum feels
gravity plus the inertial force of the moving support, and its equation
becomes

$$
\ddot{\theta} + 2\gamma \dot{\theta} + \left( \omega_0^2 - \frac{\Delta}{\ell}\, \omega^2 \cos\omega t \right) \sin\theta = 0
$$

The motion is then the sum of a slow part and a small, fast jiggle at the
frequency $\omega$. [MA §9.2.1] shows how to average the jiggle out: the slow
part moves as if in an *effective potential*, which is the old one plus the
mean kinetic energy of the jiggle. For this pendulum, per unit $m\ell^2$,
[MA §9.2.1, Example 10, pp. 234–235]

$$
U_{\mathrm{ef}}(\theta) = -\omega_0^2 \cos\theta + \frac{\Delta^2\omega^2}{4\ell^2} \sin^2\theta
$$

The new term has its minima at $\theta = 0$ and $\theta = \pi$. The bottom stays a
minimum of $U_{\mathrm{ef}}$, and so stable. The top, a maximum of $-\omega_0^2 \cos\theta$,
becomes a minimum of $U_{\mathrm{ef}}$ when the new term wins, that is when
$\Delta^2\omega^2 > 2g\ell$: then the upside-down pendulum is stable. Near the top, the
slow motion is a swing with the frequency $\sqrt{\Delta^2\omega^2/(2\ell^2) - \omega_0^2}$. This
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

Here `d` is $\Delta/\ell$, and the time step is small because the run has to
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
theory; the difference shrinks as $\omega/\omega_0$ grows.

**Parametric resonance.** Near the bottom, $\sin\theta \approx \theta$, and the same
equation without friction is Mathieu's equation,
$\ddot{\theta} + (\omega_0^2 - \varepsilon \cos\omega t)\, \theta = 0$ with $\varepsilon = \Delta\omega^2/\ell$. Its rest state becomes
unstable when the driving frequency is close to $2\omega_0/n$ for a whole number
$n$, most strongly for $\omega = 2\omega_0$: a small swing grows, fed by the moving
support. This is *parametric resonance*, which pumps a swing by changing a
parameter instead of pushing [MA §8.4.3, pp. 207–208, and Box 8.14].

!!! note "Exercises"
    1. Show that $U_{\mathrm{ef}}''(\pi) = \Delta^2\omega^2/(2\ell^2) - \omega_0^2$, which gives the condition
       $\Delta^2\omega^2 > 2g\ell$ and the frequency of the slow swing.
    2. Keep `w = 30` and `gam = 0.02` and lower `d`. Starting at
       `th = pi - 0.1`, for which `d` does the pendulum still stay up?
       Compare `d*w` at the threshold with $\sqrt{2}$.
    3. Show parametric resonance: with `d = 0.1`, `gam = 0`, `dt = 0.01`
       and `total = 100`, start at `th = 0.01` and compare the largest
       `|th|` for `w = 2`, `w = 1.5` and `w = 1`.

??? success "Solutions"
    1. $U_{\mathrm{ef}}' = \omega_0^2 \sin\theta + (\Delta^2\omega^2/(4\ell^2)) \sin 2\theta$ and
       $U_{\mathrm{ef}}'' = \omega_0^2 \cos\theta + (\Delta^2\omega^2/(2\ell^2)) \cos 2\theta$. At $\theta = \pi$ this is
       $-\omega_0^2 + \Delta^2\omega^2/(2\ell^2)$, positive when $\Delta^2\omega^2 > 2\omega_0^2\ell^2 = 2g\ell$. The slow
       swing about a minimum of $U_{\mathrm{ef}}$ has the frequency $\sqrt{U_{\mathrm{ef}}''}$.
    2. The pendulum stays up for `d = 0.0475` (`d*w = 1.425`) and falls for
       `d = 0.045` (`d*w = 1.35`). The theory predicts the threshold
       $d\,w = \sqrt{2} = 1.414$.
    3. For $w = 2 = 2\omega_0$ the swing grows from 0.01 to 1.73. For `w = 1.5`
       it stays at 0.01. For `w = 1` (the resonance with $n = 2$) it grows
       only very slowly: the higher resonances are much weaker for a small
       $\varepsilon$, here 0.1.

## Example 4: the van der Pol oscillator and flutter

A wing in an airstream can start to vibrate by itself, taking energy from
the flow: this is *flutter*. [MA ch. 9, exercise E2, p. 245] gives the van
der Pol oscillator as a simple model of it:

$$
\dot{u} = v, \qquad \dot{v} = (\mu - u^2)\, v - u
$$

For small $u$ the term $\mu v$ is a damping of the wrong sign when $\mu > 0$: it
feeds energy in. The term $-u^2 v$ takes energy out at large amplitude. The
balance of the two is a self-sustained oscillation, a limit cycle.

**The Hopf bifurcation.** The only equilibrium is the origin. Its Jacobian
$\begin{pmatrix} 0 & 1 \\ -1 & \mu \end{pmatrix}$ has trace $\mu$ and determinant 1, so the eigenvalues are
$\mu/2 \pm i \sqrt{1 - \mu^2/4}$: a stable focus for $\mu < 0$, an unstable one for
$\mu > 0$. A complex pair crosses the imaginary axis at $\mu = 0$: a Hopf
bifurcation [MA §9.1.1, p. 224].

**Averaging.** For small $\mu$ the orbit is nearly a circle of slowly
changing radius. Write $u = a \cos(t + \varphi)$ and $v = -a \sin(t + \varphi)$ with $a$
and $\varphi$ slowly varying, and replace the equations for $a$ and $\varphi$ by their
averages over one turn: the method of averaging, which [MA §9.2.3,
pp. 237–238] applies to the Rayleigh oscillator. Here it gives

$$
\dot{a} = \frac{a}{2} \left( \mu - \frac{a^2}{4} \right), \qquad \dot{\varphi} = 0
$$

For $\mu < 0$ every amplitude decays. For $\mu > 0$ the amplitude grows if
$a < 2\sqrt{\mu}$ and decays if $a > 2\sqrt{\mu}$: there is a stable limit
cycle of amplitude $2\sqrt{\mu}$ and period close to $2\pi$. Because the cycle
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
with $2\sqrt{\mu}$ to three decimals while $\mu$ is small. Further out the
agreement degrades slowly: the cycle is no longer a circle.

For large $\mu$ the cycle changes its character. It creeps slowly along the
curve where $\dot{v} = 0$ and jumps quickly between its branches: a *relaxation
oscillation*, whose period grows in proportion to $\mu$. Its analysis needs
other methods than the weakly nonlinear ones above (see Strogatz, ch. 7).
With the substitution $u = \sqrt{\mu}\, x$, the model becomes the more common
form $\ddot{x} - \mu\, (1 - x^2)\, \dot{x} + x = 0$, whose cycle has amplitude about 2 for
every $\mu$.

!!! note "Exercises"
    1. Derive the averaged equation for $a$. Hint: substitute $u$ and $v$
       into $\dot{v} = \dots$, and use $\langle \sin^2\theta \rangle = 1/2$ and
       $\langle \sin^2\theta \cos^2\theta \rangle = 1/8$.
    2. Run `mu = 0.1` from `u = 0.1` and from `u = 3`. Where do both runs
       end?
    3. [MA §9.2.3] finds that the Rayleigh oscillator
       $\ddot{x} + \omega_0^2 x = \varepsilon\, (\dot{x} - \dot{x}^3)$ has a limit cycle of amplitude
       $2 / (\sqrt{3}\, \omega_0)$. Write it as an `.ode` file and check the
       amplitude for $\omega_0 = 1$ and $\varepsilon = 0.1$.
    4. Measure the period of the cycle for `mu = 10` (use `meth="stiff"`
       and a long run). How does it compare with $2\pi$?

??? success "Solutions"
    1. With $u = a \cos\theta$, $v = -a \sin\theta$ and $\theta = t + \varphi$, the condition
       $\dot{a} \cos\theta - a \dot{\varphi} \sin\theta = 0$ and the equation for $v$ give
       $\dot{a} = -(\mu - a^2 \cos^2\theta)(-a \sin\theta) \sin\theta$, averaged over $\theta$:
       $\dot{a} = a\, (\mu/2 - a^2/8)$.
    2. Both end on the same cycle, of amplitude 0.632 = $2\sqrt{0.1}$: the
       limit cycle attracts orbits from inside and from outside.
    3. With `x' = y`, `y' = -w0^2*x + eps*(y - y^3)`, a run of 300 time
       units from `x = 0.1` ends on a cycle of amplitude 1.156, and
       $2/\sqrt{3} = 1.1547$.
    4. About 19.08, three times $2\pi$: the relaxation oscillation is much
       slower than the small cycle near the Hopf point.

## Example 5: the forced, damped pendulum and chaos

Now switch on the force in the pendulum equation,
$\ddot{\theta} + 2\gamma \dot{\theta} + \omega_0^2 \sin\theta = a_0 \cos\Omega t$. The system is no longer autonomous:
its state is $(\theta, \dot{\theta})$ together with the phase of the force, which leaves
room for chaos [MA §10.1, p. 250]. With a weak force the pendulum settles
on an oscillation that repeats with the period of the force
[MA Box 8.8, p. 199].

A clear way to see what the motion does is to look at the state once per
period $T = 2\pi/\Omega$ of the force. This *stroboscopic map* is a Poincaré
section [MA §6.6.2, p. 138; §8.4, p. 203]. An orbit of period $T$ shows up
as one point, an orbit of period $2T$ as two, a chaotic orbit as a cloud of
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

When the pendulum goes round and round, $\theta$ grows without limit:
`bound=1e6` keeps XPPAUT from stopping the run when $\lvert\theta\rvert$ passes its
default bound of 100.

The Lyapunov exponent measures the same thing in a number. Follow two
states a small distance $d_0$ apart for one period of the force, see by
which factor their distance has grown, put the second state back at the
distance $d_0$ in the direction it has moved to, and repeat. The average of
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

As the force grows the periodic orbit of period $T$ loses its stability to
one of period $2T$: a period doubling, where a Floquet multiplier passes
through $-1$ [MA §9.1.2, p. 227]. More doublings follow at shorter and shorter
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
twice, with angles that differ by $10^{-6}$:

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

The gap reaches a whole radian at $t = 114$, close to the Lyapunov time
$\ln(1/10^{-6}) / \lambda_L = 118$. (In runs that differ only in rounding, as on
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
       orbit. `th` decreases by $2\pi$ in each period of the force: the
       pendulum goes round once per period, in step with the force.
    3. $\ln(10) / 0.117 \approx 20$ time units later. Here the run gives 147
       instead of 114, 33 later; in runs that differ only in rounding the
       delay ranges from about 5 to 50. The growth rate varies along the
       orbit, and only its long-time average is $\lambda_L$.

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
