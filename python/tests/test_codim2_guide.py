"""The "Codimension-two bifurcations" page in docs/manual must keep working.

The model in the page is written out, its Python snippets run in one
namespace, the numbers the text quotes are checked against those runs and
against the formulas of the page, the app's commands run in xpplab-batch and
their echo is what the page shows (and gives the same curves in Python).
"""
import os
import re
import subprocess
from pathlib import Path

import numpy as np
import pytest

from xpplab.auto import TYPE_NAMES

ROOT = Path(__file__).resolve().parents[2]
PAGE = ROOT / "docs" / "manual" / "codim2.md"
SETTINGS = dict(parmin=-2, parmax=2, dsmax=0.05, nmax=500, npr=500)


def blocks(language):
    text = PAGE.read_text(encoding="utf-8")
    return re.findall(r"^```%s\n(.*?)^```" % language, text, re.S | re.M)


@pytest.fixture(scope="module")
def ns(tmp_path_factory):
    matplotlib = pytest.importorskip("matplotlib")
    matplotlib.use("Agg")
    folder = tmp_path_factory.mktemp("codim2")
    (ode,) = blocks("ode")
    (folder / "codim2.ode").write_text(ode, encoding="utf-8")
    code = blocks("python")
    assert len(code) == 5
    space = {}
    here = os.getcwd()
    os.chdir(folder)
    try:
        for k, snippet in enumerate(code):
            exec(compile(snippet, f"{PAGE.name} snippet {k + 1}", "exec"), space)
    finally:
        os.chdir(here)
    space["folder"] = folder
    yield space
    space["m"].close()


def curve(c):
    return np.array([(p.value, p.value2) for p in c])


def test_one_parameter_branch(ns):
    d = ns["d"]
    assert [(p.label, p.type) for p in d.labelled()] == [
        (1, "EP"), (2, "HB"), (3, "LP"), (4, "LP"), (5, "HB"), (6, "EP")]
    # b = 1/2: lam = x^3 - x, folds at 2/(3 sqrt 3), Hopf where x^2 = 5/6
    fold = 2 / (3 * np.sqrt(3))
    hopf = np.sqrt(5 / 6) * (1 - 5 / 6)
    assert d.first("LP").value == pytest.approx(fold, abs=1e-6)
    assert round(d.first("LP").value, 4) == 0.3849
    assert d.first("HB").value == pytest.approx(hopf, abs=1e-4)
    assert round(d.first("HB").value, 4) == 0.1521
    ev = sorted(d.first("HB").eigenvalues, key=lambda z: z.imag)
    assert ev == pytest.approx([-0.866025j, 0.866025j], abs=1e-4)


def test_scans_at_other_values_of_the_second_parameter():
    import tempfile
    import xpplab as xs

    (ode,) = blocks("ode")
    found = {}
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "codim2.ode"
        path.write_text(ode, encoding="utf-8")
        for b in (0.2, 0.5, 1.5):
            m = xs.load(path)
            m.set(b=b)
            m.auto.set(**SETTINGS)
            found[b] = [(p.type, p.value) for p in m.auto.equilibria("lam").labelled()]
            m.close()
    types = {b: [t for t, _ in v] for b, v in found.items()}
    assert types[0.2] == ["EP", "EP"]
    assert types[0.5] == ["EP", "HB", "LP", "LP", "HB", "EP"]
    assert types[1.5] == ["EP", "LP", "LP", "EP"]
    assert [round(v, 4) for t, v in found[1.5] if t == "LP"] == [1.3719, -1.3719]
    # lam = 2 (1 - 1/(3b))^(3/2) on the folds
    assert found[1.5][1][1] == pytest.approx(2 * (1 - 1 / 4.5) ** 1.5, abs=1e-4)
    assert [round(v, 4) for t, v in found[0.2] if t == "EP"] == [-2.0, 2.0177]


def test_labels_along_the_curves(ns):
    ends = [(7, ns["fold_up"]), (8, ns["fold_down"]), (9, ns["hopf_up"]),
            (10, ns["hopf_down"])]
    for label, c in ends:
        # only the end point is labelled: no LP, HB, cusp or BT on a curve
        assert [(p.label, p.type) for p in c.labelled()] == [(label, "EP")]
        assert {p.type for p in c} == {"", "EP"}
        assert c[0].parameter == "lam" and c[0].parameter2 == "b"
    assert {p.type for p in ns["m"].auto.diagram} <= {"", "EP", "HB", "LP"}
    assert not {"CP", "BT", "ZH", "GH", "CH"} & set(TYPE_NAMES)


def test_fold_and_hopf_curves_follow_the_formulas(ns):
    fu, fd = curve(ns["fold_up"]), curve(ns["fold_down"])
    # fold: lam = -2 x^3 with x^2 = 1 - 1/(3b), x < 0 for lam > 0
    b = fu[:, 1]
    assert fu[:, 0] == pytest.approx(2 * (1 - 1 / (3 * b)) ** 1.5, abs=2e-3)
    assert np.all(np.diff(b) > 0) and b[-1] > 3
    # hopf: x^2 = 1 - b/3, lam = x^3 - (3 - 1/b) x
    for c, k in ((ns["hopf_up"], 1), (ns["hopf_down"], 1)):
        h = curve(c)
        x = -np.sqrt(1 - h[:, 1] / 3)
        assert h[:, 0] == pytest.approx(x ** 3 - (3 - 1 / h[:, 1]) * x, abs=2e-3)
    assert fd[0] == pytest.approx(fu[0], abs=1e-6)


def test_codimension_two_points(ns):
    fu, fd = curve(ns["fold_up"]), curve(ns["fold_down"])
    # cusp at (0, 1/3): the fold curve turns there and goes on to lam < 0
    cusp = fd[fd[:, 1].argmin()]
    assert (round(cusp[0], 3) + 0.0, round(cusp[1], 3)) == (0.0, 0.333)
    assert cusp == pytest.approx([0, 1 / 3], abs=5e-4)
    assert fd[0, 0] > 0 > fd[-1, 0]
    # Bogdanov-Takens at b = 1, lam = 2 (2/3)^(3/2)
    bt = 2 * (2 / 3) ** 1.5
    assert round(bt, 4) == 1.0887
    lam_fold = np.interp(1.0, fu[:, 1], fu[:, 0])
    assert round(lam_fold, 3) == 1.089
    assert lam_fold == pytest.approx(bt, abs=5e-4)
    last = ns["hopf_up"][-1]
    assert (round(last.value, 4), round(last.value2, 4)) == (1.0877, 0.9988)
    assert last.value2 < 1 and last.value < bt
    # the two Hopf curves cross at lam = 0, b = 2 sqrt 3 - 3
    hd = curve(ns["hopf_down"])[::-1]
    b_cross = np.interp(0.0, hd[:, 0], hd[:, 1])
    assert round(b_cross, 3) == 0.464
    assert b_cross == pytest.approx(2 * np.sqrt(3) - 3, abs=1e-3)
    # the run stopped at the limit of lam, not at nmax
    end = ns["hopf_down"][-1]
    assert len(ns["hopf_down"]) == 63
    assert (round(end.value, 3), round(end.value2, 3)) == (-2.002, 0.240)


def test_hopf_curve_does_not_pass_the_bt_point():
    import tempfile
    import xpplab as xs

    (ode,) = blocks("ode")
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "codim2.ode"
        path.write_text(ode, encoding="utf-8")
        m = xs.load(path)
        m.auto.set(**SETTINGS)
        m.auto.equilibria("lam")
        m.auto.set(ds=0.02, nmax=3000)
        h = m.auto.continue_from(2, "two-parameter", second="b")
        m.close()
    b = np.array([p.value2 for p in h])
    assert len(h) == 3000 and b.max() < 1
    assert [p.type for p in h.labelled()][-1] == "EP"
    assert {p.type for p in h.special()} == set()


def test_second_hopf_curve_is_the_mirror_image(ns):
    """The figure draws the curves from HB #5 as mirror images in lam."""
    m = ns["m"]
    m.auto.set(ds=0.02, nmax=100)
    mirror = curve(m.auto.continue_from(5, "two-parameter", second="b"))
    m.auto.set(ds=-0.02, nmax=100)
    own = curve(m.auto.continue_from(2, "two-parameter", second="b"))
    # same b, opposite lam: the points of the two curves match in pairs
    for b, lam in own[::10]:
        j = np.abs(mirror[:, 1] - b).argmin()
        if abs(mirror[j, 1] - b) < 5e-3:
            assert mirror[j, 0] == pytest.approx(-lam, abs=3e-3)


def test_figure_is_written(ns):
    png = ns["folder"] / "codim2.png"
    assert png.stat().st_size > 5000
    assert png.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"


def batch_exe():
    for name in ("xpplab-batch.exe", "xpplab-batch"):
        exe = ROOT / "build" / "app" / name
        if exe.exists() and any((exe.parent / lib).exists() for lib in (
                "libxppe.dll", "libxppe.dylib", "libxppe.so")):
            return exe
    return None


def test_echoed_lines_are_what_the_app_prints(tmp_path):
    exe = batch_exe()
    if exe is None:
        pytest.skip("xpplab-batch is not built with its engine library")
    (ode,) = blocks("ode")
    (tmp_path / "codim2.ode").write_text(ode, encoding="utf-8")
    (cmds,) = blocks("commands")
    args = [str(exe), "codim2.ode", "--echo"]
    for ln in cmds.splitlines():
        if ln:
            args += ["-c", ln]
    done = subprocess.run(args, cwd=tmp_path, capture_output=True, text=True,
                          timeout=300)
    assert done.returncode == 0, done.stdout + done.stderr
    assert "error" not in done.stdout.lower(), done.stdout
    got = [ln for ln in done.stdout.splitlines() if ln.startswith(">>> ")]
    # the first two lines load the model and run it once; not from the page
    assert got[0].startswith(">>> m = xs.load(")
    assert got[1] == ">>> sol = m.run()"
    (echo,) = blocks("text")
    assert got[2:] == [ln for ln in echo.splitlines() if ln]
    assert "HB #2 at lam = 0.152112" in done.stdout
    assert "LP #3 at lam = 0.3849" in done.stdout
    assert "continuing from LP #3" in done.stdout
    assert "continuing from HB #2" in done.stdout


def test_echoed_lines_give_the_curves_of_the_page(ns):
    """The echo, run with the package, gives the curves the page's Python
    gives, point for point."""
    import tempfile
    import xpplab as xs

    (ode,) = blocks("ode")
    (echo,) = blocks("text")
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "codim2.ode"
        path.write_text(ode, encoding="utf-8")
        m = xs.load(path)
        curves = []
        for ln in echo.splitlines():
            assert ln.startswith(">>> ")
            result = eval(compile(ln[4:], "echo", "eval"), {"m": m})
            if "continue_from" in ln:
                curves.append(result)
        m.close()
    page = [ns[k] for k in ("fold_up", "fold_down", "hopf_up", "hopf_down")]
    assert len(curves) == 4
    for got, want in zip(curves, page):
        assert len(got) == len(want) > 50
        assert [(p.value, p.value2) for p in got] == [
            (p.value, p.value2) for p in want]


def test_page_is_in_the_manual():
    mk = (ROOT / "mkdocs.yml").read_text(encoding="utf-8")
    assert "manual/codim2.md" in mk
    assert PAGE.read_text(encoding="utf-8").startswith(
        "# Codimension-two bifurcations")
    for name in ("welcome.md", "auto.md"):
        text = (ROOT / "docs" / "manual" / name).read_text(encoding="utf-8")
        assert "(codim2.md)" in text, name
    roadmap = (ROOT / "ROADMAP.md").read_text(encoding="utf-8")
    assert re.search(r"^- \[x\] Codimension-two bifurcations through AUTO",
                     roadmap, re.M)
