# Data analysis

The **Analysis** window (toolbar button *Analysis*, or *Run > Analyse the
run*) holds XPPAUT's tools from *Numerics > stocHastic* and *Numerics >
Averaging*. Each tool analyses the **last run**, exactly as XPPAUT's menus
do, with XPPAUT's own code. Pick a tool on the left, set its options, and
press **Compute**; the result is plotted on the right. Drag to pan, scroll
to zoom, and right-click the plot for more options.

Every tool also has a command, listed in each section below. The last
tool, **Linearise**, does not need a run: it works at an equilibrium (the
last section of this page).

## Spectra

**Spectrum** estimates the power spectral density by Welch's method: the
run is cut into windows that overlap by half, each window is tapered
(Hamming by default) and transformed, and the results are averaged. Longer
windows resolve frequencies more finely; shorter ones average more windows
and give a smoother estimate. *Cross spectrum* and *coherence* compare two
columns: coherence near 1 at a frequency means the two signals move in step
there.

Frequencies are in cycles per unit of the model's time. A neuron firing
every 18 ms shows a peak near 0.055 and its harmonics.

**Fourier series** treats the whole run as one period and gives the cosine
and sine coefficients of each mode, or with *Amplitude and phase* their
size and phase (XPPAUT's *Power*).

Commands: `spectrum V 4096`, `spectrum V W coherence 1024`, `fourier V`,
`power V`.

## Histograms and correlations

**Histogram** counts the points of a column in each bin, over the column's
range or one you set. A *condition* such as `W>0.2` counts only the points
where it holds. **2-D histogram** gives the fraction of points in each cell
of a grid over two columns, drawn as a colour map.

**Correlation** is the cross-correlation of two columns, or the
autocorrelation of one, with their means subtracted. Lags are in time
units, so the first peak after zero of an autocorrelation is the period of
an oscillation.

Commands: `hist V 50`, `hist V 50 -0.5 0.5`, `hist2d V W 20`, `xcorr V`,
`xcorr V W 200`.

## Statistics and the Liapunov exponent

**Statistics** reports the mean and standard deviation of a column, and
the largest Liapunov exponent of the run. It is computed as XPPAUT does, by
carrying a small perturbation (`JAC_EPS`) along the run one output step at
a time, putting it back to its size after each step and averaging the
logarithms of its growth. It is negative near a stable equilibrium, close
to zero on a limit cycle and positive for chaos (about 0.9 for the Lorenz
attractor). Use a long run that has settled.

XPP Lab does this during a second integration from the initial values, in
double precision. XPPAUT works from the stored run, which is in single
precision, times included: in long runs (Lorenz to t = 1000 gives 7.6
instead of 0.9) and for large variables its rounding is as large as the
perturbation, and XPPAUT's number is wrong. With an adaptive
method, keep `TOL` well below `JAC_EPS`. Models with delays, integrals or
noise get no exponent.

Command: `mean V`.

## Phase response curves

**Phase response** works on a model with a stable limit cycle. It does what
XPPAUT users do by hand before *Averaging > New adjoint*:

1. integrate past the transient onto the cycle (*Transient*; 0 uses the
   total time, and longer runs are tried if needed);
2. measure the period from the upward crossings of the first variable;
3. integrate exactly one period, which becomes the run;
4. compute XPPAUT's adjoint along it.

Each column of the result is the infinitesimal phase response curve to
small kicks in that variable: how far a kick at each phase moves the cycle
forward, per unit of kick, in time units. Choose the variable with *Show*.

Try it: [open Morris-Lecar](example:lecar.ode), type `use snic` and
`set iapp=0.09`, then [compute the phase response](cmd:prc). Near this
saddle-node bifurcation every kick advances the neuron (a type I curve).
With `use hopf` and `set iapp=0.26` early kicks delay it instead (type II).

Command: `prc`, or `prc 500` to settle for 500 time units first.

## Weakly coupled oscillators: the H function

**H function** computes the interaction function H(φ) of two copies of the
oscillator coupled weakly: the coupling averaged over one cycle with the
adjoint. Give one formula per variable; primed names are the other copy's
variables. For gap-junction coupling through V, write `V'-V` for V and `0`
for the others. XPP Lab finds the cycle and its adjoint first.

For a symmetric pair the phase difference φ changes at a rate proportional
to −H*odd*(φ), so the stable phase-locked states are the zeros of *Hodd*
where it slopes upward. Pick *Hodd* or *Heven* with *Show*.

Command: `hfun V'-V; 0`.

## Fitting parameters to data

**Fit to data** adjusts parameters, and initial conditions, so that the
model matches measured data, by XPPAUT's Levenberg-Marquardt method. The
data file holds rows of numbers: time first, then the measured columns.

- **Variables:** the model variables to match, separated by commas.
- **Columns:** their columns in the file, counting time as column 1.
- **Vary:** the parameters and initial conditions to fit. They start from
  their current values.

The model's initial conditions apply at the first time in the file. When
the fit converges, the fitted values replace the parameters and the model
is run again to show the fitted solution.

Command: `fitdata recording.dat V 2 gca,gk`.

## Many noisy runs: the ensemble

A model with noise (`wiener` variables, or `ran()` in its formulas) gives a
different run each time. **Ensemble of noisy runs** is XPPAUT's
*stocHastic > Compute*: it runs the model *Runs* times from the same
initial values, the random numbers going on from run to run, and averages
them. The plot shows a column's **mean** at each time with a band of one
standard deviation; *Show* switches to another column, or to a variance.
The numbers are XPPAUT's own: they equal what its batch mode writes with
`@ stoch=1` (the mean) and `@ stoch=2` (the variance).

The other windows keep the last of the runs. *Export > analysis* (or
`export ens.csv analysis`) writes time and every column's mean and
variance: `V_mean`, `V_var`, ...

Try [the noisy FitzHugh-Nagumo model](example:fhn_noise.ode): set the total
time to 100 and compute 50 runs. Each run spikes at slightly different
times, so the mean's oscillations shrink as the runs fall out of step.

Command: `ensemble 50`.


## Linearisation and controllability

Near an equilibrium a model behaves like the linear system

    dx' = A dx + B du

where `dx` is the distance from the equilibrium, `A` is the Jacobian of the
right-hand side and `B` holds its derivatives with respect to the **inputs**:
the parameters you treat as controls (an injected current, a force, a
thrust). This is the starting point of control design. **Linearise**
(*Run > Linearise...*, or the Analysis window's *Linearise* tool) computes
it:

- **Linearise at** *the equilibrium* that Newton's method finds from the
  initial values (as *Find equilibrium* does), or at *the initial values*
  themselves, equilibrium or not.
- **Inputs:** tick the parameters that act as controls.

The result shows, in words first:

- where the linearisation was taken, and whether that point is an
  equilibrium;
- whether the equilibrium is **stable**: every eigenvalue of `A` has a
  negative real part (for a map, `meth=discrete`, every eigenvalue lies
  inside the unit circle);
- whether the inputs can steer the state: the system is **controllable**
  when the controllability matrix `[B, AB, ..., A^(n-1) B]` has full rank n.
  If the rank is lower, some direction of the state cannot be moved by these
  inputs, whatever they do. The smallest singular value of that matrix,
  relative to the largest, says how hard the worst direction is to move.

Below are `A` and `B` (hover over an entry for all its digits and its error
estimate), the eigenvalues and the singular values. The plot shows the
eigenvalues in the complex plane: the stable half (or, for a map, the
inside of the unit circle) is shaded, and its edge is drawn.

`A` and `B` come from central differences extrapolated to zero step
(Ridders' method), so no step size has to be chosen; they are typically
good to 1e-9 or better. If the right-hand side has a kink or a jump at the
point, a note says how far the derivatives can be trusted. At an
equilibrium the eigenvalues listed are those that *Find equilibrium*
reports; elsewhere they are the eigenvalues of `A`, from XPPAUT's own
routine.

Try it: [open Morris-Lecar](example:lecar.ode) and type
`linearize inputs iapp`. The equilibrium found from the initial values is a
saddle (one eigenvalue near 1.03, the other near -0.30). The current enters
only the equation for V, but W follows V, so `iapp` alone can steer both:
the rank is 2.

*Export > analysis* (or `export lin.csv analysis`) writes one row per state:
its value at the point, its row of `A` and of `B` (columns `A_V`, `A_W`,
`B_iapp`, ...), an eigenvalue (`eig_re`, `eig_im`), and on the first row the
rank and the residual (the largest |dx/dt| at the point; for a map
|F(x) - x|). In Python the same numbers come from `m.linearize()`; see
[Control design](https://cesarperdomodiaz.github.io/xpplab/python/control/).

## Pole placement

When the inputs can steer every state, the feedback

    u = u0 - K (x - x*)

(the nominal input minus a gain times the distance from the point) turns
the linear system into `dx' = (A - B K) dx`. Pole placement chooses `K` so
that the eigenvalues of `A - B K`, the **poles** of the closed loop, are
where you want them: in the stable half, as far in as you want the loop to
be fast, with as much imaginary part as you accept oscillation.

After *Linearise* with inputs, a **Pole placement** section appears under
the eigenvalues:

- The poles start at the eigenvalues moved into the stable region (an
  unstable one is reflected into the left half-plane; for a map, inverted
  into the unit circle). Type new values, or drag the blue circles in the
  complex plane. A real pole moves along the real axis; a complex pole takes
  its conjugate with it. *pair* joins a real pole with the next real one into
  a complex pair, *split* turns a pair into two real poles.
- **Gain K** follows as you go: one row per input, one column per state.
  With one input the gain is unique (Ackermann's formula). With several, the
  spare freedom is spent on keeping the closed loop's eigenvectors well
  apart (the method of Kautsky, Nichols and Van Dooren), so that the poles
  stay put when the model is a little different; a pole can then be repeated
  up to as many times as there are inputs.
- The blue dots in the plane are the eigenvalues of `A - B K` as computed:
  they sit on the circles when the placement worked.
- **Simulate the closed loop** writes the model with each input replaced by
  the feedback law as `NAME_closed_loop.ode` next to the model (for one of
  the examples, in Documents/XPP Lab), opens it with the current parameter
  values, initial values and numerical options, and runs it. The closed loop
  is the full nonlinear model, so its run shows how far from the point the
  linear design still works.

Commands: `poles -2+1i -2-1i` sets the poles (one per state) and computes the
gain; `closedloop [FILE]` writes, opens and runs the closed loop. Try it with
the inverted pendulum `th' = w`, `w' = g/l*sin(th) - 0.2*w + u`: type
`linearize inputs u`, then `poles -2+1i -2-1i`; the gain is `K = [14.8, 3.8]`.
`closedloop`, then `set th=0.3` and `run`: the pendulum, started 0.3 rad from
upright, comes back to it in about three seconds. In Python:
`K = lin.place_poles([-2+1j, -2-1j])` and `lin.closed_loop(K)`, with the same
numbers.

## Root locus

A root locus shows how the poles move as one number changes: each
eigenvalue draws a branch in the complex plane, from an x at the first
value to an o at the last. The **Root locus** section, under the
eigenvalues after *Linearise*, draws one over:

- **the gain K above**: the eigenvalues of `A - g B K` for g from *from g* to
  *to g*. g = 0 is the open loop and g = 1 the poles you placed, so the
  locus shows how much margin the design has, and what too much gain does.
- **one state fed back**: `u = u0 - g (x_j - x_j*)`, one input driven by one
  state, the classic root locus of a proportional controller.
- **a parameter**: the eigenvalues of the equilibrium as the parameter goes
  from *from* to *to*, the equilibrium followed by Newton's method from the
  initial values (as *Sing pts > Range* does).

From one step to the next each eigenvalue is matched with the nearest one,
so a branch follows one eigenvalue. Below the button, the values where a
branch crosses the edge of stability (the imaginary axis, or for a map the
unit circle) are listed, found between the two steps around each crossing:
over a parameter, a complex pair crossing is a Hopf bifurcation and a real
pole crossing zero a fold or a branch point.

Try it with [Morris-Lecar](example:lecar.ode): `set iapp=0.1`, `linearize`,
then `locus par iapp 0.1 0.3`. The pair of eigenvalues of the rest state
crosses into the left half-plane at iapp = 0.2037, at ±1.26i: there the
equilibrium regains its stability in a Hopf bifurcation. With the pendulum
of the last section, `locus gain 0 2` shows the unstable pole coming in at
g = 0.662, where `9.8 - 14.8 g = 0`.

Commands: `locus gain FROM TO [STEPS]`, `locus feedback INPUT STATE FROM TO
[STEPS]`, `locus par NAME FROM TO [STEPS]`; `export F.csv locus` writes the
value, the equilibrium (over a parameter) and `re1, im1, re2, ...` per row.
In Python: `lin.root_locus(K, gain=(0, 2))` and
`m.root_locus("iapp", (0.1, 0.3))`, with the same numbers.

Commands: `linearize inputs iapp`, `linearize at eq inputs iapp gca`,
`linearize at ic iapp` (`linearise` works too).
