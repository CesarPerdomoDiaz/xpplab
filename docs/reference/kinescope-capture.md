# kinescope capture

Take a picture of a figure for the Kinescope, XPPAUT's film strip; save
the pictures as an animated GIF or as PNG files.

## Syntax

```
kinescope capture
kinescope capture FIGURE
kinescope save FILE.gif
kinescope save FILE.gif MS
kinescope frames BASE
kinescope film
kinescope film FIGURE
```

`FIGURE` is `time`, `phase`, `auto` or `auto2`. `ks` is another name for
`kinescope`.

- **In the app:** View > **Kinescope**, or Run > **Kinescope...**: the
  figure, **Capture**, **Save animated GIF...** (XPPAUT's *Make AniGif*)
  and **Save frames as PNG...** (XPPAUT's *Save*); XPPAUT's keys `K C`
  (*Kinescope > Capture*). Workspace > Range > **Film it in the
  Kinescope** is `kinescope film`.
- **In Python:** draw each run with matplotlib and save the frames with
  `matplotlib.animation`.

## Description

`kinescope capture` takes a picture of a figure exactly as the window
shows it: the Time Series (`time`), the Phase Plane (`phase`), the
bifurcation diagram (`auto`) or the two-parameter diagram (`auto2`).
Without `FIGURE` it uses the figure of the last capture, the Phase Plane
at first. The pictures are kept in order, up to 200.

`kinescope save FILE.gif` writes every picture into one animated GIF,
`MS` milliseconds each (the Kinescope's speed when `MS` is not given;
250 at first). `kinescope frames BASE` writes them as `BASE_001.png`,
`BASE_002.png`, ... Relative paths are relative to the model's folder.

`kinescope film` is XPPAUT's Range with *Movie* set: it empties the
Kinescope, runs the model for each value of the Workspace's Range tab,
draws each run alone, and captures the figure after every run, so keep
that figure in view while it films. Afterwards the parameter and the
initial values are as they were.
`range iapp 0 0.3 10 film` sets the range and films it in one command.

## Examples

### From rest to spikes

Film Morris-Lecar as the current grows:

1. [Open the Morris-Lecar model](example:lecar.ode),
   [draw the nullclines](cmd:nullclines) and
   [show the Phase Plane](cmd:show phase).
2. [Capture it](cmd:kinescope capture phase).
3. [Set the current to 0.1](cmd:set iapp=0.1), [run](cmd:run) and
   [capture](cmd:kinescope capture); then the same for
   [0.2](cmd:set iapp=0.2).
4. [Save the film](cmd:kinescope save film.gif 300).

```
nullclines
show phase
kinescope capture phase
set iapp=0.1
run
kinescope capture
set iapp=0.2
run
kinescope capture
kinescope save film.gif 300
```

```python
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, PillowWriter
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
runs = [m.run(iapp=i) for i in (0.05, 0.1, 0.2)]

fig, ax = plt.subplots()
def frame(k):
    ax.clear()
    ax.plot(runs[k]["V"], runs[k]["W"])
    ax.set(xlim=(-0.6, 1.2), ylim=(-0.25, 1.2), xlabel="V", ylabel="W")
FuncAnimation(fig, frame, frames=len(runs)).save("film.gif", writer=PillowWriter(fps=1000 / 300))
```

The command window answers "Saved film.gif: 3 frames of ... pixels, 300
ms each", with the figure's size on the screen. The first frame shows the
run settling at rest, near `V` = −0.39; the second spikes, `V` going from
−0.38 to 0.26 once the run has settled; the third spikes from −0.28 to
0.29.

How to read it. Each frame is the figure as it was when captured, so the
film shows what changed between captures: here the nullclines and the
orbit moving as `iapp` grows. Change one thing between captures, and the
film is easy to follow.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `FIGURE` | the command; the Kinescope's figure box | Which figure to capture. | Keep it in view: a tab behind another cannot be captured. |
| `MS` | `kinescope save`; the Kinescope's speed | Milliseconds per frame in the GIF. | 200 to 500 for a talk. |
| `BASE` | `kinescope frames` | The start of the PNG files' names. | |

## Results

- **The Kinescope** keeps the picture, and the status line says how many
  it holds.
- **`kinescope save`** writes the GIF and says its frames, size and speed.
- **`kinescope frames`** writes one PNG per frame.

## More about

**Pictures, not data.** A capture is the window's pixels. The session's
Python script records the runs (a film's runs as `m.sweep(...)`), not the
pictures; to make the film again from Python, draw the runs, as in the
example.

**Sizes.** Frames of different sizes (the window was resized between
captures) are placed at the top left of the largest, on white.

## Tips

- Set the figure's axes first ([fit](fit.md) or the model's window), so
  that every frame has the same scale.
- Use `kinescope frames` to put single frames in a paper.
- In the Morris-Lecar model with the nullclines on, a film of `iapp` from
  0 to 0.3 (`range iapp 0 0.3 10 film`) shows the `V` nullcline rising,
  and the trajectory going from rest to a limit cycle and back to rest as
  the current grows.

## Algorithms

The picture is read from the window after the frame is drawn. The GIF is
written with one palette for all frames and LZW coding, as the GIF89a
format sets out.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: the Kinescope has no frames yet.` | `save` or `frames` before any capture. | Capture first. |
| `error: the Kinescope holds 200 frames; delete them or save them first.` | The strip is full. | Save it, then [kinescope clear](kinescope-clear.md). |
| `error: a film holds at most 200 frames: use fewer steps.` | `kinescope film` over a range of more than 200 runs. | Fewer steps in the Range tab. |
| `error: cannot write FILE` | The folder does not exist or cannot be written. | Check the path. |
| `error: the Kinescope takes pictures of the window: it needs the desktop app` | In `xpplab-batch`. | Use the desktop app. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [kinescope play](kinescope-play.md): play the pictures.
- [kinescope clear](kinescope-clear.md): delete them.
- [range](range.md): the runs that `kinescope film` films.
- [export](export.md): one figure as SVG or PNG.

## Version history

Introduced in 0.1.0.
