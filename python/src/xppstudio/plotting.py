"""Matplotlib plots of trajectories and bifurcation diagrams.

Colours follow XPPAUT's AUTO window so its users feel at home: stable
steady states red, unstable black, stable periodic orbits green, unstable
blue. Unstable branches are also dashed, so the figures work in greyscale.
"""

from __future__ import annotations

COLORS = {
    (False, True): "#d62728",   # steady, stable
    (False, False): "#000000",  # steady, unstable
    (True, True): "#2ca02c",    # periodic, stable
    (True, False): "#1f77b4",   # periodic, unstable
}


def _axes(ax):
    if ax is None:
        import matplotlib.pyplot as plt
        _, ax = plt.subplots()
    return ax


def plot_trajectory(traj, y, x="T", ax=None, **kw):
    ax = _axes(ax)
    names = y or [c for c in traj.columns if c.upper() != x.upper()][:1]
    xs = traj[x]
    for name in names:
        ax.plot(xs, traj[name], label=name, **kw)
    ax.set_xlabel("t" if x.upper() == "T" else x)
    if len(names) == 1:
        ax.set_ylabel(names[0])
    else:
        ax.legend()
    return ax


def plot_phase_plane(model, x, y, xlim=None, ylim=None, trajectory=True,
                     nullclines=True, field=True, equilibrium=True, ax=None):
    from matplotlib.collections import LineCollection
    import numpy as np
    ax = _axes(ax)
    sol = model.run() if trajectory else None
    if xlim is None or ylim is None:
        if sol is not None and len(sol):
            def pad(a):
                lo, hi = float(np.min(a)), float(np.max(a))
                d = (hi - lo) or 1.0
                return lo - 0.15 * d, hi + 0.15 * d
            xlim = xlim or pad(sol[x])
            ylim = ylim or pad(sol[y])
        else:
            xlim, ylim = xlim or (-1, 1), ylim or (-1, 1)
    if field:
        X, Y, U, V = model.vector_field(x, y, xlim, ylim, n=21)
        M = np.hypot(U / (xlim[1] - xlim[0]), V / (ylim[1] - ylim[0]))
        M[M == 0] = 1
        ax.quiver(X, Y, U / (xlim[1] - xlim[0]) / M, V / (ylim[1] - ylim[0]) / M,
                  color="0.6", angles="xy", scale=30, width=0.0025)
    if nullclines:
        xn, yn = model.nullclines(x, y, xlim, ylim)
        for segs, color, name in ((xn, COLORS[(False, True)], x),
                                  (yn, "#2ca02c", y)):
            if len(segs):
                lc = LineCollection(segs.reshape(-1, 2, 2), colors=color,
                                    linewidths=1.8, label=f"{name} nullcline")
                ax.add_collection(lc)
    if sol is not None:
        ax.plot(sol[x], sol[y], color="#1f77b4", lw=1.6, label="trajectory")
        ax.plot(sol[x][:1], sol[y][:1], "s", color="#1f77b4", ms=4)
    if equilibrium:
        try:
            eq = model.equilibrium()
            ax.plot(eq[x], eq[y], "o", ms=7,
                    mfc="k" if eq.stable else "w", mec="k", zorder=5,
                    label=f"equilibrium ({eq.kind})")
        except Exception:
            pass
    ax.set_xlim(*xlim)
    ax.set_ylim(*ylim)
    ax.set_xlabel(x)
    ax.set_ylabel(y)
    ax.legend(loc="best", fontsize=8)
    return ax


def _segments(points):
    """Split points into runs of the same branch, kind and stability. Each
    run starts at the previous run's last point so the curve is unbroken."""
    runs, cur = [], []
    for p in points:
        key = (p.branch, p.periodic, p.stable, p.two_parameter)
        if cur and (cur[-1][0] != key or abs(cur[-1][1].point) + 1 != abs(p.point)):
            same_branch = cur[-1][0][0] == key[0]
            runs.append(cur)
            cur = [cur[-1]] if same_branch else []
        cur.append((key, p))
    if cur:
        runs.append(cur)
    out = []
    for run in runs:
        key = run[-1][0]
        out.append((key, [p for _, p in run]))
    return out


def plot_diagram(diagram, y=None, ax=None, labels=True, **kw):
    ax = _axes(ax)
    pts = list(diagram)
    if not pts:
        return ax
    two_par = [p for p in pts if p.two_parameter]
    one_par = [p for p in pts if not p.two_parameter]
    var = y or diagram.variables[0]

    def yval(p, which):
        if var == "norm":
            return p.norm
        if var == "period":
            return p.period
        src = p.umax if which == "max" else p.umin
        for k, v in src.items():
            if k.upper() == var.upper():
                return v
        raise KeyError(f"no variable {var!r}")

    for (branch, periodic, stable, _), run in _segments(one_par):
        style = dict(color=COLORS[(periodic, stable)],
                     linestyle="-" if stable else "--",
                     linewidth=2.0 if stable else 1.2)
        style.update(kw)
        xs = [p.value for p in run]
        ax.plot(xs, [yval(p, "max") for p in run], **style)
        if periodic and var not in ("norm", "period"):
            ax.plot(xs, [yval(p, "min") for p in run], **style)

    for (_, _, stable, _), run in _segments(two_par):
        style = dict(color="#9467bd", linewidth=1.8)
        style.update(kw)
        ax.plot([p.value for p in run], [p.value2 for p in run], **style)

    if labels:
        for p in diagram.special():
            if p.two_parameter:
                xy = (p.value, p.value2)
            else:
                xy = (p.value, yval(p, "max"))
            ax.plot(*xy, marker="o", markersize=4, color="#ff7f0e", zorder=5)
            ax.annotate(p.type, xy, textcoords="offset points", xytext=(4, 4),
                        fontsize=8, color="#ff7f0e")

    first = pts[0]
    ax.set_xlabel(first.parameter)
    if two_par and not one_par:
        ax.set_ylabel(two_par[0].parameter2 or "")
    else:
        ax.set_ylabel(var if var in ("norm", "period") else f"{var} (max/min)")
    return ax
