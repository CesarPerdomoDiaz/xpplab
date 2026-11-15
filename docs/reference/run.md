# run

Integrate the model from its initial values and show the result.

## Syntax

```
run
```

- **In the app:** the **Run** button, **F5**, or XPPAUT's keys `I G`.
- **In Python:** `sol = m.run()`. Values can be given for the run:
  `m.run(total=200, iapp=0.1)`.

## Description

`run` solves the model's equations from time `T0`, starting at the initial
values, for a time `TOTAL`, with the method and the step that are set. The
result replaces the previous run. The Time Series and Phase Plane windows
draw it, the Data window lists it, and every tool that works on "the run"
uses it: the equilibrium found from its end, the spectrum, the export.

Nothing about a run is set in the command itself. The initial values, the
parameters and the numerical options are those shown in the Workspace
window, and `set` changes them: `set total=200`, `set dt=0.01`,
`set meth=stiff`.

With **Live** ticked in the toolbar the app runs again each time a
parameter or an initial value changes, so dragging a slider redraws the
solution.

## Examples

### One spike, then rest

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Run it](cmd:run).

```
run
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
sol = m.run()
```

The command window answers "Integrated lecar.ode: 601 points to t = 30".

How to read it. The model's file asks for a time of 30 in steps of 0.05,
which is 600 steps and 601 stored points. From its start at `V` = −0.144
the membrane potential rises to 0.2283 at `t` = 4.7, one spike, and then
falls to rest: `V` ends at −0.3825. With this little current (`iapp` =
0.05) the neuron fires once and stays quiet.

### More current, a longer time: repetitive firing

```
set iapp=0.1 total=200
run
```

```python
sol = m.run(iapp=0.1, total=200)
```

Now 4001 points to `t` = 200, and 13 spikes that repeat every 16.47 time
units.

How to read it. A run shows what the model does for one set of values. To
learn *where* between 0.05 and 0.1 the firing starts, and how the period
depends on the current, use a range of runs (`range`) or follow the steady
state with [auto steady](auto-steady.md).

### A step that is too large

The same run with Euler's method, first with a moderate step, then with an
absurd one:

```
set meth=euler dt=0.5
run
set dt=5
run
```

```python
rough = m.run(meth="euler", dt=0.5)
broken = m.run(dt=5)
```

With `dt` = 0.5 the run looks reasonable, but the spikes come every 15.83
time units, 4 % too fast. With `dt` = 5 it stops at once: "the integration
stopped early: W out of bounds at t = 15".

How to read it. A numerical solution is an approximation, and nothing on
the plot says how good it is. The test is always the same: halve the step
and run again. If the answer changes, the first one was wrong. With Euler
and `dt` = 0.05 the period is 16.43, and with the model's own fourth-order
Runge-Kutta method at that step it is 16.47.

## Settings

Set them in the Workspace window (**Numerics**), with `set NAME=VALUE`, in
the model file with `@` lines, or as keywords of `m.run()`.

| Setting | Name | Meaning | Advice |
|---|---|---|---|
| **Total time** | `total` | How long to integrate. | Long enough to pass the transient and show several periods. |
| **Step** | `dt` | The time step. For the adaptive methods, the interval between outputs. | Small enough that halving it does not change the result. |
| **Method** | `meth` | The integrator: `rungekutta` (the usual choice), `stiff`, `cvode`, `adams`, `euler`, `discrete` for maps, and others. | `stiff` or `cvode` for stiff models, where a fixed step would have to be tiny. See [Numerical methods](../manual/numerics.md). |
| **Start time** | `t0` | The time at which the run starts. | Matters only if the equations depend on `t`. |
| **Transient** | `trans` | Time to integrate before points are stored. | Drops the approach to an attractor from the plot and the data. |
| **Store every** | `nout` | Store one point in every `nout` steps. | Raise it for long runs with a small step. |
| **Bound** | `bound` | The run stops if a variable or an auxiliary quantity grows larger than this. | The default of 100 stops a diverging run early. Raise it for models with large values. |
| **Tolerances** | `tol`, `atol` | The relative and absolute error per step of the adaptive methods. | Tighten them (1e-8) when the result matters; they do nothing for fixed-step methods. |
| **Storage** | `maxstor` | The most points a run may keep. | Raise it when a long run stops storing. |

## Results

- **The run**: time and one column for each variable and each auxiliary
  quantity (`aux` lines of the model), at the stored times.
- **Time Series** plots the chosen columns against time; **Phase Plane**
  plots one variable against another; **Data** shows the numbers.
- **The command window** prints the number of points and the final time,
  and a warning if the run stopped early.
- **In Python** `m.run()` returns a `Trajectory`: `sol.t` is the time,
  `sol["V"]` a column, `sol.columns` their names, `sol.plot()` draws it,
  and `sol.to_csv("run.csv")` or `sol.to_mat("run.mat")` saves it. Values
  given to `m.run()` stay set afterwards.
- **Export:** `export FILE.csv` or `export FILE.mat` writes the run.

## More about

**The initial value problem.** The model is a system `x' = f(x, t, p)`. A
run finds the one solution that starts at the given initial values. Change
them and the solution is another one, which may end somewhere else.

**Fixed-step methods** such as Euler and Runge-Kutta advance by `dt` at
each step. Their error falls as a power of `dt`: the first power for
Euler, the fourth for Runge-Kutta. So halving the step halves Euler's
error and divides Runge-Kutta's by sixteen.

**Adaptive methods** such as `cvode` and `stiff` choose their own steps to
keep the error of each step under the tolerances, and report the solution
every `dt`.

**Stiff models** have processes on very different time scales. An explicit
fixed-step method must follow the fastest one even after it has died away,
or it becomes unstable, as Euler did above. Implicit methods (`stiff`,
`cvode`) do not have that limit.

## Tips

- After changing a parameter by hand, look at the whole run before
  trusting its end: a transient can be long.
- To start the next run where this one ended, use `ic last`. To carry on
  and append, use `continue`.
- If the curve has corners at its sharp turns, the step is fine for the
  numbers but coarse for the picture: lower `dt`.
- Turn **Live** off for a model that takes more than a moment to run, or
  each change of a slider waits for a run.

## Algorithms

`run` calls XPPAUT's own integration routine with the model's current
method: its Euler, modified Euler, fourth-order Runge-Kutta, Adams, Gear,
backward Euler, adaptive Runge-Kutta, stiff (Rosenbrock), CVODE,
Dormand-Prince and symplectic integrators, unchanged. Points are stored in
single precision, as in XPPAUT. The results are the same numbers XPPAUT
gives, bit for bit, for every example model that came with it.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `warning: the integration stopped early: X out of bounds at t = ...` | A variable or auxiliary quantity passed `bound`. Either the solution really diverges, or the step is too large for the method. | Halve `dt`, or choose `stiff` or `cvode`. If large values are expected, raise the bound: `set bound=1e6`. |
| `No model loaded.` | There is no model. | Open one first. |
| The run has one point | `total` is zero or negative. | Set a positive `total`. |
| The curve stops before the end | The run reached `maxstor` stored points. | Raise `maxstor`, or `nout`. |

## References

- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Section 2.8 (solving equations on the computer).
- Hairer, E., Nørsett, S. P. and Wanner, G. (1993). *Solving Ordinary
  Differential Equations I: Nonstiff Problems*, 2nd edition. Springer.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [Numerical methods](../manual/numerics.md): each integrator and when to
  use it.
- [Tutorial: your first model](../manual/tutorial.md).
- [Commands and shortcuts](../manual/commands.md): `set`, `continue`,
  `ic`, `range`, `export`.
- [auto steady](auto-steady.md): following a steady state instead of
  running to it.

## Version history

Introduced in 0.1.0.
