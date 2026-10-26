# XPP Lab roadmap

XPP Lab is XPPAUT, the dynamical-systems and bifurcation tool by Bard
Ermentrout, rebuilt for today. The numerical engine is XPPAUT 8.0's own C
code, AUTO included, kept bit-for-bit identical. Around it we build what
XPPAUT never had: a one-command install on Windows, macOS and Linux, a
Python API, and a modern desktop app with real documentation.

Where it is going, in a page: [docs/vision.md](docs/vision.md).

## Strategy: one engine, three front ends

```
             +--------------------------------------------+
             |  libxpp: XPPAUT 8.0 engine + stable C API   |
             |  parser, integrators, equilibria, AUTO, ... |
             +--------------------------------------------+
               |                    |                    |
       xpp (command line)   xpplab (Python)    XPP Lab (app)
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

## The main path

The milestones in the order the work follows. The engine and the two front
ends came first; then XPPAUT parity, so that its users lose nothing; then
the app and Python as one environment; then analysis beyond XPPAUT; then
scale. The other milestones (documentation, releases, coding agents) run
alongside. The milestone in brackets is the one in progress.

Path: M0 → M1 → M2 → M3 → M4 → M10 → M9 → [M7] → M11

## Milestones

### M0 Foundation: done
- [x] XPPAUT 8.0 imported unmodified (the first commit)
- [x] No-op Xlib and MinGW compatibility layer; native Windows build
- [x] CMake build: `xppengine` library and `xppaut-headless`
- [x] Golden regression suite: 187 of 197 example models produce output and
      match bit for bit; the other 10 have errors in the files themselves
- [x] CI on GitHub Actions: Windows, macOS and Linux, each compared with
      unmodified XPPAUT; every model also runs under AddressSanitizer and
      the API, AUTO and Python tests under Valgrind

### M1 Engine API (`libxpp`)
- [x] Load a model from a file or a string, with the parser's error message
      (the line number when it can be found)
- [x] Query and set variables, parameters, initial conditions, numerical
      options and auxiliaries
- [x] Integrate, returning the trajectory as arrays: all methods, named sets
- [x] Parameter sweeps and `.set` files; Range (as `Model.sweep` in Python and
      the `range` command in the app, built from repeated runs, there is no
      single `xppe_` call for it)
- [x] Equilibria with eigenvalues and stability, nullclines, vector field
- [x] Stable/unstable manifolds of saddles
- [x] Data analysis: Fourier series, spectral densities, histograms,
      correlations, Liapunov exponents, curve fitting, adjoints, phase
      response curves and H functions (checked against XPPAUT's batch
      post-processing and against known answers)
- [x] Poincaré sections (as numerical options)
- [x] Boundary value problems (shooting): `xppe_bvp`
- [x] **Headless AUTO:** equilibria, periodic orbits, two-parameter curves,
      branch switching. Branches and labelled special points (HB, LP, PD,
      TR, BP, UZ) are returned as data
- [x] AUTO: BVPs and homoclinics (`start("bvp")` and `start("homoclinic")`
      are tested against closed-form and Melnikov answers; heteroclinic
      starts are untested; see `docs/manual/auto.md` for the limits)
- [x] Shared library build with an exported, versioned C API
- [x] A command-line tool built on the API: `xpplab-batch` (XPPAUT's
      `-silent`, and the app's commands with AUTO); `xppaut-headless` stays
      XPPAUT's own `main` for the golden tests

### M2 Python package `xpplab`
- [x] `ctypes` bindings to the shared library (no Python C-API dependency)
- [x] `xs.load("lecar.ode")`, `m.run()` returning NumPy arrays or pandas
      DataFrames, `m.equilibrium()`, `m.auto.equilibria()`, `m.auto.continue_from()`
- [x] Bifurcation diagrams in matplotlib: stable branches solid, unstable
      dashed, special points labelled
- [x] Jupyter: interactive parameter sliders (`Model.interact`)
- [x] Jupyter: rich repr (`_repr_html_`; only plain `__repr__` exists)
- [x] Wheels: py3-none wheels via scikit-build-core (Windows wheel built and
      tested locally); cibuildwheel for all platforms in CI
- [ ] Publish on PyPI
- [x] Documentation site with a tutorial and a gallery (`mkdocs.yml`; built by
      the docs workflow; field tutorials are in M5)

### M3 Desktop app, shell: done
- [x] SDL3 + Dear ImGui (docking) + ImPlot, driven from C
- [x] MATLAB-style layout: model browser, workspace (parameters, initial
      conditions, numerics, sets), `.ode` editor with errors, plot tabs,
      command window, help browser
- [x] Syntax highlighting in the editor (parameters and variables too)
- [x] Open, edit and run a model; time series and phase plane; parameter
      sliders with live re-integration
- [x] Portable Windows zip, tested on a clean PATH
- [ ] First public preview release (needs the GitHub repository)

### M4 Desktop app, full XPPAUT parity
Done:
- [x] Initial conds (Go, Last, Mouse), Continue, Nullclines, Dir field,
      Window/zoom (mouse), Numerics (all methods and options, Poincare
      maps), Parameters, Erase, Sing pts (with stability), invariant
      manifolds of saddles, Xi vs t, phase plane, Data browser, array plots,
      freeze (keep) curves, sets
- [x] AUTO window: interactive diagram, grab and continue, two-parameter
      continuation, point labels explained
- [x] Classic XPP hotkeys
- [x] Export: PNG (as shown) and SVG (vector) figures; CSV data

- [x] Animator (`.ani` files), evaluated by XPPAUT's own animator code;
      comets, text, permanent and transient objects
- [x] 3D view with XPPAUT's theta/phi rotation and the model's 3D options
- [x] Boundary value problems (Bndryval, shooting) and Range (Initial conds
      > Range: parameters or initial values, "use old ICs", max/min and
      Liapunov modes), checked against XPPAUT's batch range
- [x] Curve fitting, spectra and histograms, correlations, Liapunov
      exponents, adjoints, phase response curves, H functions

To do:
- [ ] Nullclines and grab points in animations
- [ ] Text, Make window, Graphic stuff (labels, arrows, several plots)
- [ ] Command palette; MATLAB `.mat` export; PDF figures

### M5 Documentation and examples
- [x] Getting started, tutorial, full `.ode` language reference, numerics
      and AUTO guides, command reference (in the app and on the website)
- [x] Python guide and API reference (mkdocstrings)
- [x] Gallery of the example models with thumbnails (generated at build)
- [x] "Coming from XPPAUT" guide
- [x] Field tutorials, "Learn by example" on the website (neuroscience,
      ecology, chemistry, mechanics); their Python is run by the tests
- [ ] "Coming from MATLAB" and "Coming from AUTO-07p" guides
- [ ] Jupyter notebooks

### M6 Release and community
- [x] Windows portable zip, macOS `.dmg` bundle, Linux tarball (CPack),
      built by the release workflow on a version tag
- [x] Universal macOS app (Apple Silicon and Intel, macOS 11+), checked
      from the mounted `.dmg` in CI; on the preview release
- [ ] macOS notarization and Windows code signing (need developer accounts)
- [ ] Windows installer with `.ode` file association; Linux AppImage (CPack
      makes only the zip, `.dmg` and tarball; no installer or AppImage yet)
- [ ] Publish on GitHub and PyPI
- [ ] Contact Bard Ermentrout; announce on the xppaut-l list, comp-neuro
      and SIAM DS
- [ ] Journal of Open Source Software paper

### M7 Beyond XPPAUT: analysis and control
- [x] Linearise a model at an equilibrium with chosen inputs (the A and B
      matrices), in Python (`Model.linearize`); the app part is still open
- [ ] Eigenvalues in the complex plane, with a controllability check
- [ ] Pole placement: drag an eigenvalue, get the feedback gain, simulate the
      closed loop (generated as an `.ode` model)
- [ ] Root locus over a gain or a parameter
- [ ] Optimal control (LQR)
- [ ] Floquet multipliers shown on the bifurcation diagram
- [ ] Sensitivity of trajectories to parameters
- [ ] Isochrons and phase reduction
- [ ] Codimension-two bifurcations through AUTO, explained in the manual

### M8 Working with coding agents
- [x] An MCP server and a Claude Code skill (`python -m xpplab.mcp`)
- [x] A coding agent drives the running app while the researcher watches:
      the MCP tools `app_status`, `app_command`, `app_run`

### M10 XPPAUT parity sprint: the features the interface still lacked: done
- [x] Equilibria over a parameter (Sing pts > Range) and every equilibrium
      in a box (Sing pts > Monte Carlo), in the C API, Python and the app
- [x] Save and reload AUTO diagrams (exactly, unlike XPPAUT's 6 digits); import an orbit
- [x] Range over two parameters, on a grid (a colour map) or along a line
- [x] Initial values from a file, a formula, Shift, several clicks
- [x] Freeze and save nullclines
- [x] Kinescope: capture a figure, play the frames, an animated GIF; film
      the Range (XPPAUT's Movie)
- [x] A batch command-line tool (XPPAUT's -silent, with AUTO): `xpplab-batch`
- [x] Ensemble statistics over noisy runs (stocHastic > Compute), equal to XPPAUT's batch @ stoch=1 and 2

### M9 The app and Python together: done
- [x] Each action in the app written in the command window as the Python
      that does the same; File > Save session as Python script
- [x] The saved script repeats the session bit for bit (`tests/replay.py`)
- [x] Exact CSV exports: runs, bifurcation diagrams, Ranges, Analysis results
- [x] A local channel through which Python (a notebook, a script, a coding
      agent) drives the running app: `xpplab.app.connect()`
- [x] A Python console inside the app, its objects listed in the Workspace:
      the command window takes Python, run in a process of its own
- [x] A Python runtime fetched the first time the console is used
      (Install Python in the command window, or `python fetch`), and
      `python install PACKAGE` for more

### M11 Scale and reach
- [ ] A compiled engine for large models, checked against XPPAUT's
- [ ] A model builder that writes the `.ode` file
- [ ] SBML and NeuroML import
- [ ] XPP Lab in the browser, with nothing to install
- [ ] An index of toolboxes that install with pip and appear in the app
