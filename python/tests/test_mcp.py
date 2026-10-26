"""The MCP server for coding agents (xpplab.mcp).

Each tool is called in-process through the Session that the server wraps;
these tests need only NumPy (and matplotlib for the plots). The server
itself, its schemas and resources, and one real round trip over stdio with
the SDK's own client need the ``mcp`` package and are skipped without it.
"""
import asyncio
import json
import os
import sys
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs
from xpplab.mcp import Session, ToolError
from xpplab.mcp.diagnose import definition_line, diagnose

ROOT = Path(__file__).resolve().parents[2]
ODE = ROOT / "engine" / "xppaut" / "ode"

CLOCK = """\
# radial isochron clock: the limit cycle is the unit circle, period 2*pi/w
x'=x-w*y-x*(x^2+y^2)
y'=y+w*x-y*(x^2+y^2)
par w=1
init x=1,y=0
@ total=100,dt=0.01
done
"""

WELL = """\
# a damped particle in a double well: a saddle at 0, stable foci at +-1
x'=y
y'=x-x^3-c*y
par c=0.1
init x=0.2,y=0
@ total=100
done
"""

TOOLS = {"check_model", "describe_model", "find_examples", "simulate",
         "equilibria", "sweep", "continuation", "spectrum", "lyapunov",
         "limit_cycle", "phase_response", "fit", "bvp", "plot_simulation",
         "plot_phase_plane", "plot_bifurcation", "plot_table",
         "app_status", "app_command", "app_run"}


@pytest.fixture
def session(tmp_path, monkeypatch):
    monkeypatch.setenv("XPPLAB_EXAMPLES", str(ODE))
    s = Session(workdir=tmp_path / "out", cwd=tmp_path)
    yield s
    s.close()


def write(tmp_path, name, text):
    p = tmp_path / name
    p.write_text(text)
    return str(p)


# -- checking and describing models ------------------------------------------

def test_check_model_example(session):
    r = session.check_model("lecar.ode")
    assert r["ok"] and r["origin"] == "example"
    assert r["variables"] == ["V", "W"]
    assert "iapp" in r["parameters"] and "hopf" in r["parameter_sets"]
    assert "xs.load(" in r["python"]


@pytest.mark.parametrize("text, line, name, hint", [
    ("x' = -x + sinn(y)\ny' = x\npar a=1\ndone\n", 1, "SINN", "SIN"),
    ("# c\npar a=1\n\nx' = -a*x\ny' = 3*/x\ninit x=1\ndone\n", 5, None, "malformed"),
    ("x' = -x\nx' = y\ny'=1\ndone\n", 2, "X", "twice"),
    ("x' = -x + q\ny' = x\ndone\n", 1, "Q", "declare"),
    ("x' = -x*(1+y\ny' = x\ndone\n", 1, None, None),
])
def test_check_model_errors_have_lines(session, tmp_path, text, line, name, hint):
    r = session.check_model(write(tmp_path, "bad.ode", text))
    assert r["ok"] is False
    err = r["errors"][0]
    assert err["line"] == line
    assert err["source"] == text.splitlines()[line - 1]
    if name:
        assert err["name"] == name
    if hint:
        assert hint.lower() in err["hint"].lower()


def test_check_model_inline_text_and_warnings(session):
    r = session.check_model("x' = ln(x)\npar a=, b=2\ninit x=0\ndone\n")
    assert r["ok"] and r["origin"] == "text"
    assert Path(r["path"]).is_file()
    joined = " ".join(r["warnings"])
    assert "a has no value" in joined
    assert "not finite" in joined


def test_other_tools_explain_a_broken_model(session, tmp_path):
    with pytest.raises(ToolError, match="line 1.*Call check_model"):
        session.simulate(write(tmp_path, "bad.ode", "x' = -x + q\ndone\n"))


def test_unknown_model_is_explained(session):
    with pytest.raises(ToolError, match="find_examples"):
        session.simulate("no-such-model.ode")


def test_definition_line_finds_arrays():
    text = "par k=1\nx[1..5]' = -k*x[j]\naux tot=x1+x2\ndone\n"
    assert definition_line(text, "X3") == 2
    assert definition_line(text, "tot") == 3


def test_diagnose_without_a_line():
    r = diagnose("x'=1\ndone\n", "could not load m.ode: something odd", "")
    assert r == {"message": "something odd"}


def test_describe_model(session):
    r = session.describe_model("lecar.ode")
    assert r["parameters"]["iapp"] == pytest.approx(0.05)
    assert r["initial_conditions"]["V"] == pytest.approx(-0.144)
    assert r["options"]["meth"] == "rungekutta"
    assert r["auto_settings"]["parmax"] == pytest.approx(0.5)
    assert "Morris-Lecar" in r["description"]


def test_examples_and_manual_are_found(monkeypatch):
    """From a checkout they come from the repository, from a wheel from the
    package (engine/CMakeLists.txt installs them)."""
    from xpplab.mcp.session import doc_path, examples_dir
    monkeypatch.delenv("XPPLAB_EXAMPLES", raising=False)
    assert (examples_dir() / "lecar.ode").is_file()
    assert doc_path("odefile.md") and doc_path("auto.md")


def test_find_examples(session):
    r = session.find_examples("lorenz")
    assert any(e["name"] == "lorenz.ode" for e in r["examples"])
    assert session.find_examples("", limit=3)["matches"] > 100


# -- simulation -----------------------------------------------------------------

def test_simulate_values_apply_to_one_call_only(session):
    r = session.simulate("lecar.ode", values={"iapp": 0.3, "total": 300}, points=5)
    assert r["t"] == [0.0, 300.0]
    assert r["behaviour"]["kind"] == "steady state"
    assert len(r["arrays"]["T"]) == 5
    assert r["columns"]["CUR"]["max"] == pytest.approx(0.3)
    assert "m.set(iapp=0.3, total=300)" in r["python"]
    # The values were restored afterwards.
    d = session.describe_model("lecar.ode")
    assert d["parameters"]["iapp"] == pytest.approx(0.05)
    assert d["options"]["total"] == pytest.approx(30)
    # The CSV holds the whole run, identical to the package's.
    data = np.genfromtxt(r["csv"], delimiter=",", names=True)
    with xs.load(ODE / "lecar.ode") as m:
        sol = m.run(iapp=0.3, total=300)
    assert np.array_equal(data["V"], sol["V"])


def test_simulate_recognises_a_limit_cycle(session):
    r = session.simulate(CLOCK)
    assert r["behaviour"]["kind"] == "periodic"
    assert r["behaviour"]["period"] == pytest.approx(2 * np.pi, rel=1e-3)


def test_simulate_reports_bad_names(session):
    with pytest.raises(ToolError, match="not a parameter, variable or option.*iapp"):
        session.simulate("lecar.ode", values={"iap": 1})
    with pytest.raises(ToolError, match="AUTO setting"):
        session.simulate("lecar.ode", values={"parmax": 1})
    # Options and initial conditions are accepted, whatever their case.
    session.simulate("lecar.ode", values={"TOTAL": 5, "v": 0.1, "Meth": "stiff"})
    with pytest.raises(ToolError, match="unknown method"):
        session.simulate("lecar.ode", values={"meth": "magic"})
    with pytest.raises(ToolError, match="no column 'Q'"):
        session.simulate("lecar.ode", columns=["Q"])
    with pytest.raises(ToolError, match="no parameter set"):
        session.simulate("lecar.ode", parameter_set="nope")


def test_edited_file_is_reloaded(session, tmp_path):
    path = write(tmp_path, "m.ode", "x'=-a*x\npar a=1\ninit x=1\ndone\n")
    assert session.describe_model(path)["parameters"]["a"] == 1
    Path(path).write_text("x'=-a*x\npar a=2\ninit x=1\ndone\n")
    assert session.describe_model(path)["parameters"]["a"] == 2


def test_equilibria_with_guesses(session):
    r = session.equilibria(WELL, guesses=[{"x": 1}, {"x": -1}, {"x": 0.9}])
    eqs = sorted(r["equilibria"], key=lambda e: e["values"]["X"])
    assert [round(e["values"]["X"], 6) for e in eqs] == [-1, 0, 1]
    assert [e["kind"] for e in eqs] == ["stable focus", "saddle", "stable focus"]
    saddle = eqs[1]["eigenvalues"]
    assert saddle[0][0] > 0 > saddle[1][0]


def test_equilibria_match_the_package(session):
    r = session.equilibria("lecar.ode", parameter_set="hopf")
    with xs.load(ODE / "lecar.ode") as m:
        m.apply_set("hopf")
        eq = m.equilibrium()
    (e,) = r["equilibria"]
    assert e["values"]["V"] == pytest.approx(eq["V"], rel=1e-7)
    assert e["stable"] == eq.stable


def test_sweep(session):
    r = session.sweep("lecar.ode", "iapp", 0.0, 0.5, steps=6,
                      parameter_set="hopf", values={"total": 300},
                      transient=200, variables=["V"])
    assert r["runs"] == 6 and len(r["table"]["rows"]) == 6
    regimes = [x["regime"] for x in r["regimes"]]
    assert "steady" in regimes and "periodic" in regimes
    with xs.load(ODE / "lecar.ode") as m:
        m.apply_set("hopf")
        vmax = m.sweep("iapp", np.linspace(0, 0.5, 6), total=300,
                       measure=lambda s: s["V"][s.t >= 200].max())
    got = [row[1] for row in r["table"]["rows"]]
    assert got == pytest.approx(vmax, rel=1e-5)


# -- AUTO -------------------------------------------------------------------------

def test_continuation_hopf_to_periodic(session):
    r = session.continuation("lecar.ode", parameter="iapp", parameter_set="hopf")
    assert r["branch"]["kind"] == "steady states"
    hb = [p for p in r["special_points"] if p["type"] == "HB"]
    assert len(hb) == 2
    with xs.load(ODE / "lecar.ode") as m:
        m.apply_set("hopf")
        ref = m.auto.equilibria("iapp").special("HB")
    assert [p["iapp"] for p in hb] == pytest.approx([p.value for p in ref], rel=1e-7)
    assert any("from_label=2" in s for s in r["next"])
    # The values are restored: AUTO and the set changed phi, gca, ...
    assert session.describe_model("lecar.ode")["parameters"]["phi"] == pytest.approx(0.333)

    o = session.continuation("lecar.ode", from_label=hb[0]["label"],
                             action="periodic", settings={"nmax": 150})
    assert o["branch"]["kind"] == "periodic orbits"
    assert {p["type"] for p in o["special_points"]} >= {"LP"}
    assert o["ends"][-1]["reason"] == "reached nmax steps"
    # The snippet repeats both steps with the package and gets the same.
    ns = {}
    exec(o["python"], ns)
    lp = [p.value for p in ns["d2"].special("LP")]
    assert lp[0] == pytest.approx(o["special_points"][0]["iapp"], rel=1e-7)


def test_continuation_inherits_values_and_settings(session):
    r = session.continuation("lecar.ode", parameter="iapp", parameter_set="hopf",
                             values={"gca": 1.05}, settings={"dsmax": 0.01})
    label = r["special_points"][0]["label"]
    o = session.continuation("lecar.ode", from_label=label, action="periodic",
                             settings={"nmax": 50})
    code = o["python"]
    assert code.count("m.set(gca=1.05)") == 1
    assert "m.auto.set(dsmax=0.01)" in code and "m.auto.set(nmax=50)" in code
    ns = {}
    exec(code, ns)
    assert len(ns["d2"]) == o["branch"]["points"]


def test_continuation_errors(session):
    with pytest.raises(ToolError, match="outside AUTO's range"):
        session.continuation("lecar.ode", parameter="iapp", values={"iapp": 3})
    with pytest.raises(ToolError, match="diagram is empty"):
        session.continuation("lecar.ode", from_label=2, reset=True)
    session.continuation("lecar.ode", parameter="iapp", parameter_set="hopf")
    with pytest.raises(ToolError, match="from it you can: periodic"):
        session.continuation("lecar.ode", from_label=2, action="doubling")
    with pytest.raises(ToolError, match="second_parameter"):
        session.continuation("lecar.ode", from_label=2, action="two-parameter")
    with pytest.raises(ToolError, match="labels are 1, 2, 3, 4"):
        session.continuation("lecar.ode", from_label=99)
    with pytest.raises(ToolError, match="no parameter named 'iap'"):
        session.continuation("lecar.ode", parameter="iap")


# -- analysis ---------------------------------------------------------------------

def test_spectrum_peak(session):
    r = session.spectrum(CLOCK, "X", values={"total": 400, "trans": 50})
    peak = r["peaks"][0]
    assert peak["frequency"] == pytest.approx(1 / (2 * np.pi), abs=2 * r["resolution"])
    with pytest.raises(ToolError, match="kind must be"):
        session.spectrum(CLOCK, "X", kind="bogus")


def test_lyapunov(session):
    r = session.lyapunov("lorenz.ode", values={"total": 200})
    assert r["lyapunov"] > 0.5 and r["reading"].startswith("positive")
    r = session.lyapunov(CLOCK)
    assert r["lyapunov"] < 0.01 and r["reading"].startswith("zero or negative")


def test_limit_cycle_and_prc(session):
    r = session.limit_cycle(CLOCK, transient=20)
    assert r["period"] == pytest.approx(2 * np.pi, rel=1e-3)
    assert r["cycle"]["X"]["max"] == pytest.approx(1, abs=1e-3)
    p = session.phase_response(CLOCK, transient=20)
    assert p["period"] == pytest.approx(2 * np.pi, rel=1e-3)
    assert p["prc"]["X"]["type"].startswith("II")
    with pytest.raises(ToolError, match="limit cycle"):
        session.phase_response(WELL, transient=10)


def test_fit_recovers_a_rate(session, tmp_path):
    t = np.linspace(0, 5, 26)
    data = tmp_path / "decay.dat"
    np.savetxt(data, np.column_stack([t, 2 * np.exp(-0.5 * t)]))
    model = write(tmp_path, "decay.ode", "x'=-k*x\npar k=1\ninit x=2\n@ dt=0.01\ndone\n")
    r = session.fit(model, str(data), ["x"], ["k"])
    assert r["fitted"]["k"] == pytest.approx(0.5, rel=1e-3)
    assert r["rms_error"]["after"] < r["rms_error"]["before"]
    assert session.describe_model(model)["parameters"]["k"] == 1


def test_bvp(session):
    r = session.bvp("cable.ode")
    assert "VX" in r["initial_conditions"]
    assert Path(r["csv"]).is_file()


# -- plots ------------------------------------------------------------------------------

def is_png(path):
    return Path(path).read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"


def test_plots(session):
    pytest.importorskip("matplotlib")
    assert is_png(session.plot_simulation("lecar.ode", y=["V"])["file"])
    r = session.plot_phase_plane(WELL, xlim=[-2, 2], ylim=[-1.5, 1.5], format="svg")
    assert Path(r["file"]).read_text().lstrip().startswith("<?xml")
    with pytest.raises(ToolError, match="run continuation first"):
        session.plot_bifurcation("lecar.ode")
    session.continuation("lecar.ode", parameter="iapp", parameter_set="hopf")
    session.continuation("lecar.ode", from_label=2, action="two-parameter",
                         second_parameter="gca", settings={"nmax": 40})
    r = session.plot_bifurcation("lecar.ode", y="V")
    assert is_png(r["file"]) and r["two_parameter_points"] > 0
    s = session.sweep("lecar.ode", "iapp", 0, 0.4, steps=3)
    assert is_png(session.plot_table(s["csv"], y=["V_max", "V_min"])["file"])
    with pytest.raises(ToolError, match="format must be"):
        session.plot_simulation("lecar.ode", format="jpg")


# -- the MCP server ---------------------------------------------------------------------

def _content(result):
    """Content blocks of a tool result: a CallToolResult, or with version 1
    of the SDK called in-process, a list (or a (list, structured) pair)."""
    if isinstance(result, tuple):
        result = result[0]
    return result if isinstance(result, list) else result.content


def _text(result):
    return _content(result)[0].text


def test_server_tools_and_schemas(tmp_path):
    pytest.importorskip("mcp")
    from xpplab.mcp import create_server
    server = create_server(Session(workdir=tmp_path))
    tools = asyncio.run(server.list_tools())
    assert {t.name for t in tools} == TOOLS
    for t in tools:
        assert t.description and len(t.description) > 40
        schema = getattr(t, "input_schema", None) or getattr(t, "inputSchema")
        props = schema["properties"]
        if "model" in props:
            assert "example" in props["model"]["description"]
        for name, p in props.items():
            assert p.get("description") or name in ("nullclines", "field",
                                                    "trajectory", "logy", "title")


def test_server_calls_tools_in_process(tmp_path, monkeypatch):
    pytest.importorskip("mcp")
    monkeypatch.setenv("XPPLAB_EXAMPLES", str(ODE))
    from xpplab.mcp import create_server
    server = create_server(Session(workdir=tmp_path))

    async def go():
        r = await server.call_tool("check_model", {"model": "lecar.ode"})
        assert json.loads(_text(r))["ok"] is True
        r = await server.call_tool("plot_simulation", {"model": "lecar.ode"})
        assert [c.type for c in _content(r)] == ["text", "image"]
        with pytest.raises(Exception, match="find_examples"):
            await server.call_tool("simulate", {"model": "nope.ode"})
        text = await server.read_resource("xpplab://docs/ode-format")
        assert "The .ode file format" in list(text)[0].content
        ex = await server.read_resource("xpplab://examples/lecar.ode")
        assert "Morris-Lecar" in list(ex)[0].content
    asyncio.run(go())


def test_stdio_round_trip(tmp_path):
    """The server as an agent starts it: a subprocess spoken to over stdio by
    the MCP SDK's own client."""
    pytest.importorskip("mcp")
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client

    src = str(Path(xs.__file__).resolve().parents[1])
    env = dict(os.environ)
    env["PYTHONPATH"] = src + os.pathsep + env.get("PYTHONPATH", "")
    env["XPPLAB_EXAMPLES"] = str(ODE)
    params = StdioServerParameters(
        command=sys.executable, cwd=str(tmp_path), env=env,
        args=["-m", "xpplab.mcp", "--workdir", str(tmp_path / "out")])

    async def go():
        async with stdio_client(params) as (read, write):
            async with ClientSession(read, write) as client:
                await client.initialize()
                names = {t.name for t in (await client.list_tools()).tools}
                assert names == TOOLS
                r = await client.call_tool("continuation", {
                    "model": "lecar.ode", "parameter": "iapp",
                    "parameter_set": "hopf"})
                out = json.loads(_text(r))
                assert [p["type"] for p in out["special_points"]] == ["HB", "HB"]
                assert Path(out["csv"]).parent == tmp_path / "out"
                r = await client.call_tool("simulate", {"model": "nope.ode"})
                assert getattr(r, "is_error", None) or getattr(r, "isError", None)
                assert "find_examples" in _text(r)
                res = await client.read_resource("xpplab://docs/auto")
                assert "AUTO" in res.contents[0].text
    asyncio.run(asyncio.wait_for(go(), 120))
