# Coding agents

Many researchers now run their analyses through a coding agent such as
Claude Code, Codex or Claude Desktop. XPP Lab can serve those agents
through the **Model Context Protocol** (MCP): the agent writes an `.ode`
file, checks it, simulates it, runs AUTO, and looks at the figures, with
XPPAUT's own numerics. Every answer the agent gets also carries the Python
code that reproduces it, so you can check and extend what it did.

The server is part of the Python package. It runs on your machine and
reads and writes only the files you point it at and its work folder.

## Install

From PyPI:

```
pip install "xpplab[mcp]"
```

Or from a clone of the repository, with `pip install ".[mcp]"` (see the
README for Windows notes). The extra adds the official MCP SDK and matplotlib to the
package. The server needs Python 3.10 or later.

Check that it starts:

```
python -m xpplab.mcp --help
```

## Connect an agent

The server speaks MCP over stdio: the agent starts `python -m xpplab.mcp`
(or the `xpplab-mcp` command) and talks to it. Use the Python that has
xpplab installed; on Windows, give its full path if `python` is not on
`PATH`.

### Claude Code

Add the server to a project:

```
claude mcp add xpplab -- python -m xpplab.mcp
```

or put this `.mcp.json` at the root of the project:

```json
{
  "mcpServers": {
    "xpplab": {
      "command": "python",
      "args": ["-m", "xpplab.mcp"]
    }
  }
}
```

XPP Lab is also a Claude Code **plugin**: the MCP server plus a skill
that teaches Claude the workflow and the usual pitfalls of AUTO. Install it
from the repository's marketplace:

```
/plugin marketplace add CesarPerdomoDiaz/xpplab
/plugin install xpplab@xpplab
```

`/mcp` in Claude Code shows whether the server is connected.

### Codex

In `~/.codex/config.toml`:

```toml
[mcp_servers.xpplab]
command = "python"
args = ["-m", "xpplab.mcp"]
```

The repository's `AGENTS.md` describes the workflow for agents that work
inside a clone.

### Claude Desktop

Claude Desktop starts servers from its own folder, so name the work folder
and the full path of Python. In `claude_desktop_config.json` (Settings >
Developer > Edit config):

```json
{
  "mcpServers": {
    "xpplab": {
      "command": "C:\\Users\\you\\AppData\\Local\\Programs\\Python\\Python312\\python.exe",
      "args": ["-m", "xpplab.mcp", "--workdir", "C:\\Users\\you\\Documents\\xpplab"]
    }
  }
}
```

Without a filesystem tool, the agent passes model text instead of paths;
the server saves it under the work folder.

## What the agent can do

| Tool | What it does |
|---|---|
| `check_model` | Loads a model and reports parse errors with the line, the expression with a caret, and a hint ("did you mean sin?"). Warns about empty values and non-finite right-hand sides. |
| `describe_model` | Variables and initial values, parameters and defaults, auxiliary columns, named sets, numerical options, AUTO settings. |
| `find_examples` | Searches the example models that ship with XPPAUT. |
| `simulate` | One integration: min, max, mean and final value of each column, what the run does in the end (steady, periodic with its period, damped, not settled), warnings, and a CSV file. |
| `equilibria` | Newton's method from the initial values and from guesses, with eigenvalues, stability and type. |
| `continuation` | AUTO: steady states in a parameter, or from a labelled point (limit cycles from a Hopf point, two-parameter curves, branch switching). Returns the special points with labels, where the branch is stable, why it ended, and what to try next. |
| `sweep` | XPPAUT's Range: one run per value with max/min, period and Liapunov exponent, summarised as regimes. |
| `limit_cycle`, `phase_response` | One period of a stable cycle; its phase response curves (type I or II). |
| `spectrum`, `lyapunov` | Welch spectra with their peaks; the largest Liapunov exponent. |
| `fit`, `bvp` | Parameter fitting to data; boundary-value problems by shooting. |
| `plot_simulation`, `plot_phase_plane`, `plot_bifurcation`, `plot_table` | PNG or SVG figures in the work folder. The agent also receives the PNG, so it can look at it. |

The server also offers the [.ode format](manual/odefile.md) and the
[AUTO guide](manual/auto.md) as resources (`xpplab://docs/ode-format`,
`xpplab://docs/auto`), the list of examples (`xpplab://examples`) and
each example's text (`xpplab://examples/lecar.ode`), and a prompt,
`explore_model`, that asks for a complete analysis of a model.

## How calls work

- **Models** are given by path, by example name (`lecar.ode`), or by text.
  A model is loaded once and reloaded when its file changes.
- **Values for one call.** `values` sets parameters, initial conditions and
  options (`{"iapp": 0.1, "total": 500, "meth": "stiff"}`) for that call
  only; afterwards the model's own values are restored. Names the model
  does not have are rejected with the list of names it does have.
- **AUTO's diagram persists.** Each `continuation` adds to the model's
  diagram, and its labels can be continued from later. A continuation from
  a label reuses the values and AUTO settings of the call that computed
  it. Editing the file, or `reset=true`, clears the diagram.
- **Compact results, full data in files.** Results are one line of JSON.
  Whole runs, spectra, sweeps and diagrams go to CSV files in the work
  folder: `--workdir`, else `$XPPLAB_WORKDIR`, else `./xpplab-out`.
- **Reproducible.** Each result has a `python` entry. For AUTO it repeats
  every step of the diagram in order, so pasting it into Python rebuilds
  the same diagram, labels included.
- **Errors say what to do**, for example "iapp = 3 lies outside AUTO's
  range parmin..parmax = -0.2..0.5 ... Pass settings={'parmin': ...,
  'parmax': ...}".

## An example

Asked to analyse the FitzHugh-Nagumo model as the current `I` varies, an
agent typically calls:

1. `check_model("fhn.ode")`: it parses.
2. `simulate("fhn.ode")`: a steady state; with `values={"I": 0.5}`,
   periodic with period 39.5.
3. `continuation("fhn.ode", parameter="I")`: Hopf points at I = 0.331 and
   I = 1.419, the rest state unstable between them.
4. `continuation("fhn.ode", from_label=2, action="periodic")`: the branch of
   limit cycles, which folds at I = 0.324, just below the Hopf point: a
   narrow range of bistability between rest and spiking.
5. `sweep("fhn.ode", "I", 0, 2, steps=21, transient=200, values={"total": 600})`:
   steady, then periodic from I = 0.4 to 1.4, then steady, as AUTO says.
6. `plot_bifurcation("fhn.ode", y="V")`, and an explanation in words, with
   the Python code that repeats it.

## The skill

`skills/xpplab/SKILL.md` in the repository is the guide the Claude Code
plugin loads: the workflow above, how to read special points (sub- and
supercritical Hopf, folds of cycles, SNICs), and the pitfalls (AUTO's
parameter range, the sign of `ds`, `ntst` for spiky orbits, closed loops of
orbits, stiff systems). Other agents can be pointed at the same file.

## Without MCP

The tools are a thin layer over the [Python package](python/quickstart.md).
An agent that can run Python can use `xpplab` directly; the `python`
entries of the results show how.

## Troubleshooting

- **The server does not start**: run `python -m xpplab.mcp --help` with
  the same Python. "No module named mcp" means the `[mcp]` extra is missing.
- **"the XPP Lab engine library was not found"**: the package was
  imported from a source tree without a build. Install the wheel, build the
  engine (`cmake --build build`), or set `XPPLAB_ENGINE` to the library.
- **No examples**: set `XPPLAB_EXAMPLES` to a folder of `.ode` files.
