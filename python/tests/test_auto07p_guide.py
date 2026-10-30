"""The "Coming from AUTO-07p" page in docs/manual must keep working.

The model file in the page is written out, both Python snippets run in one
namespace, the numbers the text quotes are checked against those runs (and
against the formulas the page gives), every name the table gives exists, and
every command the page tells the reader to type is one the desktop app has.
"""
import os
import re
import subprocess
from pathlib import Path

import numpy as np
import pytest

from xpplab.auto import SETTINGS

ROOT = Path(__file__).resolve().parents[2]
PAGE = ROOT / "docs" / "manual" / "auto07p.md"


def blocks(language):
    text = PAGE.read_text(encoding="utf-8")
    return re.findall(r"^```%s\n(.*?)^```" % language, text, re.S | re.M)


@pytest.fixture(scope="module")
def ns(tmp_path_factory):
    matplotlib = pytest.importorskip("matplotlib")
    matplotlib.use("Agg")
    folder = tmp_path_factory.mktemp("auto07p")
    (ode,) = blocks("ode")
    (folder / "switch.ode").write_text(ode, encoding="utf-8")
    code = blocks("python")
    assert len(code) == 2
    space = {}
    here = os.getcwd()
    os.chdir(folder)
    try:
        for k, snippet in enumerate(code):
            exec(compile(snippet, f"{PAGE.name} snippet {k + 1}", "exec"), space)
    finally:
        os.chdir(here)
    yield space
    space["m"].close()


def test_steady_branch(ns):
    d = ns["d"]
    assert len(d) == 193
    assert [(p.label, p.type) for p in d.labelled()] == [
        (1, "EP"), (2, "HB"), (3, "LP"), (4, "LP"), (5, "HB"), (6, "EP")]
    assert repr(d).endswith("special: HB#2, LP#3, LP#4, HB#5>")
    assert {p.branch for p in d} == {1}
    # the formulas in the text: folds at 2/(3 sqrt 3), Hopf at x^2 = 5/6
    fold = 2 / (3 * np.sqrt(3))
    hopf_lam = np.sqrt(5 / 6) * (5 / 6 - 1)
    assert [p.value for p in d.special("LP")] == pytest.approx(
        [fold, -fold], abs=1e-6)
    hb = ns["hb"]
    assert hb.type == "HB" and hb.label == 2
    assert hb.value == pytest.approx(-hopf_lam, abs=1e-4)
    assert round(hb.value, 5) == 0.15211
    assert d.special("HB")[1].value == pytest.approx(hopf_lam, abs=1e-4)
    assert hb.u0["X"] == pytest.approx(-np.sqrt(5 / 6), abs=1e-4)
    lp = ns["lp"]
    assert lp.label == 3 and round(lp.value, 4) == 0.3849
    ev = sorted(hb.eigenvalues, key=lambda z: z.imag)
    assert ev == pytest.approx([-0.866025j, 0.866025j], abs=1e-4)
    # the start: Newton moved (-1, -0.5) onto the equilibrium at x = -1.52
    assert d[0].u0["X"] == pytest.approx(-1.5214, abs=1e-3)
    assert d[0].value == -2.0


def test_periodic_branch_and_files(ns):
    orb, sol = ns["orb"], ns["sol"]
    assert len(orb) == 30
    assert orb[0].period == pytest.approx(2 * np.pi / 0.866025, abs=1e-3)
    assert round(orb[0].period, 4) == 7.2553
    assert round(orb[-1].period, 3) == 25.648
    assert orb[-1].period > orb[10].period > orb[0].period
    assert orb[-1].label == 7 and orb[-1].type == "EP"
    assert all(p.periodic and not p.stable for p in orb)
    assert {p.branch for p in orb} == {-2}
    # the Hopf point is subcritical: the orbits go towards smaller lam
    assert orb[-1].value < orb[0].value < ns["lp"].value
    assert sol.t[-1] == pytest.approx(orb[-1].period, rel=1e-6)
    assert len(ns["d2"]) == 223
    assert [(p.label, p.type) for p in ns["d2"].labelled()][-1] == (7, "EP")
    fold = ns["fold"]
    assert len(fold) == 30
    assert fold[-1].label == 8
    assert (round(fold[-1].value, 3), round(fold[-1].value2, 3)) == (1.144, 4.068)
    assert fold[0].parameter2 == "a"


def test_save_and_load_give_the_same_points(ns, tmp_path):
    m = ns["m"]
    before = [(p.label, p.type, p.value, p.branch) for p in m.auto.diagram]
    m.auto.save(tmp_path / "again.auto")
    m.auto.reset()
    again = m.auto.load(tmp_path / "again.auto")
    assert len(before) == 253
    assert [(p.label, p.type, p.value, p.branch) for p in again] == before


def test_defaults_of_the_settings():
    (ode,) = blocks("ode")
    with _load(ode) as m:
        got = m.auto.settings
    for name, value in {"ntst": 15, "nmax": 200, "npr": 50, "ds": 0.02,
                        "dsmin": 0.001, "dsmax": 0.5, "parmin": 0,
                        "parmax": 2}.items():
        assert got[name] == value, name


def _load(ode):
    import tempfile
    import xpplab as xs

    class Ctx:
        def __enter__(self):
            self.dir = tempfile.TemporaryDirectory()
            path = Path(self.dir.name) / "switch.ode"
            path.write_text(ode, encoding="utf-8")
            self.m = xs.load(path)
            return self.m

        def __exit__(self, *exc):
            self.m.close()
            self.dir.cleanup()
    return Ctx()


def table_rows():
    return [ln for ln in PAGE.read_text(encoding="utf-8").splitlines()
            if ln.startswith("| ")][2:]


def test_names_in_the_table_exist(ns):
    """Each `m.auto.x`, `d.x`, `hb.x` in the table is a real attribute, and
    each keyword given to `set` is an AUTO setting."""
    rows = table_rows()
    assert len(rows) >= 20
    owners = {"m.auto": ns["m"].auto, "m": ns["m"], "d": ns["d"],
              "p": ns["hb"], "hb": ns["hb"], "pt": ns["hb"]}
    checked = set()
    for row in rows:
        for code in re.findall(r"`([^`]*)`", row):
            for owner, name in re.findall(r"\b(m\.auto|m|d|p|hb|pt)\.([a-z_]+)", code):
                assert hasattr(owners[owner], name), f"{owner}.{name} in: {row}"
                checked.add(f"{owner}.{name}")
            if code.startswith("m.auto.set("):
                for kw in re.findall(r"([a-z]+)=", code):
                    assert kw in SETTINGS, f"{kw} in: {row}"
    assert {"m.auto.equilibria", "m.auto.continue_from", "m.auto.start",
            "m.auto.set", "m.auto.save", "m.auto.load", "m.auto.reset",
            "m.auto.orbit", "m.auto.diagram", "d.special", "d.first",
            "d.label", "p.type"} <= checked


def test_actions_and_types_in_the_table_are_accepted(ns):
    """The action strings given to continue_from and start are real ones."""
    from xpplab.auto import _ACTIONS, TYPE_NAMES
    text = "\n".join(table_rows())
    for kind in re.findall(r'start\("(\w+)"', text):
        assert kind in _ACTIONS["start"]
    actions = set(re.findall(r'continue_from\(\w+(?:, \d+)?, "([\w-]+)"', text))
    assert actions == {"periodic", "two-parameter", "switch"}
    allowed = {a for k, v in _ACTIONS.items() if k != "start" for a in v}
    assert actions <= allowed
    for label in ("LP", "HB", "BP", "PD", "TR", "EP", "MX"):
        assert label in TYPE_NAMES


def batch_exe():
    """The built xpplab-batch, if the engine library sits next to it."""
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
    (tmp_path / "switch.ode").write_text(ode, encoding="utf-8")
    (cmds,) = blocks("commands")
    args = [str(exe), "switch.ode", "--echo"]
    for ln in cmds.splitlines():
        if ln:
            args += ["-c", ln]
    done = subprocess.run(args, cwd=tmp_path, capture_output=True, text=True,
                          timeout=180)
    assert done.returncode == 0, done.stdout + done.stderr
    got = [ln for ln in done.stdout.splitlines() if ln.startswith(">>> ")]
    # the first two lines load the model and run it once; not from the page
    assert got[0].startswith(">>> m = xs.load(")
    assert got[1] == ">>> sol = m.run()"
    (echo,) = blocks("text")
    assert got[2:] == [ln for ln in echo.splitlines() if ln]
    # the app's run gives the points the page names
    assert "HB #2 at lam = 0.152112" in done.stdout
    assert "LP #3 at lam = 0.3849" in done.stdout
    assert "AUTO computed 193 points." in done.stdout
    assert "The periodic orbit at point 7 is now the run" in done.stdout


def test_echoed_lines_are_calls_of_the_package():
    """Each echoed line runs in the package, as it is written."""
    (echo,) = blocks("text")
    (ode,) = blocks("ode")
    with _load(ode) as m:
        for ln in echo.splitlines():
            assert ln.startswith(">>> ")
            exec(compile(ln[4:], "echo", "exec"), {"m": m, "sol": None})
        d = m.auto.diagram
        assert [(p.label, p.type) for p in d.labelled()][-1] == (7, "EP")
        assert [p.type for p in d.special()][:4] == ["HB", "LP", "LP", "HB"]


def test_suppbp_keeps_stability_and_the_steady_branch():
    assert "suppbp" in SETTINGS
    (ode,) = blocks("ode")
    runs = {}
    for sup in (0, 1):
        with _load(ode) as m:
            m.auto.set(parmin=-2, parmax=2, dsmax=0.05, nmax=500, npr=500,
                       suppbp=sup)
            d = m.auto.equilibria("lam")
            orb = m.auto.continue_from(d.first("HB"), "periodic", nmax=60)
            runs[sup] = (len(d), [p.stable for p in d],
                         [(p.label, p.type) for p in d.labelled()],
                         len(orb), [p.stable for p in orb])
    assert runs[0] == runs[1]
    assert len(runs[0][4]) == 60 and not any(runs[0][4])


def test_save_load_and_fold_commands_in_the_app(tmp_path):
    """The commands the prose names run in xpplab-batch without an error."""
    exe = batch_exe()
    if exe is None:
        pytest.skip("xpplab-batch is not built with its engine library")
    (ode,) = blocks("ode")
    (tmp_path / "switch.ode").write_text(ode, encoding="utf-8")
    cmds = ["auto set parmin=-2 parmax=2 dsmax=0.05 nmax=500 npr=500",
            "auto steady lam", "auto save switch.auto", "auto reset",
            "auto load switch.auto", "auto set nmax=30",
            "auto from 3 two a"]
    args = [str(exe), "switch.ode"]
    for ln in cmds:
        args += ["-c", ln]
    done = subprocess.run(args, cwd=tmp_path, capture_output=True, text=True,
                          timeout=180)
    assert done.returncode == 0, done.stdout + done.stderr
    assert "error" not in done.stdout.lower(), done.stdout
    assert "Loaded a bifurcation diagram of 193 points" in done.stdout
    assert "continuing from LP #3" in done.stdout
    assert (tmp_path / "switch.auto").exists()


def test_page_is_in_the_manual():
    mk = (ROOT / "mkdocs.yml").read_text(encoding="utf-8")
    assert "manual/auto07p.md" in mk
    assert (ROOT / "docs" / "manual" / "auto07p.md").read_text(
        encoding="utf-8").startswith("# Coming from AUTO-07p")
    assert "(auto07p.md)" in (ROOT / "docs" / "manual" / "welcome.md").read_text(
        encoding="utf-8")
    roadmap = (ROOT / "ROADMAP.md").read_text(encoding="utf-8")
    assert re.search(r'^- \[x\] "Coming from AUTO-07p"', roadmap, re.M)
