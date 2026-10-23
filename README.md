# XPP Studio

**XPPAUT's trusted engine, with a modern interface and a Python package.**
Simulate ODEs, maps, delay and stochastic equations, explore phase planes,
and compute bifurcation diagrams with AUTO. It runs natively on Windows,
macOS and Linux, with no X11 server and no Fortran compiler.

![The phase plane of the Morris-Lecar model with nullclines](docs/images/phase-plane.png)

> Status: early development (alpha). The engine, the Python package and the
> desktop app work; see [ROADMAP.md](ROADMAP.md) for what is next.

## Why

[XPPAUT](https://sites.pitt.edu/~phase/bard/bardware/xpp/xpp.html), by
Bard Ermentrout, is one of the most used tools for dynamical systems in
neuroscience and mathematical biology. Its numerics, and the
[AUTO](https://github.com/auto-07p/auto-07p) continuation code inside it,
have been trusted for decades. Using it today, though, means installing an
X11 server (Xming, XQuartz), getting past macOS Gatekeeper, and learning an
interface from the 1990s. AUTO itself is hard to install, and no Python
package offers AUTO's bifurcation analysis with a plain `pip install`.

XPP Studio keeps XPPAUT 8.0's C code as the engine, numerics unchanged,
and builds around it:

- a **desktop app** in the spirit of MATLAB: dockable windows, a model
  editor that points at errors, parameter sliders with live re-simulation,
  interactive plots, a command window and a built-in manual;
- a **Python package** that runs the same engine, AUTO included, from scripts
  and notebooks, with NumPy and pandas output and matplotlib plots;
- a **C library** with a stable API, for anyone who wants to build on it.

Every `.ode` file written for XPPAUT works unchanged.

## Same numbers as XPPAUT

The test suite integrates every example model that ships with XPPAUT
(197 files) and checks the output **bit for bit** against unmodified
XPPAUT 8.0 run in batch mode. On Linux and macOS, CI builds the original
XPPAUT with real X11 and compares the two side by side. Fixes to genuine
upstream bugs are kept minimal, commented in the code and listed in
[docs/upstream-issues.md](docs/upstream-issues.md). One of them is why
continuing AUTO from a labelled point failed on Windows.

## The desktop app

![The bifurcation window computing the Morris-Lecar diagram with AUTO](docs/images/bifurcation.png)

- **Models**: more than 100 example models, each with its description.
- **Workspace**: every parameter with a slider; initial values; numerics;
  named parameter sets.
- **Editor**: line numbers, a snippet menu for every kind of statement, and
  error messages that point at the line.
- **Phase plane**: XPPAUT's nullclines (recomputed as you zoom), the direction
  field, equilibria marked by stability, and a click to start a trajectory.
- **Bifurcation**: AUTO without dialogs. Special points (HB, LP, BP, PD, TR)
  appear in a table, and each point offers only the continuations that make
  sense from it, in plain words. Diagrams use XPPAUT's colours, with unstable
  branches dashed.
- **Command window**: `set I=0.5`, `run`, `eq`, `auto steady I`, ... Every
  button has a command equivalent.
- **Help**: a manual with live links that open examples and run commands.
- XPPAUT's two-key commands (`I G`, `N N`, `S G`, ...) still work.

## The Python package

```python
import xppstudio as xs

m = xs.load("lecar.ode")              # any XPPAUT model file
m.parameters["iapp"] = 0.1           # parameters are a live dictionary
sol = m.run(total=200)               # NumPy arrays: sol.t, sol["V"]
eq = m.equilibrium()                 # with eigenvalues and stability

m.apply_set("hopf")
d = m.auto.equilibria("iapp")        # AUTO: steady states vs. iapp
print(d.special())                   # [<HB #2 ...>, <HB #3 ...>]
orbits = m.auto.continue_from(d.first("HB"), "periodic")
m.auto.diagram.plot("V")             # matplotlib, XPPAUT colours
df = m.auto.diagram.to_pandas()
```

![A Morris-Lecar bifurcation diagram drawn by the Python package](docs/images/python-diagram.png)

The wheel contains the compiled engine and calls it through `ctypes`. One
wheel per platform works with every Python 3 version, and nothing needs to
be compiled on your machine. Each `Model` has its own copy of the engine,
so several models can be open at once.

## Building from source

The engine and the Python package need a C compiler and CMake 3.20 or
newer. The desktop app also needs SDL3.

**Windows** (MSYS2 UCRT64):

```bash
pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,sdl3,python-numpy}
cmake -S . -B build -G Ninja && cmake --build build
ctest --test-dir build
build/app/xppstudio.exe
```

**macOS**: `brew install cmake ninja sdl3`, then the same CMake commands.

**Linux**: install CMake and a C compiler, plus SDL3 for the app, then the
same CMake commands. The engine needs no X11.

**Python package**: `pip install .` from the repository root builds the
engine and installs `xppstudio`. On Windows, XPPAUT's C code needs GCC or
Clang rather than MSVC: put MinGW-w64 on `PATH` and set `CC=gcc` and
`CMAKE_GENERATOR=Ninja` first. The wheel that comes out works with the
standard python.org Python.

## Repository layout

| Path | Contents |
|---|---|
| `engine/xppaut/` | XPPAUT 8.0 sources (imported unmodified; edits are marked `XPP Studio`) |
| `engine/platform/` | null X11, compatibility shims, the embedding layer |
| `engine/api/` | the engine API (`xppe.h`) and multi-instance loader |
| `cli/` | `xppaut-headless`, the original command line without X11 |
| `python/` | the `xppstudio` Python package and its tests |
| `app/` | the desktop application |
| `docs/` | the manual and project notes |
| `tests/` | golden-output, API and AUTO regression tests |

## Credits and licence

XPP Studio is built on **XPPAUT** by G. Bard Ermentrout and contributors
(GPL-2.0-or-later) and **AUTO** by E. J. Doedel and collaborators (BSD), as
translated to C inside XPPAUT. The interface uses Dear ImGui, ImPlot,
cimgui/cimplot (MIT) and SDL (zlib).

If XPPAUT's methods help your research, please cite G. B. Ermentrout,
*Simulating, Analyzing, and Animating Dynamical Systems: A Guide to XPPAUT
for Researchers and Students*, SIAM, 2002.

XPP Studio is free software under the GNU General Public License, version 2
or later. See [LICENSE](LICENSE).
