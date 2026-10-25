---
name: xpplab
description: Simulate ODE models and compute bifurcation diagrams with XPP Lab, which runs XPPAUT's integrators and its AUTO continuation. Use when the user wants to write or fix an XPPAUT .ode file; simulate a dynamical system (neuron, oscillator, population, chemical or mechanical model); find equilibria and their stability; follow steady states or limit cycles as a parameter changes (Hopf, fold, period doubling); sweep a parameter; or compute phase response curves, spectra or Liapunov exponents.
---

# Dynamical systems with XPP Lab

XPP Lab is XPPAUT 8.0 (Ermentrout's ODE and bifurcation tool, with AUTO)
as a library. Its MCP server, `xpplab`, gives you these tools:

| Step | Tool |
|---|---|
| Write and fix a model | `check_model` (parse errors with line numbers), `describe_model`, `find_examples` |
| Simulate | `simulate`, `plot_simulation`, `plot_phase_plane` |
| Equilibria | `equilibria` (Newton, eigenvalues, stability) |
| Bifurcations | `continuation` (AUTO), `plot_bifurcation` |
| Brute force | `sweep` (XPPAUT's Range: max/min, period, Liapunov per value) |
| Oscillations | `limit_cycle`, `phase_response`, `spectrum`, `lyapunov` |
| Data | `fit` (Levenberg-Marquardt), `bvp` (shooting), `plot_table` |

The .ode format is in the resource `xpplab://docs/ode-format`, the AUTO
guide in `xpplab://docs/auto`. If the tools are missing, see "Without the
MCP server" below.

## How the tools behave

- `model` is a path to an .ode file, the name of an example (`lecar.ode`),
  or the model's text. Prefer a file in the user's project: edit it, then
  call `check_model` on the path.
- `values` sets parameters, initial conditions and options for **one call**:
  `{"I": 0.5, "v": -1, "total": 500, "meth": "stiff"}`. The next call starts
  from the file again. Put values the user settles on into the file.
- AUTO's diagram is the only state kept between calls. `continuation` adds
  branches with numbered labels; `continuation(from_label=N, ...)` continues
  from label N with the values and settings that produced it. Editing the
  file clears the diagram; so does `reset=true`.
- Results are short JSON. Full data is in the CSV file named in `csv`.
  Every result has a `python` entry: show it to the user when they want to
  repeat or extend the analysis.

## Workflow

1. **Write the model.** One equation per line, then parameters, initial
   values and options, and `done`. Describe the model in the first comment
   lines.

   ```
   # FitzHugh-Nagumo neuron: v is the voltage, w the recovery variable
   v' = v - v^3/3 - w + I
   w' = eps*(v + a - b*w)
   par I=0, a=0.7, b=0.8, eps=0.08
   init v=-1.2, w=-0.6
   @ total=300, dt=0.05
   @ parmin=0, parmax=2, ds=0.01, dsmax=0.05, ntst=50, nmax=400
   done
   ```

   Names ignore case (`v` and `V` are the same). Use `x(t+1)=` for maps
   with `meth=discrete`, `aux` for outputs, `wiener` for noise.

2. **Check it.** `check_model` after every edit. An error gives the line,
   the expression with a caret, and a hint such as "did you mean sin?".
   Warnings flag `par a=` without a value and right-hand sides that are not
   finite at the initial values.

3. **Look at it.** `describe_model`, then `simulate`. Read `behaviour`:
   steady state, periodic (with the period), damped oscillation, or not
   settled. "Not settled" means: run longer (`total`), or test for chaos
   with `lyapunov`. `plot_phase_plane` for two-variable models shows the
   nullclines, the flow and the equilibrium; look at the image.

4. **Equilibria.** `equilibria` runs Newton's method from the initial
   values. To find others, pass `guesses`: the final state of runs that
   settle elsewhere, or where the nullclines cross.

5. **Follow the steady states.** `continuation(parameter="I")`. Set
   `parmin`/`parmax` around the range of interest (in `settings` or on an
   `@` line); the parameter's current value must lie inside it. AUTO goes
   in one direction, set by the sign of `ds`: if `next` says the other side
   is missing, run again with a negative `ds`.

6. **From each Hopf point (HB)**: `continuation(from_label=<label>,
   action="periodic")` gives the branch of limit cycles, with their period
   and amplitude (max/min). Use `ntst` 50 to 100 for spiky orbits such as
   action potentials, and a moderate `nmax`.

7. **Other points.** LP (fold): `action="two-parameter",
   second_parameter=...` traces the fold in a parameter plane, likewise for
   HB. BP: `action="switch"`. PD on orbits: `action="doubling"`.

8. **Check against simulation.** `sweep` over the same range with a
   `transient` and `total` long enough for 3 or more cycles after it. Its
   regimes should match AUTO's stable branches; `continue_runs=true`
   follows one attractor where the system is bistable.

9. **Show it.** `plot_bifurcation` (stable solid, unstable dashed; red and
   black steady states, green and blue orbits). Look at the image before
   describing it.

10. **Explain.** Say what happens as the parameter moves, in plain words:
    where the rest state loses stability, how oscillations are born (and
    whether there is bistability), their period and amplitude. Give the
    `python` snippet so the user can repeat it.

## Reading the results

| Label | Meaning | What it says about the system |
|---|---|---|
| HB | Hopf bifurcation | A pair of eigenvalues crosses the imaginary axis; small oscillations are born with the period given (`period_at_onset`). |
| LP on steady states | Fold, saddle-node | Two equilibria meet and vanish; hysteresis between branches. |
| LP on orbits | Fold of limit cycles | Stable and unstable cycles meet; with a subcritical Hopf point, bistability between rest and oscillation. |
| BP | Branch point | Branches cross; often symmetry. |
| PD | Period doubling | A cycle loses stability to one of twice the period; repeated PDs lead to chaos. |
| TR | Torus | Quasi-periodic motion appears. |
| EP, MX | End point, no convergence | Where AUTO stopped; `ends` gives the reason. |

- A Hopf point is **supercritical** when the stable cycles start right at it
  (small amplitude growing from zero), **subcritical** when the cycles near
  it are unstable and turn back at an LP of cycles. Read `stability` of the
  periodic branch near the HB.
- Stability of orbits comes from Floquet multipliers. `stable` on a
  special point refers to the branch there; trust the `stability` segments.
- A period that grows without bound along a branch signals a homoclinic
  orbit or a SNIC (saddle-node on an invariant circle).
- A phase response curve of type I only advances the cycle (typical near a
  SNIC); type II also delays it (typical near a Hopf point).

## Pitfalls

- **AUTO computed no points**: the start was not an equilibrium. Check that
  `equilibria` converges with the same values; use the end of a `simulate`
  run that settles as initial values.
- **The branch stops at once**: the parameter lies outside `parmin..parmax`
  (the tool says so), or `normmax` is too small.
- **Jagged branches or missed points**: lower `dsmax`. **MX** at the end:
  lower `ds` and `dsmax`; for orbits raise `ntst`.
- **The same special points again and again**: AUTO is going round a closed
  loop of orbits until `nmax`. Lower `nmax`; `labels` lists the repeats.
- **Stiff systems** (fast and slow time scales, e.g. Hodgkin-Huxley with
  small capacitance, chemical kinetics): the run blows up or stops early.
  Use `meth: "stiff"` or `"cvode"`, or a smaller `dt`. A run that stops
  early may also have hit `bound` (default 100): raise it if the values are
  real.
- **Transients**: `simulate`, `sweep`, `spectrum` and `lyapunov` see the
  whole run unless you skip the start: `trans` in values, or `transient`
  for `sweep`.
- **Maps** need `meth=discrete`; their equilibria are stable when every
  |eigenvalue| < 1.
- **Liapunov exponents** from one run are estimates: only a clearly positive
  value that survives a longer run suggests chaos.

## Without the MCP server

The tools are a thin layer over the `xpplab` Python package
(`pip install "xpplab[mcp]"` installs both). The same analysis in Python:

```python
import xpplab as xs
m = xs.load("fhn.ode")
sol = m.run(I=0.5)                         # sol.t, sol["V"]
eq = m.equilibrium()                       # eq.values, eq.eigenvalues, eq.kind
d = m.auto.equilibria("I", parmin=0, parmax=2)
d.special()                                # [<HB #2: ... I=0.331>, ...]
orbits = m.auto.continue_from(d.first("HB"), "periodic", ntst=50)
m.auto.diagram.plot("V")
prc = m.phase_response()                   # with I set where it oscillates
```

To start the server yourself: `python -m xpplab.mcp` (stdio).
