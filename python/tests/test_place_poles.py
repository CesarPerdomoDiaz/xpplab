import warnings
from pathlib import Path

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


FOUR = """
par u1=0, u2=0, u3=0
a'=b+u1
b'=-a+0.3*c+u2
c'=d-0.2*a+u3
d'=-c+0.5*b
init a=0, b=0, c=0, d=0
@ total=10
done
"""


def test_spare_inputs_keep_real_poles_real():
    # with three inputs on four states each real pole has a three-dimensional
    # choice of eigenvector, which may come out complex: the gain must not
    m = xs.Model.from_text(FOUR)
    lin = m.linearize(at={"a": 0.0, "b": 0.0, "c": 0.0, "d": 0.0},
                      inputs=["u1", "u2", "u3"])
    for poles in ([-1.0, -2.0, -3.0, -4.0], [-1.0, -2.0, -1.5 + 1j, -1.5 - 1j]):
        K = lin.place_poles(poles)
        assert K.shape == (3, 4) and K.dtype == float
        check_poles(lin, K, poles)
    # the same on random linear systems, where it used to fail now and then
    rng = np.random.default_rng(1)
    poles = [-1.0, -2.0, -3.0, -4.0, -2.5 + 0.5j, -2.5 - 0.5j]
    for _ in range(10):
        A, B = rng.normal(size=(6, 6)), rng.normal(size=(6, 5))
        rhs = ["+".join([f"({float(a)!r})*x{j}" for j, a in enumerate(rowa)]
                        + [f"({float(b)!r})*u{j}" for j, b in enumerate(rowb)])
               for rowa, rowb in zip(A, B)]
        text = ("par " + ", ".join(f"u{j}=0" for j in range(5)) + "\n"
                + "".join(f"x{i}'={r}\n" for i, r in enumerate(rhs))
                + "done\n")
        lin = xs.Model.from_text(text).linearize(
            at={f"x{i}": 0.0 for i in range(6)},
            inputs=[f"u{j}" for j in range(5)])
        check_poles(lin, lin.place_poles(poles), poles)


def test_closed_loop_keeps_session_state():
    m = xs.Model.from_text(THREE)
    m.set(a=2.0, total=7.0)
    m.ics["y"] = 0.25
    lin = m.linearize(at={"x": 0.0, "y": 0.0, "z": 0.0}, inputs=["u1", "u2"])
    cl = lin.closed_loop(lin.place_poles([-1.0, -2.0, -3.0]))
    assert cl.get("a") == 2.0
    assert cl.get("total") == 7.0
    assert cl.ics["y"] == 0.25


# the input feeds a fixed variable that is written before the equations
FEEDS_FIXED = """
par u=0, c=0.5
drive=u*(1+x*x)
x'=v
v'=2*x-c*v+drive
init x=0.2, v=0
@ total=20, dt=0.01
done
"""


def test_closed_loop_input_feeding_a_fixed_variable():
    m = xs.Model.from_text(FEEDS_FIXED)
    lin = m.linearize(at={"x": 0.0, "v": 0.0}, inputs=["u"])
    K = lin.place_poles([-2.0, -3.0])
    cl = lin.closed_loop(K)
    # the closed loop must equal the open loop with u = -K (x - x*) put in by hand
    for state in ([0.2, 0.0], [-0.4, 0.7]):
        x, v = state
        u = -float(K[0, 0]) * x - float(K[0, 1]) * v
        m.set(u=u)
        np.testing.assert_allclose(cl.rhs(state), m.rhs(state), rtol=1e-12, atol=1e-12)
    cl.ics["x"], cl.ics["v"] = 0.2, 0.0
    sol = cl.run()
    assert abs(sol["x"][-1]) < 1e-6 and abs(sol["v"][-1]) < 1e-6


# a map with a fixed point away from zero: x* = -0.5, y* = 0
MAP_OFFSET = """
par u=0
x(t+1)=1.2*x+0.5*y+u+0.1
y(t+1)=0.9*y+0.3*u
init x=-0.3, y=0.2
@ meth=discrete, total=80
done
"""


def test_closed_loop_of_a_map_settles_to_its_fixed_point():
    m = xs.Model.from_text(MAP_OFFSET)
    lin = m.linearize(at={"x": -0.5, "y": 0.0}, inputs=["u"])
    assert lin.discrete and lin.residual < 1e-12
    K = lin.place_poles([0.2, -0.3])
    check_poles(lin, K, [0.2, -0.3])
    cl = lin.closed_loop(K)
    assert cl.get("METH") == "discrete"
    sol = cl.run()
    np.testing.assert_allclose([sol["x"][-1], sol["y"][-1]], [-0.5, 0.0], atol=1e-9)
    # the open map is unstable (pole 1.2): it leaves the fixed point
    assert abs(m.run()["x"][-1] + 0.5) > 1.0


# an equilibrium with a nonzero state and a nonzero nominal input:
# x'' = -sin(x) - 0.3 x' + u, held at sin(x*) = u0 = 0.8
HELD = """
par u=0.8
x'=v
v'=-sin(x)-0.3*v+u
init x=0.9, v=0
@ total=30, dt=0.01
done
"""


def test_closed_loop_around_a_nonzero_equilibrium():
    m = xs.Model.from_text(HELD)
    lin = m.linearize(inputs=["u"])
    xstar = float(np.arcsin(0.8))
    assert lin.point["X"] == pytest.approx(xstar, abs=1e-6)
    assert lin.point["V"] == pytest.approx(0.0, abs=1e-6)
    K = lin.place_poles([-2.0, -3.0])
    check_poles(lin, K, [-2.0, -3.0])
    cl = lin.closed_loop(K)
    # at the equilibrium the feedback law gives back u0, so nothing moves
    np.testing.assert_allclose(cl.rhs([xstar, 0.0]), [0.0, 0.0], atol=1e-5)  # the equilibrium is found to ~1e-6
    cl.ics["x"], cl.ics["v"] = xstar + 0.4, 0.0
    sol = cl.run()
    assert sol["x"][-1] == pytest.approx(xstar, abs=1e-6)
    assert sol["v"][-1] == pytest.approx(0.0, abs=1e-6)


def test_parameter_line_keyword_p_and_a_clear_error():
    ode = "p c=0.5, u=0\nx'=-c*x+u\ninit x=1\ndone\n"
    lin = xs.Model.from_text(ode).linearize(at={"x": 0.0}, inputs=["u"])
    cl = lin.closed_loop(lin.place_poles([-2.0]))
    assert "u" not in cl._parameter_names and cl._parameter_names == ["c"]
    # an input the file does not declare on a parameter line
    lin2 = xs.Model.from_text("par c=0.5\nx'=-c*x\ninit x=1\ndone\n").linearize(
        at={"x": 0.0}, inputs=["c"])
    lin2._model.path.write_text("c=0.5\nx'=-c*x\ninit x=1\ndone\n")
    with pytest.raises(ValueError, match=r"^input 'c' is not declared on a line "
                                         r"starting with p, par, param or parameters"):
        lin2.closed_loop(np.array([[1.0]]))


ODE_DIR = Path(__file__).resolve().parents[2] / "engine" / "xppaut" / "ode"


@pytest.mark.parametrize("name, inp, poles", [
    ("lecar.ode", "iapp", [-0.5, -1.0]),
    ("lecar.ode", "iapp", [-0.6 + 0.4j, -0.6 - 0.4j]),
    ("fhn.ode", "a", [-0.5, -1.0]),
    ("fhn.ode", "a", [-0.6 + 0.4j, -0.6 - 0.4j]),
])
def test_shipped_models_at_their_equilibrium(name, inp, poles):
    m = xs.load(ODE_DIR / name)
    lin = m.linearize(inputs=[inp])
    assert lin.controllable and lin.residual < 1e-6
    with warnings.catch_warnings():
        warnings.simplefilter("error")  # an equilibrium must not warn
        K = lin.place_poles(poles)
    check_poles(lin, K, poles)
    cl = lin.closed_loop(K)
    for v, x in lin.point.items():
        cl.ics[v] = x + 1e-3
    cl.set(total=150)
    sol = cl.run()
    for v, x in lin.point.items():
        assert abs(sol[v][-1] - x) < 1e-6


def test_warns_when_the_point_is_not_an_equilibrium():
    m = xs.Model.from_text(PENDULUM)
    lin = m.linearize(at={"th": 0.5, "w": 0.0}, inputs=["u"])
    assert lin.residual > 1.0
    with pytest.warns(UserWarning, match=r"^the point is not an equilibrium "
                                         r"\(residual [0-9.]+\)"):
        K = lin.place_poles([-2.0, -3.0])
    # the warning is right: the closed loop does not stay at the point
    cl = lin.closed_loop(K)
    cl.ics["th"], cl.ics["w"] = 0.5, 0.0
    assert abs(cl.run()["th"][-1] - 0.5) > 0.3


def test_backslash_only_matters_on_the_inputs_own_line():
    # the engine reads no backslash continuation on a par line, so the line
    # rewriter is tested on the text itself
    from xpplab.control import _LAW_HERE, _remove_parameter
    lines = ["par c=0.5, \\", "  d=1", "par u=0, k=2", "x'=-c*x+u"]
    assert _remove_parameter(lines, "u") == [
        "par c=0.5, \\", "  d=1", "par k=2", _LAW_HERE, "x'=-c*x+u"]
    with pytest.raises(ValueError, match=r"^the parameter line of an input is "
                                         r"continued with a backslash"):
        _remove_parameter(["par u=0, \\", "  k=2"], "u")


