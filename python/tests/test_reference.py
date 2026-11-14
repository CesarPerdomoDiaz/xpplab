"""The numbers the reference pages quote (docs/reference) are computed here,
and each page's Python example is run as written, so a page cannot go out of
date silently."""
import math
import re
from pathlib import Path

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
