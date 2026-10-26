# Tutorial: your first model

In this tutorial you will build the FitzHugh-Nagumo model of an excitable
neuron, simulate it, look at its phase plane and find the current at which
it starts to oscillate. It takes about fifteen minutes.

## 1. Write the equations

Choose **File > New model**. The editor opens with this model:

```
v' = v - v^3/3 - w + I
w' = eps*(v + a - b*w)

par I=0.5, a=0.7, b=0.8, eps=0.08
init v=-1, w=1

@ total=200, dt=0.05, xp=v, yp=w
@ xlo=-2.5, xhi=2.5, ylo=-1, yhi=2.5
done
```

Line by line:

- `v' = ...` is the differential equation dv/dt = ... . You can also write
  `dv/dt = ...`.
- `par` declares **parameters**: numbers you will change while exploring.
- `init` gives the **initial values** of the variables.
- Lines starting with `@` set **options**: here the integration time
  (`total`), the time step (`dt`) and which variables the phase plane shows.
- `done` ends the file.

Press **Ctrl+Enter** (or **Apply** in the editor) to load it. If there is a
mistake, a red box above the text says what is wrong and on which line.

## 2. Simulate

The model runs as soon as it loads. The **Time Series** tab shows v and w
over 200 time units. Drag to pan, scroll to zoom, double-click to fit. Once
you zoom, the view stays where you put it while the model runs again;
double-click or press **Fit** to follow the whole solution again.

To run again after changing something, press **Run** in the toolbar, or F5.
With **Live** ticked (the default) a change to a parameter reruns the model
automatically.

## 3. The phase plane

Open the **Phase Plane** tab. The trajectory is drawn in the (v, w) plane.

- Tick **Nullclines**. The red curve is where dv/dt = 0, the green one where
  dw/dt = 0. Where they cross the system has an **equilibrium**.
- Tick **Flow** to see the direction field: which way the state moves from
  each point.
- With **Click to start** on, click anywhere in the plot: a new trajectory
  starts there. Tick **Keep curves** in the toolbar to keep the earlier ones.

## 4. Stability of the equilibrium

Press **Equilibrium** in the toolbar. Newton's method finds the steady
state near the current initial values. The command window prints it with
its eigenvalues, and the phase plane marks it: filled means stable, open
means unstable.

At I = 0.5 the equilibrium is unstable and the neuron fires repeatedly. Set
`I` to 0 in **Workspace > Parameters**: the trajectory now settles onto a
stable equilibrium. The neuron is at rest.

## 5. Where does firing start? A bifurcation diagram

Between I = 0 and I = 0.5 the rest state loses stability. AUTO finds
exactly where:

1. Set `I` = 0 and press **Run**, so the state sits near the rest state.
2. Open the **Bifurcation** window. Choose `I` as the parameter, set
   **Par max** to 2, and press **Follow steady states**.
3. The diagram shows the equilibrium as I varies: red is stable, black
   dashed is unstable. The orange **HB** labels mark **Hopf bifurcations**,
   where oscillations are born.
4. Click the first HB in the table (or in the plot), then **Periodic orbits
   from this Hopf point**. AUTO follows the oscillations. Green is stable,
   blue dashed unstable, and the curves show each orbit's maximum and minimum.

You have now computed the classic picture of a neuron's onset of firing.
The same steps work for any model.

## What next

- [Bifurcation analysis with AUTO](auto.md) explains the diagram and its
  settings in depth.
- The [.ode file reference](odefile.md) shows everything a model file can
  contain: functions, auxiliary outputs, delays, noise, maps and more.
- The **Models** window has more than a hundred example models to explore.
