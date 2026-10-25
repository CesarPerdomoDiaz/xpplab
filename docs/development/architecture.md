# Architecture

XPP Lab wraps XPPAUT instead of rewriting it, so the numerics stay
identical. Everything new lives around the engine.

```
 desktop app (app/)        Python package (python/)        your C program
        |                          |                               |
        |   xpp_instance.c: one private copy of the engine library per model
        +--------------------------+-------------------------------+
                                   |
                     engine API (engine/api/xppe.h)
                                   |
        embedding layer (engine/platform/embed): printf capture, exit guard,
        headless dialog answers
                                   |
                 XPPAUT 8.0 (engine/xppaut): parser, integrators,
                 equilibria, nullclines, AUTO, CVODE ...
                                   |
                 null Xlib (engine/platform/nullx): XPPAUT links, nothing draws
```

## Keeping XPPAUT unchanged

- `engine/xppaut/` started as the untouched XPPAUT 8.0 source; the first
  commit holds it exactly. Every later edit carries an `XPP Lab` comment.
  The edits are of three kinds:
  - **Embedding**: two lines in `main.c` let the engine stop after loading
    when embedded.
  - **Headless guards**: XPPAUT's dialog functions return queued answers
    when embedded.
  - **Upstream bug fixes**, listed in [Upstream issues](../upstream-issues.md).
- `engine/platform/nullx` implements the 78 Xlib functions XPPAUT calls as
  no-ops, so it links without X11 and runs in batch mode on any system.
- `engine/platform/embed/xpps_embed.h` is force-included into every engine
  file. It routes `printf` and `exit` to the embedding layer, so the engine
  can report errors to a library caller instead of ending the process.

## The engine API

`engine/api/xppe.h` is a small C API: load a model, read and set values,
integrate, find equilibria, evaluate the vector field, compute nullclines
and run AUTO. It calls the same upstream routines XPPAUT's own batch mode
and menus call, in the same order. For example, `xppe_run()` is XPPAUT's
`batch_integrate_once()` with file output switched off, and
`xppe_auto_run("p")` is the AUTO window's Run button answered with
"Periodic".

XPPAUT keeps its state in global variables, so one loaded copy of the
library holds one model. `engine/api/xpp_instance.c` (and the Python
package's `_engine.py`) copy the library file to a private temporary
location for each model, which gives every model independent globals.

## Tests

- `tests/golden.py`: every example model in batch mode, checked bit for bit
  against output recorded from unmodified XPPAUT (Windows). On Linux and
  macOS it compares side by side against XPPAUT built from the import commit
  with real X11 (`tests/build_reference.sh`).
- `tests/api_check.py`: the API returns exactly what `xppaut -silent` writes.
- `tests/auto_check.py`: a headless AUTO run (Morris-Lecar: steady states,
  then periodic orbits from a Hopf point) against a recorded diagram.
- `python/tests`: the Python package.
- `tests/modeldb_check.py`: the XPPAUT models published on ModelDB, against
  unmodified XPPAUT, on request (see [ModelDB models](modeldb.md)).
- CI also starts the desktop app, loads a model, runs AUTO and takes a
  screenshot.
