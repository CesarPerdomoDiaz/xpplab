# kinescope play

Play the Kinescope's pictures, or show one of them.

## Syntax

```
kinescope play
kinescope play MS
kinescope stop
kinescope show K
```

- **In the app:** View > **Kinescope**: **Play**, the arrows, the speed
  and **Loop**; XPPAUT's keys `K A` (*Kinescope > Autoplay*).
- **In Python:** a matplotlib animation plays its frames in a window
  (`plt.show()`) or in a notebook.

## Description

`kinescope play` shows the pictures taken with
[kinescope capture](kinescope-capture.md) one after another, `MS`
milliseconds each, in a loop while the Kinescope's **Loop** box is ticked
(it is at first). `MS` stays as the Kinescope's speed, which is also the
GIF's when `kinescope save` is given none; it is 250 at first.

`kinescope stop` stops on the picture shown. `kinescope show K` stops and
shows the `K`-th picture, counting from 1, as XPPAUT's Playback steps by
hand.

## Examples

### A film played slowly

1. Take two or more pictures, as in
   [kinescope capture](kinescope-capture.md).
2. [Play them half a second each](cmd:kinescope play 500), then
   [stop on the first](cmd:kinescope show 1).

```
kinescope play 500
kinescope show 1
```

```python
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
runs = [m.run(iapp=i) for i in (0.05, 0.1, 0.2)]

fig, ax = plt.subplots()
def frame(k):
    ax.clear()
    ax.plot(runs[k]["V"], runs[k]["W"])
    ax.set(xlim=(-0.6, 1.2), ylim=(-0.25, 1.2), xlabel="V", ylabel="W")
film = FuncAnimation(fig, frame, frames=len(runs), interval=500)
plt.show()
```

The Kinescope steps through its pictures every 500 ms, and from the last
goes back to the first. `kinescope show 1` stops on the first; the
counter beside the arrows reads `1 / 3` for three pictures.

How to read it. Played slowly, a film shows what changes between
pictures; stepped by hand, it lets you compare two side by side in time.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `MS` | `kinescope play`; the Kinescope's speed | Milliseconds per picture. | 200 to 500. |
| Loop | the Kinescope's **Loop** box | Start again after the last picture. | Untick to stop on the last. |
| `K` | `kinescope show` | The picture to show, from 1. | |

## Results

- **The Kinescope** shows its pictures in turn, or the one asked for, with
  its number.

## More about

**Speed and the GIF.** The speed set here is the one a GIF is saved with
when `kinescope save` has no `MS`, so a film can be tried at a speed and
saved at it.

## Tips

- Step with the arrows to find the picture where something changes, then
  `kinescope frames` to save it.

## Algorithms

The next picture is shown when `MS` milliseconds have passed since the
last; nothing is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: the Kinescope has N frames; it plays two or more (kinescope capture takes one)` | Fewer than two pictures. | Capture more. |
| `error: kinescope play takes the milliseconds per frame, a positive whole number (kinescope play 200)` | `MS` is not a positive number. | Give milliseconds. |
| `error: the Kinescope has N frames; show takes 1 to N` | `K` is outside the pictures. | Give a number from 1 to N. |
| `error: the Kinescope has no frames yet.` | `show` before any capture. | Capture first. |
| `error: the Kinescope takes pictures of the window: it needs the desktop app` | In `xpplab-batch`. | Use the desktop app. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [kinescope capture](kinescope-capture.md): take the pictures, save the
  film.
- [kinescope clear](kinescope-clear.md): delete them.
- [nullclines play](nullclines-play.md): play frozen nullclines in the
  Phase Plane itself.

## Version history

Introduced in 0.1.0. Since 0.1.4 `play` and `show` say what is wrong
instead of doing nothing.
