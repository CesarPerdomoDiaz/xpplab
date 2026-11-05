# Initial values, Range, boundary-value problems, 3D, animations and the Kinescope

## Initial values

**Workspace > Initial values** lists each variable's value at the start of
a run. Type a value, or set them all at once (XPPAUT's *Initial conds*
menu):

- **From end of last run** (I L) starts the next run where the last one
  ended. **From end, time goes on** (I S, *Shift*) also makes the start
  time T0 the time the run ended, so the clock carries on. It matters when
  the equations depend on t, as with a periodic forcing.
- **From equilibrium** takes the last equilibrium found.
- **From file...** (I F) reads one number per variable, in the order of the
  table, from a text file; spaces, new lines or commas separate them.
  **Save to file...** writes the current values exactly, one per line, so
  they can be read back later.
- **From a formula** (I U, *formUla*) gives a variable the value of a
  formula in the parameters and the model's functions: `V` = `vk + 0.1`. For
  numbered variables write a range, and `[j]` in the formula stands for the
  index: `u[0..199]` = `0.8*exp(-([j]-100)^2/50)` puts a bump in the middle
  of a 200-cell line (try the Amari model).

In the Phase Plane, **Click to start** begins a run wherever you click
(*Mouse*). Tick **Keep each** beside it and every click adds a trajectory
while the earlier ones stay (*mIce*): a few clicks sketch the phase
portrait. Erase (E) clears them.

Commands: `ic last`, `ic shift`, `ic read ics.dat`, `ic write ics.dat`,
`ic u[0..199] = 0.8*exp(-([j]-100)^2/50)`, `click -0.3 0` (a run from that
point of the phase plane), `click keep`.

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
- **Equilibria (no runs)** is XPPAUT's *Sing pts > Range*: at each value
  Newton's method looks for an equilibrium, starting from the one found at
  the value before (the first time from the initial values). The plot shows
  the chosen variable, red where the equilibrium is stable and black where
  it is not: steady states and their stability without AUTO. When the
  parameter matches the Bifurcation window's, the dots appear on AUTO's
  diagram too, and should sit on its steady-state branch.

*Start each run where the last ended* is XPPAUT's "Use old ICs = No": each
run continues from the end of the previous one, so a branch is followed
as the parameter moves, instead of every run starting from the initial
values. The parameter and initial values are put back afterwards.

Command: `range iapp 0 0.2 40 V` (max and min of V),
`range r 20 30 30 lyap continue`, `range V -0.5 0.5 10` (over V's initial
value, keeping the runs), `range iapp 0 0.3 31 eq V` (equilibria). `export
range.csv range` saves the table.

### Two values at once

**Vary a second value too** is XPPAUT's *Initial conds > 2par range*. Pick
the second parameter (or initial value) and its own *From* and *To*, then
how the two move:

- **On a grid** runs every pair: *Steps* values of the first times *Steps*
  values of the second (XPPAUT's "Array"). With *Max / min* or *Liapunov
  exponent* the result is a colour map over the two values, the first
  across and the second up. *Show* picks what the colour means for max and
  min: the swing (max - min, zero where the system settles to an
  equilibrium), the max or the min. For the Liapunov exponent the colours
  run from blue (negative) to red (positive), so chaotic regions stand out.
- **Together, along a line** moves both at once, *Steps* runs from
  (From, From) to (To, To) (XPPAUT's "Crv"); the result is plotted against
  the first value, as with one.

*Keep each trajectory* works for up to 16 runs. *Start each run where the
last ended* goes through the grid row by row: the first value changes
fastest. *Equilibria* follow the first value only.

Command: `range iapp 0 0.5 21 gca 0.8 1.4 13 V` (a 21 by 13 grid),
`range iapp 0 0.5 30 gca 0.8 1.2 lyap` (along a line: no second count),
`range iapp 0 0.3 3 V -0.2 0.2 2` (over a parameter and V's initial value,
keeping the runs). `export grid.csv range` writes one row per run with both
values.

## Every equilibrium at once

**All equilibria** in the Phase Plane (or **Run > Find all equilibria**,
or `eq all`) is XPPAUT's *Sing pts > Monte Carlo*. Newton's method starts
from 100 random points and keeps every distinct equilibrium it reaches;
each is marked in the phase plane, filled when stable, and listed in the
command window with its kind. The points are spread over the phase plane's
window and, for the other variables, over the range the last run covered,
widened by a quarter. `eq all 500` tries 500 points; `export eqs.csv
equilibria` saves every equilibrium found with its eigenvalues.

In the Morris-Lecar model at its defaults this finds the three equilibria
that the nullclines' crossings show: a stable node, a saddle and an
unstable focus. The random points come from XPPAUT's generator, which
repeats from one session to the next, so the same search finds the same
equilibria every time.

## Many runs and shooting

**Workspace > Many runs** runs the model from many initial values at once
and keeps every run, coloured in the phase plane and the time series by the
way it ended.

**Random initial values**: tick the variables to draw, each uniformly
between two values (the others keep their initial value), the number of
runs and a seed. **keep where** keeps only the draws where a column, a
variable or an `aux` quantity, lies between two values: `tens` from 0 to
`inf` keeps the states where a tether is taut. The same seed gives the same
initial values here and in Python (`Model.sample_initial`).

A run ends early when a global flag of the model stops it, such as

```
global -1 gam {arret=1}      # gam crosses zero going down: the ground
global -1 tn {arret=stop}    # a tension through zero; stop=0 turns it off
```

and the runs are counted by the flag that stopped them, or as having run to
the end. The command window and the status bar give the counts.

**Shoot from an equilibrium** finds the equilibrium near the initial values
(Newton's method), linearises there, and runs from a small step off it
along each real eigenvector, both ways: forward in time along the unstable
directions, backward along the stable ones, or both. These runs trace the
start of the invariant manifolds, as XPPAUT's *Initial conds > sHoot* does
after *Sing pts*; here they include a node's two unstable directions as
well as a saddle's.

`many 100 seed 1 V -0.5 0.5 W 0 1 keep tens 0 inf` and
`shoot eps 1e-5 time 50 unstable` do the same from the command window, and
`export runs.csv many` writes every run with its ending. The
[Figure window](figures.md) draws them for a paper.

## Frozen nullclines

**Freeze...** in the Phase Plane is XPPAUT's *Nullcline > Freeze*: it keeps
nullclines on the plot, drawn faded, while the live ones follow your
changes. Freeze them, move a slider, and you see how they shift and where
the equilibria go.

- **Freeze these nullclines** (N F) keeps the ones for the current values.
- **For several values of a parameter** (*Freeze > Range*) gives the
  parameter each of *Values* values from *From* to *To* in turn and keeps
  the nullclines for each; the parameter is put back afterwards. In the
  Morris-Lecar model, iapp from 0 to 0.4 lifts the V nullcline step by step
  while the W nullcline stays put.
- **Show** picks one set to show alone, with its value in the legend, and
  **Play** steps through them (*Freeze > Animate*). **Delete them** clears
  the list. Choosing other variables for the axes clears it too: frozen
  nullclines belong to their plane.

**Export... > Nullclines as CSV...** (XPPAUT's *Nullcline > Save*) writes
one row per line segment: the set (0 for the nullclines shown, then 1, 2,
... for those frozen), which nullcline (1 for the horizontal variable's,
2 for the vertical one's, as XPPAUT numbers them) and the segment's ends
x1, y1, x2, y2. SVG figures include the frozen nullclines.

Commands: `nullclines freeze`, `nullclines freeze iapp 0 0.4 6`,
`nullclines show 3` (0: all), `nullclines play`, `nullclines unfreeze`,
`export nc.csv nullclines`.

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
has an animation of the same name next to it, XPP Lab loads it and shows
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

### Nullclines in an animation

`xnull x1;y1;x2;y2;color;who` and `ynull ...` draw the nullclines of the
phase plane (the variables of the Phase Plane window) inside the animation.
The box (x1, y1)-(x2, y2) of the phase plane is mapped onto 0..1, the
animation's coordinates when the file has no `dimension` line. `who` is a
formula, evaluated at every frame. With -1 the lines are the current
nullclines, which the Animator computes when it opens the file and again
when a parameter or initial value changes, a grab ends or the phase plane's
variables change. With k = 0, 1, ... they are the k-th of a set of nullclines
frozen over a range of one parameter, and a formula of the time moves
through the set as the run goes on. wcstim.ani does this with
`10*stim(t)`, so the nullclines follow the stimulus. To make that set, choose
the parameter, the number of steps and the range in the row of the Animator
that appears for such a file, and press **Freeze range**. For wcstim that is
s0, 12 steps, from 0 to 1.2; the same as `ani nullclines s0 12 0 1.2`.
A frame with no such set draws no line.

![The animation of wcstim with its nullclines, the red one for u and the
green one for v, at t = 0.6 of the run](../images/animator-wcstim.png)

### Grab points

A `grab x;y;tol` line in an `.ani` file makes a point of the animation
draggable. Two lines of `{name=formula; ...}` follow it: the first is
evaluated while the mouse moves, the second when it is released. They can
use `mouse_x`, `mouse_y`, `mouse_vx` and `mouse_vy`, the position and the
velocity of the mouse in the animation's coordinates. A `runnow=1` in the
second line runs the model after the release.

Press **Grab** in the Animator. The window shows the frame of the initial
values, as XPPAUT's Grab button does, and a cross of size `tol` at each grab
point. Press on a point, drag it and release: the model's auxiliary
quantities follow while you drag, and on release the names in the second
line get their values. Variables become initial values, parameters keep
their new values, the Workspace shows them and, with `runnow=1`, the model
runs. The mode ends after one drag. If the model did not run, the window
keeps showing the initial values until you press Play, move the row slider
or run, and a second grab starts from the values the first one set. A press
farther than `tol` from every point grabs nothing.

![The animation of itoy in Grab mode, with a cross at the two grab points,
the end of the first arm of each magnet pair](../images/animator-itoy-grab.png)

In itoy.ani the first point sets `t1=a+atan2(mouse_y,mouse_x)`, the angle of
the first arm, and runs the model; the second point sets `t2` and does not.

`ani grab X Y X2 Y2` presses at (X, Y) and releases at (X2, Y2), in the
animation's coordinates, so that scripts can drag a point. The session's
Python then holds the initial values and parameters the grab set, followed by
`sol = m.run()` if it ran.

In Python, `ani.nullclines("u", "v", grid=40, parameter="s0", steps=12, low=0,
high=1.2)` makes the sets, `ani.frame(row)` returns the nullcline of each
`xnull` and `ynull` line as a shape with its `segments`, and
`ani.grab(x, y, x2, y2)` does the same drag as the command.

## The Kinescope

The **Kinescope** (*View > Kinescope*, or *Run > Kinescope...*) is
XPPAUT's film strip: it keeps pictures of a figure exactly as it is on the
screen, plays them back and saves them as an animated GIF for a talk, a
web page or a paper's supplement.

- Choose the figure (Phase Plane, Time Series, Bifurcation or the
  two-parameter diagram) and press **Capture** (K C) whenever it shows
  something worth keeping: change a parameter, run, capture again.
- **Play** (K A) steps through the frames at the speed set beside it, in a
  loop if *Loop* is ticked; the arrows step by hand, as XPPAUT's Playback
  does. **Delete all** (K R) empties the strip.
- **Save animated GIF...** writes every frame into one GIF (XPPAUT's Make
  AniGif), with the delay set by the speed. **Save frames as PNG...**
  writes NAME_001.png, NAME_002.png, ... (XPPAUT's Save).

**Workspace > Range > Film it in the Kinescope** is XPPAUT's Range with
*Movie* set to Yes: the model runs for each value in turn, each run drawn
alone, and the Kinescope captures its figure after every run. Keep that
figure in view while it films. In the Morris-Lecar model, a film of iapp
from 0 to 0.3 with the nullclines on shows the V nullcline rising, and the
trajectory going from rest to a limit cycle and back to rest as the
current grows.

A capture is a picture of the screen, so the figure must be in view: a
plot in a tab behind another cannot be captured (nor saved as a PNG).
The session's Python script writes a film's runs as `m.sweep(...)`; the
pictures themselves are not in the script. In Python, matplotlib's
`animation` module makes the same kind of film from those runs.

Commands: `kinescope capture phase`, `kinescope play 200` (ms a frame),
`kinescope show 3`, `kinescope stop`, `kinescope clear`, `kinescope save
film.gif 200`, `kinescope frames frame`, and `range iapp 0 0.3 10 film`
(or `kinescope film`, the Range tab's runs).
