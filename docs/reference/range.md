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
range NAME FROM TO STEPS NAME2 FROM2 TO2 VAR
range NAME FROM TO STEPS film
```

`sweep` is another name for `range`.

- **In the app:** Workspace > **Range**, or Run > **Range...** (XPPAUT's
  *Initial conds > Range*; with **Vary a second value too**, its *2par
  range*).
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
  one before (the first time from the initial values), with its stability
  (XPPAUT's *Sing pts > Range*). No run is made: steady states and their
  stability without AUTO.

`continue` starts each run where the previous one ended, instead of from the
initial values. The sweep then follows one attractor as the parameter
changes, which is how to see bistability: sweep up and then down, and the
two disagree where two attractors coexist.

`film` draws each run alone and has the Kinescope capture its figure after
every run: see [kinescope capture](kinescope-capture.md).

With a second name and range the sweep covers two values (XPPAUT's *2par
range*): on a grid of `STEPS` × `STEPS2` runs, every pair once (XPPAUT's
"Array"), or, without `STEPS2`, along a line with both changing together,
`STEPS` runs from (`FROM`, `FROM2`) to (`TO`, `TO2`) (XPPAUT's "Crv"). The
second name may also be a variable: `range iapp 0 0.3 3 V -0.2 0.2 2` runs
over `iapp` and `V`'s initial value, keeping the runs.

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
| `NAME`, `FROM`, `TO` | the command; **Range over**, **From**, **To** in the Range tab | The parameter, or a variable's initial value, and its range. | |
| `STEPS` | the command; **Steps** in the Range tab; the length of `values` in Python | The number of runs. | 20 by default. Enough to see the shape; then refine near the changes. |
| `VAR`, `lyap`, `eq` | the command; in the Range tab **Max / min of a variable**, **Liapunov exponent**, **Equilibria (no runs)**, or **Keep each trajectory** for none; `measure=` in Python | What to keep from each run. | `VAR` for a diagram, `lyap` to tell rest, oscillation and chaos apart. |
| `continue` | the command; **Start each run where the last ended** in the Range tab (XPPAUT's "Use old ICs" = No); `continue_runs=True` in Python | Start each run where the last ended. | Use it to follow an attractor, and sweep both ways to find hysteresis. |
| `NAME2 FROM2 TO2 STEPS2` | the command; **Vary a second value too**, then **On a grid** or **Together, along a line**, in the Range tab; `m.sweep_grid()` in Python | The second value, and how the two move. | A grid maps a plane of two values; a line crosses it once. |
| **Total time**, **Transient** | `set total=... trans=...` | The length of each run and the part dropped before measuring. | The transient must be long enough for the run to settle, or the measure mixes the approach with the attractor. |

## Results

- **Without `VAR`**: all the runs overlaid in the Time Series and the
  Phase Plane, up to 16.
- **With `VAR`**: the maximum and minimum against `NAME`, as grey dots in
  the Bifurcation window when it is set to that parameter and variable.
  `figure range` puts them in a figure, and `export FILE.csv range` writes a
  table with one row per value: `NAME`, `VAR_max`, `VAR_min`.
- **With `lyap`**: the exponent at each value, in the table that
  `export FILE.csv range` writes.
- **With `eq`**: the chosen variable of each equilibrium against `NAME`,
  red where the equilibrium is stable and black where it is not. When the
  parameter matches the Bifurcation window's, the dots appear on AUTO's
  diagram too, and should sit on its steady-state branch.
- **Two values on a grid**, with `VAR` or `lyap`: a colour map over the two
  values, the first across and the second up. **Show** picks what the
  colour means for the maximum and minimum: the swing (max − min, zero where
  the system settles to an equilibrium), the max or the min. For the
  Liapunov exponent the colours run from blue (negative) through white to
  red (positive), so chaotic regions stand out.
- **Two values along a line**: plotted against the first value, as with
  one.
- **Two values**, exported: `export FILE.csv range` writes one row per run
  with both values.
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

**Two values.** On a grid, `continue` goes through it row by row, the first
value changing fastest. Keeping the runs works for up to 16 runs. `eq`
follows the first value only.

## Tips

- Start with few steps over a wide range, then narrow the range around
  where things change.
- Always sweep both ways with `continue` when the diagram has folds.
- If the points near a bifurcation scatter, the runs have not settled:
  raise `total` and `trans`.
- For chaos, the swing does not tell rest from chaos: use `lyap`. Try the
  Lorenz model with `range r 20 30 30 lyap continue`: where the exponent
  rises above zero, the system is chaotic.
- For a map over two parameters, `range iapp 0 0.5 21 gca 0.8 1.4 13 V`
  runs a 21 by 13 grid; `range iapp 0 0.5 30 gca 0.8 1.2 lyap`, with no
  second count, runs along a line.

## Algorithms

Each run is an ordinary [run](run.md) with the model's method and step.
For `VAR` the maximum and minimum are taken over the stored points after
the transient. For `lyap` the exponent is computed from the run as XPPAUT's
*Liapunov exponent* does. For `eq` Newton's method starts at each value from
the equilibrium found at the value before (at the first value, from the
initial values), as XPPAUT's *Sing pts > Range* does.

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
