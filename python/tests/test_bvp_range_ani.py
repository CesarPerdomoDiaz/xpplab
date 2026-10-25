"""Boundary-value problems, XPPAUT's Range, the animator and 3-D plots."""
import math
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest

import xpplab as xs

ROOT = Path(__file__).resolve().parents[2]
ODE = ROOT / "engine" / "xppaut" / "ode"
HEADLESS = ROOT / "build" / "cli" / ("xppaut-headless" +
                                     (".exe" if sys.platform == "win32" else ""))


# -- boundary-value problems ------------------------------------------------

def test_bvp_with_a_closed_form_solution():
    """x'=y, y'=x, x(0)=y(1), y(0)=1: x(0) = cosh 1 / (1 - sinh 1)."""
    with xs.load(ODE / "dumbbvp.ode") as m:
        sol = m.bvp()
        assert m.get("X") == pytest.approx(math.cosh(1) / (1 - math.sinh(1)),
                                           rel=1e-5)
        assert sol["X"][0] == pytest.approx(sol["Y"][-1], rel=1e-5)


def test_cable_equation():
    """v'' = v, v(0)=1, v(L)=0 (L = total): v'(0) = -coth(L)."""
    with xs.load(ODE / "cable.ode") as m:
        length = float(m.get("TOTAL"))
        sol = m.bvp()
        assert m.get("VX") == pytest.approx(-1 / math.tanh(length), rel=1e-5)
        assert sol["V"][0] == pytest.approx(1, abs=1e-6)
        assert abs(sol["V"][-1]) < 1e-5


def test_bvp_needs_boundary_conditions():
    with xs.load(ODE / "lecar.ode") as m:
        with pytest.raises(xs.EngineError):
            m.bvp()


# -- Range ------------------------------------------------------------------

def batch_range(tmp_path, oldic):
    """lecar.ode through XPPAUT's batch Range over iapp, three runs."""
    if not HEADLESS.exists():
        pytest.skip("xppaut-headless not built")
    lines = (ODE / "lecar.ode").read_text().splitlines()
    done = max(i for i, l in enumerate(lines) if l.strip().lower() in ("done", "d"))
    lines.insert(done, f"@ total=50, range=1, rangeover=iapp, rangestep=2, "
                       f"rangelow=0.05, rangehigh=0.15, rangereset=yes, "
                       f"rangeoldic={oldic}")
    (tmp_path / "r.ode").write_text("\n".join(lines) + "\n")
    subprocess.run([str(HEADLESS), "r.ode", "-silent", "-outfile", "r.dat"],
                   cwd=tmp_path, check=True, capture_output=True,
                   stdin=subprocess.DEVNULL)
    return [[l.split() for l in (tmp_path / f"r.dat.{i}").read_text().splitlines()]
            for i in range(3)]


@pytest.mark.parametrize("oldic, continue_runs", [("yes", False), ("no", True)])
def test_range_matches_xppaut(tmp_path, oldic, continue_runs):
    """Every run of a Range equals XPPAUT's batch range, digit for digit,
    both starting each run afresh and continuing from the last."""
    theirs = batch_range(tmp_path, oldic)
    values = [0.05 + (0.15 - 0.05) / 2 * i for i in range(3)]  # as XPPAUT steps
    with xs.load(ODE / "lecar.ode") as m:
        v0, i0 = m.get("V"), m.get("iapp")
        runs = m.sweep("iapp", values, continue_runs=continue_runs, total=50)
        assert (m.get("V"), m.get("iapp")) == (v0, i0)  # restored afterwards
    for i, sol in enumerate(runs):
        out = tmp_path / f"mine.{i}"
        sol.save(out)
        mine = [l.split() for l in out.read_text().splitlines()]
        assert mine == theirs[i], f"run {i}"
    if continue_runs:
        assert runs[1]["V"][0] == pytest.approx(runs[0]["V"][-1], abs=1e-6)


def test_range_over_an_initial_condition():
    with xs.load(ODE / "lecar.ode") as m:
        starts = m.sweep("V", [-0.4, -0.2, 0.0], measure=lambda s: s["V"][0])
        assert np.allclose(starts, [-0.4, -0.2, 0.0], atol=1e-6)


# -- animator ---------------------------------------------------------------

def test_pendulum_animation():
    """pend.ani: a rod from (.5,.5) to the bob at .5+.4 sin x, .5-.4 cos x."""
    with xs.load(ODE / "pend.ode") as m:
        sol = m.run()
        ani = m.animation("pend.ani")
        assert ani.commands == 2 and len(ani) == len(sol)
        for row in (0, 37, len(sol) - 1):
            rod, bob = ani.frame(row)
            x = sol["X"][row]
            assert rod.shape == "line" and bob.shape == "ellipse"
            assert (rod.x1, rod.y1) == (0.5, 0.5)
            assert rod.x2 == pytest.approx(0.5 + 0.4 * math.sin(x), abs=1e-6)
            assert rod.y2 == pytest.approx(0.5 - 0.4 * math.cos(x), abs=1e-6)
            assert (bob.x1, bob.y1) == (rod.x2, rod.y2)
            assert bob.x2 == pytest.approx(0.05) and bob.filled
            assert bob.color == "#ff0000"


def test_every_bundled_animation_loads(tmp_path):
    """Each .ani with its model parses and gives a frame."""
    model_for = {"lorenz.ani": "lorenz2.ode"}  # as its first comment says
    pairs = [(a, ODE / model_for.get(a.name, a.stem + ".ode"))
             for a in sorted(ODE.glob("*.ani"))]
    pairs = [(a, o) for a, o in pairs if o.exists()]
    assert len(pairs) > 10
    for ani_file, ode in pairs:
        with xs.load(ode) as m:
            m.run(total=1)
            ani = m.animation(ani_file.name)
            assert ani.commands > 0, ani_file.name
            assert ani.frame(0) is not None


# -- 3-D --------------------------------------------------------------------

def test_plot3d():
    matplotlib = pytest.importorskip("matplotlib")
    matplotlib.use("Agg")
    with xs.load(ODE / "lorenz.ode") as m:
        ax = m.run(total=20).plot3d("X", "Y", "Z")
        assert ax.get_zlabel() == "Z"
