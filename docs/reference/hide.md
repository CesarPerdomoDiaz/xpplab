# hide

Close a window.

## Syntax

```
hide WINDOW
```

`WINDOW` is one of `time`, `phase`, `auto`, `editor`, `data`, `help`,
`console`, `workspace`, `models`, `array`, `analysis`, `3d` and
`animator`; the start of a name is enough.

- **In the app:** the window's close button, or the View menu.
- **In Python:** close a matplotlib figure with `plt.close()`.

## Description

`hide WINDOW` closes the window. Nothing it shows is lost: [show](show.md)
opens it again as it was.

## Examples

### Making room

1. [Open the Morris-Lecar model](example:lecar.ode) and
   [open Help](cmd:show help).
2. [Close Help again](cmd:hide help).

```
show help
hide help
```

How to read it. Closing a window only removes it from view; the runs,
diagrams and settings stay.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `WINDOW` | the command | Which window. | The start of a name is enough. |

## Results

- **The window** is closed.

## More about

**What cannot be hidden by name.** The Kinescope, the Figure window and
the Workspace's Python tab close with their own buttons.

## Tips

- Close windows you are not using to give the plots more room.

## Algorithms

The window is marked closed; nothing is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: no window called NAME; the windows are ...` | The name is not one of the windows. | Use a name from the list. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [show](show.md): open a window.

## Version history

Introduced in 0.1.0.
