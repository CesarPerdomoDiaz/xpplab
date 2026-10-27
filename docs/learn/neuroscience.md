# Neuroscience: from a leaky membrane to a spiking neuron

A neuron at rest sits at an equilibrium. Inject enough current and it fires
action potentials, one after another. How the rest state gives way to
firing, and what that implies for how the neuron responds to inputs, is a
question about bifurcations. This page follows Izhikevich's *Dynamical
Systems in Neuroscience*, cited as **[DSN]** below, which builds the
subject on exactly that idea, and works through three small models:

1. a **passive membrane**, which has one equilibrium and cannot fire
   [DSN §2.1];
2. the **theta neuron**, the simplest model that fires, and the clearest
   picture of how firing can start at an arbitrarily low rate
   [DSN §3.3, §6.1.2];
3. the **FitzHugh-Nagumo** model, a two-variable caricature of a spike,
   with excitability, a Hopf bifurcation and relaxation oscillations
   [DSN §4.2.6, §6.3.4].

A reference such as [DSN §7.2.5, p. 240] points to the section and page of
the book; the complete references are at the [end of the page](#references).
The author offers the book as a PDF on his website.

Each example gives the model as an `.ode` file, what you should see, and
Python that does it with `xpplab`. Save each model under the name in its
first line, in the folder where you run the Python. The snippets of a page
run one after the other, in a single script. The full Morris-Lecar story is
in [Why does a neuron fire?](../manual/neuron-tutorial.md); this page
prepares for it and goes in other directions.

## Theory for neurons

**The membrane as a dynamical system.** A patch of membrane is a capacitor
in parallel with ionic conductances. Kirchhoff's law makes the capacitive
current, `C V'`, equal to the injected current minus the ionic currents, and
that is a differential equation for the voltage [DSN §2.1.3, eq. (2.3)].
When conductances depend on the voltage and on gating variables, the model
gains more equations, and the state becomes a vector `x` with `x' = f(x)`.
An *equilibrium* `f(x*) = 0` is a state that does not change; for a neuron
it is the resting potential, where inward and outward currents balance
[DSN §2.1.4].

**Stability and phase portraits.** Near an equilibrium a small
perturbation `u = x − x*` obeys `u' = J u`, with `J` the Jacobian matrix of
`f` at `x*`. If every eigenvalue of `J` has negative real part the
equilibrium is stable, and if one has positive real part it is unstable. In
the plane, the trace and determinant of `J` sort equilibria into nodes,
foci and saddles [DSN §4.2.2–4.2.5]. The *nullclines*, the curves where one
of the two derivatives vanishes, cross at the equilibria and organise the
whole phase portrait [DSN §4.1.1].

**Bifurcations of the rest state.** As the injected current grows, the rest
state must disappear or lose stability for the neuron to fire repetitively.
For an equilibrium there are two generic ways. A real eigenvalue passes
through zero: the rest state meets an unstable equilibrium and both vanish,
a *saddle-node* (fold) bifurcation [DSN §3.3.4]. A pair of complex
eigenvalues crosses the imaginary axis: the rest state becomes unstable and
a small periodic orbit appears or disappears around it, an *Andronov-Hopf*
bifurcation, supercritical or subcritical [DSN §6.1.3–6.1.4]. If
the saddle-node happens on a closed orbit, a *saddle-node on invariant
circle* (SNIC), the orbit that remains is a spike train whose period can be
arbitrarily long [DSN §6.1.2].

**Hodgkin's classes.** Hodgkin sorted neurons by how they start to fire
when a constant current is switched on. In *Class 1* the firing rate can be
made arbitrarily small; in *Class 2* firing starts at a frequency that is
not small [DSN §7.1.2]. Izhikevich links the classes to the bifurcations:
Class 1 corresponds to a SNIC, Class 2 to a saddle-node off the invariant
circle or to an Andronov-Hopf bifurcation, sub- or supercritical
[DSN §7.1.3, p. 222]. (Other books say type I and type II.)

**Thresholds.** A threshold is not a number written in the equations. In a
neuron near a saddle-node, the stable manifold of the saddle separates the
starting points that lead to a spike from those that do not: a threshold
*manifold* [DSN §7.2.5, p. 240]. Near a subcritical Hopf bifurcation an
unstable periodic orbit plays that role. In other cases, such as the
FitzHugh-Nagumo model below, there is no sharp threshold at all: responses
grow steeply but continuously with the size of the kick, a *quasi-threshold*
in FitzHugh's term, organised by a canard trajectory that follows the
unstable middle branch of the cubic nullcline [DSN §7.2.5, p. 241].

## Example 1: the passive membrane

A membrane without voltage-gated channels has one conductance, the leak,
and with the voltage measured from rest its equation is

```
tau dv/dt = -v + R I
```

where `R` is the input resistance and `tau = C R` the **membrane time
constant** [DSN §2.1.4].

```ode
# leak.ode
v' = (-v + R*I)/tau
par tau=10, R=1, I=5
init v=0
@ total=50, dt=0.01
done
```

This is linear and has the exact solution `v(t) = R I (1 - exp(-t/tau))`
for `v(0) = 0`. The only equilibrium is `v* = R I`, with eigenvalue
`-1/tau`: stable for every parameter. The voltage relaxes to `R I`, a larger
input resistance gives a larger response to the same current, and after one
time constant the voltage has covered 63% of the way. A passive membrane
cannot fire, whatever the current; it only filters its input. It has no
mechanism that regenerates a spike, which is why Izhikevich insists that the
leaky integrate-and-fire neuron, which adds an artificial reset to this
equation, is not a spiking model [DSN §3.3.8, p. 80].

```python
import numpy as np
import xpplab as xs

leak = xs.load("leak.ode")
sol = leak.run()
exact = 5 * (1 - np.exp(-sol.t / 10))
err = abs(sol["v"] - exact).max()      # about 1e-7: the integrator is accurate

eq = leak.equilibrium()
eq.kind, eq.eigenvalues                # 'stable node', [-0.1]
```

Run it in the desktop app too: open the file, press **Run**, and drag the
`tau` slider. A larger `tau` slows the approach; the final value does not
change.

!!! note "Exercises"
    1. Show from the equation that after a step of current the voltage
       reaches 95% of its final value at `t = tau ln 20`, about 3 time
       constants. Check it on the run.
    2. Replace the constant current by `I*sin(2*pi*f*t)` (write
       `v' = (-v + R*I*sin(2*pi*f*t))/tau`). How does the amplitude of `v`
       depend on `f`? Plot it for `f` from 0.001 to 0.5 with `leak.sweep`.

??? success "Solutions"
    1. `1 - exp(-t/tau) = 0.95` gives `t = tau ln 20 = 29.96`. On the run
       `sol.t[np.argmax(sol["v"] >= 0.95 * 5)]` is 29.96 up to the step
       size.
    2. The membrane is a low-pass filter: the amplitude is
       `R I / sqrt(1 + (2 pi f tau)^2)`. It is about `R I` for
       `f << 1/(2 pi tau)` = 0.016 and falls as `1/f` above it. Use
       `leak.sweep("f", freqs, measure=lambda s: s["v"][len(s.t)//2:].max())`
       with `total=500`.

## Example 2: the theta neuron, and why firing can start slowly

Near a saddle-node bifurcation every one-dimensional neuron model looks
like `V' = I + V²`, the *topological normal form* of the saddle-node
[DSN §3.3.4, p. 75]. For `I < 0` it has a rest state `V = −sqrt(|I|)` and a
threshold `V = +sqrt(|I|)`; for `I > 0` the voltage runs off to infinity in
a finite time, which is the upstroke of a spike. Resetting `V` after each
spike gives the quadratic integrate-and-fire neuron [DSN §3.3.8]. The change
of variable `V = tan(θ/2)` maps the whole real line, infinity included, onto
a circle, and turns the model into the **theta neuron** of Ermentrout and
Kopell, which needs no reset [DSN ch. 8, exercise 7, solved on p. 462,
with a scaled version of the same substitution]:

```
theta' = (1 - cos(theta)) + (1 + cos(theta)) I
```

The neuron spikes each time `theta` passes `pi`. Izhikevich calls it the
Ermentrout-Kopell canonical model of Class 1 excitability [DSN p. 469].

```ode
# theta.ode
th' = (1-cos(th)) + (1+cos(th))*I
par I=-0.2
init th=-1
@ total=200, dt=0.01, bound=100000
done
```

The `bound` option matters: XPPAUT stops a run when a variable leaves
`[-100, 100]`, and here `theta` grows without limit as the neuron keeps
spiking.

**For `I < 0`** the right side vanishes where `cos(theta) = (1 + I)/(1 - I)`,
at two angles. One is a stable node, the **rest state**; the other is an
unstable node, the **threshold**. A perturbation that pushes `theta` past
the threshold sends it all the way round the circle: a spike.

**At `I = 0`** the two equilibria merge in a saddle-node, at `theta = 0`.
Because the circle is invariant, this is a saddle-node on invariant circle
[DSN §6.1.2].

**For `I > 0`** there is no equilibrium. `theta` goes round and round with
exact period `pi / sqrt(I)`, so the firing rate is

```
f(I) = sqrt(I) / pi
```

It starts from zero with infinite slope. The time is spent near `theta = 0`,
where the vanished equilibria have left a "ghost" and the flow is very slow
[DSN §3.3.5, p. 75]: arbitrarily slow firing, the signature of Class 1.

```python
th = xs.load("theta.ode")

# below threshold: rest state and threshold
th.parameters["I"] = -0.2
rest = th.equilibrium({"th": -1.0})
thr = th.equilibrium({"th": 1.0})
rest.kind, thr.kind                    # 'stable node', 'unstable node'
rest["th"], thr["th"]                  # -0.8411, +0.8411 (cos = 2/3)

# above threshold: count turns of theta per unit time
th.set(total=200)
currents = [0.05, 0.1, 0.25, 0.5, 1.0, 2.0]
rates = th.sweep("I", currents,
                 measure=lambda s: (s["th"][-1] - s["th"][0]) / (2 * np.pi * 200))
exact_rates = np.sqrt(currents) / np.pi
```

`rates` agrees with `exact_rates` (0.3183 at `I = 1`; at small `I` the run
holds only a few turns, and the last one is incomplete, so 0.0706 against
0.0712 at `I = 0.05`). Plot rate against `I` and against `sqrt(I)`: the second
is a straight line. The same square-root law holds for any neuron near a
SNIC, close enough to the bifurcation [DSN §6.1.2, p. 168].

You can see the bifurcation with AUTO as well: follow the rest state as `I`
rises and a limit point (LP) appears at `I = 0`.

```python
th.parameters["I"] = -0.5
th.initial_conditions["th"] = -1.0
th.equilibrium(set_initial=True)
d = th.auto.equilibria("I", parmin=-1, parmax=0.5)
d.special("LP")                        # the saddle-node, at I = 0
```

!!! note "Exercises"
    1. Show that the equilibria for `I < 0` satisfy
       `cos(theta*) = (1 + I)/(1 - I)`. For `I = -0.2` verify the value
       0.8411 quoted above.
    2. Near `I = 0`, the time to pass the ghost of the vanished equilibria
       is long. With `I = 0.001`, how long is the period? Check against
       `pi / sqrt(I)`.
    3. Start a trajectory just below the threshold, at `th = 0.8`, and
       another just above it, at `th = 0.9`, with `I = -0.2`. Describe the
       two.

??? success "Solutions"
    1. Set `1 - cos + I (1 + cos) = 0`, so `cos (I - 1) = -(1 + I)`, which
       is the stated formula. For `I = -0.2`, `cos = 0.8/1.2 = 2/3` and
       `theta* = arccos(2/3) = 0.8411`. The rest state is `-0.8411` and the
       threshold `+0.8411`: they are symmetric about 0, and the rest state
       is the stable one.
    2. The period is `pi/sqrt(0.001)` = 99.3 time units. With `total=1000`
       the run holds 10 turns: `th[-1] / (2*np.pi)` is 10.00.
    3. The first (0.8) is below the threshold and moves back down to the
       rest state, `-0.8411`. The second (0.9) goes up and round the
       circle, a spike, and ends at the rest state plus a full turn,
       `-0.8411 + 2 pi = 5.4421`. In one dimension the threshold is a
       single point, the unstable equilibrium.

## Example 3: FitzHugh-Nagumo, excitability and the Hopf bifurcation

The FitzHugh-Nagumo model imitates how Hodgkin-Huxley-type models generate
spikes, with two variables instead of four: a fast voltage `v` with an
N-shaped (cubic) nullcline, and a slow recovery variable `w` that stands for
the activation of an outward current [DSN §4.2.6, p. 106]. A common form
of the model is:

```ode
# fhn.ode
v' = v - v^3/3 - w + I
w' = eps*(v + a - b*w)
par I=0, a=0.7, b=0.8, eps=0.08
init v=-1.2, w=-0.6
@ total=200, dt=0.1, xp=v, yp=w, xlo=-2.5, xhi=2.5, ylo=-1.5, yhi=2
done
```

**Nullclines.** The `v`-nullcline `w = v - v^3/3 + I` is a cubic: it rises,
falls and rises again. The `w`-nullcline `w = (v + a)/b` is a straight
line. The current `I` only lifts the cubic. The equilibrium sits where they
cross; the phase plane of the app shows it at once
(`m.phase_plane("v", "w")` in Python).

**Excitable regime.** At `I = 0` the line crosses the cubic on its left
branch, at `v* = -1.1994`, `w* = -0.6243`. The eigenvalues are
`-0.251 ± 0.213i`: a stable focus. Start the model at that point and nothing
happens. Kick `v` to `-0.4` and the voltage rises along the cubic, then
jumps to the right branch near `v = 1.8`, creeps down and returns: a single
spike. Kick it to `-0.8` instead and it goes straight back.

```python
fhn = xs.load("fhn.ode")
eq = fhn.equilibrium()
eq.kind, eq.eigenvalues, eq["v"]       # 'stable focus', -0.25 ± 0.21i, -1.1994

fhn.set(total=150)
small = fhn.run(v=-0.8, w=eq["w"])     # returns to rest
big = fhn.run(v=-0.4, w=eq["w"])       # one spike
small["v"].max(), big["v"].max()       # -0.8 (no spike), 1.77 (a spike)
```

There is no saddle here, so no stable manifold to act as a threshold.
Between the two kicks the size of the response grows steeply but
continuously: a quasi-threshold, which Exercise 2 measures
[DSN §7.2.5, p. 241].

**Onset of firing.** Raise `I`. The cubic lifts and the crossing moves up
the left branch toward the **knee**, where the cubic turns. The equilibrium
loses stability when the trace of the Jacobian, `1 - v*^2 - eps b`, becomes
zero. That happens not at the knee (`v* = -1`) but slightly to its right,
at `|v*| = sqrt(1 - eps b) = 0.9675`: the slow variable shifts the Hopf
point by an amount of the order of `eps` [DSN §4.2.6, p. 107; §6.3.4,
p. 199]. Solving for `I` gives two Hopf bifurcations, a first near
`I = 0.3313` where the equilibrium turns unstable, and a second at
`I = 1.4187` where it becomes stable again on the right branch. Between
them the neuron fires periodically, and it starts at a frequency that is
not small: Class 2.

```python
fhn.parameters["I"] = 0
d = fhn.auto.equilibria("I", parmax=2)
hb = d.special("HB")
[round(p.value, 4) for p in hb]        # [0.3313, 1.4187]
hb[0].stable, hb[1].stable             # False, True: the flag is for the point itself

# simulate on both sides of the first Hopf point
fhn.set(total=400)
def late_range(I):
    s = fhn.run(I=I)
    v = s["v"][len(s.t) // 2:]
    return v.max() - v.min()
late_range(0.30), late_range(0.35)     # about 0.005 (rest) and 3.8 (spiking)

# the periodic orbits that leave the first Hopf point
orbits = fhn.auto.continue_from(hb[0], "periodic")
orbits.special("LP")[0].value          # 0.3241: the cycles turn back at a fold
```

The Hopf bifurcation is **subcritical**: the small cycles that leave the
Hopf point are unstable, run towards smaller `I`, and turn back at a fold of
cycles near `I = 0.3241`, where they meet the large stable spiking cycle
[DSN §6.1.4, §6.2.3]. Between 0.3241 and 0.3313 the rest state is stable
and the large spiking cycle exists as well, so the neuron is bistable
there. Izhikevich notes that in two-dimensional conductance-based models a
subcritical Hopf point always comes with such a coexistence of rest and
another attractor [DSN §6.1.4, pp. 174–175].

The oscillation has almost its full size right away: started from the
initial values of the file, the range of `v` at `I = 0.32` is 0.11 and at
`I = 0.33` it is already 3.7. In a system with a fast and a slow variable,
the cycles that grow from a Hopf point near a knee follow for a while the
unstable middle branch of the cubic (*canard* cycles), and they exist only
in an exponentially thin range of the parameter. In a simulation the cycle
seems to jump from small to large: the **canard explosion** [DSN §6.3.4,
pp. 198–199].

**Relaxation oscillations.** Make the recovery slower, `eps = 0.01`. The
fast voltage then spends most of its time near the left or right branch of
the cubic, creeping along it with `w`, and jumps in between: the trace has
long plateaus and sharp edges, the shape of a relaxation oscillation
[DSN §4.1.4, p. 98]. The period grows steeply as `eps` falls, since the
slow creeping takes a time of order `1/eps`.

```python
fhn.parameters["I"] = 0.5
fhn.set(total=2000)
periods = {}
for eps in (0.08, 0.04, 0.02, 0.01):
    s = fhn.run(eps=eps)
    t, v = s.t, s["v"]
    up = t[1:][(v[:-1] < 0) & (v[1:] >= 0)]
    periods[eps] = np.diff(up)[-3:].mean()
# periods: about 39.5, 68, 122 and 227 for eps = 0.08, 0.04, 0.02, 0.01
```

!!! note "Exercises"
    1. Compute by hand the Jacobian of the model at an equilibrium
       `(v*, w*)`, find its trace and determinant, and use them to find
       `I` for the first Hopf point; compare with AUTO's 0.3313.
    2. Measure the quasi-threshold at `I = 0`: the largest `v` reached
       from a kick to `v0` (with `w` at its rest value), for `v0` between
       -0.8 and -0.5.
    3. With `I = 0.5`, make the system oscillate, then add a pulse of
       current: replace `I` in the `v'` equation by
       `(I + 2*heav(t-100)*heav(110-t))`. What happens to the spike train?
    4. Open `fhn.ode` in the app, press **Nullclines**, and drag `I`. At what
       `I` does the crossing pass the knee of the cubic? Is it the same
       value as the Hopf bifurcation?

??? success "Solutions"
    1. `J = [[1 - v*^2, -1], [eps, -eps b]]`. The trace is
       `1 - v*^2 - eps b`; the determinant is `eps (1 - b (1 - v*^2))`. A
       Hopf bifurcation needs trace 0, so `v*^2 = 1 - eps b = 0.936`, and
       `v* = -0.9675` on the left branch. The equilibrium lies on both
       nullclines, so `I = (v* + a)/b - v* + v*^3/3` = 0.3313.
    2. Run `fhn.run(v=v0, w=eq["w"])` and look at the maximum of `v`. It
       is -0.67 for `v0 = -0.7` and 1.71 for `v0 = -0.6`. Bisect between
       them and the maximum takes intermediate values too, on a very narrow
       range of `v0`: the response is steep but continuous, so the
       "threshold" depends on what you call a spike.
    3. The pulse of current pushes `v` and `w` off the cycle; the train
       comes back to the same cycle but shifted in time (a phase shift).
       The size of the shift depends on when in the cycle the pulse arrives;
       the curve of shift against arrival time is the phase response curve
       (see `phase_response` in the [analysis guide](../python/analysis.md),
       and [DSN ch. 10]).
    4. The knee is at `v = -1`, the maximum of the cubic's left branch.
       The crossing is there when `I = (-1 + 0.7)/0.8 + 1 - 1/3 = 0.2917`.
       The Hopf point, 0.3313, is a little beyond it: the trace
       `1 - v*^2 - eps b` is still negative at the knee, where `v*^2 = 1`,
       and reaches zero only when `v*^2` has dropped to `1 - eps b`.

## Where to go next

- [Why does a neuron fire?](../manual/neuron-tutorial.md) does the same
  analysis with the Morris-Lecar model, a conductance-based neuron, and
  finds the Hopf bifurcation and the bistability with AUTO.
- The examples folder has `ml1.ode`, `hhred.ode` and `iaf.ode`
  (Morris-Lecar, a reduced Hodgkin-Huxley and an integrate-and-fire
  neuron). Open them in the app and repeat the steps above.
- [DSN] continues with bursting (ch. 9) and synchronization (ch. 10).
  Ermentrout and Terman cover the same ground from the mathematical side.
- [Bifurcation analysis with AUTO](../manual/auto.md) explains the
  diagram, its point types and the two-parameter continuations.
- The [mechanics page](mechanics.md) treats the same bifurcations in
  pendulums, following [MA], and the van der Pol oscillator, which has the
  same cubic nonlinearity as the FitzHugh-Nagumo model [DSN §4.1.4].

## References

- **[DSN]** Izhikevich, E. M. (2007). *Dynamical Systems in Neuroscience:
  The Geometry of Excitability and Bursting*. Cambridge, MA: MIT Press.
  ISBN 978-0-262-09043-8. The author makes the book available as a PDF on
  his website (izhikevich.org).
- Ermentrout, G. B. and Terman, D. H. (2010). *Mathematical Foundations of
  Neuroscience* (Interdisciplinary Applied Mathematics 35). New York:
  Springer. ISBN 978-0-387-87707-5.
- **[MA]** Sanz Recio, F. J. and Sánchez Arriaga, G. (2019). *Mecánica
  analítica: lagrangiana, hamiltoniana y sistemas dinámicos*. Madrid:
  McGraw-Hill/Interamericana de España. ISBN 978-84-486-1539-0.
