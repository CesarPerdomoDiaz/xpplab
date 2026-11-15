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
    # the model is left at the point continued from, as the page says
    assert ns["m"].get("iapp") == pytest.approx(0.2625, abs=5e-5)
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


def test_auto_orbit_page():
    text, blocks = python_blocks("auto-orbit.md")
    assert len(blocks) == 3
    ns = {}
    exec(compile(blocks[0], "auto-orbit.md example 1", "exec"), ns)
    m, point, sol = ns["m"], ns["point"], ns["sol"]
    assert point.stable and point.periodic
    assert point.value == pytest.approx(0.352, abs=5e-4)
    assert len(sol.t) == 61 and sol.t[0] == 0
    assert sol.t[-1] == pytest.approx(14.375, abs=1e-3)
    assert sol.t[-1] == pytest.approx(point.period, rel=1e-6)
    assert sol["V"].min() == pytest.approx(-0.3434, abs=1e-4)
    assert sol["V"].max() == pytest.approx(0.2760, abs=1e-4)
    steps = np.diff(sol.t)
    assert steps.max() > 2 * steps.min()             # the mesh is not even
    exec(compile(blocks[1], "auto-orbit.md the simulation", "exec"), ns)
    run = ns["run"]
    v, t = run["V"], run.t
    up = np.array([t[i - 1] + (0 - v[i - 1]) / (v[i] - v[i - 1]) * (t[i] - t[i - 1])
                   for i in range(1, len(v)) if v[i - 1] < 0 <= v[i] and t[i] > 300])
    assert np.diff(up).mean() == pytest.approx(14.3745, abs=5e-4)
    assert v[t > 300].min() == pytest.approx(-0.3434, abs=1e-4)
    assert v[t > 300].max() == pytest.approx(0.2761, abs=1e-4)

    # the unstable orbit, and what it separates
    exec(compile(blocks[2], "auto-orbit.md example 2", "exec"), ns)
    unstable = ns["unstable"]
    p5 = ns["orbits"].label(5)
    assert not p5.stable and p5.value == pytest.approx(0.2489, abs=5e-4)
    assert unstable.t[-1] == pytest.approx(29.03, abs=5e-3)
    assert 1.9 < unstable.t[-1] / 14.375 < 2.1
    m.set(iapp=p5.value)
    eq = m.equilibrium()
    centre = np.array([eq["V"], eq["W"]])
    k = int(np.argmax(unstable["V"]))
    on = np.array([unstable["V"][k], unstable["W"][k]])
    ends = {}
    for name, scale in (("inside", 0.9), ("outside", 1.1)):
        start = centre + scale * (on - centre)
        far = m.run(v=float(start[0]), w=float(start[1]), total=600, dt=0.05)
        tail = far["V"][far.t > 400]
        ends[name] = tail.max() - tail.min()
    assert ends["inside"] < 1e-3          # to rest
    assert ends["outside"] > 0.3          # to firing
    m.close()


def test_auto_save_load_and_reset_pages(tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)      # the examples write hopf.auto here
    text, blocks = python_blocks("auto-save.md")
    ns = {}
    exec(compile(blocks[0], "auto-save.md example", "exec"), ns)
    saved = tmp_path / "hopf.auto"
    assert saved.exists() and 20000 < saved.stat().st_size < 30000   # "about 25 kB"
    assert len(ns["d"]) == 49 and "(49 points)" in text
    ns["m"].close()
    with xs.load(xs.example("lecar.ode")) as m:
        with pytest.raises(xs.EngineError, match="the diagram is empty"):
            m.auto.save(tmp_path / "none.auto")

    text, blocks = python_blocks("auto-load.md")
    ns = {}
    exec(compile(blocks[0], "auto-load.md example", "exec"), ns)
    d, orbits = ns["d"], ns["orbits"]
    assert len(d) == 49 and "49 points" in text
    assert [(p.label, round(p.value, 4)) for p in d.special("HB")] == [(2, 0.2625),
                                                                       (3, 0.4569)]
    assert [(p.label, round(p.value, 4)) for p in orbits.special("LP")] == [(5, 0.2484),
                                                                            (6, 0.4657)]
    with pytest.raises(xs.EngineError, match="cannot read"):
        ns["m"].auto.load(tmp_path / "missing.auto")
    ns["m"].close()

    text, blocks = python_blocks("auto-reset.md")
    ns = {}
    exec(compile(blocks[0], "auto-reset.md example", "exec"), ns)
    m, d = ns["m"], ns["d"]
    assert len(m.auto.diagram) == 49 and len(d) == 49
    assert [p.label for p in d.special("HB")] == [2, 3]
    # without the reset: both runs, and two points labelled 2
    m.auto.reset()
    first = m.auto.equilibria("iapp", parmax=0.3)
    second = m.auto.equilibria("iapp", parmax=0.5)
    assert len(first) == 24 and len(m.auto.diagram) == 73 and "73" in text
    assert [p.label for p in second if p.label] == [1, 2, 3, 4]
    assert [p.type for p in m.auto.diagram if p.label == 2] == ["HB", "HB"]
    # a continuation from a label carries on from the highest label
    orbits = m.auto.continue_from(second.first("HB"), "periodic", nmax=50)
    assert [p.label for p in orbits if p.label][:2] == [5, 6]
    m.close()


def crossings(sol, name="V", level=0.0):
    """The times at which a column crosses a level going up."""
    v, t = sol[name], sol.t
    return np.array([t[i - 1] + (level - v[i - 1]) / (v[i] - v[i - 1]) * (t[i] - t[i - 1])
                     for i in range(1, len(v)) if v[i - 1] < level <= v[i]])


def test_run_page():
    text, blocks = python_blocks("run.md")
    assert len(blocks) == 3
    ns = {}
    exec(compile(blocks[0], "run.md example 1", "exec"), ns)
    m, sol = ns["m"], ns["sol"]
    assert len(sol.t) == 601 and sol.t[-1] == pytest.approx(30)
    assert sol["V"][0] == pytest.approx(-0.144)
    assert sol["V"].max() == pytest.approx(0.2283, abs=1e-4)
    assert sol.t[sol["V"].argmax()] == pytest.approx(4.7, abs=0.051)
    assert sol["V"][-1] == pytest.approx(-0.3825, abs=1e-4)
    assert len(crossings(sol)) == 1                      # one spike
    exec(compile(blocks[1], "run.md example 2", "exec"), ns)
    sol = ns["sol"]
    spikes = crossings(sol)
    assert len(sol.t) == 4001 and len(spikes) == 13
    assert np.diff(spikes)[-3:].mean() == pytest.approx(16.47, abs=5e-3)
    assert m.get("iapp") == pytest.approx(0.1)           # values given to run() stay set
    exec(compile(blocks[2], "run.md example 3", "exec"), ns)
    rough, broken = ns["rough"], ns["broken"]
    period = np.diff(crossings(rough))[-3:].mean()
    assert period == pytest.approx(15.83, abs=5e-3)
    assert 1 - period / 16.47 == pytest.approx(0.04, abs=5e-3)   # "4 % too fast"
    assert broken.t[-1] < 200                             # it stopped early
    assert np.diff(crossings(m.run(dt=0.05)))[-3:].mean() == pytest.approx(16.43, abs=5e-3)
    # every setting of the table is a name the model takes
    m.set(meth="rungekutta", total=30, dt=0.05, t0=0, trans=0, nout=1, bound=100,
          tol=1e-6, atol=1e-6, maxstor=5000)
    assert len(m.run().t) == 601
    m.close()


def test_eq_pages():
    text, blocks = python_blocks("eq.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "eq.md example 1", "exec"), ns)
    e = ns["e"]
    assert e.kind == "saddle"
    assert (e["V"], e["W"]) == pytest.approx((-0.142535, 0.034049), abs=1e-6)
    assert sorted(e.eigenvalues.real) == pytest.approx([-0.30101, 1.02997], abs=1e-5)
    exec(compile(blocks[1], "eq.md example 2", "exec"), ns)
    low, high = ns["low"], ns["high"]
    assert low.kind == "stable node"
    assert (low["V"], low["W"]) == pytest.approx((-0.382505, 0.001286), abs=1e-6)
    assert sorted(low.eigenvalues.real) == pytest.approx([-0.89036, -0.40276], abs=1e-5)
    assert high.kind == "unstable focus"
    assert (high["V"], high["W"]) == pytest.approx((0.02761, 0.26924), abs=1e-5)
    assert high.eigenvalues.real == pytest.approx([0.52042, 0.52042], abs=1e-5)
    assert sorted(abs(high.eigenvalues.imag)) == pytest.approx([0.7811, 0.7811], abs=1e-4)
    # a run from the initial values ends at the stable node, not the saddle
    m = ns["m"]
    m.set(v=-0.144, w=0.03)
    assert m.run()["V"][-1] == pytest.approx(low["V"], abs=1e-3)
    m.close()

    text, blocks = python_blocks("eq-all.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "eq-all.md example 1", "exec"), ns)
    found = ns["found"]
    assert len(found) == 3
    assert sorted(e.kind for e in found) == ["saddle", "stable node", "unstable focus"]
    exec(compile(blocks[1], "eq-all.md example 2", "exec"), ns)
    one = ns["one"]
    assert len(one) == 1 and one[0].kind == "unstable focus"
    assert one[0]["V"] == pytest.approx(-0.152093, abs=1e-6)
    ns["m"].close()


def test_nullclines_page():
    text, blocks = python_blocks("nullclines.md")
    assert len(blocks) == 2
    box = {"V": (-0.6, 1.2), "W": (-0.25, 1.2)}

    def distance(segments, point):
        a, b = segments[:, :2], segments[:, 2:]
        ab = b - a
        t = np.clip(((point - a) * ab).sum(1) / np.maximum((ab ** 2).sum(1), 1e-30), 0, 1)
        return np.hypot(*(a + t[:, None] * ab - point).T).min()

    ns = {}
    exec(compile(blocks[0], "nullclines.md example 1", "exec"), ns)
    m, vn, wn = ns["m"], ns["vn"], ns["wn"]
    found = m.find_equilibria(box=box)
    assert sorted(round(e["V"], 4) for e in found) == [-0.3825, -0.1425, 0.0276]
    for e in found:          # every equilibrium lies on both nullclines
        p = np.array([e["V"], e["W"]])
        assert distance(vn, p) < 1e-3 and distance(wn, p) < 1e-3, e.kind
    # above the V nullcline V decreases, below it increases
    v0 = -0.3
    w_on = vn[np.argmin(abs((vn[:, 0] + vn[:, 2]) / 2 - v0)), 1]
    assert m.rhs({"V": v0, "W": w_on + 0.05})[0] < 0 < m.rhs({"V": v0, "W": w_on - 0.05})[0]
    exec(compile(blocks[1], "nullclines.md example 2", "exec"), ns)
    one = m.find_equilibria(box=box)
    assert len(one) == 1 and one[0].kind == "unstable focus"
    assert one[0]["V"] == pytest.approx(0.0443, abs=5e-5)
    m.set(iapp=0.08)
    assert len(m.find_equilibria(box=box)) == 3
    m.close()


def test_range_page():
    text, blocks = python_blocks("range.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "range.md example 1", "exec"), ns)
    m, values, up = ns["m"], ns["values"], ns["up"]
    swing = dict(zip(np.round(values, 2), np.round(up, 3)))
    assert swing[0.24] == 0 and swing[0.47] == 0
    assert swing[0.25] == pytest.approx(0.638, abs=1e-3)
    assert swing[0.27] == pytest.approx(0.652, abs=1e-3) and max(up) == pytest.approx(0.652, abs=1e-3)
    assert swing[0.46] == pytest.approx(0.529, abs=1e-3)
    firing = [v for v, s in swing.items() if s > 0.1]
    assert min(firing) == 0.25 and max(firing) == 0.46      # bracket 0.2484 and 0.4657
    exec(compile(blocks[1], "range.md example 2", "exec"), ns)
    climb = dict(zip(np.round(values, 2), ns["climb"]))
    fall = dict(zip(np.round(values, 2), ns["fall"]))
    assert climb[0.25] < 0.05 and climb[0.26] < 0.05 and climb[0.27] > 0.6   # rests until past HB
    assert fall[0.46] < 0.05 and swing[0.46] > 0.5                            # rests where the other fired
    assert m.get("iapp") == pytest.approx(0.5)        # sweep puts the parameter back
    m.close()


def test_continue_page():
    text, blocks = python_blocks("continue.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "continue.md example 1", "exec"), ns)
    m, sol = ns["m"], ns["sol"]
    assert len(sol) == 4001 and sol.t[-1] == pytest.approx(200)
    exec(compile(blocks[1], "continue.md example 2", "exec"), ns)
    assert m.get("total") == pytest.approx(30)
    assert m.get("v") == pytest.approx(-0.144)
    first = m.run(iapp=0.1)
    assert len(first) == 601 and first.t[-1] == pytest.approx(30)
    m.set(iapp=0.1)
    one = m.run(total=200)
    diff = np.abs(sol["V"] - one["V"])
    assert diff.max() < 1e-7 and diff[:601].max() == 0     # 6e-8, none up to t = 30
    for meth, bound in (("qualrk", 2e-7), ("stiff", 5e-8)):   # More about
        m.set(meth=meth, total=30)
        s = m.continue_run(m.run(iapp=0.1), 170)
        m.set(iapp=0.1)
        assert np.abs(s["V"] - m.run(total=200)["V"]).max() < bound
    m.close()
