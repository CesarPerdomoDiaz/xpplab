# AGENTS.md

Notes for coding agents (Codex and others) working in this repository,
either to use XPP Studio for a dynamical-systems question or to change it.
Claude Code reads the same rules from `CLAUDE.md`.

XPP Studio is XPPAUT 8.0, Bard Ermentrout's ODE and AUTO bifurcation tool,
rebuilt around its unchanged numerical code: a C engine with an API
(`engine/api/xppe.h`), the Python package `xppstudio` (`python/`), and a
desktop app (`app/`).

## Using XPP Studio for an analysis

The quickest route is the MCP server, which wraps the Python package:

```
pip install ".[mcp]"          # from this clone
python -m xppstudio.mcp       # stdio; Codex starts it for you
```

For Codex, in `~/.codex/config.toml`:

```toml
[mcp_servers.xppstudio]
command = "python"
args = ["-m", "xppstudio.mcp"]
```

Its tools: `check_model`, `describe_model`, `find_examples`, `simulate`,
`equilibria`, `continuation` (AUTO), `sweep`, `limit_cycle`,
`phase_response`, `spectrum`, `lyapunov`, `fit`, `bvp`, and the plots
`plot_simulation`, `plot_phase_plane`, `plot_bifurcation`, `plot_table`.
`docs/agents.md` describes them; `skills/xppstudio/SKILL.md` is the full
guide to the workflow and to AUTO's pitfalls. Read it before a bifurcation
analysis.

In short:

1. Write the model as an `.ode` file (`docs/manual/odefile.md`) and call
   `check_model` after every edit; errors come with line numbers.
2. `simulate`, then `equilibria`.
3. `continuation(parameter=...)` for the steady states, with `parmin` and
   `parmax` around the range of interest; then
   `continuation(from_label=<HB label>, action="periodic")` for the limit
   cycles. A negative `ds` continues the other way.
4. Check the stable branches with `sweep`; draw with `plot_bifurcation`.
5. Report in words, with the `python` snippet each result carries.

`values` in a call applies to that call only. AUTO's diagram is kept per
model until its file changes or `reset=true`.

Without MCP, use the package directly (`docs/python/`):

```python
import xppstudio as xs
m = xs.load("engine/xppaut/ode/lecar.ode")
m.apply_set("hopf")
d = m.auto.equilibria("iapp")
orbits = m.auto.continue_from(d.first("HB"), "periodic")
```

The example models are in `engine/xppaut/ode/`.

## Working on the code

### Layout

- `engine/xppaut/` XPPAUT 8.0's C sources; the first commit is pristine.
- `engine/platform/` null Xlib, Windows compatibility, the embedding layer.
- `engine/api/` the C API, one file per area.
- `python/src/xppstudio/` the package (ctypes); `mcp/` is the MCP server.
- `app/` the desktop app (SDL3 + Dear ImGui through cimgui, in C).
- `docs/` the MkDocs site; `docs/manual/*.md` is also the app's help.
- `tests/` golden tests and API checks; `python/tests/` the package's.
- `skills/`, `.claude-plugin/`, `.mcp.json` the Claude Code plugin.
- `agent/` the maintainer agent that works on issues labelled `agent`.

### Build and test

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
PYTHONPATH=python/src python -m pytest -q python/tests
python tests/golden.py check --exe build/cli/xppaut-headless
```

On Windows use MSYS2 UCRT64 (`/c/msys64/ucrt64/bin` first on `PATH`) and
add `.exe`. On Linux and macOS the golden check compares with unmodified
XPPAUT built locally (`tests/build_reference.sh`, `golden.py compare`).
The MCP tests need `pip install mcp`; without it they are skipped.

### Rules

- **The numbers do not change.** `golden.py check` must report
  `{"nooutput": 10, "ok": 187}`: every example model bit-identical to
  unmodified XPPAUT. A change that alters a golden is wrong unless it fixes
  a genuine XPPAUT bug, argued in the commit.
- **Never reimplement XPPAUT's numerics.** New features call XPPAUT's own
  routines and are tested against XPPAUT or known answers.
- **Upstream edits are minimal and marked** with an `XPP Studio fix:`
  comment, and each fix gets a row in `docs/upstream-issues.md`.
- **Keep the C API backwards compatible.** When adding functions, bump
  `XPPE_API_VERSION` and `API_VERSION` in `python/src/xppstudio/_engine.py`,
  and add them to `engine/api/xpp_instance.h` and the Python signatures.
- Never commit build directories, test output (`xppstudio-out/`) or secrets.
- Never start the desktop app from a script without `--screenshot` or
  `--record`: it opens a window and waits.
- Docs: plain, direct sentences; no marketing language.
