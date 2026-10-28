"""The "Learn by example" pages in docs/learn must keep working.

Each page is run as the reader would run it: every ``ode`` block is saved
under the name on its first line, then every Python snippet runs, in order,
in one namespace. The numbers the text quotes are checked against what the
snippets (and the exercises' solutions) produce.
"""
import math
import re
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs

ROOT = Path(__file__).resolve().parents[2]
LEARN = ROOT / "docs" / "learn"
PAGES = ["neuroscience", "ecology", "chemistry", "mechanics", "kite"]


def blocks(page, language):
    text = (LEARN / f"{page}.md").read_text(encoding="utf-8")
    return re.findall(r"^```%s\n(.*?)^```" % language, text, re.S | re.M)


class Run:
    """A page run to the end: ``ns`` is the final namespace, ``history`` maps
    each name to every object bound to it, in order (a later snippet may
    reuse a name such as ``d`` or ``eq``)."""

    def __init__(self, page, workdir):
        self.page = page
        self.files = {}
        for text in blocks(page, "ode"):
            first = text.splitlines()[0]
            match = re.fullmatch(r"# (\w+\.ode)", first)
            assert match, f"{page}: an ode block does not start with '# name.ode'"
            name = match.group(1)
            assert name not in self.files, f"{page}: {name} is defined twice"
            (workdir / name).write_text(text, encoding="utf-8")
            self.files[name] = text
        self.ns = {}
        self.history = {}
        code = blocks(page, "python")
        assert len(code) >= 5
        for k, snippet in enumerate(code):
            exec(compile(snippet, f"{page}.md snippet {k + 1}", "exec"), self.ns)
            for name, value in self.ns.items():
                if name.startswith("__"):
                    continue
                seen = self.history.setdefault(name, [])
                if not seen or seen[-1] is not value:
                    seen.append(value)

    def first(self, name):
        return self.history[name][0]

    def close(self):
        for value in list(self.ns.values()):
            if isinstance(value, xs.Model):
                value.close()


@pytest.fixture(scope="module")
def runs(tmp_path_factory):
    out = {}
    cwd = Path.cwd()
    try:
        for page in PAGES:
            workdir = tmp_path_factory.mktemp(page)
            import os
            os.chdir(workdir)
            out[page] = Run(page, workdir)
    finally:
        import os
        os.chdir(cwd)
    yield out
    for run in out.values():
        run.close()


def approx(x, value, tol):
    assert x == pytest.approx(value, abs=tol)


# -- the pages as files -------------------------------------------------------

def test_every_load_has_its_model(runs):
    for page, run in runs.items():
        loaded = set()
        for snippet in blocks(page, "python"):
            loaded |= set(re.findall(r'xs\.load\("([^"]+)"\)', snippet))
        assert loaded, page
        assert loaded <= set(run.files), (page, loaded - set(run.files))
        # and every model on the page is used by a snippet
        assert set(run.files) <= loaded, (page, set(run.files) - loaded)


def test_pages_are_in_the_nav():
    nav = (ROOT / "mkdocs.yml").read_text(encoding="utf-8")
    for page in ["index"] + PAGES:
        assert f"learn/{page}.md" in nav
    assert "pymdownx.details" in nav
    for page in PAGES:
        assert (LEARN / f"{page}.md").is_file()


def test_no_display_math():
    # The site has no math renderer: formulas are plain text.
    for page in ["index"] + PAGES:
        text = (LEARN / f"{page}.md").read_text(encoding="utf-8")
        assert "$$" not in text, page


def test_links_inside_learn_exist():
    for page in ["index"] + PAGES:
        text = (LEARN / f"{page}.md").read_text(encoding="utf-8")
        for target in re.findall(r"\]\(([^)#]+\.md)(?:#[^)]*)?\)", text):
            assert (LEARN / target).resolve().is_file() or \
                target.startswith("../gallery/"), (page, target)


# -- neuroscience ---------------------------------------------------------------

def test_neuroscience_leak(runs):
    r = runs["neuroscience"]
    assert r.first("err") < 1e-6
    eq = r.first("eq")
    assert eq.kind == "stable node"
    approx(eq.eigenvalues[0].real, -0.1, 1e-6)
    # exercise 1: 95% of the final value at tau ln 20
    sol = r.first("sol")
    t95 = sol.t[np.argmax(sol["v"] >= 0.95 * 5)]
    approx(t95, 10 * np.log(20), 0.02)


def test_neuroscience_theta(runs):
    r = runs["neuroscience"]
    rest, thr = r.first("rest"), r.first("thr")
    assert (rest.kind, thr.kind) == ("stable node", "unstable node")
    approx(rest["th"], -np.arccos(2 / 3), 1e-5)
    approx(thr["th"], np.arccos(2 / 3), 1e-5)
    approx(np.arccos(2 / 3), 0.8411, 5e-5)
    rates, exact = np.asarray(r.ns["rates"]), np.asarray(r.ns["exact_rates"])
    assert np.allclose(rates, exact, rtol=0.01)
    approx(rates[0], 0.0706, 5e-5)
    approx(exact[0], 0.0712, 5e-5)
    approx(rates[4], 0.3183, 5e-5)
    lp = r.first("d").special("LP")
    assert len(lp) == 1
    approx(lp[0].value, 0.0, 1e-6)
    # exercises 2 and 3
    th = r.ns["th"]
    th.set(total=1000)
    turns = th.run(I=0.001, th=0.0)["th"][-1] / (2 * np.pi)
    approx(turns, 10.0, 0.01)
    approx(np.pi / np.sqrt(0.001), 99.3, 0.05)
    th.set(total=200)
    low = th.run(I=-0.2, th=0.8)["th"][-1]
    high = th.run(I=-0.2, th=0.9)["th"][-1]
    approx(low, -0.8411, 1e-3)
    approx(high, -0.8411 + 2 * np.pi, 1e-3)
    approx(-0.8411 + 2 * np.pi, 5.4421, 1e-4)


def test_neuroscience_fitzhugh_nagumo(runs):
    r = runs["neuroscience"]
    fhn = r.ns["fhn"]
    eq = r.history["eq"][-1]
    assert eq.kind == "stable focus"
    approx(eq["v"], -1.1994, 5e-5)
    approx(eq["w"], -0.6243, 5e-5)
    approx(eq.eigenvalues[0].real, -0.251, 5e-4)
    approx(abs(eq.eigenvalues[0].imag), 0.213, 5e-4)
    approx(r.ns["small"]["v"].max(), -0.8, 1e-6)
    approx(r.ns["big"]["v"].max(), 1.77, 5e-3)
    hb = r.ns["hb"]
    assert [round(p.value, 4) for p in hb] == [0.3313, 1.4187]
    assert [p.stable for p in hb] == [False, True]
    # the periods loop left eps = 0.01, and AUTO moved the initial values
    fhn.set(eps=0.08, total=400, v=-1.2, w=-0.6)
    late = r.ns["late_range"]
    assert late(0.30) < 0.01
    approx(late(0.32), 0.11, 0.01)
    approx(late(0.33), 3.7, 0.1)
    assert late(0.35) > 3.7
    lp = r.ns["orbits"].special("LP")[0]
    approx(lp.value, 0.3241, 5e-5)
    approx(lp.period, 66.2, 0.05)
    for start in ((-1.2, -0.6), (1.0, 1.0)):      # bistable between 0.3241 and 0.3313
        s = fhn.run(I=0.326, v=start[0], w=start[1])
        v = s["v"][len(s.t) // 2:]
        approx(v.max() - v.min(), 3.73, 0.05)
    periods = r.ns["periods"]
    assert sorted(periods) == [0.01, 0.02, 0.04, 0.08]
    for eps, expected in {0.08: 39.5, 0.04: 68, 0.02: 122, 0.01: 227}.items():
        approx(periods[eps], expected, 0.5)
    # the text's algebra: the Hopf value of I and the knee
    vstar = -np.sqrt(1 - 0.08 * 0.8)
    approx(vstar, -0.9675, 5e-5)
    approx((vstar + 0.7) / 0.8 - vstar + vstar ** 3 / 3, 0.3313, 5e-5)
    approx((-1 + 0.7) / 0.8 + 1 - 1 / 3, 0.2917, 5e-5)
    # exercise 2: the threshold
    fhn.set(total=150)
    w0 = eq["w"]
    approx(fhn.run(I=0, v=-0.7, w=w0)["v"].max(), -0.67, 0.01)
    approx(fhn.run(I=0, v=-0.6, w=w0)["v"].max(), 1.71, 0.01)


# -- ecology ----------------------------------------------------------------------

def test_ecology_harvest(runs):
    r = runs["ecology"]
    high, low = r.first("high"), r.first("low")
    assert (high.kind, low.kind) == ("stable node", "unstable node")
    approx(high["x"], 0.7236, 5e-5)
    approx(low["x"], 0.2764, 5e-5)
    approx(r.ns["ok"]["x"][-1], 0.7236, 1e-3)
    assert r.ns["gone"]["x"].min() < 0
    lp = r.first("d").special("LP")
    assert len(lp) == 1
    approx(lp[0].value, 0.25, 1e-4)
    # exercises
    fish = r.ns["fish"]
    fish.set(total=60)
    s = fish.run(h=0.3, x=1.0)
    approx(s.t[np.argmax(s["x"] < 0)], 10.3, 0.06)
    assert fish.run(h=0.2, x=0.2)["x"][-1] < 0
    approx(fish.run(h=0.15, x=0.2)["x"][-1], 0.8162, 1e-3)
    approx((1 + np.sqrt(1 - 0.6)) / 2, 0.8162, 5e-5)
    approx(fish.run(h=0.16, x=0.2)["x"][-1], 0.2, 1e-4)
    approx((1 - np.sqrt(1 - 4 * 0.16)) / 2, 0.2, 1e-12)


def test_ecology_lotka_volterra(runs):
    r = runs["ecology"]
    lv = r.ns["lv"]
    eq = r.first("eq")
    approx(eq["x"], 1.0, 1e-6)
    approx(eq["y"], 1.0, 1e-6)
    assert np.allclose(eq.eigenvalues.real, 0, atol=1e-6)
    approx(abs(eq.eigenvalues[0].imag), 1.0, 1e-6)
    H = r.ns["H"]
    spread = lambda s: H(s).max() - H(s).min()
    assert spread(r.ns["rk"]) < 1e-6
    approx(spread(r.ns["eu"]), 0.16, 0.01)
    # period 6.38 for the orbit from x = 1.5; 2 pi for small ones
    lv.set(meth="rungekutta")
    s = lv.run()
    x, t = s["x"], s.t
    rising = (x[:-1] < 1) & (x[1:] >= 1) & (np.diff(x) > 0)
    approx(np.diff(t[1:][rising])[0], 6.38, 0.01)
    # exercise 2: the predator peaks at 1.31 and the prey starts at its peak
    approx(t[np.argmax(s["y"][:700])], 1.31, 0.011)
    approx(s["x"][:700].max(), 1.5, 1e-6)
    # exercise 3
    s = lv.run(x=0.1, y=0.1)
    approx(s["x"].min(), 0.023, 5e-4)
    approx(s["y"].min(), 0.023, 5e-4)
    approx(s["x"].max(), 5.5, 0.05)
    # exercise 1: with a logistic prey term
    text = r.files["lv.ode"].replace("x' = a*x - b*x*y", "x' = a*x*(1-x/K) - b*x*y")
    text = text.replace("par a=1", "par K=5, a=1")
    m = xs.Model.from_text(text)
    e = m.equilibrium({"x": 1, "y": 0.8})
    approx(e["x"], 1.0, 1e-6)
    approx(e["y"], 0.8, 1e-6)
    approx(e.eigenvalues[0].real, -0.1, 1e-3)
    approx(abs(e.eigenvalues[0].imag), 0.889, 1e-3)
    m.close()


def test_ecology_rosenzweig_macarthur(runs):
    r = runs["ecology"]
    rm = r.ns["rm"]
    eq = r.history["eq"][-1]
    assert eq.kind == "stable focus"
    approx(eq["x"], 1 / 3, 1e-6)
    approx(eq["y"], 0.963, 5e-4)
    approx(eq.eigenvalues[0].real, -0.049, 5e-4)
    # exercise 1: the trace is x*(K - 1 - 2x*)/(K(1 + x*)), half of it is Re(lambda)
    xs_, K = 1 / 3, 1.2
    trace = xs_ * (K - 1 - 2 * xs_) / (K * (1 + xs_))
    approx(trace / 2, -0.0486, 5e-5)
    approx(trace, 2 * eq.eigenvalues[0].real, 2e-3)
    approx(xs_ * (1 - (1 + 2 * xs_) / K) / 2, -0.0648, 5e-5)   # the old, wrong form
    approx(abs(eq.eigenvalues[0].imag), 0.365, 5e-4)
    approx(r.ns["hb"].value, 5 / 3, 2e-4)
    Ks = r.ns["Ks"]
    lows, highs = np.asarray(r.ns["lows"]), np.asarray(r.ns["highs"])
    assert Ks == [1.2, 2.0, 3.0, 4.0]
    approx(lows[0], 0.333, 5e-4)
    approx(highs[0], 0.333, 5e-4)
    approx(lows[1], 0.052, 5e-4)
    approx(highs[1], 1.05, 5e-3)
    approx(lows[2], 0.0017, 5e-5)
    approx(highs[2], 2.36, 5e-3)
    assert lows[3] < 1e-4
    approx(highs[3], 3.55, 5e-3)
    # exercises 2 and 3
    rm.set(total=800)
    rm.parameters["K"] = 3.0
    s = rm.run(d=0.4)
    half = len(s.t) // 2
    approx(s["x"][half:].min(), 0.100, 1e-3)
    approx(s["x"][half:].max(), 2.10, 5e-3)
    s = rm.run(K=0.3, d=0.25)
    approx(s["x"][-1], 0.3, 1e-4)
    assert s["y"][-1] < 1e-6
    approx((1 + 0.4) / (1 - 0.4), 2.333, 5e-4)
    approx(0.4 / 0.6, 0.667, 5e-4)


def test_ecology_competition(runs):
    r = runs["ecology"]
    comp = r.ns["comp"]
    box = r.ns["box"]

    def classify(a, b):
        comp.set(a=a, b=b)
        found = comp.find_equilibria(200, box=box)
        return sorted((round(e["x"] + 0.0, 3), round(e["y"] + 0.0, 3), e.kind)
                      for e in found)

    assert classify(0.5, 0.5) == [
        (0.0, 0.0, "unstable node"), (0.0, 1.0, "saddle"),
        (0.667, 0.667, "stable node"), (1.0, 0.0, "saddle")]
    assert classify(1.5, 1.5) == [
        (0.0, 0.0, "unstable node"), (0.0, 1.0, "stable node"),
        (0.4, 0.4, "saddle"), (1.0, 0.0, "stable node")]
    assert classify(0.5, 1.5) == [
        (0.0, 0.0, "unstable node"), (0.0, 1.0, "saddle"),
        (1.0, 0.0, "stable node")]
    assert r.ns["x_ahead"]["x"][-1] == pytest.approx(1.0, abs=1e-6)
    assert r.ns["y_ahead"]["y"][-1] == pytest.approx(1.0, abs=1e-6)
    assert r.ns["x_ahead"]["y"][-1] == pytest.approx(0.0, abs=1e-6)
    # the stable manifold of the saddle lies on x = y
    comp.set(a=1.5, b=1.5, x=0.4, y=0.4)
    saddle, branches = comp.manifolds()
    assert saddle.kind == "saddle"
    stable = [tr for kind, tr in branches if kind == "stable"]
    assert len(stable) == 2
    for tr in stable:
        assert np.abs(tr["x"] - tr["y"]).max() < 1e-4
    # exercise 1: eigenvalues -(2/3)(1 +- sqrt(ab)) at a = b = 0.5
    comp.set(a=0.5, b=0.5)
    e = comp.equilibrium({"x": 0.6, "y": 0.6})
    assert e.kind == "stable node"
    assert sorted(e.eigenvalues.real) == pytest.approx(
        sorted([-(2 / 3) * 1.5, -(2 / 3) * 0.5]), abs=2e-3)


# -- chemistry -----------------------------------------------------------------------

def test_chemistry_consecutive(runs):
    pytest.importorskip("pandas")
    r = runs["chemistry"]
    sol = r.first("sol")
    i = sol["b"].argmax()
    approx(sol.t[i], 1.72, 0.006)
    approx(np.log(1 / 0.3) / 0.7, 1.72, 5e-4)
    approx(sol["b"][i], 0.5969, 5e-5)
    assert np.ptp(sol["total"]) < 1e-6
    assert abs(sol["b"] - r.ns["exact_b"]).max() < 1e-6
    # exercises
    abc = r.ns["abc"]
    s = abc.run(k1=1.0, k2=1.0)
    approx(s.t[s["b"].argmax()], 1.0, 0.011)
    s = abc.run(k1=1.0, k2=30.0)
    approx(s["b"][len(s.t) // 2], s["a"][len(s.t) // 2] / 30, 5e-4)
    s = abc.run(k1=30.0, k2=1.0, total=2)
    approx(s.t[np.argmax(s["a"] < 0.01)], np.log(100) / 30, 0.011)
    approx(np.log(100) / 30, 0.15, 0.005)


def test_chemistry_enzyme(runs):
    r = runs["chemistry"]
    full = r.first("full")
    total = r.ns["total"]
    assert np.ptp(total) < 1e-6
    i = full["c"].argmax()
    approx(full.t[i], 0.58, 0.006)
    approx(full["c"][i], 0.0815, 5e-5)
    errors = r.ns["errors"]
    assert {k: round(v, 4) for k, v in errors.items()} == {
        0.01: 0.0015, 0.05: 0.0135, 0.1: 0.0272, 0.5: 0.14}
    # exercise 1
    mm = r.ns["mm"]
    mm.parameters["e0"] = 0.1
    mm.set(k1=1000, km1=100, k2=100, meth="stiff", total=60)
    stiff = mm.run()
    assert len(stiff.t) == 6001
    approx(stiff["p"][-1], 1.0, 1e-4)
    mm.set(meth="rungekutta")
    rk = mm.run()
    assert len(rk.t) < 10
    mm.set(k1=10, km1=1, k2=1)
    rk = mm.run()
    assert len(rk.t) == 6001
    approx(rk["p"][-1], 1.0, 1e-3)
    # exercise 2: the reduced model's rate
    red = r.ns["red"]
    red.parameters["e0"] = 0.1

    def rate(s0):
        return -red.rhs({"s": s0})[0]

    approx(rate(1.0), 0.0833, 5e-5)
    approx(rate(5.0), 0.0962, 5e-5)
    approx(rate(0.2), 0.05, 1e-9)
    approx(rate(1.8), 0.09, 1e-9)


def test_chemistry_schlogl(runs):
    r = runs["chemistry"]
    sch = r.ns["sch"]
    found = r.first("found")
    got = sorted((round(e["x"], 4), e.kind) for e in found)
    assert got == [(0.2929, "stable node"), (1.0, "unstable node"),
                   (1.7071, "stable node")]
    d = r.first("d")
    assert [round(p.value, 4) for p in d.special("LP")] == [0.6361, 0.3639]
    s = 1 / np.sqrt(6)
    approx(1 - s, 0.5918, 5e-5)
    approx(1 + s, 1.4082, 5e-5)
    f = lambda x: x ** 3 - 3 * x ** 2 + 2.5 * x
    approx(f(1 - s), 0.6361, 5e-5)
    approx(f(1 + s), 0.3639, 5e-5)
    mus = r.ns["mus"]
    up, down = np.asarray(r.ns["up"]), np.asarray(r.ns["down"])
    assert mus.min() > 0 and up.min() > 0 and down.min() > 0   # physical
    at = lambda m: int(np.argmin(abs(mus - m)))
    approx(up[at(0.6)], 0.430, 5e-4)
    approx(up[at(0.65)], 1.826, 5e-4)
    approx(down[at(0.4)], 1.570, 5e-4)
    approx(down[at(0.35)], 0.174, 5e-4)
    # exercise 1: c = 3
    approx(1 - 0.5 ** (1 / 3), 0.2063, 5e-5)
    sch.set(mu=0.5, c=3, total=30)
    found = sch.find_equilibria(100, box={"x": (0, 3)})
    assert [round(e["x"], 4) for e in found] == [0.2063]
    # exercise 2
    sch.set(mu=0.45, c=2.5, total=30)
    found = sch.find_equilibria(100, box={"x": (0, 3)})
    assert sorted(round(e["x"], 4) for e in found) == [0.2474, 1.1021, 1.6505]
    approx(sch.run(x=0.5)["x"][-1], 0.2474, 5e-4)
    approx(sch.run(x=1.2)["x"][-1], 1.6505, 5e-4)
    approx(sch.run(x=0.9)["x"][-1], 0.2474, 5e-4)
    # exercise 3: the cusp, and the lower fold at zero feed when c = 9/4
    approx(1 ** 3 - 3 + 3, 1.0, 0)
    s = np.sqrt(1 - (9 / 4) / 3)
    approx(s, 0.5, 1e-12)
    approx((1 + s) ** 3 - 3 * (1 + s) ** 2 + 9 / 4 * (1 + s), 0.0, 1e-12)
    approx(1 - 3 * s ** 2 - 2 * s ** 3, 0.0, 1e-12)


def test_chemistry_brusselator(runs):
    pytest.importorskip("pandas")
    r = runs["chemistry"]
    br = r.ns["br"]
    approx(r.ns["hb"].value, 2.0, 1e-4)
    eq = r.first("eq") if "eq" in r.history else None
    # the loop printed B = 1.5 and 2.5: check with the model
    br.set(total=300)
    for B, kind, sign in ((1.5, "stable focus", -1), (2.5, "unstable focus", 1)):
        br.parameters["B"] = B
        e = br.equilibrium({"x": 1, "y": B})
        assert e.kind == kind
        approx(e.eigenvalues[0].real, sign * 0.25, 2e-3)
        approx(abs(e.eigenvalues[0].imag), 0.968, 5e-4)
    t = r.ns["t"]
    row = t.iloc[::4].head(5)
    assert [round(v, 3) for v in row["B"]] == [2.0, 2.013, 2.221, 2.687, 3.397]
    assert [round(v, 4) for v in row["period"]] == [6.2832, 6.2878, 6.3813, 6.7606, 7.8030]
    assert [round(v, 3) for v in row["X_max"]] == [1.0, 1.144, 1.806, 2.967, 4.774]
    assert [round(v, 3) for v in row["X_min"]] == [1.0, 0.879, 0.609, 0.435, 0.313]
    cycle = r.ns["cycle"]
    approx(cycle[1.8][0], 1.0, 1e-3)
    approx(cycle[1.8][1], 1.0, 1e-3)
    approx(cycle[2.2][0], 0.623, 1e-3)
    approx(cycle[2.2][1], 1.751, 1e-3)
    approx(cycle[3.0][0], 0.371, 1e-3)
    approx(cycle[3.0][1], 3.752, 2e-3)
    # exercise 1: A = 2
    br.set(total=300)
    br.parameters["A"] = 2.0
    br.parameters["B"] = 4.5
    e = br.equilibrium({"x": 2, "y": 2.25})
    assert e.kind == "stable focus"
    br.parameters["B"] = 5.5
    e = br.equilibrium({"x": 2, "y": 2.75})
    assert e.kind == "unstable focus"
    approx(abs(e.eigenvalues[0].imag), 2.0, 0.1)
    approx(2 * np.pi / 2, 3.14, 0.002)
    # exercise 2: the Hopf curve is B = 1 + A^2
    br.parameters["A"] = 1.0
    br.parameters["B"] = 1.5
    br.initial_conditions.update(x=1, y=1.5)
    d = br.auto.equilibria("B", parmax=4)
    hb = d.first("HB")
    curve = br.auto.continue_from(hb, "two-parameter", second="A").to_pandas()
    assert np.abs(curve["B"] - (1 + curve["A"] ** 2)).max() < 1e-3
    # exercise 3
    br.parameters["B"] = 3.0
    br.parameters["A"] = 1.0
    br.set(total=300)
    period, orbit = br.limit_cycle(variable="x", transient=300)
    approx(period, 7.157, 0.01)
    approx(orbit["x"].min(), 0.371, 1e-3)
    approx(orbit["x"].max(), 3.751, 2e-3)
    approx(orbit["y"].min(), 0.853, 1e-3)
    approx(orbit["y"].max(), 4.722, 2e-3)


# -- mechanics -----------------------------------------------------------------------------

def test_mechanics_pendulum(runs):
    r = runs["mechanics"]
    pend = r.ns["pend"]
    bottom, top = r.history["bottom"][0], r.first("top")
    assert top.kind == "saddle"
    assert sorted(top.eigenvalues.real) == pytest.approx([-1, 1], abs=1e-5)
    assert np.abs(bottom.eigenvalues.real).max() < 1e-6
    assert sorted(bottom.eigenvalues.imag) == pytest.approx([-1, 1], abs=1e-6)
    assert bottom.kind == "stable focus"           # what the text says it prints
    E0 = r.history["E"][0]
    approx(E0[0], 0.416, 5e-4)
    assert np.ptp(E0) < 1e-6
    # the period against the quadrature
    period, quadrature = r.ns["period"], r.ns["quadrature"]
    pend.set(total=200)
    pend.parameters["gam"] = 0.0
    for a, value in ((0.5, 6.38), (2, 8.35), (3, 16.16), (3.1, 21.04)):
        approx(period(pend.run(th=a, p=0)), value, 0.006)
        approx(quadrature(a), value, 0.006)
    # with friction
    damped = r.history["bottom"][1]
    assert damped.kind == "stable focus"
    approx(damped.eigenvalues[0].real, -0.1, 1e-6)
    approx(abs(damped.eigenvalues[0].imag), 0.995, 5e-4)
    E1 = r.history["E"][1]
    approx(E1[-1], -0.9994, 5e-5)
    assert np.diff(E1).max() < 1e-8
    pend.parameters["gam"] = 0.1
    pend.set(total=100)
    assert [round(pend.run(th=0, p=p0)["th"][-1] / (2 * np.pi))
            for p0 in (2.0, 2.5, 4.5)] == [0, 1, 2]
    saddle, branches = pend.manifolds({"th": 3.14, "p": 0})
    assert saddle.kind == "saddle"
    assert sorted(k for k, _ in branches) == ["stable", "stable", "unstable", "unstable"]
    # exercise 2: focus to node at gam = w0
    for gam, kind, ev in ((0.9, "stable focus", (-0.9, 0.436)),
                          (1.1, "stable node", (-1.558, -0.642))):
        pend.parameters["gam"] = gam
        e = pend.equilibrium({"th": 0.1, "p": 0})
        assert e.kind == kind
        if kind == "stable focus":
            approx(e.eigenvalues[0].real, ev[0], 1e-6)
            approx(abs(e.eigenvalues[0].imag), ev[1], 5e-4)
        else:
            assert sorted(e.eigenvalues.real) == pytest.approx(ev, abs=5e-4)
    # exercise 3: Lindstedt-Poincare against the measured periods
    pend.parameters["gam"] = 0.0
    pend.set(total=200)
    for a, predicted, measured in ((0.5, 6.383, 6.38), (1, 6.702, 6.70),
                                   (2, 8.378, 8.35)):
        approx(2 * np.pi / (1 - a ** 2 / 16), predicted, 5e-4)
        approx(period(pend.run(th=a, p=0)), measured, 0.006)
    # exercise 4: the threshold speed
    pend.parameters["gam"] = 0.1
    pend.set(total=100)
    lo, hi = 1.0, 3.0
    for _ in range(25):
        mid = (lo + hi) / 2
        if abs(pend.run(th=0, p=mid)["th"][-1]) > np.pi:
            hi = mid
        else:
            lo = mid
    approx(lo, 2.4149, 2e-4)


def test_mechanics_hoop(runs):
    r = runs["mechanics"]
    hoop = r.ns["hoop"]
    assert (r.first("straight").kind, r.first("bent").kind) == ("saddle", "stable focus")
    approx(r.first("bent")["th"], np.arccos(1 / 1.5 ** 2), 1e-5)
    approx(np.arccos(1 / 1.5 ** 2), 1.1102, 5e-5)
    bp = r.first("d").special("BP")
    assert len(bp) == 1
    approx(bp[0].value, 1.0, 1e-6)
    lp = r.ns["side"].special("LP")
    assert len(lp) == 1
    approx(lp[0].value, 1.0, 1e-5)
    E, TU = r.history["E"][-1], r.ns["TU"]
    assert np.ptp(E) < 1e-6
    approx(np.ptp(TU), 1.73, 5e-3)
    hoop.parameters.update(Om=1.5, gam=0.1)
    approx(hoop.run(th=0.01, p=0)["th"][-1], 1.1102, 5e-4)
    approx(hoop.run(th=-0.01, p=0)["th"][-1], -1.1102, 5e-4)
    # exercise 1: focus below 0.995, node up to 1, saddle above
    approx(np.sqrt(1 - 0.1 ** 2), 0.995, 5e-4)
    for Om, kind in ((0.99, "stable focus"), (0.997, "stable node"), (1.01, "saddle")):
        hoop.parameters["Om"] = Om
        assert hoop.equilibrium({"th": 0, "p": 0}).kind == kind
    # exercise 2: small swings about the bent state
    hoop.parameters.update(Om=1.5, gam=0)
    star = np.arccos(1 / 1.5 ** 2)
    approx(1.5 * np.sin(star), 1.3437, 5e-5)
    approx(2 * np.pi / (1.5 * np.sin(star)), 4.676, 5e-4)
    s = hoop.run(th=star + 0.01, p=0)
    p = s["p"]
    down = s.t[1:][(p[:-1] > 0) & (p[1:] <= 0)]
    approx(np.diff(down).mean(), 4.677, 2e-3)
    # exercise 3: the normal form
    approx(np.sqrt(2 * (1.05 ** 2 - 1)), 0.453, 5e-4)
    approx(np.arccos(1 / 1.05 ** 2), 0.435, 5e-4)
    # exercise 4
    hoop.parameters["gam"] = 0.1
    Oms = np.linspace(1.1, 2.5, 8)
    tab = hoop.equilibrium_sweep("Om", Oms, guess={"th": 1, "p": 0})
    assert np.abs(tab["th"] - np.arccos(1 / Oms ** 2)).max() < 1e-5


def test_mechanics_vibrating_support(runs):
    r = runs["mechanics"]
    vib = r.ns["vib"]
    vib.parameters.update(d=0.1, w=30, gam=0.02)
    s0 = vib.run(th=2.9, p=0)
    approx(s0["th"].min(), 2.90, 5e-3)
    approx(s0["th"].max(), 3.37, 5e-3)
    up = r.ns["up"]
    approx(np.diff(up).mean(), 3.343, 2e-3)
    approx(2 * np.pi / np.sqrt(0.1 ** 2 * 30 ** 2 / 2 - 1), 3.359, 5e-4)
    # exercise 2: the threshold
    vib.parameters.update(w=30, gam=0.02)
    for d, stays in ((0.0475, True), (0.045, False)):
        vib.parameters["d"] = d
        s = vib.run(th=np.pi - 0.1, p=0)
        assert (np.abs(s["th"] - np.pi).max() < 1.0) == stays
    # exercise 3: parametric resonance
    vib.parameters.update(d=0.1, gam=0)
    vib.set(dt=0.01, total=100)
    for w, top in ((2, 1.73), (1.5, 0.01), (1, 0.0102)):
        vib.parameters["w"] = w
        approx(np.abs(vib.run(th=0.01, p=0)["th"]).max(), top, 5e-3 if w == 2 else 5e-4)


def test_mechanics_van_der_pol(runs):
    r = runs["mechanics"]
    vdp = r.ns["vdp"]
    hb = r.ns["hb"]
    approx(hb.value, 0.0, 1e-6)
    small = r.ns["small"]
    assert len(small) >= 3
    assert np.abs(small["U_max"] - 2 * np.sqrt(small["mu"])).max() < 1e-3
    assert r.ns["cycles"]["stable"].all()
    # exercise 2: one cycle from inside and outside
    vdp.parameters["mu"] = 0.1
    vdp.set(total=400)
    for u0 in (0.1, 3):
        u = vdp.run(u=u0, v=0)["u"]
        approx(u[len(u) // 2:].max(), 0.632, 5e-4)
    # exercise 3: the Rayleigh oscillator
    ray = xs.Model.from_text("x' = y\ny' = -w0^2*x + eps*(y - y^3)\n"
                             "par w0=1, eps=0.1\ninit x=0.1, y=0\n"
                             "@ total=300, dt=0.02\ndone\n")
    x = ray.run()["x"]
    approx(x[len(x) // 2:].max(), 1.156, 1e-3)
    approx(2 / np.sqrt(3), 1.1547, 5e-5)
    ray.close()
    # exercise 4: the relaxation oscillation
    vdp.parameters["mu"] = 10
    vdp.set(total=600, meth="stiff")
    s = vdp.run(u=0.1, v=0)
    u = s["u"]
    up = s.t[1:][(u[:-1] < 0) & (u[1:] >= 0)]
    approx(np.diff(up)[-1], 19.08, 0.011)


def test_mechanics_forced_pendulum(runs):
    # Periodic results are the same on every platform. Chaotic ones are not:
    # the model calls sin(), whose last bit differs between C libraries, and
    # chaos amplifies it. Chaotic numbers are checked against the ranges the
    # page gives (found by perturbing sin() by 1e-15 in every step).
    r = runs["mechanics"]
    forced, strobe, lyapunov = r.ns["forced"], r.ns["strobe"], r.ns["lyapunov"]
    for a0, points, exponent in ((0.5, 1, -0.25), (1.07, 2, -0.04)):
        assert len(np.unique(np.round(strobe(a0), 3))) == points
        approx(lyapunov(a0), exponent, 5e-3)
    for a0, (lo, hi) in {1.15: (0.07, 0.12), 1.5: (0.095, 0.13)}.items():
        assert len(np.unique(np.round(strobe(a0), 3))) > 60
        assert lo < lyapunov(a0) < hi
    gap, a = r.ns["gap"], r.ns["a"]
    assert 70 < a.t[np.argmax(gap > 1)] < 220
    approx(np.log(1e6) / 0.117, 118, 0.5)
    # exercise 2: the window at 1.35
    assert len(np.unique(np.round(strobe(1.35), 3))) == 1
    approx(lyapunov(1.35), -0.25, 5e-3)
    T = r.ns["T"]
    s = forced.run(th=0, p=0, total=150 * T)
    th = s["th"][::500]
    approx(abs(th[-1] - th[-2]) / (2 * np.pi), 1.0, 1e-3)
    # exercise 3: ten times smaller
    forced.parameters["a0"] = 1.5
    th0, p0 = r.ns["th"], r.ns["p"]
    b = forced.run(th=th0 + 1e-7, p=p0, total=300)
    gap7 = np.abs(a["th"] - b["th"])
    assert 80 < a.t[np.argmax(gap7 > 1)] < 280
    approx(np.log(10) / 0.117, 20, 0.5)
    # exercise 1: following the attractor as the force grows
    th, p = 0.0, 0.0
    seen = {}
    for a0 in np.arange(1.0, 1.5001, 0.01):
        forced.parameters["a0"] = a0
        s = forced.run(th=th, p=p, total=200 * T)
        th, p = s["th"][-1] % (2 * np.pi), s["p"][-1]
        seen[round(a0, 2)] = len(np.unique(np.round(s["p"][::500][100:], 3)))
    assert all(seen[round(a0, 2)] == 1 for a0 in np.arange(1.0, 1.065, 0.01))
    assert (seen[1.07], seen[1.08], seen[1.13], seen[1.44]) == (2, 4, 3, 2)
    assert all(seen[round(a0, 2)] == 1 for a0 in np.arange(1.30, 1.425, 0.01))
    assert all(seen[a0] > 30 for a0 in (1.09, 1.2, 1.25, 1.5))


# -- the kite -----------------------------------------------------------------

def test_kite_equilibria_and_diagram(runs):
    ns = runs["kite"].ns
    approx(ns["lp"].value, 2.0098, 1e-4)
    approx(math.degrees(ns["lp"].u0["gam"]), 39.15, 0.01)
    approx(ns["hb"].value, 2.2207, 1e-4)
    approx(math.degrees(ns["hb"].u0["gam"]), 55.59, 0.01)
    approx(2 * math.pi / abs(ns["hb"].eigenvalues[0].imag) * ns["TAU"], 22.08, 0.01)
    values, sweep, jump = ns["values"], ns["sweep"], ns["jump"]
    assert (values[jump], values[jump + 1]) == (2.5, 2.4)
    approx(sweep[jump], 62.66, 0.01)
    approx(ns["focus"].min(), 2.11, 0.005)
    approx(ns["focus"].max(), 3.16, 0.005)


def test_kite_orbits_tension_and_runs(runs):
    ns = runs["kite"].ns
    approx(ns["first"].value, 2.2209, 1e-4)
    approx(ns["first"].period * ns["TAU"], 22.14, 0.01)
    assert ns["mu"].min() > 1
    vw_t, lo_t, hi_t, k = ns["vw_t"], ns["lo_t"], ns["hi_t"], ns["slack_at"]
    approx(vw_t[k - 1], 2.2458, 1e-4)
    approx(vw_t[k], 2.2462, 1e-4)
    approx(lo_t[1] * 9.81, 4.02, 0.005)
    approx(hi_t[1] * 9.81, 4.99, 0.005)
    assert ns["runs"].counts() == {0: 51, 1: 47, 3: 2}
    approx(ns["hb"].value * ns["V"], 0.07090, 1e-5)
    assert ns["np"].abs(ns["vw"][ns["big"]] * ns["V"] *
                        ns["np"].sqrt(ns["lam"][ns["big"]]) / 1.109724 - 1).max() < 0.01


def test_kite_quoted_text(runs):
    text = (LEARN / "kite.md").read_text(encoding="utf-8")
    for number in ("2.0098", "2.2207", "22.08", "62.66", "79.28", "0.07090", "67.16"):
        assert number in text
