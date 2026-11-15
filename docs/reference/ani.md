# ani

Play an XPPAUT animation of the last run: shapes drawn from the run's
values, frame by frame.

## Syntax

```
ani FILE.ani
ani play
ani stop
ani row N
```

- **In the app:** Run > **Animator...** (XPPAUT's *Viewaxes > Toon*).
- **In Python:** `a = m.animation("pend.ani")`, then `a.frame(row)`,
  `a.plot(row)` or `a.save_gif("pend.gif")`.

## Description

An animation file (`.ani`) describes a picture in terms of the model's
quantities: lines, circles, rectangles and text whose positions and
colours are formulas. `ani FILE.ani` reads it, and the Animator window
draws it for each stored point of the last run, so the run plays as a
film: a pendulum swings, a wave travels, a cell fires.

`ani play` starts the film, `ani stop` stops it, and `ani row N` shows the
frame of row `N`. The picture's coordinates run from 0 to 1 across and up.

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
| `N` | `ani row N` | The row to show. | 0 is the first. |
| `speed` | in the file | The pause between frames, from 0 to 1000 (10 if not given). | Lower is faster. |

## Results

- **The Animator window** draws the frames, and plays or steps through
  them.
- **The command window** says how many commands the file has.
- **In Python** `m.animation()` returns an `Animation`: `len()` frames,
  `frame(row)` the shapes of a frame (their kind, coordinates and colour),
  `plot(row)` a matplotlib drawing, and `save_gif()` the whole film.

## More about

**Permanent and transient.** Commands after `permanent` are drawn once and
stay, such as a pivot or a track; commands after `transient`, the default,
are drawn anew for each frame.

**Grabbing.** An animation can declare *grab points*, so that dragging a
shape in the Animator sets new initial values and starts a run from them:
`ani grab X Y X2 Y2` does the same from the command line. Among XPPAUT's
examples, `idoubpend.ani` and `itoy.ani` declare grab points.

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
