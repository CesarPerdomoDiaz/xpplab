"""xpplab.figure: the calls the desktop app's Figure window echoes."""
import math

import numpy as np
import pytest

import xpplab as xs
from xpplab.figure import diagram_xy, evaluate_expression

pytest.importorskip("matplotlib")

KITE = """
par VW=8, lam=245
par L=100, g=9.81, CLmax=1.2, CD0=0.06, kd=0.12
vt=VW/sqrt(g*L)
ua=om+vt*sin(gam)
wa=vt*cos(gam)
va=sqrt(ua^2+wa^2)
alpha=atan2(wa,ua)
cl=CLmax*sin(2*alpha)
cd=CD0+kd*cl^2
gam'=om
om'=-cos(gam)+lam*va*(cl*wa-cd*ua)
aux tens=om^2-sin(gam)+lam*va*(cl*ua+cd*wa)
init gam=1.364, om=0
@ total=20, bound=1e6, dsmax=0.05, ds=-0.01, parmin=1.5, parmax=10, nmax=800
done
"""


@pytest.fixture(scope="module")
def kite():
    with xs.Model.from_text(KITE, "kite.ode") as m:
        m.auto.equilibria("VW")
        hb = m.auto.diagram.first("HB")
        m.auto.continue_from(hb, "periodic", ds=0.002, dsmax=0.01, nmax=30, npr=1)
        yield m


def test_expressions_follow_the_apps_grammar():
    x = np.array([1.0, 4.0, 100.0])
    assert evaluate_expression("1.109724/sqrt(x)", x) == pytest.approx(1.109724 / np.sqrt(x))
    assert evaluate_expression("-x^2 + 2*x", x) == pytest.approx(-x ** 2 + 2 * x)
    assert evaluate_expression("2^-1", x) == pytest.approx([0.5] * 3)
    assert evaluate_expression("sin(pi/2) + e", x) == pytest.approx([1 + math.e] * 3)
    for bad in ("x +", "foo(x)", "(x", "y"):
        with pytest.raises(ValueError):
            evaluate_expression(bad, x)


def test_figure_document_and_files(tmp_path):
    fig = xs.Figure(120, 60, rows=1, cols=2, font="serif", fontsize=8)
    s = fig[0].line([0, 1, 2], [0, 1, 4], label=r"$\Gamma_e$", line="--", marker="o")
    assert s.color == "#1f77b4"
    assert fig[0].line([0, 1], [1, 0]).color == "#ff7f0e"
    fig[0][1].set(color="#000000", width=0.8, scale=(1, 180 / math.pi))
    fig[0].set(xlabel=r"$V_W$ [m/s]", xlim=(0, 2), legend="lower right", grid=True)
    fig[1].function("1/sqrt(x)", (0.1, 10), label="fit")
    inset = fig.inset(0, (0.6, 0.6, 0.3, 0.3))
    inset.line([0, 1], [0, 1])
    fig.grid(2, 2)
    assert len(fig.panels) == 5 and fig.panels[4].inset[0] == 0
    fig.grid(1, 2)
    fig[0].remove(1)
    assert len(fig[0].series) == 1
    for ext in ("pdf", "svg", "eps", "png"):
        fig.save(tmp_path / f"f.{ext}", dpi=150)
        assert (tmp_path / f"f.{ext}").stat().st_size > 1000
    with pytest.raises(ValueError):
        fig[0].set(legend="somewhere")
    with pytest.raises(TypeError):
        fig[0][0].set(colour="red")


def test_diagram_pieces_meet(kite):
    d = kite.auto.diagram
    x, y = diagram_xy(d, "GAM", stable=True, periodic=False)
    xu, yu = diagram_xy(d, "GAM", stable=False, periodic=False)
    hb = d.first("HB")
    # the unstable piece starts where the stable one ends: at the Hopf point
    assert hb.value in x and hb.value in xu
    assert np.all(np.isfinite(x)) and np.isnan(xu).sum() >= 0


def test_diagram_quantities(kite):
    d = kite.auto.diagram
    x, re1 = diagram_xy(d, "re1", periodic=False)
    _, im1 = diagram_xy(d, "im1", periodic=False)
    hb = d.first("HB")
    k = int(np.argmin(np.abs(x - hb.value)))
    assert abs(re1[k]) < 1e-3 and abs(im1[k]) == pytest.approx(0.908, abs=1e-3)
    xp, mu = diagram_xy(d, "mu1_abs", periodic=True)
    assert np.isnan(mu[0])              # the Hopf point has no multipliers
    assert np.all(mu[1:] >= 1)          # every orbit is unstable
    _, period = diagram_xy(d, "period", periodic=True)
    assert period[1] == pytest.approx(6.92, abs=0.05)


def test_aux_along_and_where(kite):
    d = kite.auto.diagram
    x, t = diagram_xy(d, "TENS", periodic=False, stable=True, model=kite)
    assert t[0] * 9.81 == pytest.approx(67.16, abs=0.01)
    xo, lo = diagram_xy(d, "TENS", which="min", periodic=True, model=kite)
    keep = ("TENS", "min", 0, float("inf"))
    xt, _ = diagram_xy(d, "period", periodic=True, where=keep, model=kite)
    finite = xt[np.isfinite(xt)]
    assert 0 < len(finite) <= len(xo)
    with pytest.raises(ValueError):
        diagram_xy(d, "TENS")          # an aux needs the model


def test_panel_helpers(kite, tmp_path):
    fig = xs.Figure()
    kite.set(VW=2.5)
    s = fig[0].curve(kite, "om'", "gam", (-1.5, 1.5), at={"om": 0}, n=50)
    assert len(s.x) == 50 and np.isfinite(s.y).all()
    t = fig[0].curve(kite, "tens", "gam", (0, 1.5), n=20)
    assert t.y[0] == pytest.approx(kite.evaluate({"gam": 0.0})["tens"])
    runs = kite.run_many([{"gam": 0.5}, {"gam": 1.0}], total=1)
    r = fig[0].runs(runs, "gam", "om")
    assert np.isnan(r.x).sum() == 1
    fig[0].diagram(kite.auto.diagram, "GAM", stable=True, periodic=False,
                   scale=(1, 180 / math.pi))
    fig.save(tmp_path / "kite.pdf")
