# Glossary

The words of dynamical systems and of AUTO that these pages use. On every
page of the site, resting the mouse on one of the underlined terms shows
its line from here. The textbooks in each reference page's *References*
give the full theory.

## States and their stability

**Equilibrium** (steady state, fixed point). A state where every
derivative is zero, so the system stays there. For a map, a state the map
sends to itself. See [eq](reference/eq.md).

**Jacobian.** The matrix of the partial derivatives of the right-hand side
with respect to the variables, $J_{ij} = \partial f_i / \partial x_j$. At an
equilibrium it gives the linear system that small departures follow.

**Eigenvalue** (of an equilibrium). An eigenvalue $\lambda$ of the
Jacobian there. A small departure along its eigenvector grows or decays
like $e^{\lambda t}$: the equilibrium is stable when every eigenvalue has a
negative real part (for a map, when every one lies inside the unit
circle).

**Stable, unstable.** An equilibrium or a cycle is stable when the states
near it approach it, and unstable when some move away.

**Node, focus, saddle.** An equilibrium whose eigenvalues are real and of
one sign is a node; one with a complex pair, around which the runs
spiral, is a focus; one with real eigenvalues of both signs is a saddle,
which runs approach along some directions and leave along others.

**Stable and unstable manifolds.** The states that approach a saddle as
time goes on, and those that came from it. In the plane they are curves
that divide the phase plane. See [shoot](reference/shoot.md).

**Nullcline.** A curve where one variable's derivative is zero. Where the
nullclines of all the variables cross there is an equilibrium. See
[nullclines](reference/nullclines.md).

**Direction field.** Arrows across the phase plane showing which way, and
how fast, the state moves from each point. See
[dirfield](reference/dirfield.md).

**Phase plane.** The plane of two variables, in which a run of a
two-variable model is a curve.

**Basin of attraction.** The initial states from which the runs reach a
given attractor.

**Bistability.** Two stable states (or a stable state and a stable cycle)
at the same parameter values; which one a run reaches depends on where it
starts.

**Hysteresis.** The state a system is in depends on its history: sweeping
a parameter up and then down gives different results where two attractors
coexist. See [range](reference/range.md).

## Oscillations

**Limit cycle.** An isolated closed orbit: a periodic oscillation that
nearby runs approach (stable) or leave (unstable).

**Floquet multiplier.** For a periodic orbit, the factor by which a small
departure from it is multiplied over one period. One multiplier is always
1; the orbit is stable when all the others lie inside the unit circle.

**Phase response curve** (PRC). How much a small kick at each phase of a
limit cycle advances or delays the next oscillation. See
[prc](reference/prc.md).

**Liapunov exponent.** The average rate at which nearby runs separate:
negative at a stable equilibrium, zero on a limit cycle, positive in
chaos. See [mean](reference/mean.md).

**Poincaré map.** The map from one crossing of a surface by a run to the
next, which turns a continuous flow into a discrete one.

**Stiff.** Equations whose rates differ by orders of magnitude, so that an
explicit method needs a tiny step; an implicit method such as `stiff` or
`cvode` does not.

## Bifurcations

**Bifurcation.** A qualitative change in a system's behaviour as a
parameter crosses a value: equilibria appear, vanish or change stability,
or oscillations are born or die.

**Bifurcation diagram.** A plot of the equilibria and periodic orbits
against a parameter, stable and unstable, with the bifurcations marked.
See [auto steady](reference/auto-steady.md).

**Continuation.** Following a solution, an equilibrium or a periodic
orbit, step by step as a parameter changes, by predicting along the
branch and correcting with Newton's method. AUTO does this.

**Branch.** A connected curve of solutions in a bifurcation diagram.

**Fold** (limit point, saddle-node, LP). Two equilibria, or two periodic
orbits, meet and vanish as the parameter passes a value; the branch turns
back.

**Hopf bifurcation** (HB). A pair of complex eigenvalues crosses the
imaginary axis, and a family of small periodic orbits is born. It is
supercritical when the orbits are stable, so oscillations grow smoothly
from zero, and subcritical when they are unstable, so the system jumps to
a large oscillation.

**Branch point** (BP). Two branches cross, often because of a symmetry.

**Period doubling** (PD). A periodic orbit loses stability to one of twice
its period: a Floquet multiplier crosses $-1$.

**Torus bifurcation** (TR, Neimark-Sacker). A periodic orbit loses
stability to motion on a torus, quasi-periodic: a complex pair of Floquet
multipliers crosses the unit circle.

**Homoclinic orbit.** An orbit that leaves a saddle and returns to it. A
periodic orbit whose period grows without bound as a parameter changes
often ends on one.

**Heteroclinic orbit.** An orbit that leaves one saddle and arrives at
another.

**SNIC** (saddle-node on an invariant circle). A fold that happens on a
limit cycle: as the parameter passes it the cycle's period becomes
infinite, so oscillations start at zero frequency.

**Codimension.** The number of parameters that must be tuned for a
bifurcation to happen: a fold or a Hopf point has codimension one, and is
followed as a curve in two parameters. See
[Codimension-two bifurcations](manual/codim2.md).

**Cusp.** A codimension-two point where two curves of folds meet.

**Bogdanov-Takens point.** A codimension-two point where a curve of Hopf
points ends on a curve of folds.

## AUTO's labels

**EP.** An end point of a branch: where AUTO stopped.

**MX.** A point where AUTO failed to converge.

**UZ.** A point at a value the user asked AUTO to stop or mark at.
