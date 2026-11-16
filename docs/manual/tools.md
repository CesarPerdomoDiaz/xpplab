# Initial values, Range, boundary-value problems, 3D, animations and the Kinescope

These tools sit around the ordinary run: where it starts, how it changes
with a parameter, what else the phase plane holds, and how to show a run
as a 3-D curve, an animation or a film. Every tool also has a command, and
a reference page with its settings, worked examples and what to do when
it fails: `doc NAME` in the command window opens it. This chapter says
which tool answers which question.

## Where a run starts: initial values

**Workspace > Initial values** lists each variable's value at the start of
a run. Type a value, or set them all at once, as XPPAUT's *Initial conds*
menu does: from the end of the last run (with or without the time going
on), from the last equilibrium found, from a file, or from a formula in
the parameters, which for numbered variables can give a whole line of
cells a shape. In the Phase Plane, **Click to start** begins a run wherever
you click, and with **Keep each** a few clicks sketch the phase portrait.

Try it: [open the Amari model](example:amari.ode) and type
`ic u[0..199] = 0.8*exp(-([j]-100)^2/50)`; `[j]` stands for the index, so
this puts a bump in the middle of the 200-cell line.

See [ic last](../reference/ic-last.md), [ic shift](../reference/ic-shift.md),
[ic read](../reference/ic-read.md), [ic write](../reference/ic-write.md),
[ic](../reference/ic.md), [click](../reference/click.md) and, for the
last equilibrium, [eq](../reference/eq.md).

## How the behaviour changes with a parameter: Range

**Workspace > Range** (XPPAUT's *Initial conds > Range*) runs the model
once for each of several values of a parameter, or of a variable's initial
value. It keeps the runs, or the largest and smallest value of a variable
(a brute-force bifurcation diagram, which appears on AUTO's when the
parameters match), or the Liapunov exponent of each run, or, with no runs
at all, the equilibrium and its stability at each value. Starting each run
where the last ended follows one attractor, so sweeps up and down show
hysteresis. **Vary a second value too** sweeps two values, on a grid
(drawn as a colour map) or together along a line.

Try it: [open Morris-Lecar](example:lecar.ode) and type
`range iapp 0 0.5 21 gca 0.8 1.4 13 V`: a 21 by 13 grid, coloured by the
swing of `V` (max − min), which is zero where the system settles to an
equilibrium. `export grid.csv range` writes one row per run with both
values.

See [range](../reference/range.md).

## Every equilibrium at once

**All equilibria** in the Phase Plane (or **Run > Find all equilibria**)
is XPPAUT's *Sing pts > Monte Carlo*: Newton's method from 100 random
points over the phase plane's window, keeping every distinct equilibrium
it reaches. Use it when the nullclines cross more than once, or to check
that nothing was missed.

Try it: [open Morris-Lecar](example:lecar.ode) at its defaults and
[find them all](cmd:eq all). It finds the three equilibria that the nullclines' crossings show: a
stable node, a saddle and an unstable focus. The random points come from
XPPAUT's generator, which repeats from one session to the next, so the
same search finds the same equilibria every time.

See [eq all](../reference/eq-all.md).

## Where runs go: many runs and shooting

**Workspace > Many runs** runs the model from many initial values drawn at
random in a box and keeps every run, coloured by the way it ended: by the
global flag of the model that stopped it, or as having run to the end. It
shows how many attractors there are and roughly where their basins meet.
**Shoot from an equilibrium** runs off an equilibrium along each real
eigenvector, both ways: the start of its invariant manifolds, a node's as
well as a saddle's.

Try it: `many 100 seed 1 V -0.5 0.5 W 0 1` and
`shoot eps 1e-5 time 50 unstable` from the command window;
`export runs.csv many` writes every run with its ending, and the
[Figure window](figures.md) draws them for a paper.

See [many](../reference/many.md) and [shoot](../reference/shoot.md).

## How the nullclines move: frozen nullclines

**Freeze...** in the Phase Plane (XPPAUT's *Nullcline > Freeze*) keeps
nullclines on the plot, drawn faded, while the live ones follow your
changes. Freeze them, move a slider, and you see how they shift and where
the equilibria go. Frozen for several values of a parameter, they can be
shown one at a time or played as a film.

Try it: in the Morris-Lecar model, `nullclines freeze iapp 0 0.4 6` lifts
the V nullcline step by step while the W nullcline stays put; then
`nullclines play`.

See [nullclines freeze](../reference/nullclines-freeze.md),
[nullclines play](../reference/nullclines-play.md) and
[nullclines unfreeze](../reference/nullclines-unfreeze.md); `export nc.csv
nullclines` writes them ([export](../reference/export.md)).

## Conditions at both ends: boundary-value problems

A model with `bndry` statements is a boundary-value problem: instead of all
the initial values, it gives as many conditions as there are equations, at
the start (plain names) or at the end (primed names) of the interval from
T0 to T0 + Total. *Run > Boundary-value problem*, the **Solve BVP**
toolbar button, or `bvp` solve it by XPPAUT's shooting method, starting
from the current initial values as a guess. For example, the cable
equation

```
v'=vx
vx'=v/lam2
b v-1        # v = 1 at the start
b v'         # v = 0 at the end
```

Try it: [open the cable model](example:cable.ode) and press *Solve BVP*.
If it does not converge, give a better guess for the initial values.

See [bvp](../reference/bvp.md).

## A run in three dimensions: the 3D view

**View > 3D View** shows three columns of the run, and any kept curves, in
a box you turn by dragging or let spin. A model that asks for a 3-D plot
opens with its own view, as in XPPAUT. Use it for models with three or
more variables, once the Time Series and the Phase Plane have shown the
projections.

Try it: the [Lorenz model](example:lorenz.ode), with `view3d X Y Z` and
`view3d spin`.

See [view3d](../reference/view3d.md).

## A run as a picture: animations

XPPAUT animations are `.ani` files: lines, circles, rectangles, text and
comets whose positions and colours are formulas of the variables. The
**Animator** (*View > Animator*) plays one over the run, and when a model
has an animation of the same name next to it, XPP Lab loads it and shows
an **Animate** button. An animation can also draw the phase plane's
nullclines, and declare grab points that set initial values or parameters
when dragged.

Try it: [open the double pendulum](example:doubpend.ode), [run it](cmd:run)
and press *Animate*.

See [ani](../reference/ani.md), which also describes the file.

## A film of a figure: the Kinescope

The **Kinescope** (*View > Kinescope*, or *Run > Kinescope...*) is
XPPAUT's film strip: it keeps pictures of a figure exactly as it is on the
screen, plays them back and saves them as an animated GIF for a talk, a
web page or a paper's supplement. Capture by hand whenever the figure
shows something worth keeping, or let **Workspace > Range > Film it in the
Kinescope** run each value and capture the figure after every run. A
capture is a picture of the screen, so the figure must be in view.

Try it: in the Morris-Lecar model, with the nullclines on, film `iapp`
from 0 to 0.3 (`range iapp 0 0.3 10 film`): the V nullcline rises, and the
trajectory goes from rest to a limit cycle and back to rest as the current
grows. `kinescope save film.gif 200` saves it.

See [kinescope capture](../reference/kinescope-capture.md),
[kinescope play](../reference/kinescope-play.md) and
[kinescope clear](../reference/kinescope-clear.md).
