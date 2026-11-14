"""AUTO's boundary value and homoclinic starts, checked against known answers."""
import math

import numpy as np
import pytest

import xpplab as xs

# x'' = l x on [0, 1] with x(0) = 0, x(1) = 1 has the solution
# x = sinh(sqrt(l) t) / sinh(sqrt(l)), so x'(0) = sqrt(l) / sinh(sqrt(l)) and
# x'(1) = sqrt(l) / tanh(sqrt(l)). The initial y is the exact slope for l = 1.
BVP = """\
x'=y
y'=l*x
par l=1
init x=0,y=0.8509181282393216
b x
b x'-1
@ total=1,meth=rungekutta,dt=0.001
@ parmin=0,parmax=30
done
"""

# The homoclinic test model is XPPAUT's own tstauto/tsthom.ode. The saddle at
# the origin has a Hopf point at a = b on the other equilibrium (1, 0). A
# Melnikov calculation puts the homoclinic curve at a = 6 b / 7 for small b.
HOM = """\
x'=y
y'=x*(1-x)-a*y+b*x*y
par a=0.1,b=0.2
init x=1.0,y=0
@ total=200,meth=stiff,xlo=-1,xhi=2,ylo=-1,yhi=1
@ parmin=-1,parmax=1,dsmin=1e-6,dsmax=0.05,ds=0.01,nmax=400,ntst=50
done
"""


def _load(tmp_path, text):
    path = tmp_path / "model.ode"
    path.write_text(text)
    return xs.load(path)


def test_bvp_start_matches_closed_form(tmp_path):
    with _load(tmp_path, BVP) as m:
        m.run()
        d = m.auto.start("bvp", "l", nmax=40, ds=0.05, dsmax=0.2)
        assert len(d) > 20
        assert d[0].value == pytest.approx(1.0)
        assert d[len(d) - 1].value > 4.0
        for p in d:
            s = math.sqrt(p.value)
            assert not p.periodic
            assert p.u0["X"] == pytest.approx(0.0, abs=1e-8)
            assert p.u0["Y"] == pytest.approx(s / math.sinh(s), rel=1e-5)
            assert p.umax["X"] == pytest.approx(1.0, abs=1e-8)
            assert p.umax["Y"] == pytest.approx(s / math.tanh(s), rel=1e-5)


def _near_homoclinic_orbit(m, b):
    m.set(b=b, a=b / 2)
    d = m.auto.equilibria("a", ds=0.01, nmax=100)
    orbits = m.auto.continue_from(d.first("HB"), "periodic", nmax=400,
                                  dsmax=0.02)
    # The period grows without bound as the orbit nears the homoclinic.
    labelled = [p for p in orbits if p.label and p.period > 50]
    assert labelled[-1].period > 1e6
    assert labelled[0].period < labelled[-1].period
    # The first point along the branch with a huge period; the branch may
    # run on past the homoclinic (on macOS it does), so not the last one.
    diverged = next(p for p in orbits if p.period > 1e6)
    return diverged, labelled[0]


def test_homoclinic_start_follows_the_homoclinic_curve(tmp_path):
    with _load(tmp_path, HOM) as m:
        end, start = _near_homoclinic_orbit(m, 0.2)
        m.auto.orbit(start)
        m.set(a=start.value)
        h = m.auto.start("homoclinic", "a", second="b", left=[0, 0],
                         unstable=1, stable=1, nmax=30, ds=0.01, dsmax=0.05)
        assert len(h) >= 20
        # The first point is the starting orbit's parameters; the curve
        # begins where the period of the orbits diverged.
        assert h[0].value == pytest.approx(start.value)
        assert h[0].value == pytest.approx(end.value, abs=1e-4)
        a = np.array([p.value for p in h])
        b = np.array([p.value2 for p in h])
        assert b.max() > 0.3
        # The ratio 6/7 is exact only as b goes to 0; it drifts slowly.
        small = b < 0.5
        assert small.sum() >= 5
        assert np.all(np.abs(a[small] / b[small] - 6 / 7) < 0.01)
        assert np.all(np.diff(a) > -0.01)


def test_homoclinic_start_asks_for_what_it_needs(tmp_path):
    with _load(tmp_path, HOM) as m:
        with pytest.raises(ValueError, match="second"):
            m.auto.start("homoclinic", "a", left=[0, 0])
        with pytest.raises(ValueError, match="left"):
            m.auto.start("homoclinic", "a", second="b")
        with pytest.raises(ValueError, match="right"):
            m.auto.start("heteroclinic", "a", second="b", left=[0, 0])


def _captured_form(m, kind, **kw):
    seen = {}

    def run(key, settings, form=None):
        seen["form"] = form

    m.auto._run = run
    m.auto.start(kind, "a", second="b", **kw)
    return seen["form"]


def test_homoclinic_form_writes_plain_numbers_from_numpy(tmp_path):
    with _load(tmp_path, HOM) as m:
        form = _captured_form(m, "homoclinic", left=np.array([0.25, -1.5]),
                              unstable=np.int64(1), stable=1)
        assert form == "dim unstable=1;dim stable=1;X_L=0.25;Y_L=-1.5"
        form = _captured_form(m, "heteroclinic",
                              left=np.array([0.25, -1.5], dtype=np.float32),
                              right={"X": np.float64(2.0), "Y": 3})
        assert form == "X_L=0.25;Y_L=-1.5;X_R=2.0;Y_R=3.0"


def test_homoclinic_form_too_long_for_the_engine_is_refused(tmp_path):
    with _load(tmp_path, HOM) as m:
        m.auto._run = lambda *a, **k: pytest.fail("must not run")
        big = 1.2345678901234567e-100
        with pytest.raises(ValueError, match="511"):
            # A 401-digit dimension pushes the form past 511 bytes.
            m.auto.start("heteroclinic", "a", second="b",
                         left=[big, big], right=[big] * 2,
                         unstable=10**400)


# The bistable front: u'' + c u' + u(1-u)(u-a) = 0 has a heteroclinic orbit
# from the saddle (1, 0) to the saddle (0, 0) exactly on c = sqrt(2) (1/2 - a),
# the front u = 1/(1 + exp(z/sqrt(2))), whose shape does not depend on a:
# v = u' = -u(1-u)/sqrt(2), with its minimum -1/(4 sqrt(2)) at u = 1/2.
# Both ends are saddles for 0 < a < 1; for a < 0 the end (0, 0) is a node,
# where a saddle-to-saddle connection no longer describes the front.
A0 = 0.25
Z = 20.0  # the start runs from z = -Z to z = Z along the exact front
U0 = 1 / (1 + math.exp(-Z / math.sqrt(2)))
FRONT = f"""\
u'=v
v'=-c*v-u*(1-u)*(u-a)
par a={A0},c={math.sqrt(2) * (0.5 - A0)!r}
init u={U0!r},v={-U0 * (1 - U0) / math.sqrt(2)!r}
@ total={2 * Z},dt=0.01,tol=1e-10,atol=1e-10
@ parmin=-1,parmax=2,dsmin=1e-6,dsmax=0.05,ntst=60,npr=10,nmax=60
done
"""


# Found by running: the continued points stay within 9e-11 of the curve, the
# starting run under each method; the orbit's minimum of v is within 8e-5 of
# the closed form (the mesh of 60 intervals).
@pytest.mark.parametrize("meth, ds", [("rungekutta", -0.01),
                                      ("rungekutta", 0.01),
                                      ("adams", -0.01), ("stiff", -0.01),
                                      ("cvode", -0.01)])
def test_heteroclinic_start_follows_the_front(tmp_path, meth, ds):
    with _load(tmp_path, FRONT) as m:
        m.set(meth=meth)
        sol = m.run()
        # the start must leave the saddle (1, 0) and reach (0, 0): CVODE
        # does so only at a tight tolerance, which the model sets
        assert sol["U"][-1] < 1e-5
        h = m.auto.start("heteroclinic", "c", second="a", left=[1, 0],
                         right=[0, 0], unstable=1, stable=1, ds=ds)
        assert len(h) >= 20
        c = np.array([p.value for p in h])
        a = np.array([p.value2 for p in h])
        assert a[0] == pytest.approx(A0)
        saddles = (a > 0.02) & (a < 0.48)
        assert saddles.sum() >= 8
        exact = np.sqrt(2) * (0.5 - a)
        assert np.abs(c - exact)[saddles].max() < 1e-8
        for p in [p for p, s in zip(h, saddles) if s]:
            assert p.period == pytest.approx(2 * Z)
            assert p.umin["V"] == pytest.approx(-1 / (4 * math.sqrt(2)),
                                                abs=2e-4)
            assert p.u0["U"] == pytest.approx(1, abs=1e-5)
        # it runs on along the curve in the direction of ds
        assert (a.max() > 0.45) if ds < 0 else (a.min() < 0.05)
