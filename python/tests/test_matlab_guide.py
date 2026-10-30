"""The "Coming from MATLAB" page in docs/manual must keep working.

The model file in the page is written out, every Python snippet runs in one
namespace, every name the table gives exists, and every command the page
tells the reader to type is one the desktop app has.
"""
import re
import subprocess
from pathlib import Path

import numpy as np
import pytest

ROOT = Path(__file__).resolve().parents[2]
PAGE = ROOT / "docs" / "manual" / "matlab.md"
COMMANDS_C = ROOT / "app" / "src" / "commands.c"


def blocks(language):
    text = PAGE.read_text(encoding="utf-8")
    return re.findall(r"^```%s\n(.*?)^```" % language, text, re.S | re.M)


@pytest.fixture(scope="module")
def ns(tmp_path_factory):
    matplotlib = pytest.importorskip("matplotlib")
    matplotlib.use("Agg")
    folder = tmp_path_factory.mktemp("matlab")
    (ode,) = blocks("ode")
    (folder / "vdp.ode").write_text(ode, encoding="utf-8")
    code = blocks("python")
    assert len(code) == 2
    space = {}
    import os
    here = os.getcwd()
    os.chdir(folder)
    try:
        for k, snippet in enumerate(code):
            exec(compile(snippet, f"{PAGE.name} snippet {k + 1}", "exec"), space)
    finally:
        os.chdir(here)
    yield space
    space["m"].close()


def test_numbers_in_the_text(ns):
    assert ns["sol"]["x"][-1] == pytest.approx(2.008, abs=1e-3)
    assert ns["sol"].t[-1] == pytest.approx(20.0)
    eq = ns["eq"]
    assert eq.kind == "unstable focus"
    assert eq["x"] == pytest.approx(0, abs=1e-9)
    assert eq["y"] == pytest.approx(0, abs=1e-9)
    ev = sorted(eq.eigenvalues, key=lambda z: z.imag)
    assert ev == pytest.approx([0.5 - 0.866025j, 0.5 + 0.866025j], abs=1e-5)
    lin = ns["lin"]
    assert np.allclose(lin.A, [[0, 1], [-1, 1]], atol=1e-6)
    assert np.allclose(lin.B, [[0], [1]], atol=1e-6)
    assert (lin.rank, lin.controllable) == (2, True)
    assert abs(ns["hb"].value) < 1e-6
    assert ns["hb"].type == "HB"


def test_names_in_the_table_exist(ns):
    """Each `m.x`, `eq.x`, `lin.x`, `sol.x` in the table is a real attribute."""
    table = [ln for ln in PAGE.read_text(encoding="utf-8").splitlines()
             if ln.startswith("| ")]
    assert len(table) > 10
    owners = {"m": ns["m"], "eq": ns["eq"], "lin": ns["lin"], "sol": ns["sol"],
              "d": ns["d"], "m.auto": ns["m"].auto}
    checked = set()
    for row in table:
        for code in re.findall(r"`([^`]*)`", row):
            for owner, name in re.findall(r"\b(m\.auto|m|eq|lin|sol|d)\.([a-z_]+)", code):
                assert hasattr(owners[owner], name), f"{owner}.{name} in: {row}"
                checked.add(f"{owner}.{name}")
    assert {"m.run", "m.equilibrium", "m.linearize", "m.auto.equilibria",
            "lin.controllability", "lin.rank", "eq.eigenvalues"} <= checked


def test_settings_in_the_table_are_accepted(ns):
    m = ns["m"]
    m.set(meth="stiff")
    assert m.get("METH") == "stiff"
    m.set(meth="cvode")
    assert m.get("METH") == "cvode"
    m.set(meth="5dp", tol=1e-6, atol=1e-6)
    assert m.get("METH") == "5dp"


def app_commands():
    src = COMMANDS_C.read_text(encoding="utf-8")
    return set(re.findall(r'strcmp\(cmd, "([^"]+)"\)', src))


def test_commands_exist():
    lines = [ln for b in blocks("commands") for ln in b.splitlines() if ln]
    assert len(lines) >= 5
    known = app_commands()
    for ln in lines:
        assert ln.split()[0] in known, f"unknown app command: {ln}"


def batch_exe():
    """The built xpplab-batch, if the engine library sits next to it.

    The tool copies that library to load it; a build that keeps it elsewhere
    (the macOS CI build does) cannot run it from here."""
    for name in ("xpplab-batch.exe", "xpplab-batch"):
        exe = ROOT / "build" / "app" / name
        if exe.exists() and any((exe.parent / lib).exists() for lib in (
                "libxppe.dll", "libxppe.dylib", "libxppe.so")):
            return exe
    return None


def test_echoed_lines_are_what_the_app_prints(tmp_path):
    """Run the page's commands in xpplab-batch and compare its echo."""
    exe = batch_exe()
    if exe is None:
        pytest.skip("xpplab-batch is not built with its engine library")
    (ode,) = blocks("ode")
    (tmp_path / "vdp.ode").write_text(ode, encoding="utf-8")
    (cmds,) = blocks("commands")
    args = [str(exe), "vdp.ode", "--echo"]
    for ln in cmds.splitlines():
        if ln:
            args += ["-c", ln]
    done = subprocess.run(args, cwd=tmp_path, capture_output=True, text=True,
                          timeout=120)
    assert done.returncode == 0, done.stdout + done.stderr
    out = done.stdout
    got = [ln for ln in out.splitlines() if ln.startswith(">>> ")]
    # the first two lines load the model and run it once; not from the page
    assert got[0].startswith(">>> m = xs.load(")
    assert got[1] == ">>> sol = m.run()"
    (echo,) = blocks("text")
    assert got[2:] == [ln for ln in echo.splitlines() if ln]


def test_echoed_lines_are_calls_of_the_package(ns):
    """Each echoed line runs in the package, as it is written."""
    (echo,) = blocks("text")
    for ln in echo.splitlines():
        assert ln.startswith(">>> ")
        exec(compile(ln[4:], "echo", "exec"), {"m": ns["m"], "sol": None})


def test_page_is_linked():
    mk = (ROOT / "mkdocs.yml").read_text(encoding="utf-8")
    assert "manual/matlab.md" in mk
    # The Help menu is drawn from the table of actions.
    menu = (ROOT / "app" / "src" / "actions.c").read_text(encoding="utf-8")
    assert '"matlab"' in menu
    assert "(matlab.md)" in (ROOT / "docs" / "manual" / "welcome.md").read_text(
        encoding="utf-8")
    roadmap = (ROOT / "ROADMAP.md").read_text(encoding="utf-8")
    assert re.search(r'^- \[x\] "Coming from MATLAB"', roadmap, re.M)
