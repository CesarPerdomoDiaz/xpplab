from pathlib import Path

import numpy as np
import pytest

import xpplab as xs

ODE_DIR = Path(__file__).resolve().parents[2] / "engine" / "xppaut" / "ode"

PENDULUM = """
par g=9.8, l=1, u=0
th'=w
w'=g/l*sin(th)-0.2*w+u
init th=0, w=0
@ total=20, dt=0.01
done
"""

# x'' + c x' + k x = 0: as k grows the two real poles meet at k = c^2/4 and
# leave the real axis; the real part stays at -c/2
SPRING = """
par k=0.1, c=2, u=0
x'=v
v'=-k*x-c*v+u
init x=0, v=0
done
"""


def test_gain_locus_from_open_loop_to_the_placed_poles():
    m = xs.Model.from_text(PENDULUM)
    lin = m.linearize(at={"th": 0, "w": 0}, inputs=["u"])
    K = lin.place_poles([-2 + 1j, -2 - 1j])
    rl = lin.root_locus(K, gain=(0, 1), steps=101)
    assert rl.poles.shape == (101, 2) and rl.values[0] == 0 and rl.values[-1] == 1
    np.testing.assert_allclose(np.sort_complex(rl.poles[0]),
                               np.sort_complex(lin.eigenvalues), atol=1e-9)
    np.testing.assert_allclose(np.sort_complex(rl.poles[-1]),
                               np.sort_complex(np.array([-2 - 1j, -2 + 1j])), atol=1e-9)
    # each column follows one eigenvalue: from step to step it keeps the
    # closer of the two ways to pair them
    same = np.abs(np.diff(rl.poles, axis=0)).sum(axis=1)
    swapped = np.abs(rl.poles[1:, ::-1] - rl.poles[:-1]).sum(axis=1)
    assert np.all(same <= swapped + 1e-12)
    # the unstable pole comes into the left half-plane once, where
    # det(A - g B K) = 0: 9.8 - 14.8 g = 0
    (g, z), = rl.crossings()
    assert abs(g - 9.8 / 14.8) < 1e-3 and abs(z) < 0.1
    assert "gain" in repr(rl)


def test_gain_locus_of_one_state_feedback_and_its_csv(tmp_path):
    m = xs.Model.from_text(SPRING)
    lin = m.linearize(at={"x": 0, "v": 0}, inputs=["u"])
    # u = -g x adds g to k: the poles meet at k + g = 1, g = 0.9
    rl = lin.root_locus([[1, 0]], gain=(0, 3), steps=301)
    i = int(np.argmin(np.abs(rl.values - 2.0)))
    np.testing.assert_allclose(rl.poles[i].real, [-1, -1], atol=1e-9)
    assert rl.crossings() == []
    rl.to_csv(tmp_path / "rl.csv")
    lines = (tmp_path / "rl.csv").read_text().splitlines()
    assert lines[0] == "gain,re1,im1,re2,im2" and len(lines) == 302
    with pytest.raises(ValueError, match="K must have shape"):
        lin.root_locus([[1, 0, 0]])


def test_parameter_locus_finds_a_hopf_point_of_morris_lecar():
    m = xs.load(ODE_DIR / "lecar.ode")
    m.set(iapp=0.1)
    rl = m.root_locus("iapp", (0.1, 0.3), steps=81)
    assert rl.name.lower() == "iapp" and rl.poles.shape == (81, 2)
    assert rl.x.shape == (81, 2) and np.all(np.isfinite(rl.x))
    assert float(m.get("iapp")) == 0.1  # restored
    # the equilibrium's complex pair crosses the imaginary axis in between
    hopf = [(v, z) for v, z in rl.crossings() if abs(z.imag) > 1e-3]
    assert hopf and 0.1 < hopf[0][0] < 0.3
    # at that value the equilibrium has eigenvalues on the imaginary axis
    m.set(iapp=hopf[0][0])
    ev = m.equilibrium(guess=dict(zip(m.variables, rl.x[40]))).eigenvalues
    assert min(abs(ev.real)) < 2e-3


def test_parameter_locus_of_a_model_with_a_markov_variable():
    # kepler.ode has two ODEs and a Markov variable z: the locus has two
    # branches and the equilibrium has the two ODE variables
    m = xs.load(ODE_DIR / "kepler.ode")
    rl = m.root_locus("eps", (0.05, 0.5), steps=11)
    assert rl.poles.shape == (11, 2) and rl.x.shape == (11, 2)
    assert [v.upper() for v in rl.variables] == [v.upper() for v in m.variables[:2]]


def test_parameter_locus_against_the_exact_poles():
    m = xs.Model.from_text(SPRING)
    rl = m.root_locus("k", (0.1, 4), steps=40)
    # (-c +- sqrt(c^2 - 4k)) / 2, real until k = 1, then a complex pair
    k = rl.values
    want = np.stack([(-2 + np.sqrt(4 - 4 * k + 0j)) / 2,
                     (-2 - np.sqrt(4 - 4 * k + 0j)) / 2], axis=1)
    for row, w in zip(rl.poles, want):
        np.testing.assert_allclose(np.sort_complex(row), np.sort_complex(w), atol=1e-6)
    with pytest.raises(KeyError):
        m.root_locus("nope", (0, 1))
