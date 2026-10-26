"""The "Learn by example" pages in docs/learn must keep working.

Each page is run as the reader would run it: every ``ode`` block is saved
under the name on its first line, then every Python snippet runs, in order,
in one namespace. The numbers the text quotes are checked against what the
snippets (and the exercises' solutions) produce.
"""
import re
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs

ROOT = Path(__file__).resolve().parents[2]
LEARN = ROOT / "docs" / "learn"
PAGES = ["neuroscience", "ecology", "chemistry", "mechanics"]


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
    got = sorted((round(e["x"], 3), e.kind) for e in found)
    assert got == [(0.0, "stable node"), (1.0, "unstable node"), (2.0, "stable node")]
    d = r.first("d")
    assert [round(p.value, 4) for p in d.special("LP")] == [0.3849, -0.3849]
    approx(2 / (3 * np.sqrt(3)), 0.3849, 5e-5)
    mus = r.ns["mus"]
    up, down = np.asarray(r.ns["up"]), np.asarray(r.ns["down"])
    k03, k04 = int(np.argmin(abs(mus - 0.3))), int(np.argmin(abs(mus + 0.3)))
    approx(up[int(np.argmin(abs(mus - 0.3)))], 0.214, 5e-4)
    approx(up[int(np.argmin(abs(mus - 0.4)))], 2.16, 5e-3)
    approx(down[int(np.argmin(abs(mus + 0.3)))], 1.786, 5e-3)
    approx(down[int(np.argmin(abs(mus + 0.4)))], -0.16, 5e-3)
    # exercise 2
    sch.set(mu=0.2, total=30)
    found = sch.find_equilibria(100, box={"x": (-1, 3)})
    assert sorted(round(e["x"], 4) for e in found) == [0.1211, 0.7909, 2.088]
    approx(sch.run(x=0.5)["x"][-1], 0.1211, 5e-4)
    approx(sch.run(x=1.3)["x"][-1], 2.088, 5e-4)
    approx(sch.run(x=0.7)["x"][-1], 0.1211, 5e-4)
    # exercise 1 and 3
    x = 1 - 1 / np.sqrt(3)
    approx(x ** 3 - 3 * x ** 2 + 2 * x, 0.3849, 5e-5)
    approx(1 ** 3 - 3 + 3, 1.0, 0)


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
    down, up = r.first("down"), r.first("up")
    assert (down.kind, up.kind) == ("stable focus", "saddle")
    approx(down.eigenvalues[0].real, -0.1, 1e-6)
    approx(abs(down.eigenvalues[0].imag), 0.995, 5e-4)
    assert sorted(up.eigenvalues.real) == pytest.approx([-1.105, 0.905], abs=5e-4)
    E = r.ns["E"]
    approx(E[0], 1.416, 5e-4)
    assert E[-1] < 1e-4
    assert np.all(np.diff(E) <= 1e-9)
    approx(r.ns["slow"]["x"][-1] / np.pi, 0.0, 1e-3)
    approx(r.ns["fast"]["x"][-1] / np.pi, 2.0, 1e-3)
    # exercise 2: the period without friction, against the elliptic integral
    pend = r.ns["pend"]

    def exact(amplitude):
        a, b = 1.0, np.cos(amplitude / 2)
        for _ in range(40):
            a, b = (a + b) / 2, np.sqrt(a * b)
        return 2 * np.pi / a

    pend.set(total=100)
    for amplitude, text in ((2.0, 8.34), (3.0, 16.16)):
        s = pend.run(c=0.0, x=amplitude, v=0.0)
        v = s["v"]
        down_ = s.t[1:][(v[:-1] > 0) & (v[1:] <= 0)]
        approx(np.diff(down_)[0], text, 0.011)
        approx(exact(amplitude), text, 0.01)
    approx(exact(2.0), 8.350, 5e-4)
    approx(exact(3.0), 16.156, 5e-4)
    # exercise 3: the threshold velocity
    pend.parameters["c"] = 0.2
    lo, hi = 1.0, 3.0
    for _ in range(25):
        mid = (lo + hi) / 2
        if abs(pend.run(x=0, v=mid)["x"][-1]) > np.pi:
            hi = mid
        else:
            lo = mid
    approx(lo, 2.4149, 2e-4)
    # exercise 4: the amplitude decays at c/2
    pend.set(total=60)
    s = pend.run(x=2, v=0)
    x, t = s["x"], s.t
    peaks = [i for i in range(1, len(x) - 1)
             if x[i] > x[i - 1] and x[i] >= x[i + 1] and x[i] > 0.001]
    approx(t[peaks[0]], 7.32, 0.011)
    approx(x[peaks[0]], 0.887, 5e-4)
    approx(t[peaks[1]], 13.82, 0.011)
    approx(x[peaks[1]], 0.456, 5e-4)
    approx(t[peaks[2]], 20.18, 0.011)
    approx(x[peaks[2]], 0.240, 5e-4)
    slope = np.polyfit(t[peaks], np.log(x[peaks]), 1)[0]
    approx(slope, -0.1003, 5e-4)
    # the manifolds named in the text: two unstable and two stable branches
    saddle, branches = pend.manifolds({"x": 3.1, "v": 0})
    assert saddle.kind == "saddle"
    assert sorted(k for k, _ in branches) == ["stable", "stable", "unstable", "unstable"]


def test_mechanics_beam(runs):
    r = runs["mechanics"]
    beam = r.ns["beam"]
    assert sorted((round(e["x"], 3), e.kind) for e in r.first("found")) == [
        (-1.0, "stable focus"), (0.0, "saddle"), (1.0, "stable focus")]
    beam.parameters["mu"] = 1.0
    approx(beam.run(x=0.05, v=0)["x"][-1], 1.0, 1e-3)
    approx(beam.run(x=-0.05, v=0)["x"][-1], -1.0, 1e-3)
    bp = r.first("d").special("BP")
    assert len(bp) == 1
    approx(bp[0].value, 0.0, 1e-6)
    assert np.abs(r.ns["tab"]["x"] - np.sqrt(r.ns["mus"])).max() < 1e-5
    # exercise 1
    for mu, kind in ((-0.5, "stable focus"), (-0.01, "stable node")):
        beam.parameters["mu"] = mu
        beam.initial_conditions.update(x=0, v=0)
        assert beam.equilibrium().kind == kind
    # exercise 2
    beam.parameters["mu"] = 1.0
    assert beam.equilibrium({"x": 1, "v": 0}).kind == "stable focus"
    # exercise 3: the subcritical form
    text = r.files["buckle.ode"].replace("mu*x - x^3", "mu*x + x^3")
    sub = xs.Model.from_text(text)
    sub.parameters["mu"] = -0.25
    found = sub.find_equilibria(100, box={"x": (-3, 3), "v": (-3, 3)})
    assert sorted((round(e["x"], 3), e.kind) for e in found) == [
        (-0.5, "saddle"), (0.0, "stable focus"), (0.5, "saddle")]
    sub.parameters["mu"] = 0.25
    found = sub.find_equilibria(100, box={"x": (-3, 3), "v": (-3, 3)})
    assert [(round(e["x"], 3), e.kind) for e in found] == [(0.0, "saddle")]
    sub.close()
    # exercise 4: the fold of the imperfect pitchfork
    imperfect = xs.Model.from_text(
        r.files["buckle.ode"].replace("mu*x - x^3 - c*v", "mu*x - x^3 - c*v + f")
        .replace("par mu=-0.5, c=0.5", "par mu=1, c=0.5, f=0.1")
        .replace("init x=0.1, v=0", "init x=-1, v=0"))
    imperfect.equilibrium(set_initial=True)
    dd = imperfect.auto.equilibria("mu", parmin=-1, parmax=2, ds=-0.02)
    lp = dd.special("LP")
    assert len(lp) == 1
    approx(lp[0].value, 3 * (0.05) ** (2 / 3), 2e-3)
    approx(3 * (0.05) ** (2 / 3), 0.4072, 5e-5)
    imperfect.close()


def test_mechanics_van_der_pol(runs):
    r = runs["mechanics"]
    vdp = r.ns["vdp"]
    cycle = r.ns["cycle"]
    expect = {0.1: (6.29, 2.000), 1: (6.66, 2.009), 10: (19.08, 2.014),
              50: (82.5, 2.003)}
    for mu, (period, amplitude) in expect.items():
        p, a = cycle(mu, 40 * max(mu, 2))
        approx(p, period, 0.011)
        approx(a, amplitude, 1.5e-3)
    p5, _ = cycle(5, 200)
    approx(p5, 11.61, 0.011)

    def formula(mu):
        return (3 - 2 * np.log(2)) * mu + 3 * 2.338107 * mu ** (-1 / 3)

    approx(formula(10), 19.39, 0.006)
    approx(formula(50), 82.59, 0.006)
    assert abs(formula(50) / 82.5 - 1) < 0.002
    approx(3 - 2 * np.log(2), 1.614, 5e-4)
    # exercise 4
    p20, _ = cycle(20, 800)
    approx(p20, 34.68, 0.011)
    approx(formula(20), 34.86, 0.006)
    # exercise 1: eigenvalues at mu = 0.1
    vdp.set(meth="rungekutta", total=100)
    vdp.parameters["mu"] = 0.1
    eig = vdp.equilibrium({"x": 0.1, "y": 0.1}).eigenvalues
    approx(eig[0].real, 0.05, 1e-6)
    approx(abs(eig[0].imag), 0.9987, 5e-5)


def test_mechanics_lorenz(runs):
    r = runs["mechanics"]
    lor = r.ns["lor"]
    cp = r.first("c_plus")
    assert cp.kind == "saddle-focus"
    approx(cp["x"], 8.485, 5e-4)
    approx(cp["z"], 27.0, 1e-6)
    ev = sorted(cp.eigenvalues, key=lambda z: z.real)
    approx(ev[0].real, -13.85, 5e-3)
    approx(ev[1].real, 0.094, 5e-4)
    approx(abs(ev[1].imag), 10.19, 5e-3)
    gap = r.ns["gap"]
    a = r.ns["a"]
    approx(a.t[np.argmax(gap > 1)], 27.3, 0.06)
    approx(gap.max(), 30.0, 0.1)
    # the equilibrium at r = 20 is stable; the b = 8/3 note
    lor.parameters["r"] = 20
    assert lor.equilibrium({"x": 7, "y": 7, "z": 19}).kind == "stable focus"
    approx(lor.parameters["b"], 2.6666667, 1e-9)
    odd = xs.Model.from_text("x' = -x\npar b=8/3\ndone\n")
    assert odd.parameters["b"] == 8.0
    odd.close()
    # exercise 1
    approx(10 * (10 + 8 / 3 + 3) / (10 - 8 / 3 - 1), 24.737, 5e-4)
    for rr, sign, imag in ((24, -1, 9.49), (25, 1, 9.67)):
        lor.parameters["r"] = rr
        e = lor.equilibrium({"x": 8, "y": 8, "z": rr - 1})
        pair = [z for z in e.eigenvalues if z.imag > 0][0]
        assert np.sign(pair.real) == sign
        approx(pair.imag, imag, 5e-3)
        real = [z for z in e.eigenvalues if z.imag == 0][0]
        approx(real.real, {24: -13.62, 25: -13.68}[rr], 5e-3)
    # exercise 3
    lor.set(total=100)
    for rr, end in ((10, 4.899), (20, -7.118)):
        lor.parameters["r"] = rr
        approx(lor.run(x=1, y=1, z=1)["x"][-1], end, 2e-3)
        approx(np.sqrt(8 / 3 * (rr - 1)), abs(end), 2e-3)
