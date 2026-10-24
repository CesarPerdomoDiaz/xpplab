# Range, boundary-value problems, 3D and animations

## Range: many runs over a parameter

**Workspace > Range** is XPPAUT's *Initial conds > Range*. It runs the model
once for each of *Steps* values of a parameter, or of a variable's initial
value, from *From* to *To*:

- **Keep each trajectory** overlays the runs on the plots (up to 16).
- **Max / min of a variable** records the largest and smallest value of a
  variable after a transient: a brute-force bifurcation diagram. When the
  parameter matches the Bifurcation window's, the points appear on AUTO's
  diagram.
- **Liapunov exponent** gives the largest exponent of each run. Where it
  rises above zero the system is chaotic; try the Lorenz model with `r`
  from 20 to 30.

*Start each run where the last ended* is XPPAUT's "Use old ICs = No": each
run continues from the end of the previous one, so a branch is followed
as the parameter moves, instead of every run starting from the initial
values. The parameter and initial values are put back afterwards.

Command: `range iapp 0 0.2 40 V` (max and min of V),
`range r 20 30 30 lyap continue`, `range V -0.5 0.5 10` (over V's initial
value, keeping the runs).

## Boundary-value problems

A model with `bndry` statements is a boundary-value problem: instead of
all the initial values, it gives as many conditions as there are equations,
at the start (plain names) or at the end (primed names) of the interval
from T0 to T0 + Total. For example, the cable equation

```
v'=vx
vx'=v/lam2
b v-1        # v = 1 at the start
b v'         # v = 0 at the end
```

*Run > Boundary-value problem*, the **Solve BVP** toolbar button, or the
command `bvp` solve it by XPPAUT's shooting method. Starting from the
current initial values as a guess, it adjusts them until the conditions
hold. The solution becomes the run, and its initial values replace the old
ones, so the next solve starts from it. [Open the cable model](example:cable.ode)
and press *Solve BVP* to try it.

If it does not converge, give a better guess for the initial values. From
Python, `m.bvp(max_iterations=..., tolerance=...)` also sets XPPAUT's
shooting limits.

## The 3D view

**View > 3D View** shows three columns of the run (and any kept curves) in
a box. Drag to turn it, scroll to zoom, double-click to reset the zoom.
Tick **Spin** to have it turn slowly by itself. Choose the columns with
*x*, *y* and *z*.

The angles are XPPAUT's: *theta* turns the box about the vertical axis, and
*phi* tilts it, with 90 for a side view and 0 looking straight down. A model
that asks for a 3D plot (`@ xplot=x, yplot=y, zplot=z, axes=3d, phi=60`, and
`xmin=` ... `zmax=` for the box) opens with its view, as in XPPAUT; *Reset*
returns to it. Try the [Lorenz model](example:lorenz.ode).

Commands: `view3d X Y Z`, `view3d spin` (`view3d spin off`), `view3d angles THETA PHI`.

## Animations

XPPAUT animations are `.ani` files: lines, circles, rectangles, text and
comets (points with a trail) whose positions and colours are formulas of
the variables. The **Animator** (*View > Animator*) plays one over the run:
*Play*, the row slider, *speed* in rows per second and *Loop*. When a model
has an animation of the same name next to it, XPP Studio loads it and shows
an **Animate** button; open another with *Open .ani...*.

[Open the double pendulum](example:doubpend.ode), [run it](cmd:run) and
press *Animate*. Other models with animations include pend, invpend,
elaspen, duck and the roller coasters.

A small example, pend.ani:

```
# the rod and the bob of a pendulum of angle x
line .5;.5;.5+.4*sin(x);.5-.4*cos(x);$BLACK;3
fcircle .5+.4*sin(x);.5-.4*cos(x);.05;$RED
end
```

Coordinates run from 0 to 1 unless a `dimension x1;y1;x2;y2` line says
otherwise. Commands are evaluated at every frame, except those after a
`permanent` line, which are drawn once. Colours are `$BLACK`, `$RED`,
`$ORANGE`, `$YELLOW`, `$GREEN`, `$BLUE`, `$PURPLE` and the others in
between, or a formula from 0 to 1 along the colour scale.

Commands: `ani pend.ani`, `ani play`, `ani row 200`.
