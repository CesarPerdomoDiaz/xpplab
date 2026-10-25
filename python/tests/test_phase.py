import numpy as np
import pytest

import xpplab as xs

FHN = """
v' = v - v^3/3 - w + I
w' = eps*(v + a - b*w)
par I=0.5, a=0.7, b=0.8, eps=0.08
init v=-1, w=1
@ total=100, dt=0.05
done
"""


@pytest.fixture
def fhn():
    m = xs.Model.from_text(FHN, "fhn.ode")
    yield m
    m.close()


def test_rhs_matches_the_equations(fhn):
    f = fhn.rhs({"v": 0.5, "w": 0.2})
    v, w = 0.5, 0.2
    assert f[0] == pytest.approx(v - v**3 / 3 - w + 0.5)
    assert f[1] == pytest.approx(0.08 * (v + 0.7 - 0.8 * w))


def test_nullclines_lie_on_the_curves(fhn):
    vn, wn = fhn.nullclines("v", "w", (-2.5, 2.5), (-1, 2.5), grid=120)
    assert len(vn) > 20 and len(wn) > 20
    # v-nullcline: w = v - v^3/3 + I
    pts = vn[:, :2]
    resid = pts[:, 1] - (pts[:, 0] - pts[:, 0] ** 3 / 3 + 0.5)
    assert np.max(np.abs(resid)) < 0.05
    # w-nullcline: w = (v + a)/b
    pts = wn[:, :2]
    assert np.max(np.abs(pts[:, 1] - (pts[:, 0] + 0.7) / 0.8)) < 0.05


def test_nullclines_over_a_parameter(fhn, tmp_path):
    box = ((-2.5, 2.5), (-1, 2.5))
    sets = fhn.nullcline_sweep("I", [0.0, 0.5, 1.0], "v", "w", *box, grid=80)
    assert len(sets) == 3 and fhn.get("I") == 0.5      # put back
    fhn.set(I=1.0)
    alone = fhn.nullclines("v", "w", *box, grid=80)
    assert all(np.array_equal(a, b) for a, b in zip(sets[2], alone))
    # The v-nullcline w = v - v^3/3 + I moves up with I; the w-nullcline stays.
    assert np.mean(sets[2][0][:, 1]) > np.mean(sets[0][0][:, 1])
    assert np.array_equal(sets[0][1], sets[2][1])
    t = xs.Table.from_nullclines(sets, first=1)
    assert t.columns == ["set", "nullcline", "x1", "y1", "x2", "y2"]
    assert len(t) == sum(len(x) + len(y) for x, y in sets)
    assert sorted(set(t["set"])) == [1, 2, 3] and set(t["nullcline"]) == {1, 2}
    rows = t.to_numpy()[(t["set"] == 3) & (t["nullcline"] == 1)]
    assert np.array_equal(rows[:, 2:], alone[0])
    t.to_csv(tmp_path / "nc.csv")
    back = np.loadtxt(tmp_path / "nc.csv", delimiter=",", skiprows=1)
    assert np.array_equal(back, t.to_numpy())


def test_vector_field_shape(fhn):
    X, Y, U, V = fhn.vector_field("v", "w", (-2, 2), (-1, 2), n=7)
    assert X.shape == Y.shape == U.shape == V.shape == (7, 7)
    assert U[3, 3] == pytest.approx(fhn.rhs({"v": X[3, 3], "w": Y[3, 3]})[0])


def test_sweep_finds_the_onset_of_oscillations(fhn):
    amps = fhn.sweep("I", [0.0, 0.5], measure=lambda s: np.ptp(s["V"][len(s) // 2:]),
                     total=300)
    assert amps[0] < 0.01      # at rest
    assert amps[1] > 1.0       # oscillating
    assert fhn.get("I") == pytest.approx(0.5)  # restored


def test_sweep_returns_trajectories(fhn):
    runs = fhn.sweep("I", [0.1, 0.2])
    assert len(runs) == 2 and all(len(r) > 0 for r in runs)


def test_phase_plane_plot(fhn):
    mpl = pytest.importorskip("matplotlib")
    mpl.use("Agg")
    ax = fhn.phase_plane("v", "w", xlim=(-2.5, 2.5), ylim=(-1, 2.5))
    assert ax.get_xlabel() == "v"
    frozen = fhn.nullcline_sweep("I", [0.0, 1.0], "v", "w", (-2.5, 2.5), (-1, 2.5))
    ax2 = fhn.phase_plane("v", "w", xlim=(-2.5, 2.5), ylim=(-1, 2.5), frozen=frozen)
    assert len(ax2.collections) == len(ax.collections) + 4


DUFFING = """
x' = y
y' = x - x^3 - d*y
par d=0.25
init x=0.05, y=0
@ total=30, dt=0.01
done
"""


def test_saddle_manifolds_separate_the_wells():
    with xs.Model.from_text(DUFFING, "duffing.ode") as m:
        eq, branches = m.manifolds({"x": 0, "y": 0})
        assert eq.kind == "saddle"
        kinds = sorted(k for k, _ in branches)
        assert kinds == ["stable", "stable", "unstable", "unstable"]
        ends = sorted(round(float(s["X"][-1])) for k, s in branches if k == "unstable")
        assert ends == [-1, 1]  # each unstable branch falls into one well
        assert all(s.t[-1] < 0 for k, s in branches if k == "stable")  # backward


def test_no_manifolds_for_a_node():
    with xs.Model.from_text(DUFFING, "duffing.ode") as m:
        eq, branches = m.manifolds({"x": 1, "y": 0})
        assert eq.stable and branches == []
