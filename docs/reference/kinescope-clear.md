# kinescope clear

Delete the Kinescope's pictures.

## Syntax

```
kinescope clear
```

`kinescope reset` is another name for it.

- **In the app:** View > **Kinescope** > **Delete all**; XPPAUT's keys
  `K R` (*Kinescope > Reset*).
- **In Python:** the frames are your own figures; close them with
  `plt.close("all")`.

## Description

`kinescope clear` deletes every picture of the Kinescope and stops their
playing, so that a new film starts empty. The Kinescope's figure,
speed and Loop box stay as they are.

[kinescope film](kinescope-capture.md) empties the Kinescope itself
before it films.

## Examples

### Start a new film

1. After a film is saved (see [kinescope capture](kinescope-capture.md)),
   [delete its pictures](cmd:kinescope clear).
2. [Capture](cmd:kinescope capture phase) the first picture of the next.

```
kinescope clear
kinescope capture phase
```

```python
import matplotlib.pyplot as plt

plt.close("all")
```

The command window answers "Kinescope emptied.", and the status line
then counts the new film from "Kinescope: 1 frame."

How to read it. The Kinescope holds 200 pictures; save a film, then clear
it before the next.

## Settings

`kinescope clear` has no settings.

## Results

- **The Kinescope** is empty, and the command window says so.

## More about

**Nothing to undo.** The pictures exist only in the Kinescope; save them
with `kinescope save` or `kinescope frames` before clearing if you want
them.

## Tips

- Clear before filming by hand, so that old pictures do not start the
  film.

## Algorithms

The pictures are freed; nothing is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: the Kinescope takes pictures of the window: it needs the desktop app` | In `xpplab-batch`. | Use the desktop app. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [kinescope capture](kinescope-capture.md): take pictures, save a film.
- [kinescope play](kinescope-play.md): play them.

## Version history

Introduced in 0.1.0.
