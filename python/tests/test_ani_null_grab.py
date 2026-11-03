"""The animator's nullclines (xnull, ynull) and grab points."""
import math
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs

ROOT = Path(__file__).resolve().parents[2]
ODE = ROOT / "engine" / "xppaut" / "ode"


def mapped(seg, box):
    """Segments (n, 4) of the phase plane in the animation's 0..1 window,
    for the box (x1, y1, x2, y2) of an xnull line."""
    x1, y1, x2, y2 = box
    out = np.array(seg, dtype=float)
    out[:, [0, 2]] = (out[:, [0, 2]] - x1) / (x2 - x1)
    out[:, [1, 3]] = (out[:, [1, 3]] - y1) / (y2 - y1)
    return out


def null_items(shapes):
    return [s for s in shapes if s.shape == "nullcline"]


# -- nullclines -----------------------------------------------------------------

def test_current_nullclines_of_a_small_model(tmp_path):
    """xnull ...;-1 draws XPPAUT's current nullclines in the box."""
    (tmp_path / "small.ode").write_text(
        "x'=x*(1-x)-y\ny'=a*x-y\npar a=0.5\ninit x=0.1,y=0.1\n"
        "@ total=5\ndone\n")
    (tmp_path / "small.ani").write_text(
        "xnull -0.5;-0.5;1.5;1.5;$RED;-1\n"
        "ynull -0.5;-0.5;1.5;1.5;$BLUE;-1\nend\n")
    with xs.load(tmp_path / "small.ode") as m:
        m.run()
        ani = m.animation("small.ani")
        assert ani.nullclines("x", "y", grid=60) == 0
        xn, yn = null_items(ani.frame(0))
        assert (xn.color, yn.color) == ("#ff0000", "#0000ff")
        want = m.nullclines("x", "y", (-0.5, 1.5), (-0.5, 1.5), grid=60)
        box = (-0.5, -0.5, 1.5, 1.5)
        assert len(xn.segments) == len(want[0]) > 10
        assert len(yn.segments) == len(want[1]) > 0
        assert np.allclose(xn.segments, mapped(want[0], box), atol=1e-6)
        assert np.allclose(yn.segments, mapped(want[1], box), atol=1e-6)
        # the x-nullcline is the parabola y = x(1-x): its mapped points obey it
        px = xn.segments[:, 0] * 2 - 0.5
        py = xn.segments[:, 1] * 2 - 0.5
        assert np.allclose(py, px * (1 - px), atol=0.03)


def test_wcstim_frozen_nullclines_follow_the_stimulus():
    """wcstim.ani: xnull -.1;-.1;1.1;1.1;$RED;10*stim(t) picks the frozen
    set number int(10*stim(t)); the box maps to 0..1 as (v + .1)/1.2."""
    with xs.load(ODE / "wcstim.ode") as m:
        sol = m.run()
        ani = m.animation("wcstim.ani")
        grid = 40
        assert ani.nullclines("u", "v", grid=grid, parameter="s0", steps=12,
                              low=0.0, high=1.2) == 13
        values = [1.2 * k / 12 for k in range(13)]
        sets = m.nullcline_sweep("s0", values, "u", "v", (-0.1, 1.1),
                                 (-0.1, 1.1), grid=grid)
        assert m.get("s0") == 0  # put back
        box = (-0.1, -0.1, 1.1, 1.1)
        t = sol["T"]
        z = 12 * t  # 10*stim(t), with s1 = 1.2 and tdone = 1
        rows = [r for r in range(len(t)) if t[r] < 1 and 0.1 < z[r] % 1 < 0.9]
        rows = rows[:: max(1, len(rows) // 6)]
        checked = set()
        for row in rows:
            k = int(z[row])
            xn, yn = null_items(ani.frame(row))
            assert (xn.color, yn.color) == ("#ff0000", "#00a000")
            assert np.allclose(xn.segments, mapped(sets[k][0], box), atol=1e-6)
            assert np.allclose(yn.segments, mapped(sets[k][1], box), atol=1e-6)
            checked.add(k)
        assert len(checked) >= 4
        # after the stimulus ends (t > tdone) stim = s0 = 0: set 0 again
        row = int(np.argmin(abs(t - 2.0)))
        xn, _ = null_items(ani.frame(row))
        assert np.allclose(xn.segments, mapped(sets[0][0], box), atol=1e-6)


def test_no_frozen_set_draws_nothing_and_missing_commands_are_refused(tmp_path):
    with xs.load(ODE / "wcstim.ode") as m:
        m.run()
        ani = m.animation("wcstim.ani")
        assert null_items(ani.frame(0)) == []  # nothing computed yet
    with xs.load(ODE / "pend.ode") as m:
        m.run()
        ani = m.animation("pend.ani")
        with pytest.raises(xs.EngineError):
            ani.nullclines("X", "XP")


def test_plot_draws_the_nullclines():
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    with xs.load(ODE / "wcstim.ode") as m:
        m.run()
        ani = m.animation("wcstim.ani")
        ani.nullclines("u", "v", grid=30, parameter="s0", steps=12, low=0, high=1.2)
        ax = ani.plot(0, plt.figure().gca())
        assert len(ax.collections) == 2
        plt.close("all")


# -- grab points ----------------------------------------------------------------

def test_grab_on_itoy_sets_the_angle_and_runs():
    with xs.load(ODE / "itoy.ode") as m:
        m.set(total=1)
        m.run()
        ani = m.animation("itoy.ani")
        ani.frame(0)
        points = ani.grab_points()
        assert len(points) == 2
        a = m.get("a")
        t1_before, t2_before = m.get("t1"), m.get("t2")
        # the first point is the north end of arm 1: (cos(t1+a), sin(t1+a))
        x0, y0, tol = points[0]
        assert (x0, y0) == pytest.approx((math.cos(t1_before + a),
                                          math.sin(t1_before + a)), abs=1e-12)
        assert tol == pytest.approx(0.05)
        m.set(total=2)
        rows_before = len(ani)
        mx, my = -0.3, 0.9
        # press a little off the point, within the tolerance
        idx = ani.grab(x0 + 0.01, y0, mx, my)
        assert idx == 0
        assert m.get("t1") == pytest.approx(a + math.atan2(my, mx), abs=1e-12)
        assert m.get("t2") == t2_before  # the other arm is untouched
        sol = m.last_run()
        assert len(sol) != rows_before  # the run started: total is now 2
        assert sol["T"][-1] == pytest.approx(2.0)
        assert sol["t1"][0] == pytest.approx(a + math.atan2(my, mx), abs=1e-6)  # stored as float
        # the second point is the one for arm 2, which says runnow=0
        ani.frame(0)
        x2, y2, _ = ani.grab_points()[1]
        m.set(total=1)
        n = len(m.last_run())
        assert ani.grab(x2, y2, 2.1 + m.get("d") + 0.5, 0.5) == 1
        assert m.get("t2") == pytest.approx(
            a + math.atan2(0.5, 2.1 + m.get("d") + 0.5 - 2 - m.get("d")), abs=1e-12)
        assert len(m.last_run()) == n  # no run


def test_press_outside_every_tolerance_changes_nothing():
    with xs.load(ODE / "itoy.ode") as m:
        m.set(total=1)
        m.run()
        ani = m.animation("itoy.ani")
        ani.frame(0)
        x0, y0, tol = ani.grab_points()[0]
        before = (m.get("t1"), m.get("t1p"), m.get("t2"), m.get("t2p"))
        rows = len(m.last_run())
        assert ani.grab(x0 + 2 * tol, y0, 0.0, 1.0) is None
        assert (m.get("t1"), m.get("t1p"), m.get("t2"), m.get("t2p")) == before
        assert len(m.last_run()) == rows
