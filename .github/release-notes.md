**XPP Lab @VERSION@**: XPPAUT 8.0's engine, giving the same numbers as XPPAUT, with a desktop app for Windows, macOS and Linux and a Python package. It needs no X server, compiler or Terminal.

## New in 0.1.2

- **The app and Python together.** Each action in the app now appears in the command window as the line of Python (the `xpplab` package) that does the same, and *File > Save session as Python script* saves them: the script repeats the session exactly. The command window also takes Python: click its `>>` prompt, and `m.run()` runs the open model in the app while its plots follow. On a computer without Python, **Install Python** downloads one for XPP Lab, with NumPy and Matplotlib (about 55 MB, once).
- **Drive the open app from outside.** `xpplab.app.connect()` in a notebook or a script, or a coding agent through the MCP tools `app_status`, `app_command` and `app_run`, acts in the window you have open, and you watch the plots change.
- **The rest of XPPAUT's interface:** equilibria over a parameter and every equilibrium in a box (*Sing pts > Range* and *Monte Carlo*); Range over two parameters; initial values from a file, a formula, *Shift* and several clicks; frozen and saved nullclines; the Kinescope, with animated GIFs and a film of the Range; statistics over many noisy runs (*stocHastic > Compute*). `xpplab-batch` is XPPAUT's `-silent` mode, and also runs the app's commands, AUTO included, without a window.
- **AUTO:** diagrams save and load exactly (XPPAUT keeps 6 digits), and an orbit can be imported. Boundary-value and homoclinic starts are now tested, and a homoclinic start answers XPPAUT's info box. A periodic branch that ends on a homoclinic orbit no longer shows spurious limit and branch points on its tail.
- **Linearise** (*Run > Linearise*): the A and B matrices at an equilibrium for the inputs you choose, the eigenvalues in the complex plane, and whether the inputs can steer the system. The app and `Model.linearize` give the same numbers.
- **Plots keep your zoom** until a double-click or *Fit*.
- **Learn by example** on the [website](https://cesarperdomodiaz.github.io/xpplab/learn/): field tutorials in neuroscience, ecology, chemistry and mechanics. In Jupyter, models, runs, equilibria and AUTO diagrams show as tables.

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
- **Editor, command window and figure export.** The editor's errors point at the line, and the command window takes XPPAUT's two-key commands, and Python. Figures export as PNG and SVG, data as CSV.
- **Everything as Python.** Each action in the app is also a line of Python, and a session saves as a script that repeats it.

Found a problem, or a model that works in XPPAUT but not here? Please [open an issue](https://github.com/CesarPerdomoDiaz/xpplab/issues/new/choose).

XPP Lab is free software under the GPL. It is an independent rebuild of G. Bard Ermentrout's XPPAUT, and is not endorsed by him; see [THIRD-PARTY-NOTICES.md](https://github.com/CesarPerdomoDiaz/xpplab/blob/main/THIRD-PARTY-NOTICES.md) for the work of others that it contains.
