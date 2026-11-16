# Data analysis

The **Analysis** window (toolbar button *Analysis*, or *Run > Analyse the
run*) holds XPPAUT's tools from *Numerics > stocHastic* and *Numerics >
Averaging*. Each tool analyses the **last run**, exactly as XPPAUT's menus
do, with XPPAUT's own code. Pick a tool on the left, set its options, and
press **Compute**; the result is plotted on the right. Drag to pan, scroll
to zoom, and right-click the plot for more options.

Every tool also has a command, and a reference page with its settings,
worked examples and what to do when it fails: the **?** beside a tool's
*Compute* button, or `doc NAME`, opens it. This chapter says which tool
answers which question. The last tools, for control design, do not need a
run: they work at an equilibrium.

## What is in the signal: spectra

**Spectrum** estimates the power spectral density by Welch's method, and
compares two columns by their cross spectrum or coherence. **Fourier
series** treats the whole run as one period. Frequencies are in cycles per
unit of the model's time: a neuron firing every 18 ms shows a peak near
0.055 and its harmonics.

See [spectrum](../reference/spectrum.md), [fourier](../reference/fourier.md)
and [power](../reference/power.md).

## Where the run spends its time: histograms and correlations

**Histogram** counts the points of a column in each bin, optionally only
where a condition holds; **2-D histogram** does it over two columns.
**Correlation** gives the autocorrelation of a column or the
cross-correlation of two: the first peak after zero of an autocorrelation
is the period of an oscillation.

See [hist](../reference/hist.md), [hist2d](../reference/hist2d.md) and
[xcorr](../reference/xcorr.md).

## Is it chaotic: statistics and the Liapunov exponent

**Statistics** reports the mean and standard deviation of a column, and
the largest Liapunov exponent: negative near a stable equilibrium, close to
zero on a limit cycle, positive for chaos (about 0.9 for the Lorenz
attractor). Use a long run that has settled. XPP Lab computes the exponent
during a second integration in double precision; XPPAUT's own estimate,
from the stored run in single precision, goes wrong in long runs (Lorenz
to t = 1000 gives 7.6 instead of 0.9).

See [mean](../reference/mean.md).

## How a kick shifts a rhythm: phase response curves

**Phase response** works on a model with a stable limit cycle. It does
what XPPAUT users do by hand before *Averaging > New adjoint*: settle onto
the cycle, measure its period, integrate exactly one period, and compute
XPPAUT's adjoint along it. Each column is the phase response curve to
small kicks in that variable.

Try it: [open Morris-Lecar](example:lecar.ode), type `use snic` and
`set iapp=0.09`, then [compute the phase response](cmd:prc). Near this
saddle-node bifurcation every kick advances the neuron (a type I curve).
With `use hopf` and `set iapp=0.26` early kicks delay it instead (type II).

See [prc](../reference/prc.md).

## Will two oscillators lock: the H function

**H function** computes the interaction function H(φ) of two copies of the
oscillator coupled weakly, from the coupling and the adjoint. For a
symmetric pair the stable phase-locked states are the zeros of *Hodd*
where it slopes upward. For gap-junction coupling through V, write `V'-V`
for V and `0` for the others.

See [hfun](../reference/hfun.md).

## Which parameters explain the data: fitting

**Fit to data** adjusts parameters, and initial conditions, so that the
model matches measured data, by XPPAUT's Levenberg-Marquardt method. The
data file holds rows of numbers, time first; the model's initial
conditions apply at the first time in the file.

See [fitdata](../reference/fitdata.md).

## What noise does on average: the ensemble

A model with noise (`wiener` variables, or `ran()` in its formulas) gives a
different run each time. **Ensemble of noisy runs** runs it many times from
the same initial values and draws each column's mean with a band of one
standard deviation.

Try [the noisy FitzHugh-Nagumo model](example:fhn_noise.ode): set the total
time to 100 and compute 50 runs. Each run spikes at slightly different
times, so the mean's oscillations shrink as the runs fall out of step.

See [ensemble](../reference/ensemble.md).

## Can it be controlled: linearisation and controllability

Near an equilibrium a model behaves like the linear system

$$\dot{\delta x} = A\,\delta x + B\,\delta u$$

where $\delta x$ is the distance from the equilibrium, $A$ is the Jacobian
of the right-hand side and $B$ holds its derivatives with respect to the
**inputs** $u$: the parameters you treat as controls (an
injected current, a force, a thrust). **Linearise** (*Run > Linearise...*)
computes them, says whether the equilibrium is stable, and whether the
inputs can steer every state (the controllability matrix has full rank).

Try it: [open Morris-Lecar](example:lecar.ode) and type
`linearize inputs iapp`. The equilibrium found from the initial values is a
saddle (one eigenvalue near 1.03, the other near -0.30). The current enters
only the equation for V, but W follows V, so `iapp` alone can steer both:
the rank is 2.

See [linearize](../reference/linearize.md), and the Python guide
[Control design](https://cesarperdomodiaz.github.io/xpplab/python/control/).

## Making it stable: pole placement and LQR

When the inputs can steer every state, the feedback
$u = u_0 - K\,(x - x^*)$ turns the linear system into
$\dot{\delta x} = (A - BK)\,\delta x$. **Pole placement** chooses $K$ so
that the eigenvalues of $A - BK$, the poles of the closed loop, are where
you want them. The **linear-quadratic regulator** instead takes weights,
$Q$ for how much each state's distance matters and $R$ for how dear each
input is, and finds the gain that minimises

$$J = \int_0^\infty \left(\delta x^T Q\,\delta x + \delta u^T R\,\delta u\right) dt$$
 **Simulate the closed
loop** writes the model with the feedback built in, and runs it: the full
nonlinear model, which shows how far from the point the linear design
still works.

Try it with the inverted pendulum `th' = w`, `w' = g/l*sin(th) - 0.2*w + u`:
type `linearize inputs u`, then `poles -2+1i -2-1i`; the gain is
`K = [14.8, 3.8]`. `closedloop`, then `set th=0.3` and `run`: the pendulum,
started 0.3 rad from upright, comes back to it in about three seconds.
`lqr Q 10 1 R 0.1` gives `K = [23.80, 7.392]` instead, with poles at
-3.156 and -4.436. In Python: `K = lin.place_poles([-2+1j, -2-1j])`,
`lin.closed_loop(K)` and `lin.lqr(Q=[10, 1], R=[0.1])`, with the same
numbers.

See [poles](../reference/poles.md), [lqr](../reference/lqr.md) and
[closedloop](../reference/closedloop.md).

## How much margin: the root locus

A root locus shows how the poles move as one number changes: the gain
above (g = 0 is the open loop, g = 1 the poles you placed), one state fed
back through one input, or a parameter, with the equilibrium followed as
it changes. The values where a branch crosses the edge of stability are
listed: over a parameter, a complex pair crossing is a Hopf bifurcation.

Try it with [Morris-Lecar](example:lecar.ode): `set iapp=0.1`,
`linearize`, then `locus par iapp 0.1 0.3`. The pair of eigenvalues of the
rest state crosses into the left half-plane at iapp = 0.2037, at ±1.26i:
there the equilibrium regains its stability in a Hopf bifurcation. With
the pendulum above, `locus gain 0 2` shows the unstable pole coming in at
$g = 0.662$, where $9.8 - 14.8\,g = 0$.

See [locus](../reference/locus.md).
