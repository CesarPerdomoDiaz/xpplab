# Coming from XPPAUT

XPP Lab is XPPAUT with a new interface. The parts that compute (the
`.ode` parser, the fifteen integrators, the equilibrium solver, nullclines,
AUTO) are XPPAUT 8.0's own C code. The test suite checks that every
example model shipped with XPPAUT gives bit-for-bit the same output as
unmodified XPPAUT in batch mode. The only changes fix genuine bugs, such as
memory errors; each is listed in the
[upstream issues](https://github.com/CesarPerdomoDiaz/xpplab/blob/main/docs/upstream-issues.md)
page.

## What stays the same

- Your `.ode` files, including `@` options, `set`s, tables, arrays,
  `#include`, Markov chains, delays and noise.
- The numbers: same methods, same defaults, same results.
- Your hands: the two-key commands (I G, N N, S G, ...) still work.
- AUTO's colours and conventions: red and black for steady states, green
  and blue for periodic orbits; LP, HB, BP, PD, TR labels; Grab and Run.

## What is new

- No X11 server, no XQuartz, no Xming: a native program for Windows, macOS
  and Linux.
- Dockable windows, zoomable plots, sliders for every parameter, and live
  re-integration while you drag.
- An editor with error messages that point at the offending line.
- AUTO without dialogs: the special points are listed in a table, and only
  the continuations that make sense from a point are offered, in plain words.
- A command window to script what you do, and a Python package (`xpplab`)
  that runs the same engine, including AUTO, from notebooks.

## Where XPPAUT's menus went

| XPPAUT | XPP Lab |
|---|---|
| Initial conds > Go | Run button, F5, `run` |
| Initial conds > Mouse | Phase Plane > Click to start, `click x y` |
| Initial conds > mIce | Phase Plane > Click to start > Keep each |
| Initial conds > Last, Shift | Workspace > Initial values > From end of last run, From end, time goes on; `ic last`, `ic shift` |
| Initial conds > File, formUla | Workspace > Initial values > From file..., From a formula; `ic read`, `ic u[1..20] = ...` |
| Continue | `continue` (C C) |
| Nullcline > New | Phase Plane > Nullclines |
| Nullcline > Freeze (Freeze, Range, Animate, Delete all) | Phase Plane > Freeze..., `nullclines freeze` |
| Nullcline > Save | Phase Plane > Export... > Nullclines as CSV..., `export nc.csv nullclines` |
| Dir.field | Phase Plane > Flow |
| Window/zoom | Mouse: drag, scroll, double-click |
| Sing pts > Go | Equilibrium button |
| Sing pts > Monte Carlo | Phase Plane > All equilibria, `eq all` |
| Sing pts > Range | Workspace > Range > Equilibria, `range ... eq` |
| Initial conds > 2par range | Workspace > Range > Vary a second value too, `range p1 ... p2 ...` |
| Parameters, ICs, Numerics windows | Workspace tabs |
| Erase | `erase` (E) |
| File > Auto | Bifurcation window |
| AUTO > File > Save diagram, Load diagram | Bifurcation > Save..., Open..., `auto save`, `auto load` |
| AUTO > File > Import orbit | Bifurcation > Show this orbit as the run, `auto orbit` |
| Kinescope (Capture, Reset, Playback, Autoplay, Save, Make AniGif) | View > Kinescope; `kinescope capture`, `kinescope play`, `kinescope save film.gif` |
| Initial conds > Range with Movie = Yes | Workspace > Range > Film it in the Kinescope, `range ... film` |
| Data viewer | View > Data |
| File > Write .set / Read .set | the `.ode` file keeps the values; `set` statements give named sets |

Not yet in the new interface: several curves and labels on one figure
(Graphic stuff, Text) and formulas in the data browser. The engine
supports them, so they will come; the roadmap lists them.

## AUTO files

AUTO's working files (`fort.*`, `model.b`, `model.d`, `model.s`) go to a
private temporary folder instead of your home folder, and are deleted when
you close the model.
