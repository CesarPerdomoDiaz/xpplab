# Data analysis

The **Analysis** window (toolbar button *Analysis*, or *Run > Analyse the
run*) holds XPPAUT's tools from *Numerics > stocHastic* and *Numerics >
Averaging*. Each tool analyses the **last run**, exactly as XPPAUT's menus
do, with XPPAUT's own code. Pick a tool on the left, set its options, and
press **Compute**; the result is plotted on the right. Drag to pan, scroll
to zoom, and right-click the plot for more options.

Every tool also has a command, listed in each section below.

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
the largest Liapunov exponent of the run. XPPAUT computes it by carrying a
small perturbation along the run one output step at a time, renormalising
it each step and averaging the logarithms of its growth. It is negative
near a stable equilibrium, close to zero on a limit cycle and positive for
chaos (about 0.9 for the Lorenz attractor). Use a long run that has
settled.

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

