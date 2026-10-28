"""Tools added for the kite worked example: auxiliary quantities at a state
and along a diagram, the global flag that stopped a run, many runs from
random initial values, and shooting along eigenvectors."""
import math

import numpy as np
import pytest

import xpplab as xs

KITE = """
# kite.ode
par VW=8, lam=245
par L=100, g=9.81, CLmax=1.2, CD0=0.06, kd=0.12
vt=VW/sqrt(g*L)
ua=om+vt*sin(gam)
wa=vt*cos(gam)
va=sqrt(ua^2+wa^2)
alpha=atan2(wa,ua)
cl=CLmax*sin(2*alpha)
cd=CD0+kd*cl^2
tn=om^2-sin(gam)+lam*va*(cl*ua+cd*wa)
gam'=om
om'=-cos(gam)+lam*va*(cl*wa-cd*ua)
aux deg=gam*180/pi
aux tens=tn
global -1 gam {arret=1}
global 1 gam-pi/2 {arret=1}
global -1 tn {arret=1}
init gam=1.309, om=0
@ total=47, dt=0.01, bound=1e6
done
"""


@pytest.fixture
def kite():
    with xs.Model.from_text(KITE, "kite.ode") as m:
        yield m


def test_evaluate_matches_a_stored_row(kite):
    sol = kite.run(total=1)
    row = kite.evaluate({"gam": sol["gam"][0], "om": sol["om"][0]})
    assert row["TENS"] == pytest.approx(sol["tens"][0], rel=1e-6)
    assert row["deg"] == pytest.approx(math.degrees(1.309))
    assert row["T"] == 0.0


def test_evaluate_gives_the_steady_tension(kite):
    kite.set(VW=8)
    eq = kite.equilibrium({"gam": 1.36, "om": 0})
    assert math.degrees(eq["gam"]) == pytest.approx(78.164, abs=1e-3)
    # 67.16 N for a 1 kg kite
    assert kite.evaluate(eq.values)["tens"] * 9.81 == pytest.approx(67.163, abs=1e-3)


def test_flags_and_the_event_that_stopped_a_run(kite):
    assert kite.flags == [("gam", -1), ("gam-pi/2", 1), ("tn", -1)]
    kite.set(VW=2.1)
    assert kite.run(gam=0.3, om=-0.1).event == 1   # to the ground
    full = kite.run(VW=8, gam=1.3, om=0)          # settles at 78 deg
    assert full.event == 0
    assert full.t[-1] == pytest.approx(47)


def test_sample_initial_is_reproducible_and_respects_require(kite):
    box = {"gam": (0, math.pi / 2), "om": (-0.4, 0.4)}
    a = kite.sample_initial(20, box, require={"tens": (0, math.inf)}, seed=3)
    b = kite.sample_initial(20, box, require={"tens": (0, math.inf)}, seed=3)
    assert a == b and len(a) == 20
    for s in a:
        assert 0 <= s["gam"] <= math.pi / 2
        assert kite.evaluate(s)["tens"] > 0
    assert kite.sample_initial(20, box, seed=4) != a
    with pytest.raises(ValueError):
        kite.sample_initial(5, box, require={"tens": (1e9, math.inf)}, tries=50)


def test_run_many_counts_how_runs_end(kite):
    starts = kite.sample_initial(30, {"gam": (0, 1.5), "om": (-0.4, 0.4)},
                                 require={"tens": (0, math.inf)})
    runs = kite.run_many(starts, VW=8)
    assert len(runs) == 30
    assert set(runs.counts()) <= {0, 1, 2, 3}
    assert sum(runs.counts().values()) == 30
    assert list(runs.events) == [s.event for s in runs]
    # the initial values are left as they were
    assert kite.get("gam") == pytest.approx(1.309)


def test_shoot_along_the_saddle_and_node_directions(kite):
    kite.set(VW=2.5)
    out = kite.shoot({"gam": 0.3, "om": 0}, total=20)
    assert out.equilibrium.kind == "saddle"
    assert [s for _, s in out.directions] == [1, -1]
    up, down = out
    assert math.degrees(up["gam"][-1]) == pytest.approx(62.657, abs=0.01)
    assert down.event == 3                        # the tether goes slack
    kite.set(VW=2.05)
    node = kite.shoot({"gam": 0.8, "om": 0}, total=1)
    assert node.equilibrium.kind == "unstable node"
    assert len(node) == 4                         # two real directions


def test_eigenvector_is_xppauts(kite):
    a = np.array([[0.0, 1.0], [2.0, -1.0]])         # eigenvalues 1 and -2
    v = kite.eigenvector(a, 1.0)
    assert np.linalg.norm(v) == pytest.approx(1.0)
    assert a @ v == pytest.approx(v, abs=1e-8)


def test_along_a_diagram(kite):
    kite.set(VW=8, gam=math.radians(78.164), om=0)
    d = kite.auto.equilibria("VW", ds=-0.01, dsmax=0.05, nmax=800, parmin=1.5,
                            parmax=10)
    hb = d.first("HB")
    assert hb.value == pytest.approx(2.2206, abs=1e-4)
    assert math.degrees(hb.u0["gam"]) == pytest.approx(55.589, abs=1e-3)
    steady = kite.auto.along("tens", d)
    assert steady["min"][0] * 9.81 == pytest.approx(67.163, abs=1e-3)
    assert np.array_equal(steady["min"], steady["max"])
    orbits = kite.auto.continue_from(hb, "periodic", ds=0.002, dsmax=0.01,
                                     nmax=20, ntst=60, npr=2)
    before = kite.get("VW")
    t = kite.auto.along("tens", orbits)
    known = ~np.isnan(t["min"])
    assert known.sum() >= 5
    assert np.all(t["min"][known] < t["max"][known])
    # near the Hopf point the tension swings between about 4.0 and 5.0 N
    assert t["min"][known][0] * 9.81 == pytest.approx(4.0, abs=0.1)
    assert kite.get("VW") == before
