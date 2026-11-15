# Reference

One page for each command and action: what it does, every way to call it,
worked examples, each setting, the mathematics behind it, and what to do
when it fails. In the app, `doc NAME` in the command window opens a page
(`doc auto steady`), and `doc` alone opens this index. In Python,
`xs.doc("auto steady")` opens the page in the browser. `help NAME` prints the
short summary of a command instead.

The reference is being written page by page. A command without a page yet
is described in [Commands and shortcuts](../manual/commands.md), and
`doc NAME` then prints its summary.

## Simulation

| Page | What it does |
|---|---|
| [run](run.md) | Integrate the model from its initial values and show the result. |
| [ic last](ic-last.md) | Start the next run where the last one ended. |
| [ic shift](ic-shift.md) | Start the next run where the last one ended, and let the time go on. |
| [script](script.md) | Save the session as a Python script that repeats it. |
| [echo](echo.md) | Show each action as the Python that repeats it. |
| [annotate](annotate.md) | Add text, an arrow or a marker to a plot, or clear them. |
| [show](show.md) | Bring a window forward, opening it if it is closed. |
| [hide](hide.md) | Close a window. |
| [layout](layout.md) | The Phase Plane beside the Time Series, or all plots as tabs. |
| [theme](theme.md) | Switch the colours between dark and light. |
| [palette](palette.md) | Open the command palette, or search it. |
| [clear](clear.md) | Clear the command window. |
| [quit](quit.md) | Leave the app, or end a batch script. |
| [doc](doc.md) | Open the reference page of a command, with its worked examples. |
| [help](help.md) | List the commands, show one's summary, or open a chapter of the manual. |
| [open](open.md) | Open a model file. |
| [new](new.md) | Start a new model from a template. |
| [save](save.md) | Save the model in the editor and open it again. |
| [export](export.md) | Save the last run, a result or a figure: CSV, MATLAB, SVG or PNG. |
| [ic read](ic-read.md) | Read the initial values from a text file. |
| [ic write](ic-write.md) | Write the initial values to a text file, one per line. |
| [use](use.md) | Apply one of the model's named parameter sets. |
| [sets](sets.md) | List the model's named parameter sets. |
| [set](set.md) | Give parameters, initial values or numerical options new values. |
| [get](get.md) | Show the value of a parameter, an initial value or an option. |
| [whos](whos.md) | List the variables, parameters and main options, with their values. |
| [click](click.md) | Start a run at a point of the Phase Plane, as a click on it does. |
| [freeze](freeze.md) | Keep the current run on the plots, so that the next one is drawn beside it. |
| [erase](erase.md) | Remove the kept curves and the marked equilibria from the plots. |
| [bvp](bvp.md) | Solve the boundary-value problem: the run that meets conditions at both ends. |
| [continue](continue.md) | Integrate further from the end of the last run and add the new part to it. |
| [ani](ani.md) | Play an XPPAUT animation of the run, frame by frame. |
| [view3d](view3d.md) | Show three columns of the run as a curve in three dimensions. |
| [prc](prc.md) | The phase response curves of a stable limit cycle. |
| [hfun](hfun.md) | The interaction function of two weakly coupled copies of an oscillator. |
| [mean](mean.md) | The mean and standard deviation of a column, and the largest Liapunov exponent of the run. |
| [hist](hist.md) | A histogram of a column of the last run: how much of the run is spent at each value. |
| [xcorr](xcorr.md) | The autocorrelation of a column, or the cross-correlation of two. |
| [fourier](fourier.md) | The Fourier series of a column, taken as one period: cosine and sine coefficients. |
| [power](power.md) | The Fourier series as amplitudes and phases. |
| [spectrum](spectrum.md) | The power spectral density of a column, or the cross spectrum or coherence of two. |
| [hist2d](hist2d.md) | A two-dimensional histogram of two columns: the fraction of the run in each cell. |
| [many](many.md) | Run from many initial values drawn at random in a box. |
| [range](range.md) | Run once for each value of a parameter or initial value: a brute-force diagram, hysteresis, Liapunov exponents. |

## Equilibria

| Page | What it does |
|---|---|
| [eq](eq.md) | Find an equilibrium near the initial values, with its eigenvalues and its stability. |
| [shoot](shoot.md) | Run away from an equilibrium along its eigenvectors: the start of its manifolds. |
| [eq all](eq-all.md) | Find every equilibrium in a region, from many starting points. |
| [nullclines freeze](nullclines-freeze.md) | Keep the present nullclines, or keep them for several values of a parameter. |
| [nullclines play](nullclines-play.md) | Show the frozen nullclines one at a time, as a film. |
| [nullclines unfreeze](nullclines-unfreeze.md) | Remove the frozen nullclines. |
| [dirfield](dirfield.md) | Draw the direction field in the Phase Plane. |
| [nullclines](nullclines.md) | Draw the curves where one variable stands still; their crossings are the equilibria. |

## Bifurcation analysis

| Page | What it does |
|---|---|
| [auto steady](auto-steady.md) | Follow steady states as a parameter varies, and find where they fold or lose stability. |
| [auto save](auto-save.md) | Save the diagram to a file, with what AUTO needs to continue from it later. |
| [auto load](auto-load.md) | Bring a saved diagram back, ready to be continued. |
| [auto reset](auto-reset.md) | Clear the diagram. |
| [auto orbit](auto-orbit.md) | Show the periodic orbit at a labelled point as the run. |
| [auto set](auto-set.md) | Set AUTO's numerical settings: the parameter's range, the step along a branch, how far a branch may go. |
| [auto from](auto-from.md) | Continue from a labelled point: periodic orbits from a Hopf point, a curve of bifurcations in two parameters, the rest of a branch. |

## The other reference pages

- [Commands and shortcuts](../manual/commands.md): every command in one
  table.
- [The .ode file format](../manual/odefile.md).
- [Numerical methods](../manual/numerics.md).
