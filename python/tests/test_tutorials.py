"""The tutorial pages in docs/manual must keep working.

Every Python snippet runs, in order, in one namespace; every command the
page tells the reader to type is one the desktop app has.
"""
import re
import shutil
from pathlib import Path

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
    assert len(ns["orbits"].special("LP")) > 0
    ns["m"].close()


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
    menu = (ROOT / "app" / "src" / "ui_menu.c").read_text(encoding="utf-8")
    assert '"neuron-tutorial"' in menu
