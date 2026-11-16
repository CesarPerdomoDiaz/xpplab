# click

Start a run at a point of the Phase Plane, as a click on it does.

## Syntax

```
click X Y
click keep
click keep off
```

- **In the app:** in the Phase Plane, tick **Click to start** and click a
  point (XPPAUT's *Initial conds > Mouse*); tick **Keep each** beside it
  to keep every clicked run (*Initial conds > mIce*).
- **In Python:** `m.run(V=X, W=Y)`, with the names of the plotted
  variables.

## Description

`click X Y` makes `X` and `Y` the initial values of the two variables the
Phase Plane plots, and runs. The other variables keep their initial
values. It is what a click on the plot does, typed: useful in scripts, and
to start from an exact point.

The new initial values stay, so a later `run` starts from the same point.

`click keep` keeps every run started by a click on the plots, up to 16,
so that a few clicks draw a phase portrait; `click keep off` goes back to
one run at a time. [erase](erase.md) (the key `E`) removes the kept runs.

## Examples

### The threshold of a spike

The Morris-Lecar model rests at `V` = −0.3825, `W` = 0.0013. Start just
below and just above the threshold, keeping both runs:

1. [Open the Morris-Lecar model](example:lecar.ode) and open the **Phase
   Plane** tab.
2. [Keep the runs](cmd:click keep).
3. [Start at V = −0.17](cmd:click -0.17 0.0013) and
   [at V = −0.16](cmd:click -0.16 0.0013).

```
click keep
click -0.17 0.0013
click -0.16 0.0013
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
below = m.run(V=-0.17, W=0.0013)
above = m.run(V=-0.16, W=0.0013)
print(below["V"].max(), above["V"].max())
```

From −0.17, `V` rises only to −0.1644 and falls back to rest. From −0.16
it fires a spike that reaches 0.2371, and then also returns to rest.

How to read it. A difference of 0.01 in the start decides between nothing
and a full spike: the neuron is *excitable*. With `W` = 0.0013 the
threshold is at `V` = −0.1682. It is not a fixed voltage but a curve, the
stable manifold of the saddle at `V` = −0.1425 ([eq](eq.md) draws it):
starts on one side return directly, starts on the other go round the spike.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `X`, `Y` | the command; the values in Python | The start, in the plotted variables. | The Phase Plane's x and y choose which variables they are. |
| `keep` | `click keep`, `click keep off`; **Keep each** in the Phase Plane | Keep each clicked run on the plots. | On for a phase portrait. |
| **Click to start** | Phase Plane | Whether a click on the plot starts a run. | Off when you click to zoom or to read values. |

## Results

- **The initial values** of the two plotted variables are `X` and `Y`.
- **The run** from there, in every window that shows it, and with `click
  keep` the earlier clicked runs too.
- **In Python** `m.run(V=X, W=Y)` sets the values and returns the run.

## More about

**Phase portraits.** A handful of runs from well chosen starts shows the
whole flow of a two-variable model: where runs go, which equilibria
attract them, where they separate. Start on both sides of every
nullcline and near every equilibrium.

**Excitability.** An excitable system rests, but a large enough push makes
it go round a long excursion before it rests again. The threshold is the
boundary between the two kinds of start; in a model with a saddle, as
here, it is the saddle's stable manifold.

## Tips

- Draw the nullclines and the direction field first: they show where to
  click.
- To find a threshold, halve the gap between a start that fires and one
  that does not, as for the −0.1682 above.
- `ic last` after a click run starts the next one where it ended.

## Algorithms

The two values are set as initial values and the model is run, as by
[run](run.md).

## When it fails

| Message | Why | What to do |
|---|---|---|
| `Write: click X Y (a run from that point of the phase plane), or click keep [off]` | A number is missing, or a word is not a number. | Give both coordinates. |

## References

- Izhikevich, E. M. (2007). *Dynamical Systems in Neuroscience*. MIT Press.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [run](run.md): the run each click starts.
- [erase](erase.md): remove the kept runs.
- [nullclines](nullclines.md) and [dirfield](dirfield.md): where to click.

## Version history

Introduced in 0.1.0.
