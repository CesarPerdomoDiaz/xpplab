"""The analysis tools: against XPPAUT's own batch post-processing, digit for
digit, and against cases with known answers."""
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

CLOCK = """\
# radial isochron clock: the limit cycle is the unit circle, period 2*pi/w
x'=x-w*y-x*(x^2+y^2)
y'=y+w*x-y*(x^2+y^2)
par w=1
init x=1,y=0
@ total=100,dt=0.01
done
"""


@pytest.fixture
def lecar():
    with xs.load(ODE / "lecar.ode") as m:
        m.run()
        yield m


def batch(tmp_path, options):
    """lecar.ode through XPPAUT's batch mode with post-processing options;
    the rows of the file it writes, as strings."""
    if not HEADLESS.exists():
        pytest.skip("xppaut-headless not built")
    lines = (ODE / "lecar.ode").read_text().splitlines()
    done = max(i for i, l in enumerate(lines) if l.strip().lower() in ("done", "d"))
    lines.insert(done, "@ " + options)
    (tmp_path / "pp.ode").write_text("\n".join(lines) + "\n")
    subprocess.run([str(HEADLESS), "pp.ode", "-silent", "-outfile", "pp.dat"],
                   cwd=tmp_path, check=True, capture_output=True,
                   stdin=subprocess.DEVNULL)
    return [l.split() for l in (tmp_path / "pp.dat").read_text().splitlines()
            if l.strip()]


def same_as_batch(table, rows):
    """The table, written as XPPAUT writes numbers, equals XPPAUT's own
    output. Batch post-processing writes only the first two columns."""
    k = min(len(table.columns), len(rows[0]))
    mine = [["%.8g" % v for v in row[:k]] for row in table.to_numpy()]
    theirs = [r[:k] for r in rows]
    assert len(mine) == len(theirs)
    assert mine == theirs


@pytest.mark.parametrize("options, compute", [
    ("postprocess=1, histcol=v, histbins=40, histlo=-0.5, histhi=0.5",
     lambda m: m.histogram("V", bins=40, range=(-0.5, 0.5))),
    ("postprocess=2, speccol=v", lambda m: m.fourier("V")),
    ("postprocess=3, speccol=v", lambda m: m.power("V")),
    ("postprocess=4, speccol=v, specwidth=256, specwin=2",
     lambda m: m.spectrum("V", length=256, window="hamming")),
    ("postprocess=5, speccol=v, speccol2=w, specwidth=256, specwin=1",
     lambda m: m.spectrum("V", "W", kind="cross", length=256, window="parzen")),
    ("postprocess=6, speccol=v, speccol2=w, specwidth=128, specwin=4",
     lambda m: m.spectrum("V", "W", kind="coherence", length=128, window="hann")),
    ("postprocess=7, histcol=v, histcol2=w, histbins=10, histbins2=8, "
     "histlo=-0.5, histhi=0.5, histlo2=0, histhi2=0.5",
     lambda m: m.histogram2d("V", "W", bins=(10, 8),
                             range=((-0.5, 0.5), (0, 0.5)))),
], ids=["histogram", "fourier", "power", "psd", "cross", "coherence", "hist2d"])
def test_matches_xppaut_postprocessing(lecar, tmp_path, options, compute):
    same_as_batch(compute(lecar), batch(tmp_path, options))


def test_fourier_of_pure_tones():
    """sin and cos of the same frequency land in the sin and cos columns of
    the same mode, with the same size."""
    with xs.Model.from_text("x'=0\naux s=sin(2*pi*t)\naux c=cos(2*pi*t)\n"
                            "@ total=10,dt=0.01\ndone\n") as m:
        m.run()
        fs, fc = m.fourier("S"), m.fourier("C")
        k = int(np.argmax(np.abs(fs["sin"])))
        assert fs["freq"][k] == pytest.approx(1.0, rel=1e-6)
        assert int(np.argmax(np.abs(fc["cos"]))) == k
        assert abs(fs["sin"][k]) == pytest.approx(abs(fc["cos"][k]), rel=1e-3)
        # A little leaks into cos: the run holds both t=0 and t=10.
        assert abs(fs["cos"][k]) < 0.05 * abs(fs["sin"][k])
        amp = m.power("S")
        assert amp["amplitude"][k] == pytest.approx(
            np.hypot(fs["sin"][k], fs["cos"][k]), rel=1e-6)
        assert amp["phase"][k] == pytest.approx(
            np.arctan2(fs["sin"][k], fs["cos"][k]), rel=1e-5)


def test_histogram2d_counts(lecar):
    """XPPAUT's 2-D histogram is the fraction of points in each bin."""
    sol = lecar._collect()
    h = lecar.histogram2d("V", "W", bins=(10, 8), range=((-0.5, 0.5), (0, 0.5)))
    i = np.floor((sol["V"] + 0.5) / 0.1).astype(int)
    j = np.floor((sol["W"] - 0.0) / 0.0625).astype(int)
    ok = (i >= 0) & (i < 10) & (j >= 0) & (j < 8)
    counts = np.zeros(80)
    np.add.at(counts, i[ok] + 10 * j[ok], 1.0 / len(sol))
    assert np.allclose(h["count"], counts, rtol=1e-5, atol=1e-7)


def test_analysis_leaves_the_run_alone(lecar):
    before = lecar._collect()
    lecar.fourier("V")
    lecar.histogram("W", bins=20)
    lecar.correlation("V", "W", lags=40)
    after = lecar._collect()
    assert np.array_equal(before["V"], after["V"])
    assert np.array_equal(before.t, after.t)


def test_mean_and_correlation(lecar):
    sol = lecar._collect()
    m, s = lecar.mean("V")
    assert m == pytest.approx(np.mean(sol["V"]), rel=1e-6)
    assert s == pytest.approx(np.std(sol["V"]), rel=1e-4)
    ac = lecar.correlation("V", lags=50)
    lag0 = ac["correlation"][np.argmin(np.abs(ac["lag"]))]
    assert lag0 == pytest.approx(s * s, rel=1e-3)  # autocovariance at lag 0


def test_phase_response_of_a_radial_clock():
    """Exactly known: on the unit circle the iPRC is (-sin theta, cos theta)
    (per unit time, with w = 1) and diffusive coupling gives H = sin(phi)."""
    with xs.Model.from_text(CLOCK) as m:
        prc = m.phase_response(transient=50)
        assert prc.period == pytest.approx(2 * np.pi, rel=1e-5)
        orbit = m._collect()
        theta = np.arctan2(orbit["Y"], orbit["X"])
        assert np.max(np.abs(prc["X"] + np.sin(theta))) < 0.01
        assert np.max(np.abs(prc["Y"] - np.cos(theta))) < 0.01
        assert prc["phase"][0] == 0 and prc["phase"][-1] == pytest.approx(1, abs=0.01)
        h = m.h_function({"X": "x'-x", "Y": "y'-y"})
        assert np.max(np.abs(h["H"] - np.sin(h["phi"]))) < 0.01


def test_adjoint_needs_a_periodic_orbit():
    with xs.Model.from_text("x'=-x\ninit x=1\n@ total=5\ndone\n") as m:
        with pytest.raises(xs.EngineError):
            m.fourier("X")  # nothing integrated yet
        m.run()
        with pytest.raises(xs.EngineError):
            m.h_function(["x'-x"])  # no adjoint yet


def test_lyapunov_exponents():
    with xs.Model.from_text("x'=-2*x\ninit x=1\n@ total=5,dt=0.01\ndone\n") as m:
        m.run()
        assert m.lyapunov() == pytest.approx(-2, rel=1e-3)
    with xs.load(ODE / "lorenz.ode") as m:
        m.run(total=200)
        assert 0.6 < m.lyapunov() < 1.2  # about 0.9 for the Lorenz attractor


def test_fit_recovers_a_parameter():
    with xs.Model.from_text("x'=-a*x\npar a=0.5\ninit x=1\n"
                            "@ total=4,dt=0.05\ndone\n") as m:
        sol = m.run()
        data = np.column_stack([sol.t, sol["X"]])[::10]
        m.parameters["a"] = 1.0
        fitted = m.fit(data, variables="x", parameters="a")
        assert fitted["a"] == pytest.approx(0.5, rel=1e-3)
        assert m.parameters["a"] == pytest.approx(0.5, rel=1e-3)
