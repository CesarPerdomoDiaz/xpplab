# ani

Play an XPPAUT animation of the last run: shapes drawn from the run's
values, frame by frame.

## Syntax

```
ani FILE.ani
ani play
ani stop
ani row N
ani grab X Y X2 Y2
ani nullclines
ani nullclines PAR N FROM TO
```

- **In the app:** Run > **Animator...**, or View > **Animator** (XPPAUT's
  *Viewaxes > Toon*); **Open .ani...** in it reads a file.
- **In Python:** `a = m.animation("pend.ani")`, then `a.frame(row)`,
  `a.plot(row)` or `a.save_gif("pend.gif")`.

## Description

An animation file (`.ani`) describes a picture in terms of the model's
quantities: lines, circles, rectangles, text and comets (points with a
trail) whose positions and colours are formulas. `ani FILE.ani` reads it,
and the Animator window draws it for each stored point of the last run, so
the run plays as a film: a pendulum swings, a wave travels, a cell fires.

When a model has an animation of the same name next to it, XPP Lab loads
it with the model and shows an **Animate** button in the toolbar. Example
models with animations include doubpend, pend, invpend, elaspen, duck and
the roller coasters (acoaster, jcoaster, ...).

`ani play` starts the film, `ani stop` stops it, and `ani row N` shows the
frame of row `N`. The picture's coordinates run from 0 to 1 across and up.
`ani grab X Y X2 Y2` drags a grab point of the file, and `ani nullclines`
computes the nullclines its `xnull` and `ynull` lines draw (see More
about).

The file's commands are XPPAUT's: `line`, `rline`, `rect`, `frect`,
`circ`, `fcirc`, `ellip`, `fellip`, `comet`, `text`, `vtext`, `settext`,
`speed`, `dimension`, `permanent`, `transient`, `grab`, `xnull` and
`ynull`, each with its arguments separated by semicolons, and the file
ends with `end`. A range such as
`th[0..7]` repeats a command once for each index.

## Examples

### A damped pendulum

`pend.ode` is a damped pendulum, `x` its angle, started at `x` = 2 (about
115 degrees). `pend.ani` draws it as a rod from the pivot (0.5, 0.5) to
a red bob at (0.5 + 0.4 sin x, 0.5 − 0.4 cos x).

1. [Open the pendulum](example:pend.ode).
2. [Load its animation](cmd:ani pend.ani) and [play it](cmd:ani play).

```
ani pend.ani
ani play
```

```python
import xpplab as xs

m = xs.load(xs.example("pend.ode"))
m.run()
a = m.animation("pend.ani")
print(len(a), a.frame(0), a.frame(len(a) - 1))
```

The command window says "Loaded the animation pend.ani (2 commands)."
The film has 401 frames, one per stored point to `t` = 20. In the first,
the bob is at (0.8637, 0.6665); in the last, at (0.4193, 0.1082), nearly
below the pivot.

How to read it. The positions are the file's formulas evaluated on each
row: at the start sin 2 = 0.909 puts the bob up and to the right; at the
end the angle has decayed to −0.203, a few degrees from the bottom, as
the damping takes the energy out. The film shows the swings shrinking,
which the Time Series shows as a decaying oscillation of `x`.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `FILE.ani` | the command | The animation file. | A relative name is found next to the model. |
| `N` | `ani row N`; the Animator's row slider | The row to show. | 0 is the first. |
| **speed** | the Animator | Rows a second while playing. | Slower to follow a fast event. |
| **Loop** | the Animator | Start again after the last row. | |
| `speed` | in the file | The pause between frames, from 0 to 1000 (10 if not given). | Lower is faster. |
| `PAR N FROM TO` | `ani nullclines`; the Animator's row for an `xnull` file, then **Freeze range** | The parameter and the range over which the sets of nullclines are frozen. | For wcstim.ani: `s0`, 12 steps, from 0 to 1.2. |

## Results

- **The Animator window** draws the frames: **Play**, the row slider, the
  **speed** in rows a second and **Loop**; the time of the row is shown
  beside them. A file with grab points adds **Grab**, and one with `xnull`
  or `ynull` lines a row to freeze its nullclines.
- **The command window** says how many commands the file has.
- **In Python** `m.animation()` returns an `Animation`: `len()` frames,
  `frame(row)` the shapes of a frame (their kind, coordinates and colour;
  for an `xnull` or `ynull` line, the nullcline as a shape with its
  `segments`), `plot(row)` a matplotlib drawing, and `save_gif()` the whole
  film. `nullclines("u", "v", grid=40, parameter="s0", steps=12, low=0,
  high=1.2)` makes the sets of nullclines, and `grab(x, y, x2, y2)` does
  the same drag as `ani grab`.

## More about

**The file.** A small example, `pend.ani`:

```
# the rod and the bob of a pendulum of angle x
line .5;.5;.5+.4*sin(x);.5-.4*cos(x);$BLACK;3
fcircle .5+.4*sin(x);.5-.4*cos(x);.05;$RED
end
```

Coordinates run from 0 to 1 unless a `dimension x1;y1;x2;y2` line says
otherwise. Colours are `$BLACK`, `$RED`, `$ORANGE`, `$YELLOW`, `$GREEN`,
`$BLUE`, `$PURPLE` and the others in between, or a formula from 0 to 1
along the colour scale.

**Permanent and transient.** Commands after `permanent` are drawn once and
stay, such as a pivot or a track; commands after `transient`, the default,
are drawn anew for each frame.

**Nullclines in an animation.** `xnull x1;y1;x2;y2;color;who` and
`ynull ...` draw the nullclines of the phase plane (the variables of the
Phase Plane window) inside the animation. The box (x1, y1)-(x2, y2) of the
phase plane is mapped onto 0..1, the animation's coordinates when the file
has no `dimension` line. `who` is a formula, evaluated at every frame.
With −1 the lines are the current nullclines, which the Animator computes
when it opens the file and again when a parameter or initial value
changes, a grab ends or the phase plane's variables change. With k = 0, 1,
... they are the k-th of a set of nullclines frozen over a range of one
parameter, and a formula of the time moves through the set as the run goes
on: wcstim.ani does this with `10*stim(t)`, so the nullclines follow the
stimulus. To make that set, choose the parameter, the number of steps and
the range in the row of the Animator that appears for such a file, and
press **Freeze range**. For wcstim that is `s0`, 12 steps, from 0 to 1.2;
the same as `ani nullclines s0 12 0 1.2`. A frame with no such set draws
no line.

![The animation of wcstim with its nullclines, the red one for u and the
green one for v, at t = 0.6 of the run](../images/animator-wcstim.png)

**Grab points.** A `grab x;y;tol` line makes a point of the animation
draggable. Two lines of `{name=formula; ...}` follow it: the first is
evaluated while the mouse moves, the second when it is released. They can
use `mouse_x`, `mouse_y`, `mouse_vx` and `mouse_vy`, the position and the
velocity of the mouse in the animation's coordinates. A `runnow=1` in the
second line runs the model after the release. Among XPPAUT's examples,
`idoubpend.ani` and `itoy.ani` declare grab points.

Press **Grab** in the Animator. The window shows the frame of the initial
values, as XPPAUT's Grab button does, and a cross of size `tol` at each
grab point. Press on a point, drag it and release: the model's auxiliary
quantities follow while you drag, and on release the names in the second
line get their values. Variables become initial values, parameters keep
their new values, the Workspace shows them and, with `runnow=1`, the model
runs. The mode ends after one drag. If the model did not run, the window
keeps showing the initial values until you press Play, move the row slider
or run, and a second grab starts from the values the first one set. A
press farther than `tol` from every point grabs nothing.

![The animation of itoy in Grab mode, with a cross at the two grab points,
the end of the first arm of each magnet pair](../images/animator-itoy-grab.png)

In itoy.ani the first point sets `t1=a+atan2(mouse_y,mouse_x)`, the angle
of the first arm, and runs the model; the second point sets `t2` and does
not.

`ani grab X Y X2 Y2` presses at (`X`, `Y`) and releases at (`X2`, `Y2`),
in the animation's coordinates, so that scripts can drag a point. The
session's Python then holds the initial values and parameters the grab
set, followed by `sol = m.run()` if it ran.

## Tips

- Write the coordinates in terms of the variables and parameters, and
  scale them into 0 to 1 in the file, as `.5+.4*sin(x)` does.
- Watch an animation together with the Time Series: the film shows the
  motion, the plot the numbers.

## Algorithms

XPPAUT's own animation code reads the file and evaluates its formulas on
each stored row; the app and Python draw the shapes it returns.

## When it fails

| Message | Why | What to do |
|---|---|---|
| An error with a line of the file | A command or a formula is wrong. | Check that line: the names must be the model's, and the arguments separated by semicolons. |
| Nothing moves | The run is a single point, or the formulas do not use the variables. | Run first; check the formulas. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [run](run.md): the run that is played.
- [view3d](view3d.md): another way to see a run.

## Version history

Introduced in 0.1.0.
