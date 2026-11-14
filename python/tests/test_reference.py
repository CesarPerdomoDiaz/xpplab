"""The numbers the reference pages quote (docs/reference) are computed here,
and each page's Python example is run as written, so a page cannot go out of
date silently."""
import math
import re
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs

ROOT = Path(__file__).resolve().parents[2]
REFERENCE = ROOT / "docs" / "reference"


def python_blocks(page):
    text = (REFERENCE / page).read_text(encoding="utf-8")
    return text, re.findall(r"^```python\n(.*?)^```", text, re.S | re.M)


def rows(text, heading):
    """The table rows (lists of cells) under a ### heading."""
    part = text[text.index("### " + heading) + 4:]
    part = re.split(r"^#{2,3} ", part, flags=re.M)[0]
    out = []
    for line in part.splitlines():
        if line.startswith("| ") and not set(line) <= set("|- "):
            out.append([c.strip() for c in line.strip("|").split("|")])
    return out


def number(cell):
    return float(cell.replace("−", "-"))


def test_auto_steady_page():
    text, blocks = python_blocks("auto-steady.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "auto-steady.md example 1", "exec"), ns)
    d = ns["d"]
    # the table of the first example is what AUTO gives
    table = [r for r in rows(text, "Two Hopf bifurcations in a neuron model")
             if r[0] != "Label"]
    special = d.special()
    assert [(str(p.label), p.type) for p in special] == [(r[0], r[1]) for r in table]
    for p, r in zip(special, table):
        assert p.value == pytest.approx(number(r[2]), abs=5e-5)
        assert p.u0["V"] == pytest.approx(number(r[3]), abs=5e-5)
        assert p.u0["W"] == pytest.approx(number(r[4]), abs=5e-5)
    assert len(d) == 49 and "49 points" in text
    assert d[0].value == pytest.approx(0.05)
    # stable, unstable between the Hopf points, stable again
    hb = [p.value for p in special]
    for p in list(d)[1:]:  # AUTO does not know the stability of its start
        if p.label and p.type == "HB":
            continue
        assert p.stable == (not hb[0] < p.value < hb[1]), p.value
    ns["m"].close()

    exec(compile("import xpplab as xs\n" + blocks[1], "auto-steady.md example 2",
                 "exec"), ns)
    d = ns["d"]
    table = [r for r in rows(text, "A fold, and the direction of the first step")
             if r[0] != "Label"]
    folds = d.special("LP")
    assert [(str(p.label), p.type) for p in folds] == [(r[0], r[1]) for r in table]
    exact = 2 / (3 * math.sqrt(3))
    for p, r, sign in zip(folds, table, (-1, 1)):
        assert p.value == pytest.approx(sign * exact, abs=5e-5)
        assert p.u0["X"] == pytest.approx(-sign / math.sqrt(3), abs=5e-5)
        assert number(r[2]) == pytest.approx(p.value, abs=5e-5)
        assert number(r[3]) == pytest.approx(p.u0["X"], abs=5e-5)
    assert d[0].value == pytest.approx(1.0)
    assert d[0].u0["X"] == pytest.approx(1.3247, abs=5e-5) and "1.3247" in text
    # unstable between the folds, stable outside, down to Par min
    seen = 0
    for p in list(d)[1:]:
        if p.label in (folds[0].label, folds[1].label):
            seen += 1
            continue
        assert p.stable == (seen != 1), (p.value, seen)
    assert d[len(d) - 1].value < -2
    ns["m"].close()


def test_doc_opens_the_page_of_a_command_or_a_method():
    site = "https://cesarperdomodiaz.github.io/xpplab/reference/"
    assert xs.doc("auto steady", open=False) == site + "auto-steady/"
    assert xs.doc("AUTO  Steady", open=False) == site + "auto-steady/"
    with xs.load(xs.example("lecar.ode")) as m:
        assert xs.doc(m.auto.equilibria, open=False) == site + "auto-steady/"
    assert xs.doc(open=False) == site + "reference/"
    assert xs.doc("no such thing", open=False) == site + "reference/"
    # the package knows exactly the pages that exist
    from xpplab import docs
    pages = {p.stem.replace("-", " ") for p in REFERENCE.glob("*.md")} - {"reference"}
    assert set(docs.PAGES) == pages
    assert set(docs.METHODS.values()) <= pages


def test_auto_from_page():
    text, blocks = python_blocks("auto-from.md")
    assert len(blocks) == 3
    ns = {}
    # 1. periodic orbits from the first Hopf point, in 50 steps
    exec(compile(blocks[0], "auto-from.md example 1", "exec"), ns)
    d, orbits = ns["d"], ns["orbits"]
    assert [(p.label, round(p.value, 4)) for p in d.special("HB")] == [(2, 0.2625),
                                                                       (3, 0.4569)]
    table = [r for r in rows(text, "Periodic orbits from a Hopf point") if r[0] != "Label"]
    folds = orbits.special()
    assert [(str(p.label), p.type) for p in folds] == [(r[0], r[1]) for r in table]
    for p, r in zip(folds, table):
        assert p.value == pytest.approx(number(r[2]), abs=5e-5)
        assert p.period == pytest.approx(number(r[3]), abs=5e-3)
    assert len(orbits) == 50
    lo, hi = folds[0].value, folds[1].value
    stable = [p for p in orbits if p.periodic and p.stable]
    unstable = [p for p in orbits if p.periodic and not p.stable and not p.label]
    # stable between the folds; the unstable ones lie outside the Hopf points
    assert min(p.value for p in stable) == pytest.approx(lo, abs=2e-3)
    assert max(p.value for p in stable) == pytest.approx(hi, abs=2e-3)
    assert all(p.value < 0.2625 + 1e-3 or p.value > 0.4569 - 1e-3 for p in unstable)
    periods = [p.period for p in stable]
    assert min(periods) == pytest.approx(14.1, abs=0.05) and "14.1 to 24.3" in text
    assert max(periods) == pytest.approx(24.3, abs=0.05)
    # a longer run only retraces the loop: the same two folds, again
    again = ns["m"].auto.continue_from(d.first("HB"), "periodic", nmax=150)
    values = {round(p.value, 2) for p in again.special("LP")}
    assert {0.25, 0.47} <= values <= {0.25, 0.26, 0.47}
    assert len(again.special("LP")) > 4

    # 2. the curve of Hopf points in iapp and phi
    exec(compile(blocks[1], "auto-from.md example 2", "exec"), ns)
    curve = ns["curve"]
    a = np.array([[p.value, p.value2] for p in curve])
    assert a[0] == pytest.approx([0.2625, 0.2], abs=5e-4)
    top = a[a[:, 1].argmax()]
    assert top == pytest.approx([0.3435, 1.85], abs=5e-3)
    crossings = [a[j - 1, 0] + (0.2 - a[j - 1, 1]) / (a[j, 1] - a[j - 1, 1])
                 * (a[j, 0] - a[j - 1, 0])
                 for j in range(1, len(a)) if (a[j - 1, 1] - 0.2) * (a[j, 1] - 0.2) < 0]
    assert crossings and crossings[0] == pytest.approx(0.4568, abs=5e-4)
    ns["m"].close()

    # 3. the pitchfork: one run draws both branches, and switch adds nothing
    exec(compile("import xpplab as xs\n" + blocks[2], "auto-from.md example 3",
                 "exec"), ns)
    d = ns["d"]
    bp = d.special("BP")
    assert [(p.label, round(p.value, 6) + 0.0) for p in bp] == [(2, 0.0)]
    trivial = [p for p in d if p.branch == 1]
    other = [p for p in d if p.branch == 2]
    assert all(abs(p.u0["X"]) < 1e-9 for p in trivial)
    assert min(p.value for p in trivial) == pytest.approx(-1) and \
        max(p.value for p in trivial) == pytest.approx(2, abs=0.1)
    x = np.array([p.u0["X"] for p in other])
    r = np.array([p.value for p in other])
    assert np.allclose(x ** 2, r, atol=1e-5)          # the parabola x = +-sqrt(r)
    assert x.max() == pytest.approx(1.42, abs=5e-3) and x.min() == pytest.approx(-1.42, abs=5e-3)
    assert r.max() == pytest.approx(2.02, abs=5e-3)
    # (the first points of each arm, at the branch point itself, are not judged)
    assert all(p.stable for p in other if not p.label and p.value > 0.01)
    for p in trivial[1:]:
        if not p.label:
            assert p.stable == (p.value < 0)
    switched = ns["m"].auto.continue_from(bp[0], "switch")
    assert all(abs(p.u0["X"]) < 1e-9 for p in switched)   # x = 0 again
    ns["m"].close()


def test_auto_set_page():
    text, blocks = python_blocks("auto-set.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "auto-set.md example 1", "exec"), ns)
    short = ns["short"]
    assert len(short) == 24 and "computes 24 points" in text
    assert [(p.type, round(p.value, 4)) for p in short.special()] == [("HB", 0.2625)]
    last = short[len(short) - 1]
    assert last.type == "EP" and 0.3 < last.value < 0.33
    exec(compile(blocks[1], "auto-set.md example 2", "exec"), ns)
    coarse = ns["coarse"]
    assert len(coarse) == 14 and "computes 14 points" in text
    hb = coarse.special("HB")
    assert [round(p.value, 4) for p in hb] == pytest.approx([0.2625, 0.4568], abs=1e-4)
    assert hb[1].stable          # the wrong verdict the page describes
    careful = ns["m"].auto.equilibria("iapp", dsmax=0.02)
    assert len(careful) == 49 and not careful.special("HB")[1].stable
    ns["m"].close()
