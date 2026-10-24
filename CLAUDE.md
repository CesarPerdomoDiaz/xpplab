# XPP Studio: notes for Claude

XPP Studio is XPPAUT 8.0 (Bard Ermentrout's ODE and AUTO bifurcation tool)
with a headless C engine API, a Python package and a MATLAB-style desktop
app. Read README.md for the overview and ROADMAP.md for what is planned.

## Layout

- `engine/xppaut/` XPPAUT 8.0's C sources. The first commit is pristine.
- `engine/platform/` null Xlib, Windows compatibility, the embedding layer.
- `engine/api/` the C API (`xppe.h`); one file per area (`xppe_data.c`
  analysis, `xppe_bvp.c`, `xppe_ani.c`, `xppe_auto.c`, ...).
- `python/` the `xppstudio` package (ctypes) and its tests.
- `app/` the desktop app: SDL3 + Dear ImGui + ImPlot through cimgui, in C.
- `docs/` MkDocs site; `docs/manual/*.md` is also built into the app's help.
- `tests/` golden tests (`golden.py`), API and AUTO checks, sanitizer tools.
- `agent/` the maintainer agent that runs on the lab workstation.

## Build and test

Windows (MSYS2 UCRT64 at `C:\msys64`; put `/c/msys64/ucrt64/bin` first on
PATH):

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
PYTHONPATH=python/src python -m pytest -q python/tests
python tests/golden.py check --exe build/cli/xppaut-headless.exe
```

Linux and macOS: the same, without `.exe`; golden checks there compare with
unmodified XPPAUT (`tests/build_reference.sh`, `golden.py compare`).

## Rules that must not be broken

- **The numbers do not change.** `golden.py check` must report
  `{"nooutput": 10, "ok": 187}`: every example model bit-identical to
  unmodified XPPAUT. If a change alters a golden, it is wrong unless it fixes
  a genuine XPPAUT bug, and then that must be argued in the commit.
- **Upstream edits are minimal and marked.** Every change in
  `engine/xppaut/` carries an `XPP Studio fix:` (or `XPP Studio:` for
  embedding guards) comment, and a fix gets a row in
  `docs/upstream-issues.md`.
- New engine features call XPPAUT's own routines rather than reimplementing
  its numerics, and are tested against XPPAUT (batch mode or golden data) or
  against known answers.
- Keep the API backwards compatible; bump `XPPE_API_VERSION` (and
  `API_VERSION` in `python/src/xppstudio/_engine.py`) when adding functions,
  and add new functions to `engine/api/xpp_instance.h` and the Python
  signatures.
- Never commit build directories, test output or secrets.

## Practical notes

- Write code that contains backslash escapes with the Edit/Write tools, not
  Bash heredocs: `\\` collapses in heredocs on this machine.
- Never start the desktop app without `--screenshot` or `--record` from a
  script: it opens a window and waits. Use `--screenshot file.png` (PNG), e.g.
  `build/app/xppstudio.exe examples/lecar.ode --cmd run --screenshot s.png`.
  `--record DIR --frames N --script FILE` saves N frames while timed
  commands run; `python tools/make_demo.py` re-records the README's GIFs.
- CI runs on every push: Windows, Linux, macOS (billed tenfold on this
  private repository), AddressSanitizer and Valgrind. Pushes that only touch
  `docs/` or `*.md` skip it.
- Prose in docs: plain, direct sentences; no marketing language.
