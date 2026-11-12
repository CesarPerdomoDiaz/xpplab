"""xppe_isochrons (method B of docs/design/isochrons.md), Runge-Kutta only:
the curves of the radial clock against their closed form, the multiplier, the
refusals, and that the model is left as it was.

Error budget, used by the tolerances below. A curve starts from the linear
isochron at a cycle point: its direction is perpendicular to Z, which is
XPPAUT's adjoint (implicit Euler on NSTEP points, good to about 1 %, see
test_asymptotic_phase.py), on a segment of length offset = 1e-4. A start
point is then off its isochron by about 1 % of 1e-4 plus offset^2 times the
curvature of the isochron (zero for a ray), and the backward flow for whole
periods keeps that phase error as it is. Seen from a point at radius r the
angle error is that over r, and the points are checked at r >= 1e-2. Found by
running, with three phases and NSTEP = 400: 5e-8 rad at worst; the bound
used is 1e-6.
"""
import ctypes
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs

ROOT = Path(__file__).resolve().parents[2]
MODELS = ROOT / "tests" / "models"
ODE = ROOT / "engine" / "xppaut" / "ode"

NSTEP = 400
COUNT = 3
LENGTH = 1.5


def set_cycle(m, period):
    """Settle onto the cycle, then leave one period of it, in exactly NSTEP
    steps, as the run: the adjoint needs exactly one period."""
    m.set(DT=period / NSTEP, TOTAL=60.0)
    sol = m.run()
    m.set(**{v: float(sol[v][-1]) for v in m.variables})
    m.set(TOTAL=period)
    m.run()


def isochrons(m, count=COUNT, length=LENGTH, max_points=600, max_periods=0,
              offset=0.0, spacing=0.05):
    """(table, lambda) of xppe_isochrons; raises xs.EngineError on refusal."""
    lam = ctypes.c_double(-1)
    m._engine.call("xppe_isochrons", count, length, max_points, max_periods,
                   offset, spacing, ctypes.byref(lam))
    return m._result("Isochrons"), lam.value


def refusal(m, *args, **kw):
    """The error of a refused call (the multiplier stays at -1 unless it was
    computed)."""
    with pytest.raises(xs.EngineError) as e:
        isochrons(m, *args, **kw)
    return str(e.value)


def polylines(tab, count=COUNT):
    """The table as a list of (phase, x, y) arrays, split at the NaN rows."""
    ph, x, y = tab["phase"], tab["X"], tab["Y"]
    nan = np.isnan(ph)
    assert (nan == np.isnan(x)).all() and (nan == np.isnan(y)).all()
    cuts = np.flatnonzero(nan)
    assert len(cuts) == count - 1
    parts = np.split(np.arange(len(ph)), cuts)
    out = []
    for p in parts:
        p = p[~np.isnan(ph[p])]
        assert len(p) > 0
        out.append((ph[p], x[p], y[p]))
    return out


def circ(a, b):
    """Distance between angles on the circle of length 2 pi."""
    d = (a - b) % (2 * np.pi)
    return np.minimum(d, 2 * np.pi - d)


def prepare(m):
    """The radial clock on its stable cycle, with the adjoint, under the
    default Runge-Kutta method; returns the angle of the first stored point."""
    set_cycle(m, 2 * np.pi)
    m.adjoint()
    orbit = m._collect()
    return float(np.arctan2(orbit["Y"][0], orbit["X"][0]))


def test_curves_are_the_rays_of_the_radial_clock():
    """The isochron of phase p is the ray theta = theta0 + 2 pi p, and
    lambda = exp(-2 pi) (derivations in the note). Every point of every
    curve, from r = 1e-2 to the arc length limit, is within 1e-6 rad of its
    ray (the budget is above), and each curve runs out on both sides of the
    cycle: outside to the arc length limit (the distance from the cycle
    point, 1.5, on a ray), inside to within 0.1 of the equilibrium (spacing 0.05)."""
    with xs.load(MODELS / "radial_clock.ode") as m:
        theta0 = prepare(m)
        tab, lam = isochrons(m)
        assert abs(lam / np.exp(-2 * np.pi) - 1) < 1e-5, lam
        lines = polylines(tab)
        assert len(lines) == COUNT
        for j, (ph, x, y) in enumerate(lines):
            assert (ph == j / COUNT).all()
            r = np.hypot(x, y)
            far = r >= 1e-2
            err = circ(np.arctan2(y, x), theta0 + 2 * np.pi * ph)[far]
            assert far.sum() > 50 and err.max() < 1e-6, (j, err.max())
            assert 2.4 < r.max() <= 2.5 + 1e-9 and r.min() < 0.1, (j,)
            # the cycle point is on the polyline, between its two sides
            assert np.abs(r - 1).min() < 1e-4, j
            rc = r[np.argmin(np.abs(r - 1))]
            assert min(r[0], r[-1]) < rc < max(r[0], r[-1])


def test_refinement_and_limits():
    """Neighbouring points are at most about the spacing apart (the images of
    two start points: the junction between periods adds a little), a polyline
    has at most max_points points, a larger arc length limit gives a longer
    curve, and a second call gives the same table."""
    name = "radial_clock.ode"
    with xs.load(MODELS / name) as m:
        prepare(m)
        tab, _ = isochrons(m, count=2, length=1.0, spacing=0.05)
        again, _ = isochrons(m, count=2, length=1.0, spacing=0.05)
        for c in ("phase", "X", "Y"):
            assert np.array_equal(tab[c], again[c], equal_nan=True)
        for ph, x, y in polylines(tab, 2):
            gap = np.hypot(np.diff(x), np.diff(y)).max()
            assert gap < 0.05 * 1.1, gap
            # outside the cycle the ray ends at the arc length limit: r = 2
            r = np.hypot(x, y)
            assert 2.0 - 0.05 * 1.1 < r.max() <= 2.0 + 1e-9, r.max()
        small, _ = isochrons(m, count=2, length=1.0, spacing=0.05,
                             max_points=41)
        for ph, x, y in polylines(small, 2):
            assert len(ph) <= 41
        short, _ = isochrons(m, count=2, length=0.3, spacing=0.05)
        for _, x, y in polylines(short, 2):
            assert 1.3 - 0.05 * 1.1 < np.hypot(x, y).max() <= 1.3 + 1e-9


def test_model_is_left_as_found():
    """The call changes no variable, parameter, initial value, option (DT and
    its sign, METHOD), the stored run, nor the adjoint; and a run after it is
    bit-identical to the same run in a model that never made the call."""
    name = "radial_clock.ode"
    with xs.load(MODELS / name) as m, xs.load(MODELS / name) as ref:
        for model in (m, ref):
            prepare(model)
        before = (dict(m.parameters), dict(m.initial_conditions),
                  dict(m.options), m.last_run().to_numpy().copy())
        isochrons(m, count=2)
        after = (dict(m.parameters), dict(m.initial_conditions),
                 dict(m.options), m.last_run().to_numpy().copy())
        assert before[:3] == after[:3]
        assert np.array_equal(before[3], after[3])
        assert m.get("DT") > 0
        # the adjoint is still usable
        m._engine.call("xppe_asymptotic_phase",
                       (ctypes.c_double * 2)(0.5, 0.5), 0.05, 50.0,
                       ctypes.byref(ctypes.c_double()), None)
        a = m.run(total=3.0)
        b = ref.run(total=3.0)
        assert a.columns == b.columns
        assert np.array_equal(a.to_numpy(), b.to_numpy())


def test_refused_call_leaves_the_model_too():
    """A refused call (an unstable cycle: the cycle of the radial clock with
    its sign changed after the adjoint) leaves the options, with DT positive,
    and the run as they were, and the next call works."""
    text = ("par s=1\nr=sqrt(x^2+y^2)\nx'=s*x*(1-r)-y\ny'=s*y*(1-r)+x\n"
            "init x=1,y=0\n@ total=60,dt=0.01\ndone\n")
    with xs.Model.from_text(text) as m:
        set_cycle(m, 2 * np.pi)
        m.adjoint()
        run = m.last_run().to_numpy().copy()
        opts = dict(m.options)
        m.set(s=-1.0)
        msg = refusal(m)
        assert "not stable" in msg
        # the multiplier is returned even when the call is refused
        lam = ctypes.c_double(-1)
        with pytest.raises(xs.EngineError):
            m._engine.call("xppe_isochrons", 2, 1.0, 100, 0, 0.0, 0.0,
                           ctypes.byref(lam))
        assert abs(lam.value / np.exp(2 * np.pi) - 1) < 1e-4
        m.set(s=1.0)
        assert dict(m.options) == opts
        assert np.array_equal(m.last_run().to_numpy(), run)
        tab, lam = isochrons(m, count=2)
        assert abs(lam / np.exp(-2 * np.pi) - 1) < 1e-4


@pytest.mark.parametrize("meth", ["euler", "modeuler", "adams", "gear",
                                  "cvode", "5dp"])
def test_refuses_the_other_methods_for_now(meth):
    """Only Runge-Kutta so far: any other method is an error that says so,
    and the model is left as it was."""
    with xs.load(MODELS / "radial_clock.ode") as m:
        prepare(m)
        m.set(meth=meth)
        opts = dict(m.options)
        assert "Runge-Kutta" in refusal(m)
        assert dict(m.options) == opts


def test_refusals():
    name = "radial_clock.ode"
    # no adjoint
    with xs.load(MODELS / name) as m:
        m.run()
        assert "adjoint" in refusal(m)
    with xs.load(MODELS / name) as m:
        prepare(m)
        for kw in ({"count": 0}, {"length": 0.0}, {"length": float("nan")},
                   {"max_points": 2}):
            assert "count >= 1" in refusal(m, **kw), kw
        # DT or NOUT changed since the adjoint
        m.set(DT=2 * 2 * np.pi / NSTEP)
        assert "adjoint again" in refusal(m)
        m.set(DT=2 * np.pi / NSTEP)
        isochrons(m, count=1)
        m.set(NOUT=2)
        assert "adjoint again" in refusal(m)
        m.set(NOUT=1)
        isochrons(m, count=1)
        # the run is no longer the period of the adjoint
        m.run(total=3.0)
        assert "adjoint" in refusal(m)


def test_refuses_more_than_two_variables():
    text = ("r=sqrt(x^2+y^2)\nx'=x*(1-r)-y\ny'=y*(1-r)+x\nz'=-z\n"
            "init x=1,y=0,z=1\n@ total=60,dt=0.01\ndone\n")
    with xs.Model.from_text(text) as m:
        set_cycle(m, 2 * np.pi)
        m.adjoint()
        msg = refusal(m)
        assert "two ODE variables" in msg


@pytest.mark.parametrize("text, word", [
    ("x'=-delay(x,1)+y\ny'=-y\ninit x=1,y=1\n@ total=5,delay=2\ndone\n",
     "delays"),
    ("wiener nz\nx'=-x+nz\ny'=-y\ninit x=1,y=1\n@ total=5\ndone\n", "noise"),
    ("x(t+1)=r*x*(1-x)\ny(t+1)=y/2\npar r=4\ninit x=0.3,y=1\n"
     "@ meth=discrete,total=20\ndone\n", "map"),
])
def test_refuses_delays_noise_and_maps(text, word):
    with xs.Model.from_text(text) as m:
        m.run()
        assert word in refusal(m)


def test_refuses_integral_equations():
    with xs.load(ODE / "lamvolt.ode") as m:
        m.set(TOTAL=1)
        m.run()
        assert "integrals" in refusal(m)


@pytest.mark.parametrize("extra, word", [
    ("@ fold=th", "fold"),
    ("global 1 x-0.5 {x=1}", "global flags"),
])
def test_refuses_fold_and_global_flags(extra, word):
    text = "\n".join(["x'=-x", "th'=1", "init x=1", "@ total=5", extra,
                      "done", ""])
    with xs.Model.from_text(text) as m:
        m.run()
        assert word in refusal(m)
