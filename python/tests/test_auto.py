from pathlib import Path

import pytest

import xpplab as xs

ROOT = Path(__file__).resolve().parents[2]
ODE = ROOT / "engine" / "xppaut" / "ode"


@pytest.fixture
def lecar_hopf():
    with xs.load(ODE / "lecar.ode") as m:
        m.apply_set("hopf")
        yield m


def test_equilibrium_branch_finds_two_hopf_points(lecar_hopf):
    d = lecar_hopf.auto.equilibria("iapp")
    hb = d.special("HB")
    assert [round(p.value, 4) for p in hb] == [0.2625, 0.4569]
    assert all(not p.periodic for p in d)
    assert hb[0].description == "Hopf bifurcation"


def test_periodic_orbits_from_hopf(lecar_hopf):
    d = lecar_hopf.auto.equilibria("iapp")
    orbits = lecar_hopf.auto.continue_from(d.first("HB"), "periodic", nmax=150)
    assert len(orbits) == 150
    assert all(p.periodic for p in orbits)
    folds = orbits.special("LP")
    assert folds and folds[0].value == pytest.approx(0.2484, abs=1e-3)
    assert folds[0].period > 0
    # The whole diagram holds both runs.
    assert len(lecar_hopf.auto.diagram) == len(d) + len(orbits)


def test_settings_roundtrip(lecar_hopf):
    lecar_hopf.auto.set(nmax=123, ds=0.005)
    assert lecar_hopf.auto.get("nmax") == 123
    assert lecar_hopf.auto.settings["ds"] == pytest.approx(0.005)
    with pytest.raises(KeyError):
        lecar_hopf.auto.set(bogus=1)


def test_invalid_action_is_explained(lecar_hopf):
    d = lecar_hopf.auto.equilibria("iapp")
    with pytest.raises(ValueError) as err:
        lecar_hopf.auto.continue_from(d.first("HB"), "doubling")
    assert "periodic" in str(err.value)


def test_two_parameter_hopf_curve(lecar_hopf):
    d = lecar_hopf.auto.equilibria("iapp")
    curve = lecar_hopf.auto.continue_from(d.first("HB"), "two-parameter",
                                          second="gca", nmax=60)
    assert len(curve) > 10
    assert all(p.two_parameter for p in curve)
    assert {p.parameter2 for p in curve} == {"gca"}


def test_reset(lecar_hopf):
    lecar_hopf.auto.equilibria("iapp")
    assert len(lecar_hopf.auto.diagram) > 0
    lecar_hopf.auto.reset()
    assert len(lecar_hopf.auto.diagram) == 0


def test_dataframe_and_plot(lecar_hopf):
    pytest.importorskip("pandas")
    mpl = pytest.importorskip("matplotlib")
    mpl.use("Agg")
    d = lecar_hopf.auto.equilibria("iapp")
    lecar_hopf.auto.continue_from(d.first("HB"), "periodic", nmax=100)
    full = lecar_hopf.auto.diagram
    df = full.to_pandas()
    assert {"branch", "type", "stable", "iapp", "V_max", "V_min"} <= set(df.columns)
    ax = full.plot("V")
    assert ax.get_xlabel() == "iapp"


def test_a_failed_continuation_leaves_the_parameters_usable():
    # coaster2D's steady states are not isolated (X, Z and L can rest at any
    # value), so AUTO's first Newton steps diverge and it stops with MX. It
    # used to leave the parameter at its last trial value, NaN, which broke
    # every later run of the model.
    with xs.load(ODE / "coaster2D.ode") as m:
        d = m.auto.equilibria("drag")
        assert d.special("MX"), d
        assert m.parameters["drag"] == pytest.approx(1.2)
        d = m.auto.equilibria("drag")          # again, from where it stopped
        assert m.parameters["drag"] == pytest.approx(1.2)
        sol = m.run(total=20)
        assert len(sol["t"]) > 100 and sol["M"][-1] == pytest.approx(3.14159, abs=1e-3)


def test_auto_leaves_the_parameters_as_they_were():
    # AUTO evaluates the model at trial values and XPPAUT kept the last one:
    # in 6x6.ode on x86-64 Windows and Linux a step jumps from a = 0.26 to
    # a = -1.3e6, which then stayed the model's parameter (on Intel macOS the
    # branch ends elsewhere). The diagram keeps the points; the model does not.
    with xs.load(ODE / "6x6.ode") as m:
        d = m.auto.equilibria("a")
        assert any(p.value != 0.0 for p in d.points)      # AUTO moved a
        assert m.parameters["a"] == 0.0


def test_a_singular_matrix_stops_only_the_continuation():
    # forcpend.ode meets a zero pivot in AUTO's linear solver (ge() in
    # autlib1.c), where XPPAUT exits. The continuation ends there with its
    # points kept, and the engine goes on working.
    with xs.load(ODE / "forcpend.ode") as m:
        d = m.auto.equilibria("f")
        assert d.special("BP")
        assert "AUTO stopped here" in m.log
        assert m.parameters["f"] == pytest.approx(0.1)
        d = m.auto.equilibria("w")                  # the same engine, again
        assert len(m.run(total=10)["t"]) > 10
