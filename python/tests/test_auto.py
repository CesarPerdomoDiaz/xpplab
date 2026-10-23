from pathlib import Path

import pytest

import xppstudio as xs

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
