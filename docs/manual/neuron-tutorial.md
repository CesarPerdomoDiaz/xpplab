# Tutorial: why does a neuron fire?

A neuron at rest holds a steady voltage. Inject a steady current and, above
some value, it fires: the voltage spikes again and again. What changes in
the equations at that value? In this tutorial you answer the question with
the Morris-Lecar model, a two-variable model of a membrane with a calcium
current, a potassium current and a leak. You simulate it, draw its phase
plane and nullclines, find its equilibrium, and build a bifurcation diagram
over the applied current that shows where firing starts and where it stops.
At the end you change a few parameters and watch the same model start to
fire in a different way. It takes about half an hour.

The theory follows Izhikevich's Dynamical Systems in Neuroscience, cited as
[DSN] with section and page, for example [DSN §6.1.4, p. 174]. The book
treats the onset of firing as a question about bifurcations, which is what
the app lets you compute. The full reference is at the end of the page, and
the author offers the book as a PDF on his website.

Each step shows what to do in the app and the Python that does the same.
Every command in the app also prints its Python line in the command window
(lines starting with `>>>`), so you can check the two against each other.
The Python snippets run one after the other in a single script.

## 0. The model

[Open the Morris-Lecar model](example:lecar.ode) (or choose it in the
Models window). The two variables are the voltage `v` and the fraction of
open potassium channels `w`; `iapp` is the applied current.

```
v' = iapp + gl*(vl-v) + gk*w*(vk-v) - gca*minf(v)*(v-1)
w' = lamn(v)*(ninf(v)-w)
```

**Where the first equation comes from.** Electrically, a patch of membrane
is a capacitor with ion channels in parallel. The current that charges the
capacitor, `C v'`, is the injected current minus the currents that flow
through the channels, and each of those is a conductance times the gap
between the voltage and the reversal potential of its ion
[DSN §2.1.3, eq. (2.3), p. 28]. The first line is this balance with
`C = 1` and three currents: a leak (conductance `gl`, reversal `vl`), a
potassium current (`gk*w`, reversal `vk`) and a calcium current
(`gca*minf(v)`, reversal 1). Between the potassium and calcium reversal
potentials, the calcium current flows inward and raises the voltage, and
the potassium current flows outward and lowers it [DSN §2.1.3, p. 29]. The
file uses scaled units, with the calcium reversal potential set to 1, so
the voltages and times below are not millivolts and milliseconds.

**One fast, one slow.** The two gated currents pull in opposite
directions. When the voltage rises, calcium channels open, more inward
current flows and the voltage rises further: a positive feedback, which
Izhikevich calls an amplifying gating variable. Potassium channels also
open when the voltage rises, but their outward current brings it back down:
a negative feedback, a resonant variable. A model needs one of each to
spike, and the positive feedback must act faster than the negative one
[DSN §5.1.1, pp. 129-130]. Here the calcium channels are assumed to open
at once, so their open fraction is simply `minf(v)` and needs no equation
of its own; the potassium fraction `w` lags behind and relaxes towards
`ninf(v)` at the rate `lamn(v)` [DSN §5.1.1, p. 131].

**Where the model sits in the book.** Morris and Lecar proposed it in 1981
for the voltage oscillations of the barnacle giant muscle fiber. Izhikevich
uses throughout the book a near twin with a persistent sodium current in
place of calcium, the INa,p+IK model, and points out that the two behave
alike in many respects [DSN ch. 4, p. 89; §5.1.2, p. 132]. Rinzel and
Ermentrout used Morris-Lecar to explain Hodgkin's classes of excitability
in an influential book chapter [DSN ch. 5, Bibliographical Notes, p. 157],
and the header of `lecar.ode` says the file follows that chapter.

**Two kinds of potassium current.** The file stores several parameter
sets. Its own values (saved again as the set `snic`) and the set `hopf`
differ mainly in the potassium activation curve `ninf(v)`: `v3` is the
voltage at which half the channels are open and `v4` sets how gradually
they open (`hopf` also changes `gca` and `phi` a little). With `hopf`,
`v3` falls from 0.1 to 0 and the curve is wider, so part of the potassium
current is already on at rest: about 8% of the channels are open at
`iapp` = 0.1, against about 0.1% with the file's values at `iapp` = 0.05.
Izhikevich runs the same comparison in his model, a low-threshold against
a high-threshold potassium current, and shows that shifting the activation
curve alone changes how the neuron fires [DSN §1.1.3, p. 6; Fig. 4.1,
p. 90]. This tutorial starts with `hopf` and comes back to the file's own
values in step 7. Apply the set from **Workspace > Sets**, or with the
command [use hopf](cmd:use hopf).

In Python:

```python
import xpplab as xs

m = xs.load("lecar.ode")      # the file is in the app's examples folder
m.apply_set("hopf")
m.parameters["iapp"]          # 0.05
```

## 1. Simulate: rest and firing

Set the current to 0.1 and the total time to 200 in the toolbar (or in
Workspace > Parameters), then press **Run**:

```commands
set iapp=0.1 total=200
run
```

The **Time Series** tab shows `v` falling to about -0.375 and staying
there. The neuron is at rest. Now raise the current:

```commands
set iapp=0.3
run
```

`v` now swings between about -0.37 and 0.27, again and again. The neuron
fires. With **Live** ticked you can also drag the `iapp` slider in
Workspace > Parameters and watch the trace change.

```python
m.set(total=200)
m.parameters["iapp"] = 0.1
rest = m.run()
rest["v"][-1]                 # -0.375: the steady voltage

m.parameters["iapp"] = 0.3
sol = m.run()
late = sol["v"][len(sol.t) // 2:]      # drop the transient
late.min(), late.max()                 # about -0.37 and 0.27
```

In the language of dynamical systems the two runs end on two different
kinds of objects. The rest state is an **equilibrium**: a state that does
not change, because the inward and outward currents cancel there
[DSN §2.1.4, p. 29]. Repetitive firing is a **limit cycle**: an isolated
closed orbit that the trajectories around it approach [DSN §4.1.3,
pp. 96-97]. The question "does it fire?" becomes: how does the
equilibrium give way to a limit cycle as `iapp` grows? A change of that
kind is a **bifurcation** [DSN ch. 6, p. 159]. To find it, look at the
state space.

## 2. The phase plane and the nullclines

Open the **Phase Plane** tab. The trajectory is drawn in the (v, w) plane.
Draw the nullclines and the direction field:

```commands
nullclines
dirfield
```

(or press **Nullclines** and **Flow** in the toolbar). The **v-nullcline** is
the curve where dv/dt = 0 and the **w-nullcline** is where dw/dt = 0. On the
first the arrows are vertical (only `w` changes), on the second they are
horizontal (only `v` changes). Where the two curves cross, both derivatives
vanish: that is an **equilibrium**, a steady voltage [DSN §4.1.1, p. 92].

The two curves have typical shapes. The w-nullcline is the potassium
activation curve `ninf(v)` itself, an S-shaped curve. The v-nullcline is
N-shaped, like a cubic: it falls, rises between two knees, and falls
again. Izhikevich describes the same pair of shapes for his model, and
notes that the four regions they cut the plane into match the phases of a
spike: the upstroke, the downstroke while potassium is still opening, the
refractory period when both variables fall, and the return towards the
next spike [DSN §4.1.1, p. 93].

With **Click to start** on, click in the plot to start trajectories from
other points, and **Keep curves** to keep them. At `iapp` = 0.3 every
trajectory winds out from the crossing and settles onto a closed loop, the
spike. A closed orbit in the plane must enclose at least one equilibrium
[DSN §4.1.3, p. 97], and this one encloses the crossing. At `iapp` = 0.1
every trajectory ends at the crossing.

```python
ax = m.phase_plane("v", "w", xlim=(-0.6, 1.2), ylim=(-0.25, 1.2))

xn, yn = m.nullclines("v", "w", (-0.6, 1.2), (-0.25, 1.2))
xn.shape, yn.shape            # line segments: x1, y1, x2, y2
```

Change `iapp` and the v-nullcline moves while the w-nullcline stays where
it is: the current only enters the voltage equation. The crossing moves
along the w-nullcline, and what matters is what kind of equilibrium it is.
To see the nullclines for several values at once, use
[nullclines freeze iapp 0 0.4 5](cmd:nullclines freeze iapp 0 0.4 5) and
`nullclines unfreeze` to remove them again.

## 3. The equilibrium and its stability

Press **Equilibrium** in the toolbar, or type `eq`. Newton's method finds
the steady state near the initial values and the command window prints its
eigenvalues. The phase plane marks it: filled if stable, open if not.

```commands
set iapp=0.1
eq
set iapp=0.3
eq
```

**What the eigenvalues say.** Near an equilibrium the model behaves like
its linearization, a linear system whose matrix is the Jacobian `J` of the
right-hand sides at that point. If every eigenvalue of `J` has a negative
real part, small disturbances die out and the equilibrium is stable; if one
has a positive real part, it is unstable [DSN §4.2.2-4.2.3, pp. 101-103].
In the plane there are three main kinds: a **node** (two real eigenvalues
of the same sign), a **saddle** (real, of opposite signs) and a **focus**
(a complex pair). Near a focus the trajectories turn around it; the
imaginary part of the eigenvalues sets how fast they turn and the real part
whether they spiral in or out [DSN §4.2.5, pp. 103-104].

At `iapp` = 0.1 the eigenvalues are -0.37 ± 0.23i: the real part is
negative, so the equilibrium is a **stable focus**. The neuron is at rest,
and a small disturbance spirals back. At `iapp` = 0.3 they are +0.27 ±
0.26i: the equilibrium is an **unstable focus**. Trajectories start near it
and spiral away, until they reach the loop. That is firing.

```python
m.parameters["iapp"] = 0.1
eq = m.equilibrium()
eq.kind, eq.eigenvalues       # 'stable focus', -0.37 +/- 0.23i

m.parameters["iapp"] = 0.3
eq = m.equilibrium()
eq.kind, eq.eigenvalues       # 'unstable focus', +0.27 +/- 0.26i
```

A rest state that is a focus makes the voltage ring after a brief input, a
damped oscillation below the firing level. Izhikevich calls neurons with
this property **resonators**, and those whose rest state is a node, which
return without ringing, **integrators** [DSN §7.2, p. 229; §7.2.1,
p. 230]. At `iapp` = 0.1 the damping is strong and the ringing is hard to
see; closer to firing it is clear, as step 5 shows.

Somewhere between 0.1 and 0.3 the real part of the eigenvalues crosses
zero. At that value a pair of eigenvalues sits on the imaginary axis: a
**Hopf bifurcation**, which Izhikevich calls by its full name,
Andronov-Hopf. A stable equilibrium can stop being stable in only two
generic ways as a parameter changes: a real eigenvalue reaches zero and the
equilibrium merges with another one and vanishes (a **saddle-node**
bifurcation), or a complex pair crosses the imaginary axis and the
equilibrium stays but turns unstable (Andronov-Hopf) [DSN §6.1, p. 159].

Why not a saddle-node here? Every equilibrium is a voltage `v` at which
the applied current exactly balances the steady-state ionic current, with
`w = ninf(v)`. Plot that current against `v` and you have the
**steady-state I-V curve**. With the `hopf` parameters it rises
everywhere, so each value of `iapp` has exactly one equilibrium. There is
nothing for the rest state to merge with, and it can only lose stability
[DSN §6.1, pp. 161-162; §6.1.3, p. 168]:

```python
import numpy as np

def steady_current(model, v):
    """The ionic current at rest at voltage v (w = ninf(v))."""
    p = model.parameters
    minf = 0.5 * (1 + np.tanh((v - p["v1"]) / p["v2"]))
    ninf = 0.5 * (1 + np.tanh((v - p["v3"]) / p["v4"]))
    return (p["gl"] * (v - p["vl"]) + p["gk"] * ninf * (v - p["vk"])
            + p["gca"] * minf * (v - 1))

v = np.linspace(-0.6, 1.2, 1801)
(np.diff(steady_current(m, v)) > 0).all()    # True: one equilibrium for each iapp
```

The next step finds the Hopf point exactly.

## 4. The bifurcation diagram over the applied current

Put the model back at the rest state, and let AUTO follow the equilibrium as
`iapp` grows. The parameter range of the diagram is set in the **Bifurcation**
window by **Par min** and **Par max**.

1. Set `iapp` to 0.1 and press **Run**, so the state sits near the rest
   state.
2. Open the **Bifurcation** window. Choose `iapp` as the parameter, set
   **Par max** to 0.6, and press **Follow steady states**.

```commands
set iapp=0.1
run
auto set parmax=0.6
auto steady iapp
```

The diagram plots the voltage of the equilibrium against `iapp`. The red
line is stable, the black dashed line unstable. The orange labels are
special points: two **HB** points, at `iapp` = 0.2625 and 0.4569. Below the
first the equilibrium is stable; between them it is unstable; above the
second it is stable again, at a high voltage. A neuron that sits at such a
depolarized steady voltage does not fire; this is called excitation block
[DSN §6.2, Fig. 6.19, p. 179].

```python
m.parameters["iapp"] = 0.1
m.run()
d = m.auto.equilibria("iapp", parmax=0.6)
d.special("HB")               # [<HB #2: ... iapp=0.262468 ...>, <HB #3: ...>]
hb = d.first("HB")
hb.value, hb.stable           # 0.2625, False (one pair of eigenvalues is on the axis)
hb.eigenvalues                # about +/- 0.435i: on the imaginary axis
```

The imaginary part, 0.435, is the angular frequency of the oscillations
that appear at the Hopf point, so their period starts at about
2 pi / 0.435 = 14.4 [DSN §6.1.3, p. 170].

## 5. The oscillations born at the Hopf point

The Hopf point says that oscillations appear there, not how large they are
or whether they are stable. Set **Max steps** to 55 (with the file's 2000,
AUTO goes back and forth along the same branch and labels the same points
again). Click the first HB in the table of special points (or in the
plot), then press **Periodic orbits from this Hopf point**. In the command
window, with the label shown in the table:

```commands
auto set nmax=55
auto from 2 periodic
```

AUTO follows the limit cycle as `iapp` changes. The diagram now has the
maximum and minimum of `v` over each orbit: green is a stable orbit, blue
dashed an unstable one. Read it from the left:

- The cycles that leave the first Hopf point are **unstable** and run
  towards **smaller** `iapp`. So the Hopf bifurcation is **subcritical**:
  as `iapp` rises, an unstable cycle shrinks onto the stable rest state
  and leaves it unstable [DSN §6.1.3, p. 169; §6.1.4, p. 174].
- At `iapp` = 0.2484 the branch turns back at a fold (LP). There the
  unstable cycle meets the stable cycle of large spikes, and below this
  value both are gone: a **fold limit cycle** bifurcation
  [DSN §6.2.3, pp. 181-182].
- The stable cycle, the spike train, runs to the right, past the second
  Hopf point, up to a second fold at `iapp` = 0.4657. There it meets the
  unstable cycles that grow out of the second Hopf point, which is
  subcritical too.

So there are five ranges of current:

| `iapp` | what the neuron can do |
|---|---|
| below 0.2484 | rest only |
| 0.2484 to 0.2625 | rest or fire (bistable) |
| 0.2625 to 0.4569 | fire only |
| 0.4569 to 0.4657 | fire, or sit at a high steady voltage (bistable) |
| above 0.4657 | the high steady voltage only |

Izhikevich notes that in two-dimensional conductance-based models a
subcritical Hopf point always comes with a second attractor coexisting
with the rest state [DSN §6.1.4, pp. 174-175]: here it is the spike train.

```python
orbits = m.auto.continue_from(hb, "periodic", nmax=55)
orbits.special("LP")          # the two folds: iapp = 0.2484 and 0.4657
m.auto.diagram.plot("V")      # the whole diagram, max and min of V
m.auto.diagram.to_pandas().tail()
```

**What kind of firing.** Firing appears at full size, and at a frequency
that is not small: the period of the cycle is about 26 at the first fold
and stays between about 14 and 26 over the whole range of spiking. Hodgkin
sorted neurons by how they start to fire: in **Class 1** the firing rate
can be made as low as you like by lowering the current, in **Class 2** it
starts at a clearly nonzero rate and changes little [DSN §7.1.2, p. 218].
Izhikevich links Class 2 to a rest state that loses stability at a
subcritical or supercritical Andronov-Hopf bifurcation, or disappears at a
saddle-node away from the spike's orbit [DSN §7.1.3, p. 222]. This neuron
is Class 2.

**Where the threshold is.** In the first bistable range the unstable cycle
around the rest state is the threshold. A brief kick that leaves the state
inside it returns to rest; a kick that puts the state outside starts the
spike train. The threshold is a closed curve, not a voltage, so a kick
that lowers the voltage can start firing as well as one that raises it
[DSN §7.2.5, p. 241; §7.1.6, pp. 226-227]. Try it at `iapp` = 0.255, where
the rest state is near (v, w) = (-0.207, 0.201):

```commands
set iapp=0.255 total=300
click keep
click -0.24 0.2
click -0.1 0.2
click -0.35 0.2
```

The first trajectory spirals back to rest, ringing for a few turns (the
eigenvalues there are -0.038 ± 0.44i); zoom in on the rest state, or look
at the Time Series tab, to see it. The second, a little higher, and the
third, lower, both end on the spike loop. Near a supercritical Hopf
point, by contrast, there is no unstable cycle and no sharp threshold
[DSN §7.2.5, p. 241].

```python
m.parameters["iapp"] = 0.255
m.set(total=300)

def fires(v0, w0=0.2):
    s = m.run(v=v0, w=w0)
    return s["v"][len(s.t) // 2:].max() > 0

[fires(v0) for v0 in (-0.24, -0.1, -0.35)]   # [False, True, True]
```

**History matters.** Raise the current slowly from rest and firing starts
only at 0.2625, where the rest state loses stability. In a simulation it
starts even a little later, because the state lingers near the equilibrium
that has just turned unstable, a delayed loss of stability
[DSN §6.1.4, p. 175]. Lower the current slowly from a firing state and the
spikes go on down to 0.2484. Izhikevich classifies this downward
transition separately, as the class of spiking, and notes that it can
differ from the class of excitability when the neuron is bistable
[DSN §7.1.7, p. 228]. Here both are Class 2: the cycle still has a period
of about 26 when it vanishes at the fold. Use **Workspace > Range**
(see [Bifurcation analysis with AUTO](auto.md)) to check the diagram with
simulations: the grey dots of a range run sit on the stable branches.

`m.auto.reset()` (or `auto reset` in the app) clears the diagram.

## 6. What you found

With these parameters the rest state never disappears; it loses stability
at a subcritical Andronov-Hopf bifurcation, `iapp` = 0.2625. Because the
bifurcation is subcritical, the spike train already exists below that
value, from the fold of cycles at 0.2484. In between, the neuron is
bistable and a well-placed kick switches it on. Firing starts large and at
a finite frequency: Class 2. At strong currents the same thing happens in
reverse: the equilibrium becomes stable again at the second Hopf point,
0.4569, but the spikes go on up to the second fold, 0.4657.

## 7. The other way to start firing

Now go back to the file's own parameters, the set `snic`, with its
high-threshold potassium current. Start from the rest state at
`iapp` = 0.05 and follow it with AUTO:

```commands
use snic
set iapp=0.05 v=-0.4 w=0
eq
auto reset
auto set nmax=2000
auto steady iapp
```

The rest state is now a **stable node**, so this neuron is an integrator.
The diagram is S-shaped: between `iapp` = -0.0207 and 0.0833 there are
three equilibria, the rest state, a saddle and a high-voltage state, and
the steady-state I-V curve is no longer monotonic. At `iapp` = 0.0833
(an **LP** point) the rest state merges with the saddle and both vanish: a
saddle-node bifurcation [DSN §6.1.1, p. 162]. Below it, the threshold is
the stable manifold of the saddle, the curve of points that flow into it
[DSN §7.2.5, p. 240].

```python
m2 = xs.load("lecar.ode")     # the file's own values: the set snic
m2.parameters["iapp"] = 0.05
rest2 = m2.equilibrium({"v": -0.4, "w": 0.0}, set_initial=True)
rest2.kind                    # 'stable node'

d2 = m2.auto.equilibria("iapp", parmax=0.6)
[(p.type, round(p.value, 4)) for p in d2.special()]
# [('LP', 0.0833), ('LP', -0.0207), ('HB', 0.2038)]
(np.diff(steady_current(m2, v)) > 0).all()   # False: the I-V curve has an N shape
```

Here the saddle-node happens on the loop of the spike. Below 0.0833 the
rest state and the saddle sit on a closed curve made of the trajectories
that join them; when they merge, that curve becomes the limit cycle, with
an infinite period at the bifurcation itself. This is a **saddle-node on
invariant circle** (SNIC) [DSN §6.1.2, pp. 164-165]. Just above it, the
state crawls through the spot where the equilibria were, so the period is
long and the firing rate starts from zero, growing like the square root of
the distance to the bifurcation [DSN §6.2.1, p. 180]. That is Class 1
[DSN §7.1.3, p. 222]. Measure it with runs:

```python
def period(sol):
    t, x = sol.t, sol["v"]
    up = t[1:][(x[:-1] < 0) & (x[1:] >= 0)]     # upward crossings of v = 0
    return np.diff(up)[-3:].mean()

sn = d2.first("LP")
m2.set(total=2000)
above = np.array([0.0005, 0.001, 0.002, 0.005, 0.01])
periods = m2.sweep("iapp", sn.value + above, measure=period)
periods                       # about 77, 56, 41, 27, 20
periods * np.sqrt(above)      # about 1.7 to 2.0: period ~ 1/sqrt(distance)
```

In the app, click the HB point at 0.2038 (label 4) and press **Periodic
orbits from this Hopf point**:

```commands
auto from 4 periodic
```

The branch leaves the Hopf point unstable (it is subcritical too), turns
at a fold near 0.2420, and comes back as stable spikes all the way down to
0.0833, with a period that grows into the thousands. AUTO stops there with
the label MX, because the orbit has become a loop through the saddle-node
and cannot be continued.

One more set shows another route to Class 2. [use hom](cmd:use hom)
makes the potassium current faster (`phi` = 1.15 instead of 0.333). That
does not move the equilibria, since `phi` only scales `w'`, so the rest
state vanishes at the same saddle-node, 0.0833. But the spike loop no
longer passes through it, and at `iapp` = 0.084 the neuron fires with a
period of about 4.6, where the file's values give about 64: a
**saddle-node off the invariant circle**. Izhikevich shows the same switch in his model when only
the time constant of the potassium current changes [DSN §6.1.2, p. 165,
Fig. 6.7, p. 166]. With this set the neuron keeps firing only up to about
0.0846; above that it settles at a high voltage.

```python
m2.apply_set("hom")
period(m2.run(iapp=0.084))    # about 4.6: the rate does not start from zero
```

So the answer to the title depends on the neuron. A neuron fires when its
rest state disappears or loses stability, and which of the bifurcations
does it decides how the neuron fires [DSN ch. 7, p. 215].

## Where to go next

- Follow a Hopf point over two parameters: choose a **2nd param.** in the
  Bifurcation window and the two-parameter action for the HB point (Python:
  `m.auto.continue_from(hb, "two-parameter", second="gca")`).
- Save the whole session with **File > Save session as Python script**.
  The script repeats it number for number.
- The [neuroscience page of Learn by example](https://cesarperdomodiaz.github.io/xpplab/learn/neuroscience/)
  works through a passive membrane, the theta neuron and the
  FitzHugh-Nagumo model with the same book.
- [DSN] chapter 7 goes on with the other differences between integrators
  and resonators, and chapter 9 with bursting.

## References

- [DSN] Izhikevich, E. M. (2007). Dynamical Systems in Neuroscience: The
  Geometry of Excitability and Bursting. Cambridge, MA: MIT Press.
  ISBN 978-0-262-09043-8. The author makes the book available as a PDF on
  his website (izhikevich.org).
