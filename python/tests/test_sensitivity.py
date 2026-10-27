"""Model.sensitivity (variational equations) against central finite
differences of repeated Model.run calls."""
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs

ROOT = Path(__file__).resolve().parents[2]
ODE = ROOT / "engine" / "xppaut" / "ode"

LOTKA = """\
# Lotka-Volterra with a carrying capacity, a fixed variable and an if
x'=a*x*(1-x/k)-b*x*y
y'=-c*y+d*x*y
par a=1,b=0.5,c=0.75,d=0.25,k=20
init x=2,y=1
@ meth=rungekutta,total=10,dt=0.01
done
"""

FIXED = """\
# a fixed variable, a user function, a power and an if/then/else
sq(u)=u^2+1
g=sq(x)*p
x'=-g*x+q*y+if(x>0)then(r*y)else(0)
y'=sin(x)-y^1.5/(1+q)
par p=0.3,q=0.5,r=0.2
init x=1,y=1
@ meth=rungekutta,total=5,dt=0.01
done
"""

# The engine hands back its output in single precision (about 6e-8), so a
# central difference with step h carries a rounding error near 6e-8 / h of
# the values, plus a truncation error of order h^2. With h = 1e-3 times the
# parameter both stay below 1e-4 for these models; the test allows 5e-4 of
# the largest sensitivity in each column. (Over a long stretch of a limit
# cycle the sensitivities grow and the truncation error of the difference
# grows with them, so the lecar run is short.)
RTOL = 5e-4


def finite_difference(m, name, h):
    p0 = float(m.get(name))
    step = h * max(abs(p0), 1.0)
    m.set(**{name: p0 + step})
    up = m.run()
    m.set(**{name: p0 - step})
    dn = m.run()
    m.set(**{name: p0})
    return np.column_stack([(up[v] - dn[v]) / (2 * step) for v in m.variables])


def check(m, names, h=1e-3):
    s = m.sensitivity(names)
    base = m.run()
    assert np.array_equal(s.t, base.t)
    for name in names:
        fd = finite_difference(m, name, h)
        got = s[name]
        assert got.shape == fd.shape
        assert np.abs(got).max() > 1e-3, f"{name}: sensitivity is trivially zero"
        scale = np.abs(fd).max(axis=0) + 1e-12
        err = (np.abs(got - fd) / scale).max()
        assert err < RTOL, f"{name}: relative error {err:.2e}"


def test_lecar():
    with xs.load(ODE / "lecar.ode") as m:
        m.set(total=10, dt=0.05, meth="rungekutta", iapp=0.1)
        check(m, ["iapp", "gk", "phi", "vl", "v3"])


def test_lotka_volterra():
    with xs.Model.from_text(LOTKA) as m:
        check(m, ["a", "b", "c", "d", "k"])


def test_fixed_variables_functions_and_if():
    with xs.Model.from_text(FIXED) as m:
        check(m, ["p", "q", "r"])


def test_default_is_every_parameter_and_names_ignore_case():
    with xs.Model.from_text(LOTKA) as m:
        s = m.sensitivity()
        assert sorted(s) == ["A", "B", "C", "D", "K"]
        assert s["a"] is s["A"]
        assert s["A"].shape == (len(s.t), 2)
        assert np.all(s["A"][0] == 0)


def test_current_state_and_options_are_used():
    with xs.Model.from_text(LOTKA) as m:
        m.set(b=0.6, x=3.0, total=4)
        s = m.sensitivity("b")
        run = m.run()
        assert len(s.t) == len(run)
        np.testing.assert_allclose(s.trajectory["X"], run["X"], rtol=1e-9)
        assert m.get("b") == 0.6 and m.get("x") == 3.0  # the model is untouched


def test_unsupported_models_raise():
    with xs.Model.from_text("x'=delay(x,1)-x\npar a=1\ninit x=1\ndone\n") as m:
        with pytest.raises(ValueError, match="delay"):
            m.sensitivity("a")
    with xs.Model.from_text(LOTKA) as m:
        with pytest.raises(KeyError):
            m.sensitivity("nope")


def test_initial_condition_line_x0():
    text = LOTKA.replace("init x=2,y=1", "x(0)=2" + chr(10) + "y(0)=1")
    with xs.Model.from_text(text) as m:
        assert m.get("y") == 1.0
        s = m.sensitivity("b")
        assert np.abs(s["b"]).max() > 1e-3
        assert np.all(s["b"][0] == 0)
