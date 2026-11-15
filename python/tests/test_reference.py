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


def peaks(run):
    v = run["V"]
    i = np.where((v[1:-1] > v[:-2]) & (v[1:-1] > v[2:]) & (v[1:-1] > 0.1))[0] + 1
    return run.t[i], v[i]


def test_ic_last_page():
    text, blocks = python_blocks("ic-last.md")
    assert len(blocks) == 1
    ns = {}
    exec(compile(blocks[0], "ic-last.md example", "exec"), ns)
    m, first, second = ns["m"], ns["first"], ns["second"]
    assert first["V"][0] == pytest.approx(-0.144)
    assert first["V"][-1] == pytest.approx(-0.2384, abs=1e-4)
    assert first["W"][-1] == pytest.approx(0.00706, abs=1e-5)
    assert second["V"][0] == first["V"][-1] and second.t[0] == 0
    assert m.get("V") == pytest.approx(-0.23842227676, abs=1e-11)   # full precision
    assert float(np.float32(m.get("V"))) == pytest.approx(-0.2384222746, abs=1e-10)
    t1, v1 = peaks(first)
    t2, v2 = peaks(second)
    assert list(np.round(t1, 2)) == [2.05, 18.5] and list(np.round(t2, 2)) == [4.95, 21.45]
    assert round(v1[0], 4) == 0.2553 and round(v1[1], 4) == 0.2615
    assert round(v2[0], 4) == 0.2613 and round(v2[1], 4) == 0.2615
    m.close()


def test_ic_shift_page():
    text, blocks = python_blocks("ic-shift.md")
    assert len(blocks) == 1
    ns = {}
    exec(compile(blocks[0], "ic-shift.md example", "exec"), ns)
    m, first, second = ns["m"], ns["first"], ns["second"]
    assert first.t[-1] == pytest.approx(9.9852, abs=1e-4)
    assert m.get("T0") == pytest.approx(9.9852, abs=1e-4)
    assert second.t[0] == pytest.approx(9.9852, abs=1e-4)
    assert second.t[-1] == pytest.approx(19.9704, abs=1e-4)
    assert second["x"][-1] == pytest.approx(-0.8317, abs=1e-4)
    one = xs.load(xs.example("forcpend.ode"))
    one.set(total=20, trans=0)
    whole = one.run()
    assert np.array_equal(second["x"], whole["x"][len(first) - 1:])   # to the last digit
    other = xs.load(xs.example("forcpend.ode"))
    other.set(total=10, trans=0)
    a = other.run()
    other.ic_last()
    b = other.run()
    assert b.t[0] == 0 and b["x"][-1] == pytest.approx(-7.546, abs=1e-3)
    gap = np.abs(b["x"] - whole["x"][len(a) - 1:]).max()
    assert gap == pytest.approx(6.7, abs=0.05) and gap > 2 * np.pi
    for x in (m, one, other):
        x.close()


def test_whos_page():
    text, blocks = python_blocks("whos.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "whos.md example 1", "exec"), ns)
    m = ns["m"]
    assert m.variables == ["V", "W"] and len(m.parameters) == 12
    assert dict(m.initial_conditions) == pytest.approx({"V": -0.144, "W": 0.03})
    assert m.options["METH"] == "rungekutta" and m.options["TOTAL"] == 30
    assert "Output columns: 7" in text and "cur`, `ica`, `ik`, `rate`" in text
    exec(compile(blocks[1], "whos.md example 2", "exec"), ns)
    p = dict(m.parameters)
    assert p["iapp"] == pytest.approx(0.1)
    assert (p["phi"], p["v3"], p["v4"], p["gca"]) == pytest.approx((0.2, 0, 0.3, 1.1))
    m.close()


def test_ic_write_page(tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    text, blocks = python_blocks("ic-write.md")
    assert len(blocks) == 1
    ns = {}
    exec(compile(blocks[0], "ic-write.md example", "exec"), ns)
    m, path = ns["m"], ns["path"]
    lines = path.read_text().split()
    assert len(lines) == 2 and float(lines[0]) == m.get("V") and float(lines[1]) == m.get("W")
    assert lines[0].startswith("-0.23842227676")           # Python: full precision
    # The app's file, from the stored run: the numbers the page shows.
    stored = [float(np.float32(m.get(v))) for v in ("V", "W")]
    assert repr(stored[0]) == "-0.23842227458953857"
    assert repr(stored[1]) == "0.007057134062051773"
    m.close()


def test_ic_read_page(tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    text, blocks = python_blocks("ic-read.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "ic-read.md example 1", "exec"), ns)
    m = ns["m"]
    saved = [float(x) for x in (tmp_path / "firing.txt").read_text().split()]
    assert [m.get("V"), m.get("W")] == saved                 # to the last digit
    assert m.get("V") == pytest.approx(-0.23842227676, abs=1e-11)
    assert float(np.float32(m.get("V"))) == pytest.approx(-0.2384222746, abs=1e-10)  # the app's
    assert float(np.float32(m.get("W"))) == pytest.approx(0.007057134062, abs=1e-12)
    exec(compile(blocks[1], "ic-read.md example 2", "exec"), ns)
    assert dict(m.ics) == {"V": -0.3, "W": 0.01}
    (tmp_path / "short.txt").write_text("0.5\n")
    with pytest.raises(xs.EngineError, match="holds 1 number; the model has 2 variables"):
        m.read_ics("short.txt")
    with pytest.raises(xs.EngineError, match="cannot open"):
        m.read_ics("none.txt")
    (tmp_path / "head.txt").write_text("V W\n1 2\n")
    with pytest.raises(xs.EngineError, match="holds 0 numbers"):
        m.read_ics("head.txt")
    (tmp_path / "more.txt").write_text("1;2;3\n")
    m.read_ics("more.txt")
    assert dict(m.ics) == {"V": 1, "W": 2}                   # the rest ignored
    m.close()


def test_use_page():
    text, blocks = python_blocks("use.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "use.md example 1", "exec"), ns)
    m = ns["m"]
    snic = sorted((e.kind, round(e["V"], 4)) for e in ns["snic"])
    assert snic == [("saddle", -0.1425), ("stable node", -0.3825), ("unstable focus", 0.0276)]
    assert [(e.kind, round(e["V"], 4)) for e in ns["hopf"]] == [("stable focus", -0.4417)]
    assert (m.get("v3"), m.get("v4"), m.get("gca"), m.get("phi")) == pytest.approx((0, 0.3, 1.1, 0.2))
    fresh = xs.load(xs.example("lecar.ode"))
    own = [fresh.get(p) for p in ("v3", "v4", "gca", "phi")]
    fresh.apply_set("snic")
    assert [fresh.get(p) for p in ("v3", "v4", "gca", "phi")] == own   # the model's own values
    with pytest.raises(xs.EngineError, match="no set named nope"):
        fresh.apply_set("nope")
    fresh.close()
    exec(compile(blocks[1], "use.md example 2", "exec"), ns)
    assert m.get("total") == 100 and m.get("iapp") == pytest.approx(0.1)
    m.close()


def test_sets_page():
    text, blocks = python_blocks("sets.md")
    ns = {}
    exec(compile(blocks[0], "sets.md example", "exec"), ns)
    m = ns["m"]
    assert m.sets == ["vvst", "pp", "snic", "hopf", "hom", "cool"]
    m.apply_set("cool")
    cool = [m.get(p) for p in ("v3", "v4", "gca", "phi")]
    assert m.get("iapp") == pytest.approx(0.080918)
    m.apply_set("hom")
    assert [m.get(p) for p in ("v3", "v4", "gca", "phi")] == cool     # cool is hom with a current
    m.close()


def test_dirfield_page():
    text, blocks = python_blocks("dirfield.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "dirfield.md example 1", "exec"), ns)
    m, X, Y, U, V = (ns[k] for k in ("m", "X", "Y", "U", "V"))
    assert X.shape == (21, 21)
    assert np.round(m.rhs({"V": 1.0, "W": 0.2}), 2).tolist() == [-1.38, 2.97]
    assert np.round(m.rhs({"V": -0.5, "W": 0.5}), 3).tolist() == [-0.148, -0.669]
    # anticlockwise: right side up, top left, left side down, bottom right
    assert m.rhs({"V": 1.0, "W": 0.2})[1] > 0 and m.rhs({"V": 0.2, "W": 1.0})[0] < 0
    assert m.rhs({"V": -0.5, "W": 0.5})[1] < 0 and m.rhs({"V": 0.2, "W": 0.0})[0] > 0
    exec(compile(blocks[1], "dirfield.md example 2", "exec"), ns)
    assert np.abs(m.rhs({"V": -0.3825, "W": 0.0013})).max() < 1e-3
    assert round(np.abs(U).max(), 2) == 5.56 and round(np.abs(V).max(), 2) == 9.24
    assert X[np.unravel_index(np.abs(U).argmax(), U.shape)] == pytest.approx(1.2)
    assert X[np.unravel_index(np.abs(V).argmax(), V.shape)] == pytest.approx(1.2)
    assert m.vector_field("V", "W", (0, 1), (0, 1))[0].shape == (20, 20)   # n's default
    m.close()


def test_click_page():
    text, blocks = python_blocks("click.md")
    assert len(blocks) == 1
    ns = {}
    exec(compile(blocks[0], "click.md example", "exec"), ns)
    m, below, above = ns["m"], ns["below"], ns["above"]
    assert round(below["V"].max(), 4) == -0.1644 and round(above["V"].max(), 4) == 0.2371
    assert round(below["V"][-1], 4) == round(above["V"][-1], 4) == -0.3825   # both rest
    assert (m.get("V"), m.get("W")) == pytest.approx((-0.16, 0.0013))       # the values stay
    lo, hi = -0.2, -0.15                                                     # the threshold
    for _ in range(20):
        mid = (lo + hi) / 2
        lo, hi = (lo, mid) if m.run(V=mid, W=0.0013)["V"].max() > 0 else (mid, hi)
    assert round(lo, 4) == -0.1682
    m.close()


def test_erase_page():
    text, blocks = python_blocks("erase.md")
    ns = {}
    exec(compile(blocks[0], "erase.md example", "exec"), ns)
    assert len(ns["runs"]) == 2 and len(ns["found"]) == 3
    ns["m"].close()


def test_freeze_page():
    text, blocks = python_blocks("freeze.md")
    ns = {}
    exec(compile(blocks[0], "freeze.md example", "exec"), ns)
    rest, firing = ns["runs"]
    assert (round(rest["V"].min(), 4), round(rest["V"].max(), 4)) == (-0.4306, 0.2283)
    assert round(rest["V"][-1], 4) == -0.3825 and len(peaks(rest)[0]) == 1
    assert round(firing["V"].max(), 4) == 0.2615 and len(peaks(firing)[0]) == 2
    ns["m"].close()


def test_mean_page():
    text, blocks = python_blocks("mean.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "mean.md example 1", "exec"), ns)
    m = ns["m"]
    mean, sd = m.mean("V")
    assert (round(mean, 6), round(sd, 6)) == (-0.184298, 0.181866)
    assert round(m.lyapunov(), 6) == 0.009812
    run = m.run()
    assert sd == pytest.approx(run["V"].std(ddof=0), rel=1e-12)       # a population
    m.set(total=2000)
    m.run()
    assert round(m.lyapunov(), 4) == 0.0022
    m.set(iapp=0.05, total=30)
    m.run()
    assert [round(x, 6) for x in m.mean("V")] == [-0.299913, 0.172776]
    assert round(m.lyapunov(), 4) == -0.1755
    exec(compile(blocks[1], "mean.md example 2", "exec"), ns)
    lorenz = ns["lorenz"]
    assert [round(x, 5) for x in lorenz.mean("X")] == [-2.29813, 7.41067]
    assert round(lorenz.lyapunov(), 4) == 0.8667
    for total, value in ((400, 0.8707), (2000, 0.8777)):
        lorenz.set(total=total, maxstor=200000)
        lorenz.run()
        assert round(lorenz.lyapunov(), 4) == value
    m.close()
    lorenz.close()


def test_hist_page():
    text, blocks = python_blocks("hist.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "hist.md example 1", "exec"), ns)
    m, h = ns["m"], ns["h"]
    assert len(h) == 11 and h.columns == ["bin", "count"]
    assert h["count"].tolist() == [1020, 858, 750, 349, 173, 138, 144, 142, 151, 275, 1]
    assert round(h["bin"][0], 4) == -0.3804 and round(h["bin"][-1], 4) == 0.2615
    assert round(h["bin"][1] - h["bin"][0], 4) == 0.0642
    run = m.last_run()
    assert len(run) == 4001
    assert np.histogram(run["V"], bins=10)[0][-1] == 276
    exec(compile(blocks[1], "hist.md example 2", "exec"), ns)
    assert ns["w"]["count"].sum() == 857 == (run["W"] > 0.2).sum()
    # a point less than a width below LO counts in the first bin (truncation)
    lo = run["V"].min() + 0.5 * 0.0642
    g = m.histogram("V", bins=10, range=(lo, lo + 0.642))
    assert g["count"][0] == ((run["V"] >= lo - 0.0642) & (run["V"] < lo + 0.0642)).sum()
    m.close()


def test_xcorr_page():
    text, blocks = python_blocks("xcorr.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "xcorr.md example 1", "exec"), ns)
    m, run, c = ns["m"], ns["run"], ns["c"]
    lag, cor = c["lag"], c["correlation"]
    assert len(c) == 802 and lag[0] == -20 and lag[-1] == 20
    assert round(lag[1] - lag[0], 5) == 0.04994 and round(lag[400], 3) == -0.025
    assert round(cor[400], 5) == 0.03308 == round(run["V"].var(), 5)
    later = lag > 5
    j = np.argmax(cor[later])
    assert abs(lag[later][j] - 16.45) < 0.05 and round(cor[later][j], 4) == 0.0311
    assert round(cor[later][j] / cor[400], 2) == 0.94
    k = np.where((lag > 0) & (lag < 16))[0]
    assert round(lag[k[np.argmin(cor[k])]], 2) == 8.51 and round(cor[k].min(), 4) == -0.0142
    assert len(m.correlation("V")) == 102 and m.correlation("V")["lag"][-1] == 2.5
    exec(compile(blocks[1], "xcorr.md example 2", "exec"), ns)
    vw = ns["vw"]
    near = np.abs(vw["lag"]) < 8
    assert round(vw["lag"][near][np.argmax(vw["correlation"][near])], 2) == 1.32
    v, w = run["V"] - run["V"].mean(), run["W"] - run["W"].mean()
    assert (v[:-26] * w[26:]).mean() > 10 * abs((v[26:] * w[:-26]).mean())   # W follows V
    fft = m.correlation("V", lags=800, fft=True)["correlation"]
    assert np.abs(fft - cor)[:-2].max() < 1e-7
    m.close()


def test_spectrum_page():
    text, blocks = python_blocks("spectrum.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "spectrum.md example 1", "exec"), ns)
    m, s, k = ns["m"], ns["s"], ns["k"]
    f, p = s["freq"], s["power"]
    assert len(m.last_run()) == 40001 and len(s) == 4096
    assert round(f[1] - f[0], 5) == 0.00244 and round(f[k], 4) == 0.0610
    assert abs(f[k] - 1 / 16.45) < f[1] - f[0]
    peaks = [i for i in range(1, len(p) - 1)
             if p[i] > p[i - 1] and p[i] >= p[i + 1] and p[i] > 0.1 * p[k]]
    assert [round(f[i], 4) for i in peaks[:4]] == [0.0610, 0.1221, 0.1831, 0.2417]
    assert [round(p[i] / p[k], 2) for i in peaks[1:4]] == [0.63, 0.30, 0.11]
    assert f[-1] < 10 and f[-1] > 9.99                        # 1/(2 dt)
    c = m.spectrum("V", "W", kind="coherence", length=8192)["coherence"]
    assert all(round(c[i], 3) == 1.0 for i in peaks[:4])
    exec(compile(blocks[1], "spectrum.md example 2", "exec"), ns)
    coarse = ns["coarse"]
    assert len(coarse) == 256 and round(coarse["freq"][1], 3) == 0.039
    assert np.argmax(coarse["power"][1:]) + 1 == 1
    m.close()


def test_hist2d_page():
    text, blocks = python_blocks("hist2d.md")
    ns = {}
    exec(compile(blocks[0], "hist2d.md example", "exec"), ns)
    m, h, k = ns["m"], ns["h"], ns["k"]
    c = h["count"]
    assert len(h) == 100 and (c > 0).sum() == 36
    assert round(c[k], 3) == 0.193 and round(h["x"][k], 3) == -0.284 and round(h["y"][k], 4) == 0.0265
    run = m.last_run()
    on_edge = ((run["V"] == run["V"].max()) | (run["W"] == run["W"].max())).sum()
    assert round(c.sum(), 4) == 0.9995 == round(1 - on_edge / len(run), 4)
    assert len(m.histogram2d("V", "W")) == 400                 # 20 by 20
    m.close()


def test_bvp_page():
    text, blocks = python_blocks("bvp.md")
    assert len(blocks) == 2
    ns = {}
    exec(compile(blocks[0], "bvp.md example 1", "exec"), ns)
    m, sol = ns["m"], ns["sol"]
    assert round(m.get("V"), 12) == 1 and round(m.get("VX"), 11) == -1.31303528556
    assert abs(m.get("VX") + 1 / np.tanh(1)) < 1e-10
    assert np.abs(sol["V"] - ns["exact"]).max() < 4e-8 and sol.t[-1] == pytest.approx(1)
    guess = m.get("VX")
    exec(compile(blocks[1], "bvp.md example 2", "exec"), ns)
    loose = ns["loose"]
    assert round(loose, 7) == -0.7615917 and abs(loose + np.tanh(1)) == pytest.approx(2.5e-6, abs=1e-7)
    assert round(m.get("VX"), 11) == -0.76159415592 and abs(m.get("VX") + np.tanh(1)) < 4e-11
    other = xs.load(xs.example("cable.ode"))            # from the first solution
    other.set(total=1, dt=0.01, VX=guess, a=0, b=1)
    other.bvp()
    assert round(other.get("VX"), 11) == -0.76159415594
    m.close()
    other.close()


def knee(v_nullcline):
    """The lower knee of Morris-Lecar's N-shaped V nullcline: its lowest W
    for V between -0.45 and -0.1."""
    seg = np.asarray(v_nullcline).reshape(-1, 4)
    pts = np.vstack([seg[:, :2], seg[:, 2:]])
    sel = (pts[:, 0] > -0.45) & (pts[:, 0] < -0.1)
    return pts[sel, 1].min()


LECAR_BOX = {"V": (-0.6, 1.2), "W": (-0.25, 1.2)}


def test_nullclines_freeze_page():
    text, blocks = python_blocks("nullclines-freeze.md")
    ns = {}
    exec(compile(blocks[0], "nullclines-freeze.md example", "exec"), ns)
    m, values, frozen = ns["m"], ns["values"], ns["frozen"]
    knees = [round(knee(vn), 2) for vn, wn in frozen]
    assert knees == [-0.09, -0.06, -0.03, 0.0, 0.03]
    assert all(np.array_equal(np.asarray(frozen[0][1]), np.asarray(f[1])) for f in frozen)
    counts = []
    for v in values:
        m.set(iapp=v)
        counts.append(len(m.find_equilibria(box=LECAR_BOX)))
    assert counts == [3, 3, 3, 3, 1]
    m.close()


def test_nullclines_play_page():
    text, blocks = python_blocks("nullclines-play.md")
    ns = {}
    exec(compile(blocks[0], "nullclines-play.md example", "exec"), ns)
    m, values = ns["m"], ns["values"]
    assert len(ns["frozen"]) == 21
    assert round(values[16], 3) == 0.08 and round(values[17], 3) == 0.085
    m.set(iapp=values[16])
    assert len(m.find_equilibria(box=LECAR_BOX)) == 3
    m.set(iapp=values[17])
    assert len(m.find_equilibria(box=LECAR_BOX)) == 1
    m.close()


def test_nullclines_unfreeze_page():
    text, blocks = python_blocks("nullclines-unfreeze.md")
    ns = {}
    exec(compile(blocks[0], "nullclines-unfreeze.md example", "exec"), ns)
    assert ns["frozen"] == [] and ns["m"].get("iapp") == pytest.approx(0.05)
    ns["m"].close()


def test_export_page(tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    text, blocks = python_blocks("export.md")
    assert len(blocks) == 1
    ns = {}
    exec(compile(blocks[0], "export.md example", "exec"), ns)
    m, sol = ns["m"], ns["sol"]
    lines = (tmp_path / "run.csv").read_text().splitlines()
    assert lines[0] == "T,V,W,CUR,ICA,IK,RATE" and len(lines) == 602
    first = lines[1].split(",")
    assert first[1:4] == ["-0.14399999380111694", "0.029999999329447746", "0.05000000074505806"]
    back = np.genfromtxt(tmp_path / "run.csv", delimiter=",", names=True)
    assert np.array_equal(back["V"], sol["V"])                          # exact
    assert (tmp_path / "run.mat").stat().st_size > 0 and sol.params["iapp"] == pytest.approx(0.05)
    found = m.find_equilibria(box=LECAR_BOX)
    focus = [e for e in found if e.kind == "unstable focus"][0]
    ev = sorted(focus.eigenvalues, key=lambda z: -z.imag)
    assert round(ev[0].real, 4) == 0.5204 and round(ev[0].imag, 4) == 0.7811
    m.close()
