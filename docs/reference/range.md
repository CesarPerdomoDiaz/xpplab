# range

Run the model once for each value of a parameter or an initial value, and
collect what each run does.

## Syntax

```
range NAME FROM TO STEPS
range NAME FROM TO STEPS VAR
range NAME FROM TO STEPS VAR continue
range NAME FROM TO STEPS lyap
range NAME FROM TO STEPS eq
range NAME FROM TO STEPS NAME2 FROM2 TO2 STEPS2 VAR
```

`sweep` is another name for `range`.

- **In the app:** Workspace > **Range**, or XPPAUT's *Initial conds >
  Range*.
- **In Python:** `m.sweep("iapp", values, measure=...)`, and
  `m.sweep_grid(...)` for two values on a grid.

## Description

`range NAME FROM TO STEPS` runs the model `STEPS` times, with `NAME` going
from `FROM` to `TO` in equal steps (20 if `STEPS` is left out). `NAME` is a
parameter or a variable; for a variable, it is the initial value that
changes. After the sweep the parameter or initial value is put back.

What is kept depends on the last word:

- **nothing**: the runs themselves, drawn together in the Time Series and
  Phase Plane.
- **a variable `VAR`**: the largest and smallest value of `VAR` in each run.
  Plotted against `NAME` this is a *brute-force bifurcation diagram*: a
  single point where the run settles to rest, two where it oscillates. The
  points appear as grey dots in the Bifurcation window when it is set to the
  same parameter and plots the same variable, over AUTO's diagram if there
  is one.
- **`lyap`**: the largest Liapunov exponent of each run: negative for rest,
  zero for an oscillation, positive for chaos.
- **`eq`**: the equilibrium at each value, found by Newton's method from the
  one before, with its stability (XPPAUT's *Sing pts > Range*).

`continue` starts each run where the previous one ended, instead of from the
initial values. The sweep then follows one attractor as the parameter
changes, which is how to see bistability: sweep up and then down, and the
two disagree where two attractors coexist.

With a second name and range the sweep covers two values: on a grid of
`STEPS` × `STEPS2` runs, or, without `STEPS2`, along a line with both
changing together.

## Examples

### A brute-force diagram that agrees with AUTO

The Morris-Lecar model with the set `hopf` rests below `iapp` ≈ 0.25 and
above 0.46, and fires in between ([auto from](auto-from.md) found the
folds of the firing at 0.2484 and 0.4657). Sweep the current across that
range, with a transient of 100 dropped from each run so that only the
settled behaviour counts:

1. [Open the Morris-Lecar model](example:lecar.ode) and
   [apply the set hopf](cmd:use hopf).
2. [Set the run length and the transient](cmd:set total=200 trans=100).
3. [Sweep the current](cmd:range iapp 0.2 0.5 31 V).

```
use hopf
set total=200 trans=100
range iapp 0.2 0.5 31 V
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.apply_set("hopf")
values = np.round(np.linspace(0.2, 0.5, 31), 4)
swing = lambda run: run["V"].max() - run["V"].min()
up = m.sweep("iapp", values, measure=swing, total=200, trans=100)
```

The swing of `V` (its largest minus its smallest value) is 0 up to
`iapp` = 0.24, jumps to 0.638 at 0.25, rises to 0.652 at 0.27, falls slowly
to 0.529 at 0.46, and is 0 again from 0.47.

How to read it. Where the swing is 0 the neuron rests; elsewhere it fires,
with spikes of that size. The edges, between 0.24 and 0.25 and between 0.46
and 0.47, bracket the folds AUTO found. A sweep is cruder than continuation
(it only sees what is stable, at the values tried) but it needs nothing but
runs, and it is the honest check of a diagram.

### Up and down: where two behaviours coexist

Each run above started from the model's initial values. Now follow one
attractor: start at rest at `iapp` = 0.2 and climb with `continue`, then
start firing at 0.5 and come down:

```
range iapp 0.2 0.5 31 V continue
range iapp 0.5 0.2 31 V continue
```

```python
m.set(iapp=0.2)
m.equilibrium(set_initial=True)
climb = m.sweep("iapp", values, measure=swing, total=200, trans=100,
                continue_runs=True)
m.set(iapp=0.5)
fall = m.sweep("iapp", values[::-1], measure=swing, total=200, trans=100,
               continue_runs=True)[::-1]
```

Climbing from rest, the neuron is still at rest at 0.25 and 0.26 and starts
firing at 0.27. Coming down from rest at 0.5 it stays at rest at 0.46, where
the sweep from the initial values fired.

How to read it. At 0.25 and 0.26 the neuron rests if it was resting and
fires if it was firing; at 0.46 likewise. These are the two strips where
[auto from](auto-from.md) found both a stable rest state and a stable
orbit: between the fold at 0.2484 and the Hopf point at 0.2625, and between
the Hopf point at 0.4569 and the fold at 0.4657. Which state the neuron is
in depends on its history: *hysteresis*.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `STEPS` | the command; the length of `values` in Python | The number of runs. | 20 by default. Enough to see the shape; then refine near the changes. |
| `VAR`, `lyap`, `eq` | the command; `measure=` in Python | What to keep from each run. | `VAR` for a diagram, `lyap` to tell rest, oscillation and chaos apart. |
| `continue` | the command; `continue_runs=True` in Python | Start each run where the last ended. | Use it to follow an attractor, and sweep both ways to find hysteresis. |
| **Total time**, **Transient** | `set total=... trans=...` | The length of each run and the part dropped before measuring. | The transient must be long enough for the run to settle, or the measure mixes the approach with the attractor. |

## Results

- **Without `VAR`**: all the runs, in the Time Series and the Phase Plane.
- **With `VAR`**: the maximum and minimum against `NAME`, as grey dots in
  the Bifurcation window when it is set to that parameter and variable.
  `figure range` puts them in a figure, and `export FILE.csv range` writes a
  table with one row per value: `NAME`, `VAR_max`, `VAR_min`.
- **With `lyap`**: the exponent at each value, in the table that
  `export FILE.csv range` writes.
- **In Python** `m.sweep()` returns a list of runs, or with `measure=` an
  array of the measure at each value. The parameter and the initial values
  are put back afterwards.

## More about

**Brute-force diagrams and continuation.** A sweep shows what the model
*does* at each value: it lands on a stable attractor, and only one, the one
its start leads to. Continuation shows every steady state and orbit,
stable or not, and where they change. The two together are the standard:
continuation for the structure, simulation for the check.

**Hysteresis.** Where two stable states coexist, a system that is changed
slowly stays on the one it was on until that one disappears, then jumps.
Sweeping with `continue` imitates that slow change, so sweeps up and down
jump at different values, and the gap between the jumps is the bistable
range.

**The transient.** A run takes time to settle onto its attractor. Near a
bifurcation it takes longer, because the attractor is only weakly stable,
so a sweep close to a bifurcation needs a longer transient than one far
from it.

## Tips

- Start with few steps over a wide range, then narrow the range around
  where things change.
- Always sweep both ways with `continue` when the diagram has folds.
- If the points near a bifurcation scatter, the runs have not settled:
  raise `total` and `trans`.
- For chaos, the swing does not tell rest from chaos: use `lyap`.

## Algorithms

Each run is an ordinary [run](run.md) with the model's method and step.
For `VAR` the maximum and minimum are taken over the stored points after
the transient. For `lyap` the exponent is computed from the run as XPPAUT's
*Liapunov exponent* does. For `eq` Newton's method starts at each value from
the equilibrium found at the value before, as XPPAUT's *Sing pts > Range*
does.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `Write: range NAME FROM TO [STEPS] ...` | A word is missing, or `NAME` is not a parameter or variable of the model. | Check the name with `whos` and the order of the words. |
| Points scattered near a change | The runs have not settled. | Raise `total` and `trans`. |
| A run stops early | It diverged at that value. | As for [run](run.md): check the step, or raise `bound`. |

## References

- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapters 3 (hysteresis) and 8.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [run](run.md): each run of the sweep.
- [auto steady](auto-steady.md) and [auto from](auto-from.md): the same
  picture by continuation.
- [eq](eq.md): one equilibrium.

## Version history

Introduced in 0.1.0. Sweeps over two values since 0.1.2.
