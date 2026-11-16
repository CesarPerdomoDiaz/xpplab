# layout

Arrange the plots: the Phase Plane beside the Time Series, or all the plots
as tabs.

## Syntax

```
layout side
layout tabs
```

- **In the app:** View > **Phase plane beside time series**.
- **In Python:** arrange matplotlib axes as you like, for example with
  `plt.subplots(1, 2)`.

## Description

`layout side` puts the Phase Plane beside the Time Series, so that a run
can be watched in time and in the plane at once. `layout tabs` puts the
plots in one place as tabs, each using the whole space. The windows are
arranged afresh either way.

## Examples

### Time and plane together

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Put the Phase Plane beside the Time Series](cmd:layout side), and
   [draw the nullclines](cmd:nullclines).

```
layout side
nullclines
```

The Time Series and the Phase Plane are side by side; the run shows as a
spike in time on one side and as a loop round the nullclines on the other.

How to read it. Seeing both at once ties each part of the spike in time to
its place in the plane: the fast upstroke crosses the plane quickly, the
slow recovery creeps along the left branch of the `V` nullcline.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `side` or `tabs` | the command | The arrangement. | `side` for two-variable models, `tabs` for many plots on a small screen. |

## Results

- **The windows** are arranged as asked.

## More about

**Not kept between sessions.** The app starts with the plots as tabs;
the theme, by contrast, is remembered.

## Tips

- With `layout tabs`, [show](show.md) selects a plot's tab.

## Algorithms

The windows are docked afresh; nothing is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: write layout side or layout tabs` | Another word. | Write `side` or `tabs`. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [show](show.md), [theme](theme.md).

## Version history

Introduced in 0.1.0. Words other than `side` and `tabs` are refused since
0.1.4.
