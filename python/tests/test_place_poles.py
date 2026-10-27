import numpy as np
import pytest

import xpplab as xs

# unstable inverted-pendulum-like system, one input
PENDULUM = """
par g=9.8, l=1, u=0
th'=w
w'=g/l*sin(th)-0.2*w+u
init th=0, w=0
@ total=20, dt=0.01
done
"""

# three states, two inputs; a weak nonlinear term
THREE = """
par a=1, u1=0, u2=0
x'=-a*x+y+u1
y'=z+0.1*x*x
z'=0.5*x-z+y+u2
init x=0, y=0, z=0
@ total=20, dt=0.01
done
"""

UNCONTROLLABLE = """
par u=0
x'=-x+u
y'=-2*y
init x=0, y=0
@ total=10
done
"""

# a map x(t+1) = 1.2 x + 0.5 y + u, y(t+1) = 0.9 y + 0.3 u
MAP = """
par u=0
x(t+1)=1.2*x+0.5*y+u
y(t+1)=0.9*y+0.3*u
init x=0, y=0
@ meth=discrete, total=60
done
"""


def sorted_poles(z):
    return sorted(np.asarray(z, dtype=complex), key=lambda w: (w.real, w.imag))


def check_poles(lin, K, poles):
    got = np.linalg.eigvals(lin.A - lin.B @ K)
    np.testing.assert_allclose(sorted_poles(got), sorted_poles(poles), atol=1e-6)


def test_single_input_ackermann_real_and_complex():
    m = xs.Model.from_text(PENDULUM)
    lin = m.linearize(at={"th": 0.0, "w": 0.0}, inputs=["u"])
    assert lin.controllable
    for poles in ([-2.0, -3.0], [-1 + 2j, -1 - 2j]):
        K = lin.place_poles(poles)
        assert K.shape == (1, 2)
        check_poles(lin, K, poles)


def test_two_inputs_kautsky():
    m = xs.Model.from_text(THREE)
    lin = m.linearize(at={"x": 0.0, "y": 0.0, "z": 0.0}, inputs=["u1", "u2"])
    assert lin.controllable
    for poles in ([-1.0, -2.0, -3.0], [-2.0, -1 + 1j, -1 - 1j]):
        K = lin.place_poles(poles)
        assert K.shape == (2, 3)
        check_poles(lin, K, poles)


def test_map_poles_inside_unit_circle():
    m = xs.Model.from_text(MAP)
    lin = m.linearize(at={"x": 0.0, "y": 0.0}, inputs=["u"])
    assert lin.discrete and lin.controllable
    K = lin.place_poles([0.2, -0.3])
    check_poles(lin, K, [0.2, -0.3])


def test_uncontrollable_raises_clear_error():
    lin = xs.Model.from_text(UNCONTROLLABLE).linearize(inputs=["u"])
    with pytest.raises(ValueError, match=r"^cannot place poles: the pair \(A, B\) "
                                         r"is not controllable \(rank 1 of 2 states\)"):
        lin.place_poles([-1, -2])


def test_no_inputs_and_bad_poles():
    m = xs.Model.from_text(PENDULUM)
    lin0 = m.linearize(at={"th": 0.0, "w": 0.0})
    with pytest.raises(ValueError, match="no inputs"):
        lin0.place_poles([-1, -2])
    lin = m.linearize(at={"th": 0.0, "w": 0.0}, inputs=["u"])
    with pytest.raises(ValueError, match="need 2 poles"):
        lin.place_poles([-1])
    with pytest.raises(ValueError, match="no conjugate partner"):
        lin.place_poles([-1 + 1j, -2])


def test_closed_loop_settles_to_equilibrium():
    m = xs.Model.from_text(PENDULUM)
    lin = m.linearize(at={"th": 0.0, "w": 0.0}, inputs=["u"])
    K = lin.place_poles([-2 + 1j, -2 - 1j])
    cl = lin.closed_loop(K)
    assert "u" not in cl._parameter_names  # the input is now the feedback law
    cl.ics["th"] = 0.3
    cl.ics["w"] = 0.0
    sol = cl.run()
    assert abs(sol["th"][-1]) < 1e-6 and abs(sol["w"][-1]) < 1e-6
    # the open loop falls away from the upright position
    m.ics["th"] = 0.3
    assert abs(m.run()["th"][-1]) > 1.0


def test_closed_loop_two_inputs():
    m = xs.Model.from_text(THREE)
    point = {"x": 0.0, "y": 0.0, "z": 0.0}
    lin = m.linearize(at=point, inputs=["u1", "u2"])
    K = lin.place_poles([-1.0, -2.0, -3.0])
    cl = lin.closed_loop(K)
    for v, d in zip("xyz", (0.1, -0.1, 0.05)):
        cl.ics[v] = d
    sol = cl.run()
    for v in "xyz":
        assert abs(sol[v][-1]) < 1e-5


FULL = """
par u1=0, u2=0
x'=y+u1
y'=-x+0.5*y+u2
init x=0, y=0
@ total=10
done
"""


def test_as_many_inputs_as_states_and_more():
    m = xs.Model.from_text(FULL)
    lin = m.linearize(at={"x": 0.0, "y": 0.0}, inputs=["u1", "u2"])
    for poles in ([-1.0, -2.0], [-1 + 1j, -1 - 1j], [-1.0, -1.0]):
        K = lin.place_poles(poles)
        assert K.shape == (2, 2)
        check_poles(lin, K, poles)
    # three inputs on two states: one input is a copy of another
    m3 = xs.Model.from_text(FULL.replace("par u1=0, u2=0", "par u1=0, u2=0, u3=0")
                            .replace("y+u1", "y+u1+u3"))
    lin3 = m3.linearize(at={"x": 0.0, "y": 0.0}, inputs=["u1", "u2", "u3"])
    assert lin3.B.shape == (2, 3)
    check_poles(lin3, lin3.place_poles([-3.0, -4.0]), [-3.0, -4.0])


def test_closed_loop_keeps_session_state():
    m = xs.Model.from_text(THREE)
    m.set(a=2.0, total=7.0)
    m.ics["y"] = 0.25
    lin = m.linearize(at={"x": 0.0, "y": 0.0, "z": 0.0}, inputs=["u1", "u2"])
    cl = lin.closed_loop(lin.place_poles([-1.0, -2.0, -3.0]))
    assert cl.get("a") == 2.0
    assert cl.get("total") == 7.0
    assert cl.ics["y"] == 0.25
