import numpy as np
import pytest

import xpplab as xs

OSC = """
# damped oscillator x'' + c x' + k x = u
par c=0.5, k=4, u=0
x'=v
v'=-k*x-c*v+u
init x=0.1, v=0
@ total=10
done
"""

DOUBLE = """
par u=0
x'=v
v'=u
init x=0, v=0
@ total=10
done
"""

# the second state is driven by nothing: (A, B) is not controllable
UNCONTROLLABLE = """
par u=0
x'=-x+u
y'=-2*y
init x=0, y=0
@ total=10
done
"""

NONLINEAR = """
par a=1, b=2, u=0
x'=-a*x+x*y*y+u
y'=y-b*y^3+a*u
init x=0.5, y=0.5
@ total=10
done
"""


def test_damped_oscillator():
    m = xs.Model.from_text(OSC)
    lin = m.linearize(inputs=["u"])
    np.testing.assert_allclose(lin.A, [[0, 1], [-4, -0.5]], atol=1e-7)
    np.testing.assert_allclose(lin.B, [[0], [1]], atol=1e-7)
    np.testing.assert_allclose(lin.controllability, [[0, 1], [1, -0.5]], atol=1e-6)
    assert lin.rank == 2 and lin.controllable
    expected = np.roots([1, 0.5, 4])
    np.testing.assert_allclose(sorted(lin.eigenvalues, key=lambda z: z.imag),
                               sorted(expected, key=lambda z: z.imag), atol=1e-6)
    assert lin.residual < 1e-8


def test_double_integrator():
    m = xs.Model.from_text(DOUBLE)
    lin = m.linearize(at={"x": 1.0, "v": 0.0}, inputs=["u"])
    np.testing.assert_allclose(lin.A, [[0, 1], [0, 0]], atol=1e-7)
    np.testing.assert_allclose(lin.B, [[0], [1]], atol=1e-7)
    assert lin.rank == 2 and lin.controllable
    np.testing.assert_allclose(lin.eigenvalues, [0, 0], atol=1e-6)


def test_uncontrollable_pair():
    m = xs.Model.from_text(UNCONTROLLABLE)
    lin = m.linearize(inputs=["u"])
    np.testing.assert_allclose(lin.A, [[-1, 0], [0, -2]], atol=1e-7)
    np.testing.assert_allclose(lin.B, [[1], [0]], atol=1e-7)
    assert lin.rank == 1 and not lin.controllable


def test_no_inputs_is_not_controllable():
    lin = xs.Model.from_text(DOUBLE).linearize(at={"x": 0.0, "v": 0.0})
    assert lin.B.shape == (2, 0) and lin.rank == 0 and not lin.controllable


def test_eigenvalues_match_equilibrium():
    m = xs.Model.from_text(NONLINEAR)
    eq = m.equilibrium({"x": 0.1, "y": 0.7})
    lin = m.linearize(at=eq.values, inputs=["u", "a"])
    key = lambda z: (round(z.real, 6), round(z.imag, 6))  # noqa: E731
    np.testing.assert_allclose(
        sorted(np.linalg.eigvals(lin.A), key=key),
        sorted(eq.eigenvalues, key=key), atol=1e-2)  # XPPAUT's own Jacobian is coarser
    assert lin.B.shape == (2, 2)
    assert lin.residual < 1e-8


def test_parameters_are_restored():
    m = xs.Model.from_text(NONLINEAR)
    m.linearize(inputs=["u", "a"])
    assert m.get("u") == 0 and m.get("a") == 1


def test_unknown_names():
    m = xs.Model.from_text(DOUBLE)
    with pytest.raises(KeyError):
        m.linearize(at={"x": 0.0, "v": 0.0}, inputs=["nope"])
    with pytest.raises(KeyError):
        m.linearize(at={"nope": 1.0}, inputs=["u"])


# The step is chosen by the method, not fixed: the Jacobian matches the
# analytic one on models with very different scales.
@pytest.mark.parametrize("text, at, A, B", [
    # fast oscillation in the state: a step of 1e-6 is fine, 0.1 is not
    ("par w=1000, p=0.5\nx'=sin(w*x)+p*x^2\ninit x=0.3\ndone\n",
     {"x": 0.3}, lambda: [[1000 * np.cos(300.0) + 2 * 0.5 * 0.3]], lambda: [[0.09]]),
    # steep exponential
    ("par p=2\nx'=exp(-50*x)*p\ninit x=0.5\ndone\n",
     {"x": 0.5}, lambda: [[-100 * np.exp(-25.0)]], lambda: [[np.exp(-25.0)]]),
    # large state and parameter
    ("par p=1e6\nx'=-1e-6*x^2+1e-6*p*x\ninit x=1e6\ndone\n",
     {"x": 1e6}, lambda: [[-2.0 + 1.0]], lambda: [[1.0]]),
    # tiny state
    ("par p=1\nx'=1e6*x^2*p\ninit x=1e-6\ndone\n",
     {"x": 1e-6}, lambda: [[2.0]], lambda: [[1e6 * 1e-12]]),
])
def test_step_adapts_to_the_model(text, at, A, B):
    lin = xs.Model.from_text(text).linearize(at=at, inputs=["p"])
    np.testing.assert_allclose(lin.A, A(), rtol=1e-6, atol=1e-12)
    np.testing.assert_allclose(lin.B, B(), rtol=1e-6, atol=1e-12)
    assert np.all(lin.A_error < 1e-4 * np.maximum(1, np.abs(lin.A)))


def test_fixed_step_option():
    m = xs.Model.from_text(DOUBLE)
    lin = m.linearize(at={"x": 1.0, "v": 0.0}, inputs=["u"], step=1e-4)
    np.testing.assert_allclose(lin.A, [[0, 1], [0, 0]], atol=1e-7)
    assert np.all(np.isnan(lin.A_error))


def test_linear_model_is_exact_for_a_linear_system():
    lin = xs.Model.from_text(OSC).linearize(inputs=["u"])
    assert lin.error([0.5, -0.3], [2.0]) < 1e-6
    assert lin.valid_radius() == float("inf")


def test_valid_radius_shrinks_with_nonlinearity():
    text = "par k=%g, u=0\nx'=-x-k*x^3+u\ninit x=0\ndone\n"
    r_weak = xs.Model.from_text(text % 1).linearize(inputs=["u"]).valid_radius()
    r_strong = xs.Model.from_text(text % 100).linearize(inputs=["u"]).valid_radius()
    assert 0 < r_strong < r_weak < float("inf")
    # the error crosses tol (0.05) near the radius: x^3 term over x
    lin = xs.Model.from_text(text % 100).linearize(inputs=["u"])
    assert lin.error([r_strong * 10]) > 0.05 > lin.error([r_strong * 0.5])


def test_nonsmooth_point_warns():
    m = xs.Model.from_text("x'=if(x>0)then(1)else(0)\ninit x=0\ndone\n")
    with pytest.warns(UserWarning, match="not be smooth"):
        m.linearize(at={"x": 0.0})


def test_eigenvalues_at_a_point_are_those_of_A():
    m = xs.Model.from_text(NONLINEAR)
    lin = m.linearize(at={"x": 0.3, "y": -0.2}, inputs=["u"])
    key = lambda z: (round(z.real, 8), round(z.imag, 8))  # noqa: E731
    np.testing.assert_allclose(sorted(lin.eigenvalues, key=key),
                               sorted(np.linalg.eigvals(lin.A), key=key),
                               rtol=1e-9, atol=1e-9)
    assert lin.residual > 0.01  # not an equilibrium


def test_singular_values_are_those_of_the_controllability_matrix():
    lin = xs.Model.from_text(NONLINEAR).linearize(inputs=["u", "a"])
    np.testing.assert_allclose(lin.singular_values,
                               np.linalg.svd(lin.controllability, compute_uv=False),
                               rtol=1e-12)
    np.testing.assert_allclose(lin.controllability[:, :2], lin.B)
    np.testing.assert_allclose(lin.controllability[:, 2:], lin.A @ lin.B, rtol=1e-12)


MAP = """
# the delayed logistic map
par r=1.9, eps=0
y(t+1)=x
x(t+1)=r*x*(1-y)+eps
init x=0.5, y=0.5
@ meth=discrete, total=50
done
"""


def test_map_is_linearised_as_a_map():
    m = xs.Model.from_text(MAP)
    lin = m.linearize(inputs=["eps"])
    xstar = 1 - 1 / 1.9
    np.testing.assert_allclose([lin.point["X"], lin.point["Y"]], [xstar, xstar], atol=1e-6)
    # F(x, y) = (x, r x (1 - y)): rows y, x as the model declares them
    np.testing.assert_allclose(lin.A, [[0, 1], [-1.9 * xstar, 1.9 * (1 - xstar)]], atol=1e-6)
    np.testing.assert_allclose(lin.B, [[0], [1]], atol=1e-9)
    assert lin.discrete and lin.residual < 1e-6
    # |eigenvalue|^2 = det A = r - 1: stable for r < 2
    np.testing.assert_allclose(np.abs(lin.eigenvalues) ** 2, [0.9, 0.9], rtol=1e-6)


def test_to_csv(tmp_path):
    m = xs.Model.from_text(OSC)
    lin = m.linearize(inputs=["u"])
    path = tmp_path / "lin.csv"
    lin.to_csv(path)
    lines = path.read_text().splitlines()
    assert lines[0] == "state,at,A_X,A_V,B_u,eig_re,eig_im,rank,residual"
    first, second = (line.split(",") for line in lines[1:])
    assert first[0] == "X" and second[0] == "V" and len(lines) == 3
    assert [float(c) for c in first[2:5]] == [lin.A[0, 0], lin.A[0, 1], lin.B[0, 0]]
    assert complex(float(second[5]), float(second[6])) == lin.eigenvalues[1]
    assert first[7] == "2" and float(first[8]) == lin.residual
    assert second[7:] == ["", ""]


def test_overflowing_controllability_matrix():
    text = "par u=0\nx'=1e200*y\ny'=1e200*z\nz'=u\ninit x=0, y=0, z=0\ndone\n"
    with pytest.warns(UserWarning, match="rank could not be computed"):
        lin = xs.Model.from_text(text).linearize(at={"x": 0, "y": 0, "z": 0},
                                                 inputs=["u"])
    assert lin.rank == -1 and not lin.controllable
    np.testing.assert_allclose(lin.A[0, 1], 1e200)


def test_step_must_be_positive():
    with pytest.raises(ValueError):
        xs.Model.from_text(OSC).linearize(inputs=["u"], step=0)


def test_plot():
    matplotlib = pytest.importorskip("matplotlib")
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    lin = xs.Model.from_text(OSC).linearize(inputs=["u"])
    ax = lin.plot()
    x0, x1 = ax.get_xlim()
    assert x0 < -0.25 < 0 < x1  # the eigenvalues and the imaginary axis
    assert ax.get_ylim()[1] > 1.98
    assert any(line.get_label() == "eigenvalues" for line in ax.lines)
    plt.close(ax.figure)
    ax = xs.Model.from_text(MAP).linearize().plot()
    assert ax.get_xlim()[0] < -1 and ax.get_xlim()[1] > 1  # the unit circle
    plt.close(ax.figure)
