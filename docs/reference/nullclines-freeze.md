# nullclines freeze

Keep the present nullclines on the Phase Plane, or draw and keep them for
several values of a parameter, to see how they move.

## Syntax

```
nullclines freeze
nullclines freeze PARAMETER FROM TO N
```

- **In the app:** Phase Plane > **Freeze...** (XPPAUT's *Nullcline >
  Freeze*, and *Freeze > Range*).
- **In Python:** `frozen = m.nullcline_sweep("iapp", values, "V", "W",
  xlim, ylim)`, a list of the nullclines for each value.

## Description

`nullclines freeze` keeps a copy of the nullclines now drawn. The live
nullclines go on following the parameters; the frozen ones stay where
they were, drawn fainter, so the two can be compared.

`nullclines freeze PARAMETER FROM TO N` computes and keeps the nullclines
for `N` values of `PARAMETER`, evenly from `FROM` to `TO`, and puts the
parameter back. Up to 400 sets are kept. [nullclines play](nullclines-play.md)
shows them one at a time, as a film, and
[nullclines unfreeze](nullclines-unfreeze.md) removes them.

## Examples

### The neuron's nullclines as the current rises

1. [Open the Morris-Lecar model](example:lecar.ode) and open the **Phase
   Plane** tab.
2. [Freeze the nullclines for five currents](cmd:nullclines freeze iapp 0 0.1 5).

```
nullclines freeze iapp 0 0.1 5
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
values = [0, 0.025, 0.05, 0.075, 0.1]
frozen = m.nullcline_sweep("iapp", values, "V", "W", (-0.6, 1.2), (-0.25, 1.2))
```

The status line says "Froze the nullclines for 5 values of iapp." Five
copies of the `V` nullcline appear, one above the other; the `W` nullcline
does not depend on `iapp`, so its five copies coincide. The lower knee of
the N-shaped `V` nullcline rises by about 0.03 in `W` for each step:
from `W` ≈ −0.09 at `iapp` = 0 to 0.03 at 0.1.

How to read it. While the knee is below the `W` nullcline the two curves
cross three times: three equilibria for the first four values. Between
0.075 and 0.1 the knee passes above it, two crossings vanish, and one
equilibrium is left: the fold at `iapp` = 0.0833 that
[auto steady](auto-steady.md) finds.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `PARAMETER`, `FROM`, `TO`, `N` | the command; the Freeze window; `values` in Python | The parameter and its values. | A handful of values shows the motion; many make a smooth film. |
| **The window** | the Phase Plane's view; `xlim`, `ylim` in Python | Where the nullclines are computed. | As for [nullclines](nullclines.md). |

## Results

- **The Phase Plane** draws the frozen nullclines fainter than the live
  ones. One picked with `nullclines show K` is drawn as strongly as the
  live ones, with its parameter value in the legend.
- **The status line** says how many were frozen.
- **In Python** `m.nullcline_sweep()` returns a list with, for each value,
  the pair of nullclines as [nullclines](nullclines.md) returns them.

## More about

**Which curve moves.** A parameter that appears only in one equation moves
only that variable's nullcline. Here `iapp` is in `V`'s equation, so only
the `V` nullcline moves; a parameter in `W`'s equation, such as `phi`,
would leave it still. Telling which curve moves, and which way, explains
a bifurcation before it is computed.

**The sweep and the parameter.** The parameter is set to each value in turn
and put back afterwards, so the model is unchanged.

## Tips

- Freeze once before dragging a slider, to keep the starting point in
  view.
- Freeze over a range that brackets a bifurcation, and play it back with
  [nullclines play](nullclines-play.md).

## Algorithms

For each value the nullclines are computed as [nullclines](nullclines.md)
computes them, over the Phase Plane's window, and kept.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `Write: nullclines freeze, or nullclines freeze PAR FROM TO N ...` | A number is missing, or `PAR` is not a parameter. | Give all four. |
| `error: at most 400 frozen nullclines are kept.` | Too many are kept already. | `nullclines unfreeze` first. |

## References

- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapter 8.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [nullclines](nullclines.md): the live nullclines.
- [nullclines play](nullclines-play.md) and
  [nullclines unfreeze](nullclines-unfreeze.md).
- [auto steady](auto-steady.md): the fold, computed.

## Version history

Introduced in 0.1.0.
