import subprocess
from html.parser import HTMLParser
import sys
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs

ROOT = Path(__file__).resolve().parents[2]
ODE = ROOT / "engine" / "xppaut" / "ode"


@pytest.fixture
def lecar():
    with xs.load(ODE / "lecar.ode") as m:
        yield m


def test_names(lecar):
    assert lecar.variables == ["V", "W"]
    assert lecar.columns == ["T", "V", "W", "CUR", "ICA", "IK", "RATE"]
    assert "iapp" in lecar.parameters
    assert lecar.sets == ["vvst", "pp", "snic", "hopf", "hom", "cool"]


def test_parameters_are_live_and_case_insensitive(lecar):
    assert lecar.parameters["iapp"] == pytest.approx(0.05)
    lecar.parameters["IAPP"] = 0.2
    assert lecar.get("iapp") == pytest.approx(0.2)
    with pytest.raises(KeyError):
        lecar.parameters["nope"]


def test_initial_conditions_and_options(lecar):
    assert lecar.ics["V"] == pytest.approx(-0.144)
    assert lecar.options["TOTAL"] == 30
    lecar.set(total=12.5, meth="cvode")
    assert lecar.get("total") == 12.5
    assert lecar.get("meth") == "cvode"
    with pytest.raises(ValueError):
        lecar.set(meth="no-such-method")


def test_run_matches_batch_mode(lecar, tmp_path):
    """The Python result equals what xppaut -silent writes, digit for digit."""
    sol = lecar.run()
    assert len(sol) == 601
    assert sol.t[0] == 0 and sol.t[-1] == pytest.approx(30)
    out = tmp_path / "mine.dat"
    sol.save(out)
    ref = ROOT / "build" / "cli" / ("xppaut-headless" +
                                    (".exe" if sys.platform == "win32" else ""))
    if not ref.exists():
        pytest.skip("xppaut-headless not built")
    (tmp_path / "lecar.ode").write_bytes((ODE / "lecar.ode").read_bytes())
    subprocess.run([str(ref), "lecar.ode", "-silent", "-outfile", "ref.dat"],
                   cwd=tmp_path, check=True, capture_output=True,
                   stdin=subprocess.DEVNULL)
    mine = [l.split() for l in out.read_text().splitlines()]
    theirs = [l.split() for l in (tmp_path / "ref.dat").read_text().splitlines()]
    assert mine == theirs


def test_run_is_repeatable_and_responds_to_parameters(lecar):
    a = lecar.run()["V"]
    b = lecar.run()["V"]
    np.testing.assert_array_equal(a, b)
    c = lecar.run(iapp=0.2)["V"]
    assert not np.allclose(a, c)


def test_sets(lecar):
    lecar.apply_set("hopf")
    assert lecar.get("phi") == pytest.approx(0.2)
    assert lecar.get("v4") == pytest.approx(0.3)


def test_equilibrium(lecar):
    eq = lecar.equilibrium()
    assert set(eq.values) == {"V", "W"}
    assert eq.eigenvalues.shape == (2,)
    assert eq.kind in {"saddle", "stable node", "unstable node",
                       "stable focus", "unstable focus"}
    eq2 = lecar.equilibrium(set_initial=True)
    assert lecar.ics["V"] == pytest.approx(eq2["V"])


def test_two_models_are_independent():
    with xs.load(ODE / "lecar.ode") as a, xs.load(ODE / "lecar.ode") as b:
        a.parameters["iapp"] = 0.3
        assert b.parameters["iapp"] == pytest.approx(0.05)
        assert not np.allclose(a.run()["V"], b.run()["V"])


def test_bad_model_raises_with_message():
    with pytest.raises(xs.EngineError) as err:
        xs.load(ODE / "hedge.ode")
    assert err.value.kind == "load"
    # hedge.ode is "x'=" with nothing on the right, then "y'=eps*x".
    assert "compiling X'" in str(err.value)


def test_from_text():
    m = xs.Model.from_text("x'=-a*x\npar a=2\ninit x=1\n@ total=1, dt=0.01\ndone\n")
    sol = m.run()
    assert sol["X"][-1] == pytest.approx(np.exp(-2), rel=1e-6)
    m.close()


def test_closed_model_refuses_work():
    m = xs.load(ODE / "lecar.ode")
    m.close()
    with pytest.raises(xs.EngineError):
        m.run()


def test_continue_run_appends_and_restores(lecar):
    sol = lecar.run(total=20)
    ics = dict(lecar.ics)
    longer = lecar.continue_run(sol, 15)
    assert len(longer) == len(sol) + len(lecar.last_run()) - 1
    assert longer.t[-1] == pytest.approx(35)
    assert np.array_equal(longer["V"][:len(sol)], sol["V"])
    assert np.all(np.diff(longer.t) > 0)
    # Initial conditions, T0 and the total time are as they were.
    assert dict(lecar.ics) == ics
    assert lecar.get("T0") == 0 and lecar.get("TOTAL") == 20


def test_last_run_is_the_engines_run(lecar):
    sol = lecar.run()
    assert np.array_equal(lecar.last_run()["V"], sol["V"])


def test_csv_is_exact(lecar, tmp_path):
    sol = lecar.run(iapp=0.1, total=50)
    sol.to_csv(tmp_path / "run.csv")
    lines = (tmp_path / "run.csv").read_text().splitlines()
    assert lines[0] == ",".join(sol.columns)
    back = np.array([[float(x) for x in l.split(",")] for l in lines[1:]])
    assert np.array_equal(back, sol.to_numpy())


def test_raising_maxstor_grows_the_storage(lecar):
    # Setting MAXSTOR alone used to leave the storage at its old size, so a
    # long run wrote past its end and crashed the process.
    lecar.set(total=300, maxstor=10000)
    assert len(lecar.run()) == 6001
    lecar.set(maxstor=100)  # less than allocated: runs grow it again
    assert len(lecar.run()) == 6001


def test_equilibrium_sweep_follows_the_branch(lecar):
    t = lecar.equilibrium_sweep("IAPP", np.linspace(0, 0.3, 7))
    assert t.columns == ["iapp", "V", "W", "unstable", "stable"]
    assert len(t) == 7
    # Two Hopf points between 0.05 and 0.25 (see test_auto): unstable there.
    assert list(t["unstable"]) == [1, 1, 2, 2, 2, 0, 0]
    assert lecar.parameters["iapp"] == pytest.approx(0.05)  # restored
    # Each row is an equilibrium at its value.
    lecar.parameters["iapp"] = 0.3
    eq = lecar.equilibrium({"V": t["V"][-1], "W": t["W"][-1]})
    assert eq["V"] == pytest.approx(t["V"][-1])


def test_find_equilibria_finds_all_three(lecar, tmp_path):
    box = {"V": (-0.6, 0.6), "W": (-0.1, 0.8)}
    eqs = lecar.find_equilibria(guesses=200, box=box)
    assert sorted(e.kind for e in eqs) == ["saddle", "stable node", "unstable focus"]
    # XPPAUT's random numbers repeat: a fresh model finds the same list.
    with xs.load(ODE / "lecar.ode") as other:
        again = other.find_equilibria(guesses=200, box=box)
    assert [e.values for e in again] == [e.values for e in eqs]
    eqs.to_csv(tmp_path / "eqs.csv")
    lines = (tmp_path / "eqs.csv").read_text().splitlines()
    assert lines[0] == "V,W,unstable,re1,im1,re2,im2"
    assert len(lines) == 4


def test_two_values_together_and_on_a_grid(lecar):
    peak = lambda s: s["V"][len(s) // 2:].max()   # noqa: E731
    gca = lecar.get("gca")
    line = lecar.sweep(("iapp", "gca"), [(0.1, 0.9), (0.3, 1.2)], measure=peak)
    grid = lecar.sweep_grid("iapp", [0.1, 0.3], "gca", [0.9, 1.0, 1.2], measure=peak)
    assert line.shape == (2,) and grid.shape == (3, 2)
    assert grid[0, 0] == line[0] and grid[2, 1] == line[1]   # the same runs
    assert lecar.get("iapp") == pytest.approx(0.05) and lecar.get("gca") == gca
    lecar.set(iapp=0.3, gca=1.0)
    assert peak(lecar.run()) == grid[1, 1]


def test_shift_lets_the_time_go_on(lecar):
    sol = lecar.run(total=20)
    lecar.ic_last(shift=True)
    assert lecar.get("T0") == pytest.approx(20)
    assert lecar.ics["V"] == pytest.approx(sol["V"][-1], abs=1e-6)
    again = lecar.run()
    assert again.t[0] == pytest.approx(20) and again.t[-1] == pytest.approx(40)
    lecar.ic_last()                       # Last alone leaves T0 alone
    assert lecar.get("T0") == pytest.approx(20)


def test_initial_values_from_a_file(lecar, tmp_path):
    lecar.run(iapp=0.1, total=33)
    lecar.ic_last()
    ics = dict(lecar.ics)
    path = lecar.write_ics(tmp_path / "ics.dat")
    lecar.set(V=0, W=0)
    assert dict(lecar.read_ics(path).ics) == ics          # exactly
    (tmp_path / "comma.txt").write_text("0.25, -0.125  7\n")
    assert dict(lecar.read_ics(tmp_path / "comma.txt").ics) == {"V": 0.25, "W": -0.125}
    (tmp_path / "short.dat").write_text("1.5\n")
    with pytest.raises(xs.EngineError, match="1 number"):
        lecar.read_ics(tmp_path / "short.dat")
    with pytest.raises(xs.EngineError, match="cannot open"):
        lecar.read_ics(tmp_path / "missing.dat")
    assert dict(lecar.ics) == {"V": 0.25, "W": -0.125}    # unchanged


def test_initial_values_from_a_formula(lecar):
    lecar.ic_formula("V", "vk + 0.1*gca")
    assert lecar.ics["V"] == pytest.approx(-0.7 + 0.1 * lecar.get("gca"))
    for target, formula in [("Q", "1"), ("V", "1+"), ("V[1..2", "1"),
                            ("V", "sin([j"), ("u[1..3]", "[j]")]:
        with pytest.raises(xs.EngineError):
            lecar.ic_formula(target, formula)
    assert lecar.ics["V"] == pytest.approx(-0.6)          # unchanged
    with xs.load(ODE / "amari.ode") as m:
        m.ic_formula("u[0..199]", "exp(-([j]-100)^2/50)")
        got = np.array([m.ics["u%d" % j] for j in range(200)])
        np.testing.assert_allclose(got, np.exp(-(np.arange(200) - 100.0) ** 2 / 50),
                                   rtol=1e-15)
        m.ic_formula("u[3..5]", "[j+1]/2")
        assert [m.ics["u%d" % j] for j in (3, 4, 5)] == [2, 2.5, 3]


def test_a_grid_as_a_table():
    t = xs.Table.from_grid("a", [1, 2], "b", [10, 20, 30], {"z": [[1, 2], [3, 4], [5, 6]]})
    assert list(t["a"]) == [1, 2, 1, 2, 1, 2]
    assert list(t["b"]) == [10, 10, 20, 20, 30, 30]
    assert list(t["z"]) == [1, 2, 3, 4, 5, 6]


class _Balanced(HTMLParser):
    """Fails on mismatched tags: the output must be well-formed."""

    def __init__(self):
        super().__init__()
        self.stack = []
        self.text = ""

    def handle_starttag(self, tag, attrs):
        self.stack.append(tag)

    def handle_endtag(self, tag):
        assert self.stack and self.stack.pop() == tag

    def handle_data(self, data):
        self.text += data


def _check_html(html):
    assert html.startswith("<")
    p = _Balanced()
    p.feed(html)
    p.close()
    assert not p.stack
    return p.text


def test_model_html(lecar):
    text = _check_html(lecar._repr_html_())
    assert "lecar.ode" in text and "iapp" in text and "initial value" in text
    assert "-0.144" in text


def test_model_html_escapes_names():
    with xs.Model.from_text("x'=-x\ninit x=1\ndone\n", name="a&b'.ode") as m:
        html = m._repr_html_()
    assert "a&amp;b&#x27;.ode" in html and "a&b'" not in html
    _check_html(html)


def test_trajectory_html(lecar):
    text = _check_html(lecar.run()._repr_html_())
    assert "points" in text and "RATE" in text


def test_trajectory_html_empty_and_escaped():
    assert "empty" in _check_html(xs.Trajectory({"T": np.array([])})._repr_html_())
    tr = xs.Trajectory({"T": np.array([0.0, 1.0]), "<&>": np.array([1.0, 2.0])})
    html = tr._repr_html_()
    assert "&lt;&amp;&gt;" in html and "<&>" not in html
    _check_html(html)


def test_equilibrium_html(lecar):
    eq = lecar.find_equilibria()
    text = _check_html(eq._repr_html_())
    html = eq._repr_html_()
    assert "stability" in text
    # One <td> per label, so "unstable" is not mistaken for "stable".
    assert html.count("<td>stable</td>") == sum(e.stable for e in eq)
    assert html.count("<td>unstable</td>") == sum(not e.stable for e in eq)
    for one in eq:
        title = "stable" if one.stable else "unstable"
        html = one._repr_html_()
        assert f"Equilibrium: {title} ({one.kind})" in html
        assert "eigenvalue" in _check_html(html)


def test_equilibrium_html_empty_and_escaped():
    assert "0 equilibria" in _check_html(xs.EquilibriumList(["V"])._repr_html_())
    e = xs.Equilibrium({"<&>": 1.0}, np.array([-1.0 + 2j, -1.0 - 2j]))
    html = e._repr_html_()
    assert "&lt;&amp;&gt;" in html
    assert "Equilibrium: stable (stable focus)" in html
    unstable = xs.Equilibrium({"x": 0.0}, np.array([0.5]))
    assert "Equilibrium: unstable (unstable node)" in unstable._repr_html_()
    _check_html(html)
