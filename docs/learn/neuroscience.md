# Neuroscience: from a leaky membrane to a spiking neuron

A neuron is an electrical circuit with a decision in it. Below some input
it sits at rest; above it, it fires. The mathematics of that decision is
bifurcation theory, and three small models are enough to see all of it:

1. a **passive membrane**, which has one equilibrium and cannot fire;
2. the **theta neuron**, the simplest model that fires, and the clearest
   picture of how the firing rate grows from zero;
3. the **FitzHugh-Nagumo** model, a two-variable caricature of a spike, with
   an excitable regime, a Hopf bifurcation and relaxation oscillations.

Each example gives the model as an `.ode` file, what you should see, and
Python that does it with `xpplab`. Save each model under the name in its
first line, in the folder where you run the Python. The snippets of a page
run one after the other, in a single script. The full Morris-Lecar story is
in [Why does a neuron fire?](../manual/neuron-tutorial.md); this page
prepares for it and goes in other directions.

## Theory in five paragraphs

**State, vector field, equilibrium.** A model is a system of differential
equations `x' = f(x; p)`, where `x` is the state (voltages, gating
variables) and `p` are parameters (currents, conductances). An
**equilibrium** is a state with `f(x*) = 0`: nothing changes there. For a
neuron it is the resting potential.

**Linear stability.** Near `x*` the dynamics are `u' = J u`, where `J` is
the Jacobian matrix of `f` at `x*` and `u = x - x*`. The eigenvalues of `J`
decide everything locally. If all have negative real part the equilibrium
is **stable**: small disturbances die out. If one has positive real part it
is **unstable**. Complex eigenvalues mean the return, or the escape, is a
spiral (a **focus**); real ones mean it is monotone (a **node**). One
positive and one negative real eigenvalue make a **saddle**.

**Bifurcations.** When a parameter moves and an eigenvalue reaches the
imaginary axis, the equilibrium changes character. Two cases cover most of
neuroscience. A real eigenvalue crossing zero is a **saddle-node**: two
equilibria collide and disappear. A complex pair crossing is a **Hopf
bifurcation**: the equilibrium loses stability and a periodic orbit,
a train of spikes, is born around it.

**Excitability.** A neuron is **excitable** when the rest state is stable
but a large enough disturbance sends the state on a long excursion (a
spike) before it returns. The threshold is not a number in the equations;
it is a separatrix in the phase plane, often the stable manifold of a
saddle or the edge of a small unstable orbit.

**Two ways to start firing.** In **type I** excitability the firing rate
can be arbitrarily small at onset, because the orbit is born from a
saddle-node on an invariant circle (SNIC) and has a very long period. In
**type II** excitability firing begins at a nonzero rate, at a Hopf
bifurcation. Example 2 is type I, example 3 type II.

## Example 1: the passive membrane

The membrane of a cell without voltage-gated channels is a capacitor in
parallel with a leak conductance. With the time in milliseconds and the
voltage relative to rest,

```
tau dv/dt = -v + R I
```

where `tau = Cm R` is the **membrane time constant**.

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
`-1/tau`: stable for every parameter. A passive membrane cannot fire,
whatever the current; it only filters its input. After one time constant
the voltage has covered 63% of the way to `R I`.

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

A neuron near a saddle-node on an invariant circle is described by a single
angle `theta`: the neuron spikes each time `theta` passes `pi`. The
**theta neuron** (Ermentrout and Kopell) is the normal form,

```
theta' = (1 - cos(theta)) + (1 + cos(theta)) I
```

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

**For `I > 0`** there is no equilibrium. `theta` goes round and round with
exact period `pi / sqrt(I)`, so the firing rate is

```
f(I) = sqrt(I) / pi
```

It starts from zero with infinite slope: arbitrarily slow firing, the
signature of type I.

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
is a straight line.

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
    2. Near `I = 0`, the time to pass the "ghost" of the vanished
       equilibria is long. With `I = 0.001`, how long is the period? Check
       against `pi / sqrt(I)`.
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
       `-0.8411 + 2 pi = 5.4421`.

## Example 3: FitzHugh-Nagumo, excitability and the Hopf bifurcation

FitzHugh and Nagumo reduced the Hodgkin-Huxley model to two variables: a
fast voltage `v` with a cubic nonlinearity, and a slow recovery variable
`w`.

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
spike. Kick it to `-0.8` instead and it goes straight back. Between the two
is the threshold, near `v = -0.65`.

```python
fhn = xs.load("fhn.ode")
eq = fhn.equilibrium()
eq.kind, eq.eigenvalues, eq["v"]       # 'stable focus', -0.25 ± 0.21i, -1.1994

fhn.set(total=150)
small = fhn.run(v=-0.8, w=eq["w"])     # returns to rest
big = fhn.run(v=-0.4, w=eq["w"])       # one spike
small["v"].max(), big["v"].max()       # -0.8 (no spike), 1.77 (a spike)
```

**Onset of firing.** Raise `I`. The cubic lifts and the crossing moves up
the left branch toward the **knee**, where the cubic turns. The equilibrium
loses stability when the trace of the Jacobian, `1 - v*^2 - eps b`, becomes
zero. With `eps b = 0.064`, that is at `|v*| = 0.9675`. Solving for `I`
gives two Hopf bifurcations, a first near `I = 0.3313` where the
equilibrium turns unstable, and a second at `I = 1.4187` where it becomes
stable again on the right branch. Between them the neuron fires
periodically. Because the oscillation appears with nonzero amplitude and
frequency, this is type II.

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

The oscillation has almost its full size right away: started from the
initial values of the file, the range of `v` at `I = 0.32` is 0.11 and at
`I = 0.33` it is already 3.7. This sudden growth
in a narrow range of `I` is the **canard explosion** of a relaxation-like
system. The Hopf bifurcation is **subcritical**: the small unstable cycles
that leave the Hopf point run towards smaller `I` and turn back at a fold
of cycles near `I = 0.3241`. Between 0.3241 and 0.3313 the rest state is
stable and large stable spiking cycles exist as well, so the neuron is
bistable there.

**Relaxation oscillations.** Make the recovery slower, `eps = 0.01`. The
fast voltage then spends most of its time near the left or right branch of
the cubic, creeping along it with `w`, and jumps in between: the trace has
long plateaus and sharp edges, the shape of a relaxation oscillation. The
period grows steeply as `eps` falls.

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
    2. Find the threshold at `I = 0` to two digits: the smallest `v` (with
       `w` at its rest value) from which the run produces a spike.
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
    2. Run `fhn.run(v=v0, w=eq["w"])` for `v0` from -1.2 to -0.4 and look at
       the maximum of `v`. The maximum is -0.67 for
       `v0 = -0.7` and 1.71 for `v0 = -0.6`: the threshold is between them.
       Bisect.
    3. The pulse of current pushes `v` and `w` off the cycle; the train
       comes back to the same cycle but shifted in time (a phase shift).
       The size of the shift depends on when in the cycle the pulse arrives;
       the curve of shift against arrival time is the phase response curve
       (see `phase_response` in the [analysis guide](../python/analysis.md)).
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
- [Bifurcation analysis with AUTO](../manual/auto.md) explains the
  diagram, its point types and the two-parameter continuations.
