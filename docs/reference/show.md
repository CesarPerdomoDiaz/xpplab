# show

Bring a window forward, opening it if it is closed.

## Syntax

```
show WINDOW
```

`WINDOW` is one of `time`, `phase`, `auto`, `editor`, `data`, `help`,
`console`, `workspace`, `models`, `array`, `analysis`, `3d`, `animator`,
`kinescope`, `figure` and `python`; the start of a name is enough.

- **In the app:** the View menu.
- **In Python:** figures are drawn with matplotlib: `sol.plot("V")`,
  `m.phase_plane("V", "W")`, `diagram.plot()`.

## Description

`show WINDOW` opens the window and puts it in front: the Time Series
(`time`), the Phase Plane (`phase`), the Bifurcation window (`auto`), the
model's editor, the Data table, Help, the console, the Workspace, the
Models window, the Array plot, the Analysis window, the 3-D view, the
Animator, the Kinescope or the Figure window. `show python` opens the
Workspace at its Python tab.

It is how a script of commands, or an agent driving the app, makes sure
the window it is about to use is the one in view.

## Examples

### Looking at a result

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Draw the nullclines](cmd:nullclines) and
   [bring the Phase Plane forward](cmd:show phase).

```
nullclines
show phase
```

The Phase Plane comes to the front with the nullclines drawn.

How to read it. Commands that draw in a window open it themselves, as
`nullclines` does; `show` is for bringing back a window that is open but
hidden behind others, or for opening one by name.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `WINDOW` | the command | Which window. | The start of a name is enough: `show ph`. |

## Results

- **The window** is open and in front.

## More about

**Windows and tabs.** With `layout tabs`, the plots share one place as
tabs; `show` selects the tab. With `layout side`, the Phase Plane sits
beside the Time Series. See [layout](layout.md).

## Tips

- In a script of commands, `show` before `export FILE.png` makes sure the
  picture is of the window you mean.

## Algorithms

The window is marked open and given the focus; nothing is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: no window called NAME; the windows are ...` | The name is not one of the windows. | Use a name from the list. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [hide](hide.md): close a window.
- [layout](layout.md): how the plots are arranged.

## Version history

Introduced in 0.1.0. An unknown name is reported since 0.1.4.
