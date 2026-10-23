# XPP Studio roadmap

XPP Studio is XPPAUT, the dynamical-systems and bifurcation tool by Bard
Ermentrout, rebuilt for today. The numerical engine is XPPAUT 8.0's own C
code, AUTO included, kept bit-for-bit identical. Around it we build what
XPPAUT never had: a one-command install on Windows, macOS and Linux, a
Python API, and a modern desktop app with real documentation.

"XPP Studio" is a working name until we have talked to Bard Ermentrout.

## Why this, and why now (research, October 2026)

**How people do continuation today**

| Tool | Where it is used | Main problem |
|---|---|---|
| MatCont (MATLAB) | Most cited (~2,000 citing works) and growing; dynamical-systems courses | Needs a MATLAB licence, and new MATLAB releases break it |
| XPPAUT | Computational neuroscience (144 ModelDB models), math-bio courses | X11 required: Xming on Windows, XQuartz and Gatekeeper overrides on macOS; citations falling |
| AUTO-07p | Engineering; driven through its Python CLUI | Fortran build; no PyPI or conda package; Windows builds fail |
| PyDSTool | Python | Broken on current Python and NumPy; unmaintained |
| PyCoBi, auto-AUTO | Python wrappers of AUTO-07p | You must compile AUTO-07p with gfortran yourself |
| BifurcationKit.jl | Julia, rising fast | Different language; no .ode models |

**What users ask for and nobody delivers**

1. `pip install` with AUTO included. auto-07p issue #1 has been open since 2019.
2. XPP decoupled from X11 with Python bindings (xppaut issue #10). Ermentrout
   wrote that he was looking for funding to modernise the interface.
3. Scriptable, reproducible AUTO runs. Even XPPAUT's own AUTO only runs from
   its GUI.
4. Documentation that teaches, plus machine-readable output.
5. Modern, publication-quality figures, with unstable branches drawn dashed.

**What XPPAUT users refuse to lose**

The `.ode` file format, the keyboard workflow (`I G`, `N N`, `F Q`...), its
speed, AUTO integrated with the phase plane (grab a point, continue from it),
nullclines and direction fields, and the breadth of model types: ODEs, maps,
DDEs, SDEs, DAEs, BVPs and Volterra integral equations.

**What this means for us**

Python is the clearest gap. The MATLAB space already has MatCont, so we
offer export and interoperability there rather than a head-on competitor.
XPPAUT's AUTO is already C (translated from Fortran), so we can ship native
wheels that need no Fortran or C compiler at install or run time. No other
AUTO-based Python package can do that.

## Strategy: one engine, three front ends

```
             +--------------------------------------------+
             |  libxpp: XPPAUT 8.0 engine + stable C API   |
             |  parser, integrators, equilibria, AUTO, ... |
             +--------------------------------------------+
               |                    |                    |
       xpp (command line)   xppstudio (Python)    XPP Studio (app)
       drop-in for          pip wheels, NumPy,    MATLAB-style desktop,
       xppaut -silent       Jupyter, plotting     docs, XPP hotkeys
```

The order is chosen so something useful ships early:

1. **Engine library with a C API.** Headless AUTO comes first, because nobody
   has it.
2. **Python package.** Smallest interface surface, largest unmet demand.
3. **Desktop app.** Built on the same API, for teaching and interactive
   exploration.

## Principles

- **The numbers do not change.** Every example model must keep giving
  bit-identical output to unmodified XPPAUT 8.0 (`tests/golden.py`, run by
  CTest). Upstream files are edited only where a build requires it, and each
  edit is commented.
- **Every existing `.ode`, `.set` and `.ani` file works unchanged.**
- **Everything the GUI does can also be done from a script**, and the app
  shows the equivalent command for each action.
- **Classic XPP keyboard shortcuts stay available.**
- **License:** GPL-2.0-or-later, as XPPAUT is. AUTO's parts are BSD-3.
  Upstream authors are credited everywhere.

## Milestones

### M0 Foundation: done
- [x] XPPAUT 8.0 imported unmodified (commit `cc1ba74`)
- [x] No-op Xlib and MinGW compatibility layer; native Windows build
- [x] CMake build: `xppengine` library and `xppaut-headless`
- [x] Golden regression suite: 187 of 197 example models produce output and
      match bit for bit; the other 10 have errors in the files themselves
- [x] CI on GitHub Actions: Windows, macOS, Linux (workflow written; runs once the repository is on GitHub)

### M1 Engine API (`libxpp`)
- [x] Load a model from a file or a string, with the parser's error message
      (the line number when it can be found)
- [x] Query and set variables, parameters, initial conditions, numerical
      options and auxiliaries
- [x] Integrate, returning the trajectory as arrays: all methods, named sets
- [ ] Ranges, parameter sweeps, `.set` files
- [x] Equilibria with eigenvalues and stability, nullclines, vector field
- [ ] Stable/unstable manifolds
- [ ] Poincaré sections, Lyapunov exponents, FFT and spectra, curve fitting,
      averaging and adjoints (PRC)
- [ ] Boundary value problems (shooting)
- [x] **Headless AUTO:** equilibria, periodic orbits, two-parameter curves,
      branch switching. Branches and labelled special points (HB, LP, PD,
      TR, BP, UZ) are returned as data
- [ ] AUTO: BVPs and homoclinics (reachable through the menu keys; untested)
- [x] Shared library build with an exported, versioned C API
- [ ] `xpp` command-line tool built on the API

### M2 Python package `xppstudio`
- [x] `ctypes` bindings to the shared library (no Python C-API dependency)
- [x] `xs.load("lecar.ode")`, `m.run()` returning NumPy arrays or pandas
      DataFrames, `m.equilibrium()`, `m.auto.equilibria()`, `m.auto.continue_from()`
- [x] Bifurcation diagrams in matplotlib: stable branches solid, unstable
      dashed, special points labelled
- [ ] Jupyter: rich repr and interactive parameter sliders
- [x] Wheels: py3-none wheels via scikit-build-core (Windows wheel built and
      tested locally); cibuildwheel for all platforms in CI
- [ ] Publish on PyPI
- [ ] Documentation site with tutorials and a gallery

### M3 Desktop app, shell
- [x] SDL3 + Dear ImGui (docking) + ImPlot, driven from C
- [x] MATLAB-style layout: model browser, workspace (parameters, initial
      conditions, numerics, sets), `.ode` editor with errors, plot tabs,
      command window, help browser
- [ ] Syntax highlighting in the editor
- [x] Open, edit and run a model; time series and phase plane; parameter
      sliders with live re-integration
- [ ] Preview release

### M4 Desktop app, full XPPAUT parity
- [ ] Every XPPAUT menu: ICs, Continue, Nullcline, Dir field, Window/zoom,
      phAsespace, Kinescope, Graphic stuff, nUmerics, File, Parameters,
      Erase, Make window, Text, Sing pts, Viewaxes, Xi vs t, Restore,
      3D params, Bndryval
- [ ] Data browser, animator (`.ani`), array plots, freeze curves, 3D
- [x] AUTO window: interactive diagram, grab and continue, two-parameter
      continuation, point labels explained
- [x] Classic XPP hotkeys
- [ ] Command palette, undo/redo
- [ ] Export: PNG, SVG and PDF figures; CSV and MATLAB `.mat` data

### M5 Documentation and examples
- [ ] Getting started, tutorials for each field, a full `.ode` language
      reference, numerics and AUTO guides
- [ ] Gallery of every example model with thumbnails
- [ ] "Coming from XPPAUT", "Coming from MATLAB/MatCont" and "Coming from
      AUTO-07p" guides

### M6 Release and community
- [ ] Windows installer and portable zip, notarized universal macOS `.dmg`,
      Linux AppImage; `.ode` file association
- [ ] Contact Bard Ermentrout; announce on the xppaut-l list, comp-neuro
      and SIAM DS
- [ ] Journal of Open Source Software paper
