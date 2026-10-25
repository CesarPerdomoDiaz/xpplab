**XPP Studio @VERSION@**: XPPAUT 8.0's engine, giving the same numbers as XPPAUT, with a desktop app for Windows, macOS and Linux and a Python package. It needs no X server, compiler or Terminal.

## Download and run

| Your computer | Download | The first time you open it |
|---|---|---|
| **Windows** 10 or 11, 64-bit | [XPPStudio-Windows-x64.zip](https://github.com/CesarPerdomoDiaz/xppstudio/releases/download/@TAG@/XPPStudio-Windows-x64.zip) | Unzip it anywhere and run `xppstudio.exe`. If Windows says "Windows protected your PC", click *More info*, then *Run anyway*. |
| **macOS** 11 or later, Apple silicon or Intel | [XPPStudio-macOS.dmg](https://github.com/CesarPerdomoDiaz/xppstudio/releases/download/@TAG@/XPPStudio-macOS.dmg) | Open it and drag the app to *Applications*. On macOS 15 and later, open the app once, then click *Open Anyway* in *System Settings > Privacy & Security*. On macOS 14 and earlier, Control-click the app and choose *Open*. |
| **Linux**, x86-64 (built on Ubuntu 22.04) | [XPPStudio-Linux-x86_64.tar.gz](https://github.com/CesarPerdomoDiaz/xppstudio/releases/download/@TAG@/XPPStudio-Linux-x86_64.tar.gz) | `tar xzf XPPStudio-Linux-x86_64.tar.gz`, then run `XPPStudio-Linux-x86_64/xppstudio`. |

The warnings appear because the apps are not yet signed with a paid certificate. The checksums at the end let you check a download.

**Try it:** the Morris-Lecar model opens. Press **Run**, open the **Phase Plane** tab, tick **Nullclines**, then drag the `iapp` slider. The **File** menu opens the other 150 example models that come with XPPAUT, and **F1** opens the manual and its tutorial.

## Python

The same engine, with AUTO, from Python 3.9 or newer on any of the three systems:

```
pip install xppstudio==@VERSION@
```

Then see the [Python guide](https://cesarperdomodiaz.github.io/xppstudio/python/quickstart/).

## What's inside

- **XPPAUT 8.0's own engine.** Every example model gives the same output as unmodified XPPAUT, digit for digit. The only changes fix genuine bugs, listed in [upstream-issues](https://github.com/CesarPerdomoDiaz/xppstudio/blob/main/docs/upstream-issues.md).
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
- **Editor, command window and figure export.** The editor's errors point at the line, and the command window takes XPPAUT's two-key commands. Figures export as PNG and SVG, data as CSV.

Found a problem, or a model that works in XPPAUT but not here? Please [open an issue](https://github.com/CesarPerdomoDiaz/xppstudio/issues/new/choose).

XPP Studio is free software under the GPL. It is an independent rebuild of G. Bard Ermentrout's XPPAUT, and is not endorsed by him; see [THIRD-PARTY-NOTICES.md](https://github.com/CesarPerdomoDiaz/xppstudio/blob/main/THIRD-PARTY-NOTICES.md) for the work of others that it contains.
