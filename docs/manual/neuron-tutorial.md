# Tutorial: why does a neuron fire?

A neuron at rest sits at a steady voltage. Inject a current and, above some
value, it fires: the voltage spikes again and again. What happens in the
equations at that value? In this tutorial you answer the question with the
Morris-Lecar model, a two-variable model of a membrane with a calcium
current, a potassium current and a leak. You simulate it, draw its phase
plane and nullclines, find its equilibrium, and end with a bifurcation
diagram over the applied current that shows where firing starts and where it
stops. It takes about twenty minutes.

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

The file ships with a parameter set for the standard textbook regime. This
tutorial uses the set named `hopf`, in which firing starts at a Hopf
bifurcation. Apply it from **Workspace > Sets**, or with the command
[use hopf](cmd:use hopf).

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

So the answer to "does it fire?" depends on `iapp`. To see why, look at the
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
horizontal (only `v` changes). Where the two curves cross, both derivatives vanish: that is an
**equilibrium**, a steady voltage.

With **Click to start** on, click in the plot to start trajectories from
other points, and **Keep curves** to keep them. At `iapp` = 0.3 every
trajectory winds out from the crossing and settles onto a closed loop, the
spike. At `iapp` = 0.1 they all spiral into the crossing.

```python
ax = m.phase_plane("v", "w", xlim=(-0.6, 1.2), ylim=(-0.25, 1.2))

xn, yn = m.nullclines("v", "w", (-0.6, 1.2), (-0.25, 1.2))
xn.shape, yn.shape            # line segments: x1, y1, x2, y2
```

Change `iapp` and the v-nullcline moves while the w-nullcline stays where
it is: the current only enters the voltage equation. The crossing moves
along the w-nullcline, and what matters is what kind of equilibrium it is. To see the nullclines for several values at once, use
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

At `iapp` = 0.1 the eigenvalues are -0.37 ± 0.23i: the real part is
negative, so the equilibrium is a **stable focus**. The neuron is at rest,
and any small disturbance spirals back. At `iapp` = 0.3 they are +0.27 ±
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

Somewhere between 0.1 and 0.3 the real part of the eigenvalues crosses
zero. At that value a pair of eigenvalues sits on the imaginary axis: a
**Hopf bifurcation**. The next step finds it exactly.

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
first the neuron is at rest; between them the equilibrium is unstable and
the neuron fires; above the second the equilibrium is stable again.

```python
m.parameters["iapp"] = 0.1
m.run()
d = m.auto.equilibria("iapp", parmax=0.6)
d.special("HB")               # [<HB #2: ... iapp=0.262468 ...>, <HB #3: ...>]
hb = d.first("HB")
hb.value, hb.stable           # 0.2625, False
hb.eigenvalues                # about +/- 0.435i: on the imaginary axis
```

## 5. The oscillations born at the Hopf point

The Hopf point says that oscillations appear there, not how large they are.
Click the first HB in the table of special points (or in the plot), then
press **Periodic orbits from this Hopf point**. In the command window, with
the label shown in the table:

```commands
auto from 2 periodic
```

AUTO follows the limit cycle as `iapp` changes. The diagram now has the
maximum and minimum of `v` over each orbit: green is a stable orbit, blue
dashed an unstable one. The branch leaves the Hopf point towards **smaller**
`iapp`, then turns back at a fold (LP) near `iapp` = 0.2484 and runs on
as stable large spikes. The Hopf bifurcation is **subcritical**. Two
things follow:

- Between 0.2484 and 0.2625 the neuron is **bistable**: the rest state and a
  large oscillation are both stable, and a big enough kick moves it from one
  to the other.
- Firing starts with large spikes, not a small oscillation.

Use **Workspace > Range** (see
[Bifurcation analysis with AUTO](auto.md)) to check the diagram with
simulations: the grey dots of a range run sit on the stable branches.

```python
orbits = m.auto.continue_from(hb, "periodic", nmax=150)
orbits.special("LP")          # the folds of the cycle branch
m.auto.diagram.plot("V")      # the whole diagram, max and min of V
m.auto.diagram.to_pandas().tail()
```

`m.auto.reset()` (or `auto reset` in the app) clears the diagram.

## 6. What you found

The neuron fires when the applied current is large enough to make the rest
state unstable. In Morris-Lecar with these parameters that happens at a
subcritical Hopf bifurcation, `iapp` = 0.2625, a little above where large
spikes already exist. Firing stops at `iapp` = 0.4569, at the second Hopf
point, where the equilibrium becomes stable again.

Where to go next:

- Use the file's original parameters (leave out `use hopf`) and repeat the
  steps: the equilibrium has no Hopf point, and the diagram has an LP point
  instead, where firing starts through a saddle-node.
- Follow the Hopf point over two parameters: choose a **2nd param.** in the
  Bifurcation window and the two-parameter action for the HB point (Python:
  `m.auto.continue_from(hb, "two-parameter", second="gca")`).
- Save the whole session with **File > Save session as Python script**.
  The script repeats it number for number.
