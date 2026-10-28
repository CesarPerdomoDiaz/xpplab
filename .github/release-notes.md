**XPP Lab @VERSION@**: XPPAUT 8.0's engine, giving the same numbers as XPPAUT, with a desktop app for Windows, macOS and Linux and a Python package. It needs no X server, compiler or Terminal.

## New in 0.1.3

- **Control design**, in the app (the *Linearise* tool) and in Python, with the same numbers in both:
  - **Pole placement:** drag the closed loop's poles in the complex plane, or type them, and the feedback gain follows. *Simulate the closed loop* writes the model with each input replaced by the feedback law, opens it and runs it, so you see how far from the point the linear design still works (`lin.place_poles`, `lin.closed_loop`).
  - **Optimal control (LQR):** the gain that minimises a weighted cost of the states and the inputs, for continuous models and maps (`lin.lqr`).
  - **Root locus** over a gain or a parameter, with the values where a pole crosses into or out of stability: over a parameter, a Hopf point shows up as a pair crossing the imaginary axis (`lin.root_locus`, `m.root_locus`).
- **Figures for papers:** the *Figure* window puts several plots, insets and labels with mathematical notation on one page, and exports it as PDF, SVG, EPS or PNG at any size. `xpplab.Figure` draws the same figure from Python.
- **Many runs and shooting:** a batch of starting points with the flag that stopped each run, and runs along the eigenvectors of an equilibrium. Auxiliary quantities at a state and along a bifurcation diagram, and Floquet multipliers along a branch of periodic orbits.
- **Sensitivity** to parameters along a trajectory, from the variational equations (`Model.sensitivity`).
- **The Liapunov exponent is fixed.** XPPAUT computes it from the stored run, which keeps single precision, times included, so long runs went wrong: Lorenz to t = 1000 gave 7.6 for an exponent of about 0.9. XPP Lab now computes it during the integration in double precision, in *Statistics*, in *Range* and in `Model.lyapunov()`. XPPAUT's number is still there as `Model.lyapunov(stored=True)`.
- **Learn by example:** a kite on a tether, from its Lagrangian to its bifurcations, and the neuron tutorial with the theory behind it. A *Coming from MATLAB* guide maps MATLAB's ODE and control functions to XPP Lab.

## Download and run

| Your computer | Download | The first time you open it |
|---|---|---|
| **Windows** 10 or 11, 64-bit | [XPPLab-Windows-x64.zip](https://github.com/CesarPerdomoDiaz/xpplab/releases/download/@TAG@/XPPLab-Windows-x64.zip) | Unzip it anywhere and run `xpplab.exe`. If Windows says "Windows protected your PC", click *More info*, then *Run anyway*. |
| **macOS** 11 or later, Apple silicon or Intel | [XPPLab-macOS.dmg](https://github.com/CesarPerdomoDiaz/xpplab/releases/download/@TAG@/XPPLab-macOS.dmg) | Open it and drag the app to *Applications*. On macOS 15 and later, open the app once, then click *Open Anyway* in *System Settings > Privacy & Security*. On macOS 14 and earlier, Control-click the app and choose *Open*. |
| **Linux**, x86-64 (built on Ubuntu 22.04) | [XPPLab-Linux-x86_64.tar.gz](https://github.com/CesarPerdomoDiaz/xpplab/releases/download/@TAG@/XPPLab-Linux-x86_64.tar.gz) | `tar xzf XPPLab-Linux-x86_64.tar.gz`, then run `XPPLab-Linux-x86_64/xpplab`. |

The warnings appear because the apps are not yet signed with a paid certificate. The checksums at the end let you check a download.

**Try it:** the Morris-Lecar model opens. Press **Run**, open the **Phase Plane** tab, tick **Nullclines**, then drag the `iapp` slider. The **File** menu opens the other 150 example models that come with XPPAUT, and **F1** opens the manual and its tutorial.

## Python

The same engine, with AUTO, from Python 3.9 or newer on any of the three systems:

```
pip install xpplab==@VERSION@
```

Then see the [Python guide](https://cesarperdomodiaz.github.io/xpplab/python/quickstart/).

## What's inside

- **XPPAUT 8.0's own engine.** Every example model gives the same output as unmodified XPPAUT, digit for digit. The only changes fix genuine bugs, listed in [upstream-issues](https://github.com/CesarPerdomoDiaz/xpplab/blob/main/docs/upstream-issues.md).
- **Phase plane:**
  - nullclines and the direction field;
  - equilibria by stability, and the manifolds of saddles;
  - click-to-start trajectories.
- **Bifurcations:** AUTO without the dialogs. A table of special points, and for each one the continuations that make sense.
- **Workspace:** a slider for every parameter, with live re-simulation, plus parameter sweeps, Range, and boundary-value problems.
- **3D view and the Animator:** XPPAUT's `.ani` files, played over the run.
- **Analysis:**
  - phase response curves and the H function;
  - spectra, histograms, correlations and the Liapunov exponent;
  - fitting to data.
- **Control:** linearisation with a controllability check, pole placement, LQR and root loci, and the closed loop run as a model.
- **Editor, command window and figure export.** The editor's errors point at the line, and the command window takes XPPAUT's two-key commands, and Python. Figures export as PDF, SVG, EPS and PNG, data as CSV.
- **Everything as Python.** Each action in the app is also a line of Python, and a session saves as a script that repeats it.

Found a problem, or a model that works in XPPAUT but not here? Please [open an issue](https://github.com/CesarPerdomoDiaz/xpplab/issues/new/choose).

XPP Lab is free software under the GPL. It is an independent rebuild of G. Bard Ermentrout's XPPAUT, and is not endorsed by him; see [THIRD-PARTY-NOTICES.md](https://github.com/CesarPerdomoDiaz/xpplab/blob/main/THIRD-PARTY-NOTICES.md) for the work of others that it contains.
