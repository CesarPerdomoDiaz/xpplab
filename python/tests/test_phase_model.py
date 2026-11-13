"""xppe_phase_model (method D of docs/design/isochrons.md): the phase-reduced
model of a coupled pair, against the closed forms of the coupled
lambda-omega system (test 3 of the note), against xppe_hfunction, against
full simulations of the pair, and its refusals.

Table: phase (time units, 0 to T), Z_<var>, H, H_odd, H_even, locked, stable.

Error budget, used by the tolerances below (the closed forms are in the
note; the accuracy is XPPAUT's adjoint and make_h, not the integrator's):

* Z is the adjoint, implicit Euler on NSTEP points, good to about 1 %;
  H is a mean of Z times the coupling, so it inherits that, and the float
  storage in adj2.c (1e-7 relative);
* the locked states come from the roots of H_odd on the grid of NSTEP
  points, refined on a cubic, so they are as good as H_odd is: H_odd has
  slope 1 at 0, so an error e in H_odd moves the root by e.
"""
import ctypes
import os
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs

ROOT = Path(__file__).resolve().parents[2]
MODELS = ROOT / "tests" / "models"
ODE = ROOT / "engine" / "xppaut" / "ode"

# Valgrind (CI) sets LD_PRELOAD for the process it runs. The methods other
# than the first six, the unstable-cycle adjoint and the formula that does
# not parse reach engine code that no other test runs; they are skipped
# there and run in the ordinary test jobs, as do the long simulations of
# the pair (they only integrate, 50 times slower there).
UNDER_VALGRIND = "vgpreload" in os.environ.get("LD_PRELOAD", "")
not_under_valgrind = pytest.mark.skipif(
    UNDER_VALGRIND, reason="engine paths not run under valgrind elsewhere")

NSTEP = 2000
W = 1.5
Q = 1.5
PERIOD = 2 * np.pi / W
DIFF = ["x'-x", "y'-y"]


def set_cycle(m, period, nstep=NSTEP):
    """Settle onto the cycle, then leave one period of it, in exactly nstep
    steps, as the run: the adjoint needs exactly one period."""
    m.set(DT=period / nstep, TOTAL=60.0)
    sol = m.run()
    m.set(**{v: float(sol[v][-1]) for v in m.variables})
    m.set(TOTAL=period)
    m.run()


def phase_model(m, coupling):
    arr = (ctypes.c_char_p * len(coupling))(*[c.encode() for c in coupling])
    m._engine.call("xppe_phase_model", len(coupling), arr)
    return m._result("Phase model")._data


def roots(tab):
    """(phases, stable) of the locked states of a table."""
    ok = ~np.isnan(tab["locked"])
    return tab["locked"][ok], tab["stable"][ok].astype(bool)


def closed_forms(phase, alpha_sign=1.0):
    """H, H_odd and H_even of the coupled pair, w = q = 1.5, for the coupling
    x'-x, y'-y (alpha_sign = 1) or x-x', y-y' (-1), from the note."""
    psi = W * phase
    h = (np.sin(psi) - Q * (np.cos(psi) - 1)) / W
    return (alpha_sign * h, alpha_sign * np.sin(psi) / W,
            -alpha_sign * Q * (np.cos(psi) - 1) / W)


@pytest.fixture(scope="module")
def pair():
    with xs.load(MODELS / "lambda_omega.ode") as m:
        m.set(w=W, q=Q)
        set_cycle(m, PERIOD)
        m.adjoint()
        yield m


def test_closed_forms_and_locked_states(pair):
    """H, H_odd, H_even against the closed forms. Z is good to about 1 % of
    its size (|Z| = sqrt(1 + q^2)/w = 1.8 here), so H is good to about
    0.02 in absolute value: found 1e-5 on this grid, bound 1e-3."""
    tab = phase_model(pair, DIFF)
    ph = tab["phase"]
    assert len(ph) == NSTEP + 1
    assert ph[0] == 0 and abs(ph[-1] - PERIOD) < 1e-9
    h, hodd, heven = closed_forms(ph)
    assert np.abs(tab["H"] - h).max() < 1e-3
    assert np.abs(tab["H_odd"] - hodd).max() < 1e-3
    assert np.abs(tab["H_even"] - heven).max() < 1e-3
    # Z on the cycle: (-q r + theta)/w, so |Z| is the same everywhere
    zmag = np.hypot(tab["Z_X"], tab["Z_Y"])
    assert np.abs(zmag - np.hypot(1, Q) / W).max() < 0.02 * zmag.mean()
    lk, st = roots(tab)
    assert len(lk) == 2
    assert abs(lk[0]) < 1e-3 and st[0]
    assert abs(lk[1] - PERIOD / 2) < 1e-3 and not st[1]


def test_repulsive_coupling_flips_the_stability(pair):
    """x-x', y-y': H changes sign, so synchrony is unstable and antiphase
    stable."""
    tab = phase_model(pair, ["x-x'", "y-y'"])
    h, hodd, heven = closed_forms(tab["phase"], -1.0)
    assert np.abs(tab["H_odd"] - hodd).max() < 1e-3
    lk, st = roots(tab)
    assert len(lk) == 2
    assert abs(lk[0]) < 1e-3 and not st[0]
    assert abs(lk[1] - PERIOD / 2) < 1e-3 and st[1]


# Tolerances by method: the first-order methods store a cycle that is
# only first-order accurate (and euler's adjoint is built on it), the rest
# are as good as the default one. Found by running: euler 3e-2, the others
# 1e-5 .. 1e-4.
METHODS = [("rungekutta", 1e-3), ("adams", 1e-3), ("gear", 1e-3),
           ("stiff", 1e-3), ("cvode", 1e-3), ("modeuler", 1e-3),
           ("backeul", 5e-2), ("qualrk", 1e-3), ("5dp", 1e-3),
           ("83dp", 1e-3), ("2rb", 1e-3), ("euler", 5e-2)]


@pytest.mark.parametrize("meth, tol", [
    pytest.param(m, t, marks=not_under_valgrind) if i >= 6 else (m, t)
    for i, (m, t) in enumerate(METHODS)])
def test_every_method(meth, tol):
    """The closed forms and the locked states under every method that can
    integrate a cycle. The adaptive ones run at TOL = ATOL = 1e-9."""
    with xs.load(MODELS / "lambda_omega.ode") as m:
        m.set(w=W, q=Q, meth=meth)
        if meth in ("gear", "stiff", "cvode", "5dp", "83dp", "2rb"):
            m.set(TOL=1e-9, ATOL=1e-9)
        set_cycle(m, PERIOD)
        m.adjoint()
        tab = phase_model(m, DIFF)
        h, hodd, heven = closed_forms(tab["phase"])
        assert np.abs(tab["H"] - h).max() < tol, meth
        assert np.abs(tab["H_odd"] - hodd).max() < tol, meth
        assert np.abs(tab["H_even"] - heven).max() < tol, meth
        lk, st = roots(tab)
        assert len(lk) == 2, (meth, lk)
        assert abs(lk[0]) < tol and st[0], meth
        assert abs(lk[1] - PERIOD / 2) < tol and not st[1], meth


def test_nout_greater_than_one():
    """With NOUT = 4 the stored points are 4 steps apart; the phase grid and
    T follow them."""
    with xs.load(MODELS / "lambda_omega.ode") as m:
        m.set(w=W, q=Q, NOUT=4)
        set_cycle(m, PERIOD, nstep=4 * 500)
        m.adjoint()
        tab = phase_model(m, DIFF)
        assert len(tab["phase"]) == 501
        assert abs(tab["phase"][-1] - PERIOD) < 1e-9
        h, hodd, heven = closed_forms(tab["phase"])
        assert np.abs(tab["H_odd"] - hodd).max() < 1e-3


# -- full simulations of the pair --------------------------------------------

PAIR = """# two lambda-omega oscillators, coupled with strength eps
par w={w},q={q},eps={eps}
g1=1-x1^2-y1^2
g2=1-x2^2-y2^2
x1'=x1*g1-y1*(w+q*g1)+eps*({c})*(x2-x1)
y1'=y1*g1+x1*(w+q*g1)+eps*({c})*(y2-y1)
x2'=x2*g2-y2*(w+q*g2)+eps*({c})*(x1-x2)
y2'=y2*g2+x2*(w+q*g2)+eps*({c})*(y1-y2)
init x1=1,y1=0,x2={x2},y2={y2}
@ total={total},dt=0.02
done
"""


def pair_phase_difference(c, eps, phi0, total):
    """theta_2 - theta_1 (time units, mod T) at the end of a simulation of the
    pair from the phase difference phi0. Their phases are the asymptotic
    phases (theta - q ln r)/w of the note."""
    a = W * phi0
    text = PAIR.format(w=W, q=Q, eps=eps, c=c, x2=np.cos(a), y2=np.sin(a),
                       total=total)
    with xs.Model.from_text(text) as p:
        sol = p.run()
    th = []
    for i in (1, 2):
        x, y = sol["X%d" % i][-1], sol["Y%d" % i][-1]
        th.append((np.arctan2(y, x) - Q * np.log(np.hypot(x, y))) / W)
    return (th[1] - th[0]) % PERIOD


def circ(d):
    """Distance of d to 0 on the circle of length T."""
    d = d % PERIOD
    return min(d, PERIOD - d)


@not_under_valgrind
def test_the_pair_settles_at_synchrony():
    """eps = 0.02: dphi/dt = -2 eps sin(w phi)/w, so near 0 the rate is
    -2 eps = -0.04 and the distance shrinks by e^-16 in 400. A start at
    1.0 (a quarter of the period) ends within 1e-3 of 0 (the second order
    in eps, here about 1e-3 at most, is the bound)."""
    for phi0 in (1.0, 3.0):
        d = pair_phase_difference(1, 0.02, phi0, 400)
        assert circ(d) < 1e-3, (phi0, d)


@not_under_valgrind
def test_the_repulsive_pair_settles_at_antiphase():
    """The same with eps -> -eps (c = -1): antiphase is the stable state."""
    for phi0 in (0.2, 3.7):
        d = pair_phase_difference(-1, 0.02, phi0, 400)
        assert circ(d - PERIOD / 2) < 1e-3, (phi0, d)


# -- consistency with xppe_hfunction -----------------------------------------


def test_h_is_the_hfunction_column(pair):
    """H is the same numbers as xppe_hfunction's, bit for bit; with two
    equations H_odd and H_even are 0.5 (H[k] -/+ H[n-1-k])."""
    adj = pair.adjoint()
    tab = phase_model(pair, DIFF)
    hf = pair.h_function(DIFF)
    assert list(hf.columns) == ["phi", "H"]
    assert np.array_equal(tab["H"], hf["H"])
    h = hf["H"]
    assert np.array_equal(tab["H_odd"], 0.5 * (h - h[::-1]))
    assert np.array_equal(tab["H_even"], 0.5 * (h + h[::-1]))
    # Z is the adjoint's
    assert np.array_equal(tab["Z_X"], adj["X"])
    assert np.array_equal(tab["Z_Y"], adj["Y"])


def test_hodd_and_heven_are_the_columns_with_more_than_two_equations():
    """Morris-Lecar has auxiliary variables, so NEQ > 2 and XPPAUT returns
    Hodd and Heven."""
    with xs.load(ODE / "lecar.ode") as m:
        m.set(iapp=0.1)
        per, _ = m.limit_cycle(transient=100, variable="V", total=200)
        set_cycle(m, per)
        m.adjoint()
        coupling = ["V'-V", "0"]
        tab = phase_model(m, coupling)
        hf = m.h_function(coupling)
        assert list(hf.columns) == ["phi", "H", "Hodd", "Heven"]
        assert np.array_equal(tab["H"], hf["H"])
        assert np.array_equal(tab["H_odd"], hf["Hodd"])
        assert np.array_equal(tab["H_even"], hf["Heven"])
        assert abs(tab["phase"][-1] - per) < 1e-9
        lk, st = roots(tab)
        assert len(lk) == 2 and abs(lk[1] - per / 2) < 1e-2 * per
        assert abs(lk[0]) < 1e-2 * per and st[0] and not st[1]


# -- refusals -----------------------------------------------------------------


def snapshot(m):
    """Everything a refusal or a call must leave as it was."""
    run = m._collect()
    return ({k: np.array(run[k]) for k in run.columns},
            {k: float(m.get(k)) for k in ("DT", "TOTAL", "NOUT")},
            {v: float(m.get(v)) for v in m.variables})


def same(a, b):
    assert a[1] == b[1] and a[2] == b[2]
    assert list(a[0]) == list(b[0])
    for k in a[0]:
        assert np.array_equal(a[0][k], b[0][k]), k


def refused(m, coupling=DIFF):
    with pytest.raises(xs.EngineError) as e:
        phase_model(m, coupling)
    return str(e.value)


def test_the_model_is_left_as_found(pair):
    before = snapshot(pair)
    phase_model(pair, DIFF)
    same(before, snapshot(pair))
    # and the adjoint and the table of H are still usable
    assert len(pair.adjoint()["T"]) == NSTEP + 1
    same(before, snapshot(pair))


def test_refuses_without_an_adjoint():
    with xs.load(MODELS / "lambda_omega.ode") as m:
        m.run()
        before = snapshot(m)
        assert "adjoint" in refused(m)
        same(before, snapshot(m))


def test_refuses_a_run_that_is_not_one_period():
    with xs.load(MODELS / "lambda_omega.ode") as m:
        m.set(w=W, q=Q)
        set_cycle(m, PERIOD)
        m.adjoint()
        m.run(total=3.0)
        before = snapshot(m)
        assert "adjoint" in refused(m)
        same(before, snapshot(m))


@pytest.mark.parametrize("change", [{"DT": 2 * PERIOD / NSTEP}, {"NOUT": 2}])
def test_refuses_when_dt_or_nout_changed_since_the_adjoint(change):
    with xs.load(MODELS / "lambda_omega.ode") as m:
        m.set(w=W, q=Q)
        set_cycle(m, PERIOD)
        m.adjoint()
        phase_model(m, DIFF)
        m.set(**change)
        before = snapshot(m)
        assert "adjoint again" in refused(m)
        same(before, snapshot(m))


@not_under_valgrind
def test_refuses_an_unstable_cycle():
    """r' = r(r - 1): the circle r = 1 is a cycle, repelling. The adjoint,
    which XPPAUT integrates backward, does not converge, so there is none,
    and the phase model says so."""
    text = ("x'=x*g-y\ny'=y*g+x\ng=x^2+y^2-1\ninit x=1,y=0\n"
            "@ total=6.283185307179586,dt=0.0031415926535897933\ndone\n")
    with xs.Model.from_text(text) as m:
        m.run()
        with pytest.raises(xs.EngineError):
            m.adjoint()
        before = snapshot(m)
        assert "adjoint" in refused(m)
        same(before, snapshot(m))


def test_refuses_a_wrong_number_of_formulas(pair):
    before = snapshot(pair)
    assert "one coupling formula per" in refused(pair, ["x'-x"])
    same(before, snapshot(pair))
    assert len(phase_model(pair, DIFF)["phase"]) == NSTEP + 1


@not_under_valgrind
def test_refuses_a_formula_that_does_not_parse(pair):
    before = snapshot(pair)
    assert "coupling" in refused(pair, ["x'-(", "0"])
    same(before, snapshot(pair))
    # a refusal does not spoil the next call
    assert len(phase_model(pair, DIFF)["phase"]) == NSTEP + 1


@pytest.mark.parametrize("text, word", [
    ("x'=-delay(x,1)+y\ny'=-y\ninit x=1,y=1\n@ total=5,delay=2\ndone\n",
     "delays"),
    ("wiener nz\nx'=-x+nz\ny'=-y\ninit x=1,y=1\n@ total=5\ndone\n", "noise"),
    ("x(t+1)=r*x*(1-x)\npar r=4\ninit x=0.3\n@ meth=discrete,total=20\n"
     "done\n", "map"),
])
def test_refuses_delays_noise_and_maps(text, word):
    with xs.Model.from_text(text) as m:
        m.run()
        before = snapshot(m)
        assert word in refused(m, ["0"] * len(m.variables))
        same(before, snapshot(m))


def test_refuses_integral_equations():
    with xs.load(ODE / "lamvolt.ode") as m:
        m.set(TOTAL=1)
        m.run()
        assert "integrals" in refused(m, ["0"] * len(m.variables))


# -- a pair of Morris-Lecar cells (test 4 of the note) ------------------------

ML = dict(phi=0.15, v3=0.05, v4=0.2, gca=1.0, iapp=0.25)
# Excitatory synapse onto V, steep threshold at V = 0.3, reversal at 1:
# G = (1/(1 + exp(-(V' - 0.3)/0.02))) (1 - V) for V, 0 for W.
ML_COUPLING = ["(1/(1+exp(-(V'-0.3)/0.02)))*(1-V)", "0"]
ML_PAIR = """# two Morris-Lecar cells (lecar.ode), synaptic coupling of strength eps
par iapp={iapp},phi={phi},va=-.01,vb=0.15,v3={v3},v4={v4},gca={gca},eps={eps}
par vk=-.7,vl=-.5,gk=2.0,gl=.5
minf(v)=.5*(1+tanh((v-va)/vb))
ninf(v)=.5*(1+tanh((v-v3)/v4))
lamn(v)=phi*cosh((v-v3)/(2*v4))
s(v)=1/(1+exp(-(v-0.3)/0.02))
pa'=iapp+gl*(vl-pa)+gk*qa*(vk-pa)-gca*minf(pa)*(pa-1)+eps*s(pb)*(1-pa)
qa'=lamn(pa)*(ninf(pa)-qa)
pb'=iapp+gl*(vl-pb)+gk*qb*(vk-pb)-gca*minf(pb)*(pb-1)+eps*s(pa)*(1-pb)
qb'=lamn(pb)*(ninf(pb)-qb)
init pa={pa},qa={qa},pb={pb},qb={qb}
@ total={total},dt=0.01,meth=5dp,tol=1e-9,atol=1e-9
done
"""
ML_DT = 0.01


def upward_crossings(t, v, level=0.0):
    i = np.where((v[:-1] < level) & (v[1:] >= level))[0]
    return t[i] + (level - v[i]) * (t[i + 1] - t[i]) / (v[i + 1] - v[i])


def ml_pair_difference(per, orbit, eps, phi0, total):
    """theta_2 - theta_1 at the end of a simulation of the pair that starts
    with both cells on the cycle, cell 2 ahead by phi0 (time units): the
    cell that crosses V = 0 upward earlier is ahead, so the difference is
    the last crossing of cell 1 minus that of cell 2, taken to (-T/2, T/2]."""
    k = int(round((phi0 % per) / ML_DT)) % (len(orbit["V"]) - 1)
    d = dict(ML, eps=eps, total=total, pa=orbit["V"][0], qa=orbit["W"][0],
             pb=orbit["V"][k], qb=orbit["W"][k])
    with xs.Model.from_text(ML_PAIR.format(**d)) as p:
        sol = p.run()
    c1 = upward_crossings(sol.t, sol["PA"])
    c2 = upward_crossings(sol.t, sol["PB"])
    return (c1[-1] - c2[-1] + per / 2) % per - per / 2


@pytest.fixture(scope="module")
def morris_lecar():
    with xs.load(ODE / "lecar.ode") as m:
        m.set(**ML, DT=ML_DT)
        per, orbit = m.limit_cycle(transient=300, variable="V", total=300)
        orbit = {"V": np.array(orbit["V"]), "W": np.array(orbit["W"])}
        set_cycle(m, per)
        m.adjoint()
        yield m, per, orbit


def test_morris_lecar_has_a_locked_state_that_is_neither_0_nor_half(
        morris_lecar):
    """H_odd of this pair has four zeros: synchrony and antiphase, both
    unstable, and a mirror pair at +-phi* = 0.884 (0.061 T), both stable (found
    by searching with Model.h_function; the note gives the search). The
    simulations below are the check."""
    m, per, _ = morris_lecar
    tab = phase_model(m, ML_COUPLING)
    lk, st = roots(tab)
    assert len(lk) == 4
    assert abs(lk[0]) < 1e-3 and abs(lk[2] - per / 2) < 1e-3
    assert lk[1] == pytest.approx(0.884, abs=0.01)
    assert lk[3] == pytest.approx(per - lk[1], abs=0.01)
    assert list(st) == [False, True, False, True]
    # roots of H_odd: the table's own column changes sign at them
    ho = tab["H_odd"]
    ph = tab["phase"]
    for r in lk:
        i = int(np.argmin(np.abs(ph - r)))
        assert abs(ho[i]) < 0.02 * np.abs(ho).max()


@not_under_valgrind
@pytest.mark.parametrize("phi0, which", [
    (3.0, 1), (0.6, 1), (12.0, 3), (-0.6, 3), (6.9, 1), (7.6, 3)])
def test_morris_lecar_pair_goes_to_the_predicted_state(morris_lecar, phi0,
                                                       which):
    """eps = 0.01: the phase difference goes from phi0 to the stable locked
    state on its side of the unstable ones (0 and T/2 repel; the start 0.6
    from 0 leaves it), and ends within 0.02 (found: 0.009, which is first
    order in eps: 0.005 at eps = 0.005, 0.003 at 0.0025) of the predicted phase."""
    m, per, orbit = morris_lecar
    lk, st = roots(phase_model(m, ML_COUPLING))
    d = ml_pair_difference(per, orbit, 0.01, phi0, 6000)
    expect = (lk[which] + per / 2) % per - per / 2
    assert abs(d - expect) < 0.02, (phi0, d, expect)


@not_under_valgrind
def test_morris_lecar_error_shrinks_with_eps(morris_lecar):
    """The difference from the predicted phase is first order in eps."""
    m, per, orbit = morris_lecar
    lk, _ = roots(phase_model(m, ML_COUPLING))
    errs = [abs(ml_pair_difference(per, orbit, eps, 3.0, total) - lk[1])
            for eps, total in ((0.02, 3000), (0.005, 12000))]
    assert errs[1] < errs[0] / 2 and errs[1] < 0.01, errs
