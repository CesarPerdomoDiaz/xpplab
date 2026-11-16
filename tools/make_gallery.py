"""Build the gallery for the documentation website.

    python tools/make_gallery.py [--limit N]

- docs/gallery/index.md: the classic models of examples/classics, each with
  a figure made here by running the model with the xpplab package, its
  equations, and the description from the file's first lines.
- docs/gallery/xppaut.md: the example models that came with XPPAUT, each
  with a small plot.

Writes those pages and docs/gallery/img/*.png (not committed).
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

import matplotlib  # noqa: E402

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

try:  # an installed package (with its engine) wins over the source tree
    import xpplab as xs
except ImportError:
    sys.path.insert(0, str(ROOT / "python" / "src"))
    import xpplab as xs

ODE = ROOT / "engine" / "xppaut" / "ode"
CLASSICS = ROOT / "examples" / "classics"
OUT = ROOT / "docs" / "gallery"
REPO = "https://github.com/CesarPerdomoDiaz/xpplab/blob/main/"
BLUE, RED, GREEN, YELLOW = "#1d4ed8", "#d62728", "#2ca02c", "#c98a00"


def describe(path: Path, limit: int = 4):
    """(title, description) from the comment lines that open a model."""
    lines = []
    for line in path.read_text(errors="replace").splitlines():
        s = line.strip()
        if not s.startswith("#"):
            if lines:
                break
            continue
        s = s.lstrip("#").strip()
        if s:
            lines.append(s)
        if limit and len(lines) >= limit:
            break
    return (lines[0] if lines else path.stem), " ".join(lines[1:])


def equations(path: Path) -> str:
    """The lines of a model that are not comments, options or 'done'."""
    keep = []
    for line in path.read_text().splitlines():
        s = line.strip()
        if s and not s.startswith(("#", "@")) and s != "done":
            keep.append(s)
    return "\n".join(keep)


# ---- The classics: one figure each ----------------------------------------

def axes(xlabel, ylabel, square=False):
    fig, ax = plt.subplots(figsize=(4.2, 3.2) if square else (4.6, 3.0), dpi=110)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.tick_params(labelsize=8)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    return fig, ax


def phase(x, y, starts=None, skip=0.0, lw=0.7, **run):
    """A phase-plane figure of variables x and y, from the model's own start
    or from several."""
    def draw(m):
        fig, ax = axes(x.lower(), y.lower(), square=True)
        for k, start in enumerate(starts or [{}]):
            sol = m.run(**start, **run)
            keep = sol.t >= skip
            ax.plot(sol[x][keep], sol[y][keep], lw=lw,
                    color=[BLUE, RED, GREEN, YELLOW][k % 4])
        return fig
    return draw


def series(names, labels=None, **run):
    def draw(m):
        fig, ax = axes("t", "")
        sol = m.run(**run)
        for k, n in enumerate(names):
            ax.plot(sol.t, sol[n], lw=1.2, color=[BLUE, RED, GREEN, YELLOW][k % 4],
                    label=(labels or names)[k])
        if len(names) > 1:
            ax.legend(fontsize=8, frameon=False)
        else:
            ax.set_ylabel((labels or names)[0])
        return fig
    return draw


def scatter(x, y, skip=100):
    def draw(m):
        fig, ax = axes(x.lower(), y.lower(), square=True)
        sol = m.run()
        ax.plot(sol[x][skip:], sol[y][skip:], ",", color=BLUE)
        return fig
    return draw


def space_time(prefix, n, length, xlabel="x"):
    """u(x, t) of a model with variables prefix1..prefixn, as an image."""
    def draw(m):
        fig, ax = axes(xlabel, "t")
        sol = m.run()
        u = np.array([sol[f"{prefix}{j}"] for j in range(1, n + 1)]).T
        im = ax.imshow(u, origin="lower", aspect="auto", cmap="inferno",
                       extent=[0, length, sol.t[0], sol.t[-1]])
        fig.colorbar(im, ax=ax, label="u")
        return fig
    return draw


def logistic_diagram(m):
    """The long-run values of the logistic map against r: one run for each
    r, the first 300 steps dropped."""
    fig, ax = axes("r", "x")
    rs, xs_ = [], []
    for r in np.linspace(2.6, 4.0, 400):
        sol = m.run(r=float(r), total=400)
        tail = sol["X"][300:]
        rs.append(np.full(len(tail), r))
        xs_.append(tail)
    ax.plot(np.concatenate(rs), np.concatenate(xs_), ",", color=BLUE, alpha=0.6)
    return fig


def kuramoto(m):
    fig, ax = axes("t", "order parameter r")
    for K, color in ((0.2, RED), (2.0, BLUE)):
        sol = m.run(K=K)
        ax.plot(sol.t, sol["r"], lw=1.2, color=color, label=f"K = {K:g}")
    ax.set_ylim(0, 1.05)
    ax.legend(fontsize=8, frameon=False)
    return fig


def mackey_glass(m):
    fig, ax = axes("x(t)", "x(t - 17)", square=True)
    sol = m.run()
    lag = int(round(17 / (sol.t[1] - sol.t[0])))
    x = sol["X"][2000:]
    ax.plot(x[lag:], x[:-lag], lw=0.6, color=BLUE)
    return fig


def ornstein_uhlenbeck(m):
    fig, ax = axes("t", "x")
    for k in range(4):
        sol = m.run()
        ax.plot(sol.t, sol["X"], lw=0.6, color="0.65")
    ens = m.ensemble(400)
    t, mean = ens.mean.t, ens.mean["X"]
    sd = ens.std["X"]
    ax.fill_between(t, mean - sd, mean + sd, color=BLUE, alpha=0.2, lw=0)
    ax.plot(t, mean, lw=1.4, color=BLUE)
    return fig


def kepler(m):
    fig, ax = axes("x", "y", square=True)
    sol = m.run()
    ax.plot(sol["X"], sol["Y"], lw=1.0, color=BLUE)
    ax.plot([0], [0], "o", color=YELLOW, ms=9)
    ax.set_aspect("equal")
    return fig


ROSETTE = [{"th": a, "w": 0.0} for a in (0.5, 1.5, 2.5, 3.1)]
ORBITS = [{"x": a, "y": 1.0} for a in (1.2, 1.6, 2.0, 2.6)]

#: file stem -> (how its figure is drawn, the caption under it)
FIGURES = {
    "lorenz": (phase("X", "Z", lw=0.4), "The butterfly: x against z."),
    "rossler": (phase("X", "Y", skip=50, lw=0.4), "x against y, after the transient."),
    "chua": (phase("X", "Y", skip=20, lw=0.3), "The double scroll: x against y."),
    "vanderpol": (phase("X", "Y", starts=[{"x": 0.1, "y": 0.0}, {"x": 3.0, "y": 3.0}], lw=1.0),
                  "A start inside and a start outside end on the same cycle."),
    "duffing": (phase("X", "Y", skip=50, lw=0.3), "x against its velocity: the mass visits both wells."),
    "pendulum": (phase("TH", "W", starts=ROSETTE, lw=0.8),
                 "Four swings with friction, each spiralling in to rest."),
    "oscillator": (series(["X"], ["x"]), "The ringing dies away."),
    "lotka_volterra": (phase("X", "Y", starts=ORBITS, lw=1.0),
                       "Rabbits against foxes: a closed curve for each start."),
    "sir": (series(["S", "I", "R"], ["susceptible", "infected", "recovered"]),
            "One outbreak: the infected rise, peak when s = 0.4, and fade."),
    "logistic_map": (logistic_diagram, "Where the population ends up, for each growth rate r."),
    "henon": (scatter("X", "Y"), "20 000 points of one orbit."),
    "fitzhugh_nagumo": (series(["V", "W"], ["v", "w"]), "Repetitive firing at I = 0.5."),
    "hodgkin_huxley": (series(["V"], ["v (mV)"]), "A train of action potentials at I = 10."),
    "brusselator": (phase("X", "Y", lw=1.0), "The start spirals out to the limit cycle."),
    "heat": (space_time("U", 49, 1.0), "Temperature along the rod as time goes up."),
    "kuramoto": (kuramoto, "Weak coupling leaves them apart; strong coupling locks them."),
    "mackey_glass": (mackey_glass, "The count now against the count 17 time units ago."),
    "kepler": (kepler, "The orbit, with the sun at one focus."),
    "ornstein_uhlenbeck": (ornstein_uhlenbeck,
                           "Four runs (grey), and the mean of 400 with one standard "
                           "deviation either side."),
    "bouncing_ball": (series(["H"], ["height"]), "Each bounce lasts 0.8 of the one before."),
    "fisher_wave": (space_time("U", 100, 50.0), "The front advances at a steady speed, close to 2."),
}

#: file stem -> the model's equations as LaTeX, for the website
EQUATIONS = {
    "lorenz": r"""\begin{aligned}
\dot x &= \sigma\,(y - x)\\
\dot y &= x\,(r - z) - y\\
\dot z &= x y - b z
\end{aligned}
\qquad \sigma = 10,\; r = 28,\; b = \tfrac{8}{3}""",
    "rossler": r"""\begin{aligned}
\dot x &= -y - z\\
\dot y &= x + a y\\
\dot z &= b + z\,(x - c)
\end{aligned}
\qquad a = 0.2,\; b = 0.2,\; c = 5.7""",
    "chua": r"""\begin{aligned}
\dot x &= \alpha\,\bigl(y - x - f(x)\bigr)\\
\dot y &= x - y + z\\
\dot z &= -\beta y
\end{aligned}
\qquad f(x) = m_1 x + \tfrac12 (m_0 - m_1)\bigl(|x + 1| - |x - 1|\bigr)""",
    "vanderpol": r"""\ddot x - \mu\,(1 - x^2)\,\dot x + x = 0, \qquad \mu = 1""",
    "duffing": r"""\ddot x + \delta\,\dot x - x + x^3 = \gamma \cos \omega t,
\qquad \delta = 0.3,\; \gamma = 0.5,\; \omega = 1.2""",
    "pendulum": r"""\ddot\theta + c\,\dot\theta + \sin\theta = u, \qquad c = 0.1,\; u = 0""",
    "oscillator": r"""m\,\ddot x + c\,\dot x + k\,x = 0, \qquad m = 1,\; k = 1,\; c = 0.2""",
    "lotka_volterra": r"""\begin{aligned}
\dot x &= a x - b x y\\
\dot y &= -c y + d x y
\end{aligned}
\qquad a = b = c = d = 1""",
    "sir": r"""\begin{aligned}
\dot s &= -\beta s i\\
\dot i &= \beta s i - \gamma i\\
\dot r &= \gamma i
\end{aligned}
\qquad \beta = 0.5,\; \gamma = 0.2""",
    "logistic_map": r"""x_{n+1} = r\,x_n\,(1 - x_n)""",
    "henon": r"""\begin{aligned}
x_{n+1} &= 1 - a x_n^2 + y_n\\
y_{n+1} &= b x_n
\end{aligned}
\qquad a = 1.4,\; b = 0.3""",
    "fitzhugh_nagumo": r"""\begin{aligned}
\dot v &= v - \tfrac{1}{3} v^3 - w + I\\
\dot w &= \varepsilon\,(v + a - b w)
\end{aligned}
\qquad I = 0.5,\; a = 0.7,\; b = 0.8,\; \varepsilon = 0.08""",
    "hodgkin_huxley": r"""\begin{aligned}
C\,\dot v &= I - g_{\mathrm{Na}}\, m^3 h\,(v - E_{\mathrm{Na}})
             - g_{\mathrm{K}}\, n^4 (v - E_{\mathrm{K}}) - g_{L}\,(v - E_{L})\\
\dot m &= \alpha_m(v)\,(1 - m) - \beta_m(v)\,m\\
\dot h &= \alpha_h(v)\,(1 - h) - \beta_h(v)\,h\\
\dot n &= \alpha_n(v)\,(1 - n) - \beta_n(v)\,n
\end{aligned}""",
    "brusselator": r"""\begin{aligned}
\dot x &= a - (b + 1)\,x + x^2 y\\
\dot y &= b x - x^2 y
\end{aligned}
\qquad a = 1,\; b = 3""",
    "heat": r"""\frac{\partial u}{\partial t} = D\,\frac{\partial^2 u}{\partial x^2},
\qquad u(0, t) = u(1, t) = 0""",
    "kuramoto": r"""\dot\theta_j = \omega_j + \frac{K}{N} \sum_{k=1}^{N} \sin(\theta_k - \theta_j),
\qquad r\,e^{i\psi} = \frac{1}{N} \sum_{k=1}^{N} e^{i\theta_k}, \qquad N = 10""",
    "mackey_glass": r"""\dot x(t) = \beta\,\frac{x(t - \tau)}{1 + x(t - \tau)^{n}} - \gamma\,x(t),
\qquad \beta = 0.2,\; \gamma = 0.1,\; n = 10,\; \tau = 17""",
    "kepler": r"""\ddot{\mathbf r} = -\frac{\mathbf r}{|\mathbf r|^{3}},
\qquad E = \tfrac12 |\dot{\mathbf r}|^2 - \frac{1}{|\mathbf r|},
\qquad L = x\,\dot y - y\,\dot x""",
    "bouncing_ball": r"""\ddot h = -g, \qquad \dot h \to -e\,\dot h \ \text{ when } h = 0,
\qquad g = 9.81,\; e = 0.8""",
    "fisher_wave": r"""\frac{\partial u}{\partial t} = \frac{\partial^2 u}{\partial x^2} + u\,(1 - u)""",
    "ornstein_uhlenbeck": r"""dx = -\theta\,x\,dt + \sigma\,dW, \qquad \theta = 1,\; \sigma = 0.5""",
}

#: titles as they are written, where the model file keeps to plain letters
TITLES = {"rossler": "The Rössler attractor", "henon": "The Hénon map"}

#: the order of the gallery, by subject
SECTIONS = [
    ("Chaos", ["lorenz", "rossler", "chua", "duffing", "logistic_map", "henon", "mackey_glass"]),
    ("Oscillators", ["oscillator", "pendulum", "vanderpol", "brusselator", "kuramoto"]),
    ("Life", ["lotka_volterra", "sir", "fitzhugh_nagumo", "hodgkin_huxley"]),
    ("Mechanics and waves", ["kepler", "bouncing_ball", "heat", "fisher_wave"]),
    ("Noise", ["ornstein_uhlenbeck"]),
]


def classics_page():
    lines = ["# Gallery", "",
             "Classic dynamical systems, each ready to run. In the app they are "
             "under **Models > Classics**; in Python, "
             "`xs.load(xs.example(\"classics/lorenz\"))`. Every figure on this "
             "page is made by running the model file shown, with its own "
             "settings. Under each set of equations is the model file that "
             "says the same thing in the app's language.", "",
             "The example models that came with XPPAUT are on "
             "[their own page](xppaut.md).", ""]
    done = 0
    # At a glance: a card per model, which links its section below (#125).
    cards = ['<div class="grid cards gallery" markdown>', ""]
    for section, names in SECTIONS:
        for name in names:
            title = TITLES.get(name, describe(CLASSICS / f"{name}.ode", limit=0)[0])
            cards += [f"-   [![{title}](img/classic-{name}.png)](#{name})", "",
                      f"    **[{title}](#{name})**  ", f"    {section}", ""]
    lines += cards + ["</div>", ""]
    for section, names in SECTIONS:
        lines += [f"## {section}", ""]
        for name in names:
            path = CLASSICS / f"{name}.ode"
            draw, caption = FIGURES[name]
            title, about = describe(path, limit=0)
            title = TITLES.get(name, title)
            with xs.load(path) as m:
                fig = draw(m)
            fig.tight_layout(pad=0.4)
            fig.savefig(OUT / "img" / f"classic-{name}.png")
            plt.close(fig)
            lines += [f"### {title} {{#{name}}}", "",
                      f"![{title}](img/classic-{name}.png)", "",
                      f"*{caption}*", "", about, "",
                      "$$", EQUATIONS[name], "$$", "",
                      f'??? note "The model file: {name}.ode"', "",
                      "    ```ode",
                      *("    " + line for line in equations(path).splitlines()),
                      "    ```", "",
                      f"`xs.example(\"classics/{name}\")` · "
                      f"[{name}.ode]({REPO}examples/classics/{name}.ode)", ""]
            done += 1
    missing = sorted(p.stem for p in CLASSICS.glob("*.ode")
                     if p.stem not in {n for _, ns in SECTIONS for n in ns}
                     or p.stem not in EQUATIONS)
    if missing:
        raise SystemExit(f"classics not in the gallery: {missing}")
    (OUT / "index.md").write_text("\n".join(lines), encoding="utf-8")
    return done


# ---- XPPAUT's own examples -------------------------------------------------

def xppaut_page(limit):
    index = json.loads((ROOT / "tests" / "golden" / "index.json").read_text())
    entries = []
    for path in sorted(ODE.glob("*.ode"), key=lambda p: p.name.lower()):
        info = index.get(f"ode/{path.stem}", {})
        if info.get("status") != "ok" or info.get("seconds", 99) > 3:
            continue
        try:
            with xs.load(path) as m:
                sol = m.run()
                cols = [c for c in m.variables[:3]]
                fig, ax = plt.subplots(figsize=(3.2, 1.9), dpi=80)
                for c in cols:
                    ax.plot(sol.t, sol[c], lw=1.1, label=c)
                ax.set_xlabel("t", fontsize=7)
                ax.tick_params(labelsize=6)
                if len(cols) > 1:
                    ax.legend(fontsize=6, loc="upper right", frameon=False)
                fig.tight_layout(pad=0.3)
                fig.savefig(OUT / "img" / f"{path.stem}.png")
                plt.close(fig)
                entries.append((path, m.variables, m._parameter_names))
        except Exception as e:  # a model that does not run is skipped
            print(f"skip {path.name}: {str(e).splitlines()[0]}")
        if limit and len(entries) >= limit:
            break
    lines = ["# XPPAUT's example models", "",
             f"The {len(entries)} example models that ship with XPPAUT, run by "
             "XPP Lab, each with its first variables against time. Open any of "
             "them from the app's **Models** window, or in Python with "
             "`xs.load(xs.example(\"name.ode\"))`. The "
             "[gallery](index.md) has the classic systems.", "",
             '<div class="grid cards gallery" markdown>', ""]
    for path, variables, params in entries:
        title, rest = describe(path)
        about = " ".join(x for x in (title if title != path.stem else "", rest) if x)
        lines += [f"-   ![{path.stem}](img/{path.stem}.png)", "",
                  f"    **{path.stem}** · [model file]({REPO}engine/xppaut/ode/{path.name})",
                  f"    {{ #{path.stem} }}", "",
                  f"    {about or 'No description in the file.'}", "",
                  f"    Variables: {', '.join(variables[:10])}"
                  f"{' ...' if len(variables) > 10 else ''}. "
                  f"Parameters: {', '.join(params[:10]) or 'none'}"
                  f"{' ...' if len(params) > 10 else ''}.", ""]
    lines += ["</div>", ""]
    (OUT / "xppaut.md").write_text("\n".join(lines), encoding="utf-8")
    return len(entries)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=0)
    args = ap.parse_args()
    (OUT / "img").mkdir(parents=True, exist_ok=True)
    print(f"gallery: {classics_page()} classics, {xppaut_page(args.limit)} of XPPAUT's models")


if __name__ == "__main__":
    main()
