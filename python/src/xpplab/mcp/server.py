"""The MCP server: the tools of :mod:`xpplab.mcp.session` over the Model
Context Protocol, with the manual and the example models as resources.

Built on the official MCP Python SDK (``MCPServer``, called ``FastMCP``
before version 2 of the SDK). Every tool returns one line of JSON; the plot
tools also return the PNG itself so that the agent can look at it.
"""

import argparse
import io
import json
import os
import sys
from typing import Annotated, Any, Dict, List, Optional, Union

from pydantic import Field

try:  # mcp >= 2
    from mcp.server.mcpserver import Image, MCPServer as _Server
    from mcp.server.mcpserver.exceptions import ToolError as _SDKToolError
except ImportError:  # mcp 1.x
    from mcp.server.fastmcp import FastMCP as _Server, Image
    from mcp.server.fastmcp.exceptions import ToolError as _SDKToolError

from .. import __version__
from .session import Session, ToolError, describe_file, doc_path, examples_dir

INSTRUCTIONS = """\
XPP Lab runs XPPAUT's integrators and its AUTO continuation on .ode model
files (Ermentrout's XPPAUT 8.0, bit for bit).

- model: a path to an .ode file, the name of an example (find_examples), or
  the text of a model. The format is in the resource xpplab://docs/ode-format.
- values: parameters, initial conditions and options for one call, e.g.
  {"iapp": 0.1, "V": -0.3, "total": 200, "meth": "stiff"}. Each call starts
  from the file's values plus these; nothing carries over between calls.
- AUTO's diagram is the one thing that persists: continuation adds to it,
  continuation(from_label=N) continues from its labelled points with the
  values and settings that computed them, and plot_bifurcation draws it.
  It is cleared when the model file changes, or with reset=true.
- Results are compact JSON; full data goes to CSV files in the work folder;
  every result has a "python" entry that reproduces it with the xpplab
  package.

- If the researcher has the XPP Lab desktop app open, app_status,
  app_command and app_run act in that window instead, so they watch the
  plots change: app_command takes any command of the app's command window
  (run, set iapp=0.2, nullclines, auto steady iapp, range ..., export ...).

Typical session: write the .ode file, check_model, describe_model, simulate,
equilibria, continuation(parameter=...) for steady states, then
continuation(from_label=<HB label>, action="periodic") for the limit cycles,
and plot_bifurcation. Check stable branches against simulation with sweep.
"""

# -- argument types (their descriptions become the tools' JSON schemas) -------

ModelArg = Annotated[str, Field(description=(
    "A path to an .ode file (relative to the server's folder), the name of an "
    "example such as 'lecar.ode', or the full text of a model (any string "
    "with a newline is taken as text)."))]
Values = Annotated[Optional[Dict[str, Union[float, str]]], Field(
    description=(
        "Values for this call only: parameters, initial conditions (by "
        "variable name) and numerical options such as total, dt, t0, trans, "
        "meth ('stiff', 'cvode', '5dp', ...), tol, bound. Names ignore case. "
        "Example: {\"iapp\": 0.1, \"total\": 500}."))]
ParameterSet = Annotated[Optional[str], Field(
    description="A named parameter set from the file "
                              "('set name {...}'), applied before values.")]
Format = Annotated[str, Field(description="'png' or 'svg'.")]
Show = Annotated[bool, Field(description=(
    "Also return the PNG image itself, so you can look at it."))]


def _json(result: dict) -> str:
    return json.dumps(result, ensure_ascii=False)


def create_server(session: Optional[Session] = None):
    """Build the MCP server around a Session (a new one by default)."""
    s = session or Session()
    server = _Server("xpplab", instructions=INSTRUCTIONS)
    server._xpp_session = s  # for tests and embedding

    def call(method, **kw) -> str:
        try:
            return _json(getattr(s, method)(**kw))
        except ToolError as err:
            raise _SDKToolError(str(err)) from None

    def picture(method, show: bool, **kw):
        try:
            result = getattr(s, method)(**kw)
        except ToolError as err:
            raise _SDKToolError(str(err)) from None
        if show and result["file"].endswith(".png"):
            return [_json(result), Image(path=result["file"])]
        return _json(result)

    tool = server.tool(structured_output=False)

    # -- models ------------------------------------------------------------------

    @tool
    def check_model(model: ModelArg) -> str:
        """Load a model and report whether it parses. Errors come with the line
        number, the offending expression with a caret, and a hint (e.g. a
        misspelled name and what it may have meant). Also warns about values
        left empty and right-hand sides that are not finite at the initial
        conditions. Call this after every edit of an .ode file."""
        return call("check_model", model=model)

    @tool
    def describe_model(model: ModelArg) -> str:
        """Variables with their initial conditions, parameters with their
        defaults, auxiliary columns, named parameter sets, the numerical
        options (total, dt, method, ...) and AUTO's main settings."""
        return call("describe_model", model=model)

    @tool
    def find_examples(
            query: Annotated[str, Field(description=(
                "Words to look for in the file names and descriptions, e.g. "
                "'hopf', 'neuron', 'lorenz'. Empty lists every example."))] = "",
            limit: Annotated[int, Field(description="At most this many.")] = 25,
    ) -> str:
        """Search the example models that ship with XPPAUT (Morris-Lecar,
        FitzHugh-Nagumo, Lorenz, ...). Pass a name as model to any tool."""
        return call("find_examples", query=query, limit=limit)

    # -- the running desktop app ---------------------------------------------------

    @tool
    def app_status() -> str:
        """Is the XPP Lab desktop app open? Its model, parameters and initial
        values. The app_* tools act in that window, where the researcher
        sees the plots follow."""
        return call("app_status")

    @tool
    def app_command(
            command: Annotated[str, Field(description=(
                "A command of the app's command window, e.g. 'run', "
                "'set iapp=0.2 total=200', 'nullclines', 'eq', 'auto steady "
                "iapp', 'auto from 2 periodic', 'range iapp 0 0.3 20 V', "
                "'export run.csv'. 'help' lists them."))],
    ) -> str:
        """Run a command in the open XPP Lab window, as if the researcher had
        typed it. Returns what the command window showed and the Python the
        app wrote for it."""
        return call("app_command", command=command)

    @tool
    def app_run(values: Values = None) -> str:
        """Set values in the open XPP Lab window (they stay set, unlike
        simulate's), run the model there and summarise the run: min, max,
        mean and final value of each column, and what it does in the end."""
        return call("app_run", values=values)

    # -- simulation --------------------------------------------------------------

    @tool
    def simulate(
            model: ModelArg, values: Values = None,
            parameter_set: ParameterSet = None,
            columns: Annotated[Optional[List[str]], Field(description=(
                "Columns to summarise (default: every variable and auxiliary)."))] = None,
            points: Annotated[int, Field(description=(
                "Also return this many evenly spaced samples of each column "
                "inline (0 = none, at most 2000). The CSV always has every point."))] = 0,
    ) -> str:
        """Integrate the model once with XPPAUT's integrator. Returns min, max,
        mean and final value of each column, what the run does in the end
        (steady state, periodic with its period, damped oscillation, not
        settled), warnings (blow-up, early stop), and the CSV file of the
        whole run."""
        return call("simulate", model=model, values=values,
                    parameter_set=parameter_set, columns=columns, points=points)

    @tool
    def equilibria(
            model: ModelArg, values: Values = None,
            parameter_set: ParameterSet = None,
            guesses: Annotated[Optional[List[Dict[str, float]]], Field(description=(
                "More starting points for Newton's method, each a dict of "
                "variable values (others keep their initial values)."))] = None,
    ) -> str:
        """Equilibria by Newton's method from the initial conditions and from
        each guess, with the eigenvalues of the Jacobian, stability and type
        (stable node, unstable focus, saddle, ...). Duplicates are removed."""
        return call("equilibria", model=model, values=values,
                    parameter_set=parameter_set, guesses=guesses)

    @tool
    def sweep(
            model: ModelArg,
            parameter: Annotated[str, Field(description=(
                "The parameter (or variable, for its initial value) to vary."))],
            start: Annotated[Optional[float], Field(description="First value.")] = None,
            stop: Annotated[Optional[float], Field(description="Last value.")] = None,
            steps: Annotated[int, Field(description="Number of values from start to stop.")] = 21,
            at: Annotated[Optional[List[float]], Field(description=(
                "The values themselves, instead of start/stop/steps."))] = None,
            values: Values = None, parameter_set: ParameterSet = None,
            variables: Annotated[Optional[List[str]], Field(description=(
                "Variables whose max and min to record (default: the first three)."))] = None,
            transient: Annotated[float, Field(description=(
                "Time to skip at the start of each run before measuring."))] = 0.0,
            continue_runs: Annotated[bool, Field(description=(
                "Start each run where the previous one ended (follows one "
                "attractor; XPPAUT's 'Use old ICs = No')."))] = False,
            lyapunov: Annotated[bool, Field(description=(
                "Also the largest Liapunov exponent of each run (slower)."))] = False,
    ) -> str:
        """XPPAUT's Range: one simulation per parameter value. Records the
        max and min of variables after the transient, the period, and
        optionally the Liapunov exponent, and summarises the regimes (steady,
        periodic, irregular, chaotic) over the range. A brute-force
        bifurcation diagram, useful to check AUTO's stable branches."""
        return call("sweep", model=model, parameter=parameter, start=start,
                    stop=stop, steps=steps, at=at, values=values,
                    parameter_set=parameter_set, variables=variables,
                    transient=transient, continue_runs=continue_runs,
                    lyapunov=lyapunov)

    # -- AUTO ---------------------------------------------------------------------

    @tool
    def continuation(
            model: ModelArg,
            parameter: Annotated[Optional[str], Field(description=(
                "Follow steady states as this parameter varies, starting from "
                "the equilibrium nearest the initial conditions."))] = None,
            from_label: Annotated[Optional[int], Field(description=(
                "Instead, continue from this labelled point of the diagram "
                "(labels come from earlier continuation results)."))] = None,
            action: Annotated[Optional[str], Field(description=(
                "What to do from the labelled point. From HB: 'periodic' "
                "(limit cycles), 'two-parameter', 'extend', 'new'. From LP: "
                "'two-parameter'. From BP: 'switch', 'extend', 'new', "
                "'two-parameter'. From PD: 'doubling', 'two-parameter', "
                "'fixed-period', 'extend'. From TR: 'two-parameter', "
                "'fixed-period', 'extend'. From a regular point: 'extend'."))] = None,
            second_parameter: Annotated[Optional[str], Field(description=(
                "The second parameter of a two-parameter continuation."))] = None,
            values: Values = None, parameter_set: ParameterSet = None,
            settings: Annotated[Optional[Dict[str, float]], Field(description=(
                "AUTO numerics: parmin, parmax (the parameter range; AUTO stops "
                "at its ends), ds (first step; its sign is the direction), "
                "dsmin, dsmax (smaller gives smoother branches), nmax (most "
                "steps), npr (label every npr steps), ntst (mesh for periodic "
                "orbits; 50-100 for spiky ones), normmax, epsl, epsu, epss."))] = None,
            reset: Annotated[bool, Field(description=(
                "Clear this model's diagram first."))] = False,
    ) -> str:
        """AUTO continuation. With parameter: a branch of steady states. With
        from_label: periodic orbits from a Hopf point, two-parameter curves
        of folds or Hopf points, branch switching, period doubling. Returns
        the special points (HB Hopf, LP fold, BP branch point, PD period
        doubling, TR torus) with their labels, parameter values, stability
        and the actions possible from each; where the branch is stable; why
        it ended; advice for the next step; and a CSV of every point. Labels
        stay valid until the model file changes or reset=true."""
        return call("continuation", model=model, parameter=parameter,
                    from_label=from_label, action=action,
                    second_parameter=second_parameter, values=values,
                    parameter_set=parameter_set, settings=settings, reset=reset)

    # -- analysis ------------------------------------------------------------------

    @tool
    def spectrum(
            model: ModelArg,
            column: Annotated[str, Field(description="The column to analyse, e.g. 'V'.")],
            values: Values = None, parameter_set: ParameterSet = None,
            column2: Annotated[Optional[str], Field(description=(
                "Second column, for kind='cross' or 'coherence'."))] = None,
            kind: Annotated[str, Field(description=(
                "'psd' (power spectral density), 'cross' or 'coherence'."))] = "psd",
            length: Annotated[Optional[int], Field(description=(
                "Window length in points (default 512, less for short runs)."))] = None,
            window: Annotated[str, Field(description=(
                "'square', 'parzen', 'hamming', 'bartlett' or 'hann'."))] = "hamming",
    ) -> str:
        """Spectrum of one run by Welch's method (XPPAUT's Spec.dns): the
        strongest peaks with frequency and period, and the CSV of the whole
        spectrum. Use trans in values to skip a transient."""
        return call("spectrum", model=model, column=column, values=values,
                    parameter_set=parameter_set, column2=column2, kind=kind,
                    length=length, window=window)

    @tool
    def lyapunov(model: ModelArg, values: Values = None,
                 parameter_set: ParameterSet = None) -> str:
        """The largest Liapunov exponent along one run, XPPAUT's way, with a
        reading. Positive suggests chaos; a stable equilibrium gives a clearly
        negative value, and a limit cycle a value near zero or somewhat
        below. Use a long run (total) that has settled."""
        return call("lyapunov", model=model, values=values,
                    parameter_set=parameter_set)

    @tool
    def limit_cycle(
            model: ModelArg, values: Values = None,
            parameter_set: ParameterSet = None,
            transient: Annotated[Optional[float], Field(description=(
                "Time to integrate first to settle on the cycle (default: total)."))] = None,
            variable: Annotated[Optional[str], Field(description=(
                "Variable whose crossings measure the period (default: the first)."))] = None,
            period: Annotated[Optional[float], Field(description=(
                "The period, if known."))] = None,
    ) -> str:
        """Find one period of a stable limit cycle by simulation: the period,
        the range of each variable on the cycle, and the CSV of one period."""
        return call("limit_cycle", model=model, values=values,
                    parameter_set=parameter_set, transient=transient,
                    variable=variable, period=period)

    @tool
    def phase_response(
            model: ModelArg, values: Values = None,
            parameter_set: ParameterSet = None,
            transient: Annotated[Optional[float], Field(description=(
                "Time to settle on the cycle first (default: total)."))] = None,
            variable: Annotated[Optional[str], Field(description=(
                "Variable whose crossings measure the period."))] = None,
            period: Annotated[Optional[float], Field(description=(
                "The period, if known."))] = None,
    ) -> str:
        """Infinitesimal phase response curves (PRC) of a stable limit cycle,
        from XPPAUT's adjoint: for each variable the extremes, where they
        occur in phase, and whether the curve is type I (advances only) or
        type II (advances and delays). The CSV holds phase, time and one
        column per variable."""
        return call("phase_response", model=model, values=values,
                    parameter_set=parameter_set, transient=transient,
                    variable=variable, period=period)

    @tool
    def fit(
            model: ModelArg,
            data: Annotated[str, Field(description=(
                "A text or CSV file of rows without a header: time first, then "
                "the measured columns."))],
            fit_variables: Annotated[List[str], Field(description=(
                "Model variables to match to the data."))],
            fit_parameters: Annotated[List[str], Field(description=(
                "Parameters (and initial conditions) to adjust."))],
            columns: Annotated[Optional[List[int]], Field(description=(
                "Data column of each fit variable, counting time as 1 "
                "(default 2, 3, ...)."))] = None,
            values: Values = None, parameter_set: ParameterSet = None,
            tolerance: Annotated[float, Field(description="0 for XPPAUT's default.")] = 0.0,
            max_iterations: Annotated[int, Field(description="0 for XPPAUT's default.")] = 0,
    ) -> str:
        """Fit parameters and initial conditions to data with XPPAUT's
        Levenberg-Marquardt. Returns the fitted values and the RMS error
        before and after; pass the fitted values in values afterwards."""
        return call("fit", model=model, data=data, fit_variables=fit_variables,
                    fit_parameters=fit_parameters, columns=columns,
                    values=values, parameter_set=parameter_set,
                    tolerance=tolerance, max_iterations=max_iterations)

    @tool
    def bvp(model: ModelArg, values: Values = None,
            parameter_set: ParameterSet = None) -> str:
        """Solve the model's boundary-value problem (its bndry statements) by
        XPPAUT's shooting method over [t0, t0+total], from the initial
        conditions as the first guess. Returns the initial conditions that
        solve it and the CSV of the solution."""
        return call("bvp", model=model, values=values, parameter_set=parameter_set)

    # -- plots -------------------------------------------------------------------------

    @tool
    def plot_simulation(
            model: ModelArg,
            y: Annotated[Optional[List[str]], Field(description=(
                "Columns to plot (default: the first two variables)."))] = None,
            x: Annotated[str, Field(description="The x-axis column.")] = "T",
            values: Values = None, parameter_set: ParameterSet = None,
            format: Format = "png", show: Show = True) -> Any:
        """Plot a run: columns against time, or against another column."""
        return picture("plot_simulation", show, model=model, y=y, x=x,
                       values=values, parameter_set=parameter_set, format=format)

    @tool
    def plot_phase_plane(
            model: ModelArg,
            x: Annotated[Optional[str], Field(description="Horizontal variable.")] = None,
            y: Annotated[Optional[str], Field(description="Vertical variable.")] = None,
            values: Values = None, parameter_set: ParameterSet = None,
            xlim: Annotated[Optional[List[float]], Field(description="[min, max].")] = None,
            ylim: Annotated[Optional[List[float]], Field(description="[min, max].")] = None,
            nullclines: bool = True, field: bool = True, trajectory: bool = True,
            format: Format = "png", show: Show = True) -> Any:
        """Phase plane of two variables: the trajectory, XPPAUT's nullclines,
        the direction field, and the nearest equilibrium (filled if stable)
        with the manifolds of a saddle. Other variables keep their initial
        values."""
        return picture("plot_phase_plane", show, model=model, x=x, y=y,
                       values=values, parameter_set=parameter_set, xlim=xlim,
                       ylim=ylim, nullclines=nullclines, field=field,
                       trajectory=trajectory, format=format)

    @tool
    def plot_bifurcation(
            model: ModelArg,
            y: Annotated[Optional[str], Field(description=(
                "A variable (its max and min on orbits), 'norm' or 'period'."))] = None,
            format: Format = "png", show: Show = True) -> Any:
        """Draw the AUTO diagram computed so far for this model, in XPPAUT's
        colours: stable steady states red, unstable black (dashed); stable
        periodic orbits green, unstable blue; special points labelled.
        Two-parameter curves go in a second panel."""
        return picture("plot_bifurcation", show, model=model, y=y, format=format)

    @tool
    def plot_table(
            csv_file: Annotated[str, Field(description=(
                "A CSV file with a header line, such as one this server wrote."))],
            x: Annotated[Optional[str], Field(description="x column (default: the first).")] = None,
            y: Annotated[Optional[List[str]], Field(description="y columns (default: the second).")] = None,
            logy: bool = False,
            title: Optional[str] = None,
            format: Format = "png", show: Show = True) -> Any:
        """Plot columns of a CSV file: a sweep, a spectrum (logy), a PRC."""
        return picture("plot_table", show, csv_file=csv_file, x=x, y=y,
                       logy=logy, title=title, format=format)

    # -- resources ---------------------------------------------------------------------

    def _doc(name: str, title: str) -> str:
        p = doc_path(name)
        if p is None:
            return (f"{title} is not installed with this copy of xpplab. See "
                    f"https://github.com/CesarPerdomoDiaz/xpplab/blob/main/"
                    f"docs/manual/{name}")
        return p.read_text(encoding="utf-8")

    @server.resource("xpplab://docs/ode-format", name="ode-format",
                     title="The .ode file format",
                     description="How to write an XPPAUT model file: equations, "
                                 "parameters, options, expressions.",
                     mime_type="text/markdown")
    def ode_format() -> str:
        return _doc("odefile.md", "The .ode reference")

    @server.resource("xpplab://docs/auto", name="auto-guide",
                     title="Bifurcation analysis with AUTO",
                     description="Special points, the numerical settings, and "
                                 "what to do when AUTO fails.",
                     mime_type="text/markdown")
    def auto_guide() -> str:
        return _doc("auto.md", "The AUTO guide")

    @server.resource("xpplab://examples", name="examples",
                     title="Example models",
                     description="The example .ode files, one line each.",
                     mime_type="text/markdown")
    def examples() -> str:
        ex = examples_dir()
        if ex is None:
            return "No example folder found; set XPPLAB_EXAMPLES."
        lines = [f"# Example models ({ex})", "",
                 "Pass a name as `model` to any tool; read one with "
                 "xpplab://examples/<name>.", ""]
        for p in sorted(ex.glob("*.ode"), key=lambda p: p.name.lower()):
            about = describe_file(p)
            lines.append(f"- {p.name}" + (f": {about}" if about else ""))
        return "\n".join(lines) + "\n"

    @server.resource("xpplab://examples/{name}", name="example-model",
                     title="An example model",
                     description="The text of one example .ode file.",
                     mime_type="text/plain")
    def example_model(name: str) -> str:
        ex = examples_dir()
        p = (ex / name) if ex else None
        if p is None or p.parent != ex or not p.is_file():
            raise ValueError(f"no example named {name!r}")
        return p.read_text(encoding="latin-1")

    # -- prompt ------------------------------------------------------------------------

    @server.prompt(name="explore_model",
                   description="Analyse a model: simulate it, find its "
                               "equilibria and bifurcations, and explain them.")
    def explore_model(model: str, parameter: str = "") -> str:
        target = f" in {parameter}" if parameter else ""
        return (
            f"Analyse the XPPAUT model {model} with the xpplab tools.\n"
            f"1. check_model, then describe_model.\n"
            f"2. simulate it and say what it does (steady state, oscillation, ...).\n"
            f"3. equilibria: where are they, and are they stable?\n"
            f"4. continuation of the steady states{target} (choose parmin/parmax "
            f"around the interesting range); then from each Hopf point, "
            f"continuation with action='periodic'.\n"
            f"5. plot_bifurcation, and check the stable branches with sweep.\n"
            f"6. Explain the dynamics in plain words, and give the python "
            f"snippets so that the analysis can be repeated.")

    return server


def _protect_stdio() -> None:
    """Serve MCP from private copies of stdin and stdout, and point file
    descriptors 0 and 1 elsewhere. XPPAUT's C code prints with printf; if
    anything reached the real stdout it would corrupt the protocol."""
    try:
        sys.stdout.flush()
        wire_out = os.dup(1)
        wire_in = os.dup(0)
    except (OSError, ValueError, AttributeError):
        return
    try:
        os.dup2(2, 1)
    except OSError:
        null = os.open(os.devnull, os.O_WRONLY)
        os.dup2(null, 1)
        os.close(null)
    null = os.open(os.devnull, os.O_RDONLY)
    os.dup2(null, 0)
    os.close(null)
    sys.stdout = io.TextIOWrapper(os.fdopen(wire_out, "wb"), encoding="utf-8",
                                  line_buffering=True)
    sys.stdin = io.TextIOWrapper(os.fdopen(wire_in, "rb"), encoding="utf-8")


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        prog="python -m xpplab.mcp",
        description="XPP Lab's MCP server, over stdio: XPPAUT simulations "
                    "and AUTO continuation for coding agents.")
    ap.add_argument("--workdir", help="folder for CSV files, figures and "
                    "inline models (default: $XPPLAB_WORKDIR, else "
                    "./xpplab-out)")
    ap.add_argument("--version", action="version",
                    version=f"xpplab {__version__}")
    args = ap.parse_args(argv)
    session = Session(workdir=args.workdir)
    server = create_server(session)
    _protect_stdio()
    try:
        server.run("stdio")
    finally:
        session.close()
    return 0
