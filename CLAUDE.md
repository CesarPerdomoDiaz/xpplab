# XPP Lab: notes for Claude

XPP Lab is XPPAUT 8.0 (Bard Ermentrout's ODE and AUTO bifurcation tool)
with a headless C engine API, a Python package and a MATLAB-style desktop
app. Read README.md for the overview and ROADMAP.md for what is planned.

## Layout

- `engine/xppaut/` XPPAUT 8.0's C sources. The first commit is pristine.
- `engine/platform/` null Xlib, Windows compatibility, the embedding layer.
- `engine/api/` the C API (`xppe.h`); one file per area (`xppe_data.c`
  analysis, `xppe_bvp.c`, `xppe_ani.c`, `xppe_auto.c`, ...).
- `python/` the `xpplab` package (ctypes) and its tests;
  `python/src/xpplab/mcp/` is the MCP server for coding agents.
- `skills/`, `.claude-plugin/`, `.mcp.json` the Claude Code plugin (skill
  plus MCP server); `AGENTS.md` the same guidance for other agents.
- `app/` the desktop app: SDL3 + Dear ImGui + ImPlot through cimgui, in C.
- `docs/` MkDocs site; `docs/manual/*.md` is also built into the app's help.
- `tests/` golden tests (`golden.py`), API and AUTO checks, sanitizer tools.
- `agent/` the maintainer agent (see `agent/README.md`).

## Build and test

Windows (MSYS2 UCRT64 at `C:\msys64`; put `/c/msys64/ucrt64/bin` first on
PATH):

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
PYTHONPATH=python/src python -m pytest -q python/tests
python tests/golden.py check --exe build/cli/xppaut-headless.exe
python tests/replay.py --app build/app/xpplab.exe
```

`tests/replay.py` runs app sessions, saves each as a Python script (the
app's echo, `app/src/echo.c`) and checks that the script, run with the
package, writes the same CSV files bit for bit. An action added to the app
needs its line in the echo: call `echo_begin()`/`echo_end()` around it and
write the line when it succeeds.

Linux and macOS: the same, without `.exe`; golden checks there compare with
unmodified XPPAUT (`tests/build_reference.sh`, `golden.py compare`).

## Rules that must not be broken

- **The numbers do not change.** `golden.py check` must report
  `{"nooutput": 10, "ok": 187}`: every example model bit-identical to
  unmodified XPPAUT. If a change alters a golden, it is wrong unless it fixes
  a genuine XPPAUT bug, and then that must be argued in the commit.
- **Upstream edits are minimal and marked.** Every change in
  `engine/xppaut/` carries an `XPP Lab fix:` (or `XPP Lab:` for
  embedding guards) comment, and a fix gets a row in
  `docs/upstream-issues.md`.
- New engine features call XPPAUT's own routines rather than reimplementing
  its numerics, and are tested against XPPAUT (batch mode or golden data) or
  against known answers.
- Keep the API backwards compatible; bump `XPPE_API_VERSION` (and
  `API_VERSION` in `python/src/xpplab/_engine.py`) when adding functions,
  and add new functions to `engine/api/xpp_instance.h` and the Python
  signatures.
- Never commit build directories, test output or secrets.

## Practical notes

- Write code that contains backslash escapes with the Edit/Write tools, not
  Bash heredocs: `\\` collapses in heredocs on this machine.
- Never start the desktop app without `--screenshot` or `--record` from a
  script: it opens a window and waits. Use `--screenshot file.png` (PNG), e.g.
  `build/app/xpplab.exe examples/lecar.ode --cmd run --screenshot s.png`.
  `--record DIR --frames N --script FILE` saves N frames while timed
  commands run; `python tools/make_demo.py` re-records the README's GIFs.
- On a Mac over SSH, start the app through LaunchServices so it can open a
  window (someone must be logged in at the Mac):
  `open -W -n --stderr ~/err.txt build/app/xpplab.app --args "$PWD/engine/xppaut/ode/lecar.ode" --screenshot "$HOME/shot.png"`.
  Paths must be absolute; stderr has the line with the window size and
  scales.
- CI runs on every push: Windows, Linux, macOS, AddressSanitizer and
  Valgrind. Pushes that only touch
  `docs/` or `*.md` skip it.
- Prose in docs: plain, direct sentences; no marketing language.
