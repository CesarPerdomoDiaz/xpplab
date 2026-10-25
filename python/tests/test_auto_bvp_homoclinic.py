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
    longest = max(orbits, key=lambda p: p.period)
    return longest, labelled[0]


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
