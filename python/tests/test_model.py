import subprocess
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
