# fit

Fit every plot's axes to its data.

## Syntax

```
fit
```

`wf` is another name for `fit`.

- **In the app:** each plot's **Fit** button, a double-click on a plot, or
  XPPAUT's keys `W F` (*Window > Fit*).
- **In Python:** matplotlib fits a new plot to its data; `ax.autoscale()`
  fits one again.

## Description

`fit` sets the axes of every plot (the Time Series, the Phase Plane, the
Array plot and the bifurcation diagrams) to the range of what they show,
and lets them follow the data again as it changes. It undoes zooming and
panning, and a model file's own plot window (`xlo`, `xhi`, ...).

## Examples

### From the model's window to the run

The Morris-Lecar model sets its Phase Plane to `V` from −0.6 to 1.2 and
`W` from −0.25 to 1.2, a window made for the nullclines. The run itself
covers much less:

1. [Open the Morris-Lecar model](example:lecar.ode) and
   [show the Phase Plane](cmd:show phase).
2. [Fit the plots](cmd:fit).

```
show phase
fit
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
sol = m.run()
print(sol["V"].min(), sol["V"].max(), sol["W"].min(), sol["W"].max())
```

The Phase Plane's axes close in on the run: `V` from −0.43 to 0.23 and
`W` from 0 to 0.4, the run's own range.

How to read it. A window chosen for the whole picture can make a small
orbit hard to see; fitting shows the run at the largest scale. To go back
to the model's window, open the model again.

## Settings

`fit` has no settings.

## Results

- **Every plot** fits its axes to its data, and follows it from then on.
- **The status line** says "Fitted the plots to their data."

## More about

**Following.** After a fit, the axes follow each new run until you zoom
or pan again; then they stay where you put them.

## Tips

- Double-click a single plot to fit only that one.

## Algorithms

Each plot's axes are set to the range of its curves; nothing is computed.

## When it fails

`fit` does not fail.

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [show](show.md): bring a plot forward.
- [layout](layout.md): arrange the plots.

## Version history

Introduced in 0.1.0. Fits the plots itself since 0.1.4; before, it said
how to.
