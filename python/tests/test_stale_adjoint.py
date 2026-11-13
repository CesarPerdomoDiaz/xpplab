"""The functions that work on the adjoint of the last run (the H function,
the asymptotic phase and the phase model) refuse an adjoint that belongs to
an earlier run. XPPAUT keeps its adjoint after later runs, so a run of the
same length, after a parameter change or from another point of the cycle,
used to pass every check and be combined with the old adjoint, with no
error. The engine fingerprints the run when the adjoint is computed."""
import ctypes
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs

ROOT = Path(__file__).resolve().parents[2]
MODEL = ROOT / "tests" / "models" / "lambda_omega.ode"
NSTEP = 400
W = 1.5
PERIOD = 2 * np.pi / W
DIFF = ["x'-x", "y'-y"]


def one_period(m, start=(1.0, 0.0)):
    """One period of the cycle r = 1, from the point start, in NSTEP steps."""
    m.set(DT=PERIOD / NSTEP, TOTAL=PERIOD, x=start[0], y=start[1])
    m.run()


def h_function(m):
    m.h_function(DIFF)


def asymptotic_phase(m):
    arr = (ctypes.c_double * 2)(1.2, 0.0)
    ph, el = ctypes.c_double(), ctypes.c_double()
    m._engine.call("xppe_asymptotic_phase", arr, 0.02, 50.0,
                   ctypes.byref(ph), ctypes.byref(el))


def phase_model(m, coupling=DIFF):
    arr = (ctypes.c_char_p * len(coupling))(*[c.encode() for c in coupling])
    m._engine.call("xppe_phase_model", len(coupling), arr)


USERS = [h_function, asymptotic_phase, phase_model]


def prepared():
    m = xs.load(MODEL)
    m.set(w=W, q=1.5)
    one_period(m)
    m.adjoint()
    return m


@pytest.mark.parametrize("use", USERS, ids=lambda f: f.__name__)
def test_the_adjoint_of_this_run_is_used(use):
    """No new run since the adjoint: the call works, and so does a second
    identical run (same start, same parameters: the same stored run)."""
    with prepared() as m:
        use(m)
        one_period(m)
        use(m)


@pytest.mark.parametrize("use", USERS, ids=lambda f: f.__name__)
def test_a_run_after_a_parameter_change_is_refused(use):
    """Same TOTAL and DT, so the same number of points, but another orbit."""
    with prepared() as m:
        m.set(w=1.6)
        one_period(m)
        with pytest.raises(xs.EngineError, match="compute the adjoint again"):
            use(m)
        m.adjoint()  # and once it is computed again, the call works
        use(m)


@pytest.mark.parametrize("use", USERS, ids=lambda f: f.__name__)
def test_a_run_from_another_point_of_the_cycle_is_refused(use):
    """The same cycle, started a quarter turn on: Z and the orbit would be
    out of phase."""
    with prepared() as m:
        one_period(m, start=(0.0, 1.0))
        with pytest.raises(xs.EngineError, match="compute the adjoint again"):
            use(m)


def test_a_coupling_that_gives_nan_is_refused():
    """H that is not a number somewhere: an error, not a table of NaN locked
    states."""
    with prepared() as m:
        with pytest.raises(xs.EngineError, match="not a number"):
            phase_model(m, ["sqrt(-1)+x'-x", "y'-y"])
