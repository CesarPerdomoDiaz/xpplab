"""xppe_asymptotic_phase (method A of docs/design/isochrons.md): against the
closed forms of the radial clock and the lambda-omega system, against the
flow, against the adjoint on Morris-Lecar, and its refusals.

The phase is theta/T in [0, 1), where the stored cycle's first point is
theta = 0. Error budget, used by the tolerances below:

* the end state is within delta of the cycle, so without the correction the
  phase is off by about |Z| delta / T, and with it by O(delta^2) (for the
  lambda-omega pair q delta^2 / (2 * 2 pi), from ln r = (r-1) - (r-1)^2/2 + ...);
* the fourth-order Runge-Kutta steps (the default method) of size T/NSTEP
  add nothing measurable;
* Z is XPPAUT's adjoint, implicit Euler on NSTEP points, good to about 1 %
  (see test_phase_response_of_a_radial_clock), so the correction itself has
  a relative error of that size: |Z| delta * 1 % / T;
* the stored cycle is a polyline of spacing T/NSTEP, so its distance from
  the true cycle is below 1e-5.
"""
import ctypes
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs
from xpplab import _engine

ROOT = Path(__file__).resolve().parents[2]
MODELS = ROOT / "tests" / "models"
ODE = ROOT / "engine" / "xppaut" / "ode"

NSTEP = 2000


def set_cycle(m, period):
    """Settle onto the cycle, then leave one period of it, in exactly NSTEP
    steps, as the run: the adjoint needs exactly one period."""
    m.set(DT=period / NSTEP, TOTAL=60.0)
    sol = m.run()
    m.set(**{v: float(sol[v][-1]) for v in m.variables})
    m.set(TOTAL=period)
    m.run()


def call(m, x, delta, tmax=50.0):
    """(error or None, phase, elapsed) of xppe_asymptotic_phase."""
    arr = (ctypes.c_double * len(x))(*x)
    ph, el = ctypes.c_double(-1), ctypes.c_double(-1)
    try:
        m._engine.call("xppe_asymptotic_phase", arr, delta, tmax,
                       ctypes.byref(ph), ctypes.byref(el))
    except xs.EngineError as e:
        return e, ph.value, el.value
    return None, ph.value, el.value


def phase(m, x, delta, tmax=50.0):
    err, ph, el = call(m, x, delta, tmax)
    assert err is None, err
    return ph, el


def circ(a, b):
    """Distance between two phases on the circle of length 1."""
    d = (a - b) % 1.0
    return min(d, 1.0 - d)


@pytest.fixture(scope="module")
def clock():
    with xs.load(MODELS / "radial_clock.ode") as m:
        set_cycle(m, 2 * np.pi)
        m.adjoint()
        orbit = m._collect()
        m.theta0 = float(np.arctan2(orbit["Y"][0], orbit["X"][0]))
        yield m


@pytest.fixture(scope="module")
def lamom():
    with xs.load(MODELS / "lambda_omega.ode") as m:
        m.set(w=1.5, q=1.5)
        set_cycle(m, 2 * np.pi / 1.5)
        m.adjoint()
        orbit = m._collect()
        m.theta0 = float(np.arctan2(orbit["Y"][0], orbit["X"][0]))
        yield m


POINTS = [(r, a) for r in (0.3, 0.6, 0.9, 1.15, 1.6, 2.5)
          for a in (0.4, 2.0, 3.5, 5.9)]


def test_radial_clock(clock):
    """Phase = theta/2pi (mod 1), inside and outside the cycle. The
    isochrons are rays, so the correction (Z is tangential, the offset
    radial) changes nothing; what is left is the integrator, Z's 1 % times
    the offset delta/(2 pi) and the polyline: below 2e-4 for delta = 0.05."""
    worst = 0.0
    for r, a in POINTS:
        x = [r * np.cos(a), r * np.sin(a)]
        ph, el = phase(clock, x, 0.05)
        worst = max(worst, circ(ph, (a - clock.theta0) / (2 * np.pi)))
        assert el >= 0
    assert worst < 2e-4, worst


def test_lambda_omega_has_curved_isochrons(lamom):
    """Phase = (theta - q ln r)/2pi (mod 1) with w = q = 1.5: the offset in
    r is delta = 0.02 at most, the error with the correction is
    q delta^2/(4 pi) = 5e-5 plus Z's 1 % of q delta/(2 pi) = 5e-5: below 3e-4.
    The curve is what the radial clock cannot show: at r = 0.3 the phase
    differs from theta/2pi by q ln(1/0.3)/2pi = 0.29."""
    worst = 0.0
    for r, a in POINTS:
        x = [r * np.cos(a), r * np.sin(a)]
        ph, _ = phase(lamom, x, 0.02)
        expect = (a - 1.5 * np.log(r) - lamom.theta0) / (2 * np.pi)
        worst = max(worst, circ(ph, expect))
    assert worst < 3e-4, worst
    # not the straight rays
    ph, _ = phase(lamom, [0.3 * np.cos(2.0), 0.3 * np.sin(2.0)], 0.02)
    assert circ(ph, (2.0 - lamom.theta0) / (2 * np.pi)) > 0.2


@pytest.mark.parametrize("name, par", [("radial_clock.ode", {}),
                                       ("lambda_omega.ode",
                                        {"w": 1.5, "q": 1.5})])
def test_the_correction_works(name, par):
    """The error with the end-state correction is clearly smaller than
    without it, and it shrinks like delta^2. (For the radial clock the
    correction is zero by symmetry, so only the lambda-omega model is
    checked for the order.)"""
    period = 2 * np.pi / par.get("w", 1.0)
    with xs.load(MODELS / name) as m, xs.load(MODELS / name) as flow:
        m.set(**par)
        flow.set(**par)
        set_cycle(m, period)
        m.adjoint()
        orbit = m._collect()
        gam = np.column_stack([orbit["X"], orbit["Y"]])
        h = period / NSTEP
        theta0 = np.arctan2(gam[0, 1], gam[0, 0])
        q = par.get("q", 0.0)
        flow.set(DT=h)

        def plain(x, s):
            """Phase of the end state by the nearest point of the polyline,
            without Z."""
            flow.set(x=x[0], y=x[1], TOTAL=s)
            end = flow.run()
            e = np.array([end["X"][-1], end["Y"][-1]])
            a, d = gam[:-1], gam[1:] - gam[:-1]
            u = np.clip(((e - a) * d).sum(1) / (d * d).sum(1), 0, 1)
            k = int(np.argmin(((e - a - u[:, None] * d) ** 2).sum(1)))
            return ((k + u[k]) * h - s) / period % 1.0

        errs = {}
        for delta in (0.2, 0.1, 0.05, 0.025):
            ec, ep = [], []
            for r, a in POINTS:
                if r < 0.5:
                    continue
                x = [r * np.cos(a), r * np.sin(a)]
                ph, el = phase(m, x, delta)
                expect = (a - q * np.log(r) - theta0) / (2 * np.pi)
                ec.append(circ(ph, expect))
                ep.append(circ(plain(x, el), expect))
            errs[delta] = (max(ec), max(ep))
        print(name, {d: "%.2e / %.2e" % e for d, e in errs.items()})
        if q == 0:
            return
        for delta, (ec, ep) in errs.items():
            assert ec < ep / 3, (delta, ec, ep)
        # delta^2: halving delta divides the error by about 4
        for d in (0.2, 0.1, 0.05):
            ratio = errs[d][0] / errs[d / 2][0]
            assert 3.3 < ratio < 4.8, (d, ratio)
        # and the uncorrected error shrinks only like delta
        for d in (0.2, 0.1):
            ratio = errs[d][1] / errs[d / 2][1]
            assert 1.5 < ratio < 2.6, (d, ratio)


def test_the_phase_advances_with_the_flow(lamom):
    """phase(flow_t(x)) = phase(x) + t/T (mod 1). Both sides carry the
    error of the method (about 3e-4 at delta = 0.05, see above): below 1e-3."""
    period = 2 * np.pi / 1.5
    with xs.load(MODELS / "lambda_omega.ode") as flow:
        flow.set(w=1.5, q=1.5, DT=period / NSTEP)
        for x in ([0.4, 0.3], [-1.7, 0.9], [0.2, -1.1]):
            p0, _ = phase(lamom, x, 0.05)
            for t in (0.37, 1.9, 3.3):
                flow.set(x=x[0], y=x[1], TOTAL=t)
                end = flow.run()
                p1, _ = phase(lamom, [end["X"][-1], end["Y"][-1]], 0.05)
                assert circ(p1, p0 + t / period) < 1e-3, (x, t, p0, p1)


def test_elapsed_time(lamom):
    """A state inside delta already has elapsed 0; a far one takes time,
    in whole output steps."""
    ph, el = phase(lamom, [1.0, 0.0], 0.05)
    assert el == 0.0
    _, el = phase(lamom, [2.0, 0.0], 0.05)
    h = 2 * np.pi / 1.5 / NSTEP
    assert 0.5 < el < 3.0 and abs(el / h - round(el / h)) < 1e-6
    # tmax only limits the search: it does not change the answer
    assert phase(lamom, [2.0, 0.0], 0.05, tmax=50.0) == phase(
        lamom, [2.0, 0.0], 0.05, tmax=5.0)


def test_gradient_of_the_phase_is_the_adjoint():
    """On Morris-Lecar (iapp = 0.1: a stable cycle of period 16.47), the
    centred difference of the phase across the cycle, theta/T per unit of
    state, equals Z/T from the adjoint. The offsets are 0.01 (delta = 1e-4,
    so the flow does the work); the difference is exact to O(0.01^2) and Z
    is good to about 1 % on 2000 points (found: 1.2 %): 2 % of the largest |Z|/T."""
    with xs.load(ODE / "lecar.ode") as m:
        m.set(iapp=0.1)
        per, _ = m.limit_cycle(transient=100, variable="V", total=200)
        set_cycle(m, per)
        adj = m.adjoint()
        orbit = m._collect()
        gam = np.column_stack([orbit["V"], orbit["W"]])
        z = np.column_stack([adj["V"], adj["W"]]) / per
        eps = 0.01
        worst = 0.0
        for k in (150, 500, 900, 1300, 1700):
            for j in (0, 1):
                e = np.zeros(2)
                e[j] = eps
                pp, _ = phase(m, gam[k] + e, 1e-4)
                pm, _ = phase(m, gam[k] - e, 1e-4)
                d = (pp - pm + 0.5) % 1.0 - 0.5
                worst = max(worst, abs(d / (2 * eps) - z[k, j]))
        assert worst < 0.02 * np.abs(z).max(), (worst, np.abs(z).max())


def test_refusals():
    clock = (MODELS / "radial_clock.ode").read_text()
    with xs.Model.from_text(clock) as m:
        m.run()
        err, ph, el = call(m, [0.5, 0.5], 0.05)
        assert err is not None and "adjoint" in str(err) and ph == -1
    with xs.load(MODELS / "radial_clock.ode") as m:
        set_cycle(m, 2 * np.pi)
        m.adjoint()
        for delta, tmax in ((0.0, 10.0), (-1.0, 10.0), (0.05, 0.0),
                            (0.05, -2.0), (float("nan"), 10.0)):
            err, ph, el = call(m, [0.5, 0.5], delta, tmax)
            assert err is not None and (ph, el) == (-1, -1), (delta, tmax)
            assert "delta > 0" in str(err) and "tmax > 0" in str(err)
        # the origin is the unstable equilibrium: the basin's boundary
        err, ph, el = call(m, [0.0, 0.0], 0.05, 20.0)
        assert err is not None and "undefined" in str(err) and ph == -1
        # a point near it takes long: too short a limit, then enough
        err, _, _ = call(m, [1e-6, 0.0], 0.05, 5.0)
        assert err is not None and "undefined" in str(err)
        assert call(m, [1e-6, 0.0], 0.05, 40.0)[0] is None
        # the run is no longer the period of the adjoint
        m.run(total=3.0)
        err, ph, el = call(m, [0.5, 0.5], 0.05)
        assert err is not None and "adjoint" in str(err) and ph == -1


@pytest.mark.parametrize("text, word", [
    ("x'=-delay(x,1)+y\ny'=-y\ninit x=1,y=1\n@ total=5,delay=2\ndone\n",
     "delays"),
    ("wiener nz\nx'=-x+nz\ny'=-y\ninit x=1,y=1\n@ total=5\ndone\n",
     "noise"),
])
def test_refuses_delays_and_noise(text, word):
    with xs.Model.from_text(text) as m:
        m.run()
        err, ph, el = call(m, [0.5, 0.5], 0.05)
        assert err is not None and word in str(err) and ph == -1
