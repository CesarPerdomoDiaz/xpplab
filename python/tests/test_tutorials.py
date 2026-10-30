"""The tutorial pages in docs/manual must keep working.

Every Python snippet runs, in order, in one namespace; every command the
page tells the reader to type is one the desktop app has.
"""
import re
import shutil
from pathlib import Path

import numpy as np
import pytest

ROOT = Path(__file__).resolve().parents[2]
PAGE = ROOT / "docs" / "manual" / "neuron-tutorial.md"
ODE = ROOT / "engine" / "xppaut" / "ode" / "lecar.ode"
COMMANDS_C = ROOT / "app" / "src" / "commands.c"


def blocks(language):
    text = PAGE.read_text(encoding="utf-8")
    return re.findall(r"^```%s\n(.*?)^```" % language, text, re.S | re.M)


def test_python_snippets_run(tmp_path, monkeypatch):
    matplotlib = pytest.importorskip("matplotlib")
    matplotlib.use("Agg")
    pytest.importorskip("pandas")
    shutil.copy(ODE, tmp_path / "lecar.ode")
    monkeypatch.chdir(tmp_path)
    code = blocks("python")
    assert len(code) >= 5
    ns = {}
    for k, snippet in enumerate(code):
        exec(compile(snippet, f"{PAGE.name} snippet {k + 1}", "exec"), ns)
    # the numbers the text quotes
    hb = ns["d"].special("HB")
    assert [round(p.value, 4) for p in hb] == [0.2625, 0.4569]
    assert ns["eq"].kind == "unstable focus"
    assert ns["rest"]["v"][-1] == pytest.approx(-0.375, abs=1e-3)
    folds = ns["orbits"].special("LP")
    assert [round(p.value, 4) for p in folds] == [0.2484, 0.4657]
    assert folds[0].period == pytest.approx(25.8, abs=0.1)
    stable = [p.period for p in ns["orbits"] if p.periodic and p.stable]
    assert 14 < min(stable) and max(stable) < 26
    assert [ns["fires"](v0) for v0 in (-0.24, -0.1, -0.35)] == [False, True, True]
    # step 7: the file's own values, then the set hom
    iv = ns["steady_current"]
    assert (np.diff(iv(ns["m"], ns["v"])) > 0).all()
    assert not (np.diff(iv(ns["m2"], ns["v"])) > 0).all()   # phi leaves it alone
    assert ns["rest2"].kind == "stable node"
    assert [(p.type, round(p.value, 4)) for p in ns["d2"].special()] == [
        ("LP", 0.0833), ("LP", -0.0207), ("HB", 0.2038)]
    assert ns["periods"] == pytest.approx([77, 56, 41, 27, 20], abs=1)
    scaled = ns["periods"] * np.sqrt(ns["above"])
    assert 1.7 < scaled.min() and scaled.max() < 2.05
    assert ns["period"](ns["m2"].run(iapp=0.084)) == pytest.approx(4.6, abs=0.05)
    ns["m"].close()
    ns["m2"].close()


def app_commands():
    src = COMMANDS_C.read_text(encoding="utf-8")
    return set(re.findall(r'strcmp\(cmd, "([^"]+)"\)', src))


def test_commands_exist():
    text = PAGE.read_text(encoding="utf-8")
    lines = [ln for b in blocks("commands") for ln in b.splitlines() if ln]
    lines += re.findall(r"\]\(cmd:([^)]+)\)", text)
    lines += re.findall(r"`((?:auto|nullclines|eq|use) [^`]*)`", text)
    assert lines
    known = app_commands()
    for ln in lines:
        assert ln.split()[0] in known, f"unknown app command: {ln}"


def test_page_is_linked():
    mk = (ROOT / "mkdocs.yml").read_text(encoding="utf-8")
    assert "manual/neuron-tutorial.md" in mk
    # The Help menu is drawn from the table of actions.
    menu = (ROOT / "app" / "src" / "actions.c").read_text(encoding="utf-8")
    assert '"neuron-tutorial"' in menu
