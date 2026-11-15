"""The classic models of examples/classics, each against something known
about it: a closed form, a conserved quantity or a textbook number. The
gallery of the documentation shows these models, so they must be right."""
import math

import numpy as np
import pytest

import xpplab as xs

NAMES = [p.stem for p in xs.examples.classics()]


def load(name):
    return xs.load(xs.example(f"classics/{name}"))


def upward(t, x, level=0.0, after=0.0):
    """The times at which x crosses level going up, after a time."""
    return np.array([t[i] for i in range(1, len(x))
                     if x[i - 1] < level <= x[i] and t[i] > after])


def test_the_classics_are_found():
    assert len(NAMES) == 20 and "lorenz" in NAMES and "heat" in NAMES
    assert xs.example("classics/lorenz.ode").name == "lorenz.ode"
    assert xs.example("classics/lorenz") != xs.example("lorenz.ode")  # XPPAUT's own
    with pytest.raises(FileNotFoundError, match="classics/lorenz.ode"):
        xs.example("classics/lorens")


@pytest.mark.parametrize("name", NAMES)
def test_each_classic_runs_and_says_what_it_is(name):
    text = xs.example(f"classics/{name}").read_text()
    lines = text.splitlines()
    assert lines[0].startswith("# ") and lines[1].startswith("# "), "title and description"
    with load(name) as m:
        sol = m.run()
        for v in m.variables:
            assert np.isfinite(sol[v]).all(), v


def test_lorenz_is_bounded_and_chaotic():
    with load("lorenz") as m:
        a = m.run()
        b = m.run(x=1.000001)
        assert np.abs(a["X"]).max() < 25 and a["Z"].max() < 55
        # two starts 1e-6 apart are far apart well before the end
        assert np.abs(a["X"] - b["X"])[-2000:].max() > 5
        assert (a["X"] > 5).any() and (a["X"] < -5).any()   # both wings


def test_van_der_pol_cycle():
    """mu = 1: period 6.6633 and amplitude 2.0086 (the standard values)."""
    with load("vanderpol") as m:
        sol = m.run(total=200)
        period = np.diff(upward(sol.t, sol["X"], after=100)).mean()
        assert period == pytest.approx(6.6633, abs=2e-3)
        assert sol["X"][sol.t > 100].max() == pytest.approx(2.0086, abs=1e-3)


def test_harmonic_oscillator_closed_form():
    """Period 2 pi / sqrt(k/m - (c/2m)^2); amplitude decays as exp(-c t/2m)."""
    with load("oscillator") as m:
        sol = m.run()
        x, t = sol["X"], sol.t
        peaks = [i for i in range(1, len(x) - 1) if x[i] > x[i - 1] and x[i] >= x[i + 1]]
        assert np.diff(t[peaks]).mean() == pytest.approx(2 * math.pi / math.sqrt(0.99),
                                                         abs=2e-3)
        rate = math.log(x[peaks[0]] / x[peaks[1]]) / (t[peaks[1]] - t[peaks[0]])
        assert rate == pytest.approx(0.1, abs=1e-3)


def test_pendulum_small_swings():
    with load("pendulum") as m:
        sol = m.run(c=0, th=0.05, total=40)
        assert np.diff(upward(sol.t, sol["TH"])).mean() == pytest.approx(2 * math.pi,
                                                                         abs=5e-3)


def test_lotka_volterra_conserves_its_invariant():
    with load("lotka_volterra") as m:
        sol = m.run()
        h = sol["X"] - np.log(sol["X"]) + sol["Y"] - np.log(sol["Y"])
        assert h.max() - h.min() < 1e-5


def test_sir_peak_and_final_size():
    """With R0 = beta/gamma = 2.5: the infected peak when s = 1/R0, at
    i0 + s0 - 1/R0 + ln(1/(R0 s0))/R0, and the final s solves
    ln(s/s0) = R0 (s - 1)."""
    with load("sir") as m:
        sol = m.run(total=400)
        s0, i0, r0 = 0.999, 0.001, 2.5
        peak = i0 + s0 - 1 / r0 + math.log(1 / (r0 * s0)) / r0
        assert sol["I"].max() == pytest.approx(peak, abs=1e-5)
        s_end = float(sol["S"][-1])
        assert math.log(s_end / s0) == pytest.approx(r0 * (s_end - 1), abs=1e-6)
        assert (sol["S"] + sol["I"] + sol["R"]) == pytest.approx(1.0, abs=1e-6)


def test_logistic_map_two_cycle():
    """r = 3.2: the 2-cycle (r + 1 -/+ sqrt((r - 3)(r + 1))) / 2r."""
    with load("logistic_map") as m:
        sol = m.run(r=3.2, total=400)
        root = math.sqrt(0.2 * 4.2)
        want = sorted([(4.2 - root) / 6.4, (4.2 + root) / 6.4])
        assert sorted(sol["X"][-2:]) == pytest.approx(want, abs=1e-6)


def test_henon_attractor_stays_in_its_box():
    with load("henon") as m:
        sol = m.run()
        assert -1.3 < sol["X"][100:].min() and sol["X"][100:].max() < 1.3
        assert len(set(np.round(sol["X"][-1000:], 6))) > 900   # not periodic


def test_heat_equation_decay_rate():
    """The slowest mode decays at the rate of the grid's Laplacian,
    4 n^2 sin^2(pi / 2n), which is pi^2 to 0.03 % for n = 50."""
    with load("heat") as m:
        sol = m.run(total=0.5)
        late = sol.t > 0.3
        rate = -np.polyfit(sol.t[late], np.log(sol["U25"][late]), 1)[0]
        assert rate == pytest.approx(4 * 2500 * math.sin(math.pi / 100) ** 2, rel=1e-4)
        assert rate == pytest.approx(math.pi ** 2, rel=1e-3)


def test_fisher_front_speed():
    """The front travels at a speed close to 2 (it approaches 2 slowly from
    below, and the grid of step 0.5 changes it a little)."""
    with load("fisher_wave") as m:
        sol = m.run(total=40)
        u = np.array([sol[f"U{j}"] for j in range(1, 101)])
        t = sol.t
        pos = np.array([0.5 * np.argmax(u[:, i] < 0.5) for i in range(len(t))])
        k = (t > 10) & (t < 20)
        assert np.polyfit(t[k], pos[k], 1)[0] == pytest.approx(2.0, abs=0.2)


def test_kepler_conserves_energy_and_angular_momentum():
    """Energy v^2/2 - 1/r = -0.68 and angular momentum 0.8; the period is
    2 pi a^(3/2) with a = -1/(2 E)."""
    with load("kepler") as m:
        sol = m.run()
        assert sol["energy"] == pytest.approx(-0.68, abs=1e-6)
        assert sol["angmom"] == pytest.approx(0.8, abs=1e-6)
        period = np.diff(upward(sol.t, sol["Y"])).mean()
        assert period == pytest.approx(2 * math.pi * (1 / 1.36) ** 1.5, abs=2e-3)


def test_bouncing_ball_bounces():
    """First fall sqrt(2 h / g); each bounce lasts e = 0.8 of the last."""
    with load("bouncing_ball") as m:
        sol = m.run()
        h, t = sol["H"], sol.t
        hits = np.array([t[i] for i in range(1, len(h) - 1)
                         if h[i] <= h[i - 1] and h[i] < h[i + 1] and h[i] < 0.01])
        assert hits[0] == pytest.approx(math.sqrt(2 / 9.81), abs=2e-3)
        gaps = np.diff(hits)[:5]
        assert gaps[1:] / gaps[:-1] == pytest.approx(0.8, abs=5e-3)
        assert h.min() > -1e-3


def test_kuramoto_synchronises_with_enough_coupling():
    with load("kuramoto") as m:
        assert m.run()["r"][-1] > 0.9
        assert m.run(K=0.1)["r"][2000:].mean() < 0.4


def test_brusselator_and_fitzhugh_nagumo_oscillate():
    with load("brusselator") as m:
        sol = m.run(total=200)
        gaps = np.diff(upward(sol.t, sol["X"], level=1.0, after=100))
        assert gaps.std() < 1e-2 and gaps.mean() == pytest.approx(7.157, abs=5e-3)
        assert m.run(b=1.5, total=200)["X"][-1] == pytest.approx(1.0, abs=1e-3)  # b < 1 + a^2
    with load("fitzhugh_nagumo") as m:
        sol = m.run(total=400)
        assert len(upward(sol.t, sol["V"], after=100)) >= 5
        rest = m.run(I=0, total=300)
        assert rest["V"][-1] == pytest.approx(-1.1994, abs=1e-3)   # rests without current


def test_hodgkin_huxley_rests_and_fires():
    with load("hodgkin_huxley") as m:
        sol = m.run(total=200)
        spikes = upward(sol.t, sol["V"])
        assert len(spikes) >= 10 and sol["V"].max() > 20
        assert np.diff(spikes)[-3:].mean() == pytest.approx(14.6, abs=0.3)
        # a value given to run() stays set, so the run without current is last
        assert m.run(I=0, total=100)["V"][-1] == pytest.approx(-65.0, abs=0.1)


def test_the_chaotic_ones_visit_both_sides():
    for name, var, level in (("chua", "X", 1.0), ("duffing", "X", 0.5)):
        with load(name) as m:
            x = m.run()[var][10000:]
            assert (x > level).mean() > 0.1 and (x < -level).mean() > 0.1, name
    with load("rossler") as m:
        assert m.run()["Z"].max() > 10
    with load("mackey_glass") as m:
        x = m.run()["X"][4000:]
        assert x.max() - x.min() > 0.6
        assert m.run(tau=2)["X"][-1] == pytest.approx(1.0, abs=1e-3)   # short delay: steady
