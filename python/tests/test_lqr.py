import numpy as np
import pytest

import xpplab as xs

DOUBLE_INTEGRATOR = """
par u=0
x'=v
v'=u
init x=0, v=0
done
"""

PENDULUM = """
par g=9.8, l=1, u=0
th'=w
w'=g/l*sin(th)-0.2*w+u
init th=0, w=0
@ total=20, dt=0.01
done
"""

# x(n+1) = 2 x(n) + u: the discrete Riccati equation p^2 - 4p - 1 = 0 gives
# p = 2 + sqrt(5) and K = 2p / (1 + p), the golden ratio
MAP = """
par u=0
x(t+1)=2*x+u
init x=0
@ meth=discrete, total=20
done
"""


def test_double_integrator_known_gain():
    m = xs.Model.from_text(DOUBLE_INTEGRATOR)
    lin = m.linearize(at={"x": 0, "v": 0}, inputs=["u"])
    res = lin.lqr()
    np.testing.assert_allclose(res.K, [[1, np.sqrt(3)]], atol=1e-10)
    np.testing.assert_allclose(res.P, [[np.sqrt(3), 1], [1, np.sqrt(3)]], atol=1e-10)
    # the poles of A - B K: s^2 + sqrt(3) s + 1
    np.testing.assert_allclose(np.sort_complex(res.poles),
                               np.sort_complex(np.roots([1, np.sqrt(3), 1])), atol=1e-10)
    assert "LQR" in repr(res)


def test_map_golden_ratio():
    m = xs.Model.from_text(MAP)
    lin = m.linearize(at={"x": 0}, inputs=["u"])
    assert lin.discrete
    res = lin.lqr()
    np.testing.assert_allclose(res.P, [[2 + np.sqrt(5)]], rtol=1e-12)
    np.testing.assert_allclose(res.K, [[(1 + np.sqrt(5)) / 2]], rtol=1e-12)
    assert abs(res.poles[0]) < 1


def test_weights_forms_and_the_closed_loop():
    m = xs.Model.from_text(PENDULUM)
    lin = m.linearize(at={"th": 0, "w": 0}, inputs=["u"])
    a = lin.lqr(Q=[10, 1], R=[0.1])
    b = lin.lqr(Q=np.diag([10.0, 1.0]), R=0.1)
    np.testing.assert_array_equal(a.K, b.K)
    # the Riccati equation holds: A^T P + P A - P B R^-1 B^T P + Q = 0
    A, B, P = lin.A, lin.B, a.P
    res = A.T @ P + P @ A - P @ B @ B.T @ P / 0.1 + np.diag([10.0, 1.0])
    assert np.max(np.abs(res)) < 1e-8 * np.max(np.abs(P))
    assert np.all(a.poles.real < 0)
    # more weight on the angle: a stiffer gain on it
    assert lin.lqr(Q=[100, 1], R=[0.1]).K[0, 0] > a.K[0, 0]
    cl = lin.closed_loop(a.K)
    cl.ics["th"] = 0.3
    assert abs(cl.run()["th"][-1]) < 1e-6


def test_refusals():
    m = xs.Model.from_text(PENDULUM)
    lin = m.linearize(at={"th": 0, "w": 0}, inputs=["u"])
    with pytest.raises(ValueError, match="Q needs 2 weights"):
        lin.lqr(Q=[1, 2, 3])
    with pytest.raises(ValueError, match="R must be 1 x 1"):
        lin.lqr(R=np.eye(2))
    with pytest.raises(ValueError, match="positive definite"):
        lin.lqr(R=[0])
    with pytest.raises(ValueError, match="symmetric"):
        lin.lqr(Q=[[1, 2], [0, 1]])
    # an unstable mode the input cannot reach
    m2 = xs.Model.from_text("par u=0\nx'=x+u\ny'=y\ndone\n")
    lin2 = m2.linearize(at={"x": 0, "y": 0}, inputs=["u"])
    with pytest.raises(ValueError, match="no stabilising solution"):
        lin2.lqr()
    with pytest.raises(ValueError, match="needs inputs"):
        m.linearize(at={"th": 0, "w": 0}).lqr()
