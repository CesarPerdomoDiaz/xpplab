# Commands and keyboard shortcuts

## Keyboard shortcuts

| Keys | Action |
|---|---|
| F5 | Integrate (run) |
| Ctrl+Enter | Apply the edited model |
| Ctrl+O, Ctrl+S, Ctrl+N | Open, save, new model |
| Ctrl+Q | Quit (⌘ instead of Ctrl on a Mac) |
| F1 | Help |

## XPPAUT's two-key commands

XPPAUT users drive it with two-letter commands, and those keep working
here whenever the cursor is not in a text field. They can be switched off
under **View > XPPAUT keyboard commands**.

| Keys | XPPAUT menu | Action |
|---|---|---|
| I G | Initial conds > Go | Integrate |
| I L | Initial conds > Last | Start from the end of the last run |
| I S | Initial conds > Shift | The same, and the time goes on |
| N N | Nullcline > New | Draw nullclines |
| N E | Nullcline > Erase | Hide nullclines |
| N F | Nullcline > Freeze | Keep the nullclines drawn |
| K C, K A, K R | Kinescope > Capture, Autoplay, Reset | Capture a figure, play the frames, empty the Kinescope |
| D D | Dir.field > Direct field | Toggle the direction field |
| S G | Sing pts > Go | Find an equilibrium |
| E | Erase | Clear kept curves |
| W F | Window > Fit | Fit the plots (or double-click a plot) |
| F Q | File > Quit | Quit |

## The command window

Type a command and press Enter. Use the Up and Down arrows to recall earlier
commands. `help` lists them all.

| Command | Action |
|---|---|
| `run` | Integrate. |
| `set I=0.5 total=200` | Set parameters, initial values or options. |
| `get I` | Show a value. |
| `whos` | List variables and parameters with their values. |
| `ic last` | Start the next run where the last one ended. |
| `ic shift` | The same, and the start time T0 becomes the time the run ended (XPPAUT's Shift). |
| `ic read ics.dat`, `ic write ics.dat` | Read the initial values from a text file, one number per variable in order; or write them, one per line. |
| `ic u[1..20] = sin([j]/3)` | Initial values from a formula: one variable (`ic V = vk + 0.1`) or a numbered range, `[j]` the index (XPPAUT's formUla). |
| `click -0.3 0`, `click keep` | Start a run at that point of the phase plane, as a click does; `click keep` keeps each such run (`click keep off` stops). |
| `kinescope capture phase`, `kinescope play 200`, `kinescope save film.gif` | XPPAUT's Kinescope: a picture of a figure (time, phase, auto, auto2) as it is shown; play the frames (200 ms each); write them as an animated GIF (`kinescope frames NAME`: PNG files). `kinescope show 3`, `kinescope stop`, `kinescope clear`. |
| `py CODE`, `py` | Run a line of Python in the console (`py` alone: the command window takes Python; see below). |
| `python use PATH`, `python stop` | Choose the console's Python (remembered), or stop it. |
| `show python` | Bring the Workspace's Python tab forward. |
| `serve`, `serve off` | Let Python drive the app (`app = xpplab.app.connect()` in a notebook or script), or stop it. The app lets Python connect from the start unless it was started with `--no-serve`. |
| `ensemble 50` | Run a noisy model 50 times and average: the mean and variance of every column at each time (XPPAUT's stocHastic > Compute). `export ens.csv analysis` saves them. |
| `range iapp 0 0.3 10 film` | Film the Range: each run drawn alone and captured by the Kinescope (XPPAUT's Range with Movie). |
| `eq` | Find an equilibrium and report its stability. |
| `eq all 200` | Every equilibrium Newton's method finds from 200 random points (100 if left out) in the phase plane's window: XPPAUT's Sing pts > Monte Carlo. |
| `nullclines`, `nullclines off` | Show or hide nullclines. |
| `nullclines freeze`, `nullclines freeze iapp 0 0.4 6` | Keep the nullclines drawn, or those for 6 values of iapp from 0 to 0.4 (XPPAUT's Freeze, Freeze > Range). `nullclines show 3` shows the third set alone (0: all), `nullclines play` steps through them, `nullclines unfreeze` deletes them. |
| `dirfield` | Toggle the direction field. |
| `erase` | Remove kept curves. |
| `use hopf` | Apply a named parameter set. |
| `auto steady iapp` | Follow steady states as iapp varies. |
| `auto from 2 periodic` | Continue from labelled point 2. Actions: `periodic`, `two`, `extend`, `switch`, `doubling`, `fixed`. |
| `auto set ds=0.01 nmax=300` | AUTO numerics. |
| `auto reset` | Clear the diagram. |
| `auto save d.auto`, `auto load d.auto` | Save the diagram, with AUTO's settings and restart data, as XPPAUT's `.auto` file; load one back to continue from its points. |
| `auto orbit 6` | Make the periodic orbit at point 6 the current run (one period). |
| `range I 0 1.6 50 V` | Run 50 times with I from 0 to 1.6 and plot the max and min of V (XPPAUT's Range). Leave out `V` to keep each trajectory, write `lyap` for the Liapunov exponent, add `continue` to start each run where the last ended, or `eq` to follow the equilibrium instead of running (XPPAUT's Sing pts > Range). The name can also be a variable (its initial value). After the first range a second one, `range I 0 1.6 50 gk 1 3 20 V`, runs a 50 by 20 grid (XPPAUT's 2par range); without its count, `range I 0 1.6 50 gk 1 3 V`, both move together along a line. `sweep` does the same. |
| `bvp` | Solve the boundary-value problem given by the model's `bndry` statements. |
| `view3d X Y Z` | Show three columns in the 3D view. `view3d spin` turns it slowly (`view3d spin 60` at 60 degrees a second, `view3d spin off` stops it); `view3d angles 30 60` sets theta and phi. |
| `ani pend.ani`, `ani play`, `ani row 100` | Open an XPPAUT animation, play it, or show one frame. |
| `spectrum V 1024` | Power spectral density of V in windows of 1024 points. `spectrum V W coherence` compares two columns (`psd`, `cross`, `coherence`). |
| `fourier V`, `power V` | Fourier series of V over the run: cos and sin, or amplitude and phase. |
| `hist V 50 -1 1` | Histogram of V in 50 bins from -1 to 1 (leave out the range to take it from the data). |
| `hist2d V W 20` | Two-dimensional histogram of V and W. |
| `xcorr V W 100` | Cross-correlation of V and W over about 100 lags; `xcorr V` is the autocorrelation. |
| `mean V` | Mean and standard deviation of V, and the run's largest Liapunov exponent. |
| `prc` | Phase response curves of the limit cycle (`prc 500` settles for 500 time units first). |
| `hfun V'-V; 0` | H function for weak coupling, one formula per variable; primed names are the other cell. |
| `fitdata v.dat V 2 gca,gk` | Fit gca and gk so that V matches column 2 of v.dat. |
| `open file.ode`, `new`, `save` | Model files. |
| `export run.csv` | Save the last run as CSV. Every number is written exactly. |
| `export d.csv auto` | Save the points of the bifurcation diagram as CSV (`range`: the last Range that measures or follows equilibria; `analysis`: the Analysis result; `equilibria`: the last `eq all`; `nullclines`: those shown and frozen). |
| `export fig.svg phase` | Save a figure (`time`, `phase`, `auto`, `auto2`) as SVG; `.png` saves it as shown. |
| `script session.py` | Save the session as a Python script that repeats it (see below). |
| `echo off`, `echo on` | Hide or show each action as Python in the command window. |
| `continue 50` | Integrate 50 more time units from the end of the last run (C C). |
| `clc` | Clear the command window. |
| `show phase`, `hide help` | Bring a window forward, or close it: `time`, `phase`, `auto`, `editor`, `data`, `analysis`, `3d`, `animator`, `help`, `console`, `workspace`, `models`. |
| `layout side`, `layout tabs` | Put the phase plane beside the time series (also in **View**), or all plots back in tabs. |
| `help auto` | Open a manual page. |

## Your session as Python

Each action that changes the model or computes something is also written in
the command window, in green after `>>>`, as the line of Python that does
the same with the [xpplab package](https://cesarperdomodiaz.github.io/xpplab/python/quickstart/):

```
>>> m = xs.load("lecar.ode")
>>> sol = m.run()
>>> m.parameters["iapp"] = 0.1
>>> sol = m.run()
>>> m.auto.equilibria("iapp")
>>> m.auto.continue_from(2, "periodic")
```

A slider is written once, when you let it go, with the value it ended at.
**File > Save session as Python script** (or `script session.py`) saves all
the lines so far, with the import they need. Run the script with Python
(`pip install xpplab`) and it repeats the session's computations number for
number: the same runs, the same bifurcation diagram, the same spectra, to
the last bit, because the package runs the same engine. Data exported as
CSV during the session is written again by the script, so the two can be
compared file by file.

Figures are drawn again with matplotlib: the same data, but not the same
picture. Changes to the view (zoom, windows, colours) are not written.
**View > Show actions as Python** (or `echo off`) hides the lines in the
command window; the script keeps them either way.

## Python in the command window

Click the `>>` before the command line (it becomes `py>`), or type `py`,
and the line takes Python instead of commands. Click again, or type
`:cmd`, to go back. A single line of Python also runs from command mode as
`py CODE`.

```
py> m.parameters["iapp"] = 0.1
py> sol = m.run(total=200)
py> np.max(sol["V"])
np.float64(0.26150020956993103)
py> eq = m.equilibrium(); eq.kind
'unstable focus'
```

The Python runs in a process of its own, started the first time, with
these names already defined:

| Name | What it is |
|---|---|
| `m` | The model open here. `m.parameters`, `m.initial_conditions`, `m.set()`, `m.get()` and `m.run()` act in the app, so its sliders and plots follow, and the lines the app writes for your actions run here unchanged. Other methods (`m.equilibrium()`, `m.sweep()`, `m.nullclines()`, `m.ensemble()`, `m.auto`, ...) compute with a private copy of the model at the app's current values. |
| `app` | The app itself: `app.command("auto steady iapp")` runs any command. |
| `xs`, `np` | The xpplab package and NumPy. |

A statement over several lines (`for`, `if`, `def`) shows `...` until an
empty line ends it. Tab completes names. Output and errors appear in the
command window, errors in red. While a line runs, **Stop** ends the
process (its variables are lost) and the next line starts a fresh one; an
endless loop or a crash in your code never stops the app. The
**Workspace > Python** tab lists your variables with their types and
values.

The console uses the copy of the xpplab package that comes with the app,
and a Python 3 with NumPy found on your computer: the one `python use
C:/path/to/python.exe` chose (it is remembered), else `$XPPLAB_PYTHON`,
else the first on the PATH. `python stop` ends it; `python` says which is
in use.

## Command-line options

The desktop app takes a model file and these options:

```
xpplab [model.ode] [options]
```

With no model, it opens `lecar.ode` from its examples the first time it
starts.

| Option | Action |
|---|---|
| `--cmd "command"` | Run a command-window command after start-up. Repeat it to run several (up to 32), in order. |
| `--then "command"` | Run a command a few frames later, once the windows have been drawn, for example to export a figure. Repeatable, up to 32. |
| `--screenshot file.png` | Draw a number of frames, save the window to the file and exit. A name ending in `.png` gives a PNG; any other name gives a BMP. |
| `--frames N` | How many frames `--screenshot` draws before it saves (default 12), or how many frames `--record` saves. |
| `--size WxH` | Window size in pixels (default `1500x920`). |
| `--reset-layout` | Put the windows back in their default places instead of the saved layout. |
| `--record DIR` | Save `--frames` frames as `DIR/frame_00000.png`, `DIR/frame_00001.png`, ... and exit. |
| `--fps N` | Frames a second of a recording (default 20). Time in the app runs at this rate however long each frame takes to save, so animations stay smooth. |
| `--script FILE` | Run commands at given frames of a recording. Each line is `FRAME command`; lines starting with `#` are comments. |
| `--at FRAME "command"` | Run one command at a given frame of a recording. Repeatable. |
| `--log FILE` | Write the command window's text to the file when the app exits. |
| `--no-serve`, `--serve` | Start without the channel through which Python drives the app (the Python guide's *Driving the desktop app*); a screenshot or recording opens it only with `--serve`. |

Frames for `--at` and `--script` count from the first saved frame, which is
frame 0. A command written `@command` runs without being echoed in the
command window. Screenshots and recordings always start from the default
layout and do not save it. If one fails, the error is written to a file
named after it with `.err` added, for example `s.png.err` or `frames.err`.
The app then exits with status 1; a run that works exits with 0, so scripts
and CI can tell the two apart. The folder for `--record` must already exist.

Take a screenshot of the phase plane after a run:

```
xpplab lecar.ode --cmd run --cmd "show phase" --screenshot s.png
```

Record five seconds at 20 frames a second. The script `demo.txt`:

```
# frame command
0 set iapp=0.1
10 run
40 show phase
60 @nullclines
```

and the command:

```
xpplab lecar.ode --record frames --fps 20 --frames 100 --script demo.txt
```

The frames in `frames/` can be turned into a video or an animated GIF with
any tool; `tools/make_demo.py` does this for the README.

## Without a window: xpplab-batch

`xpplab-batch`, next to the app (inside the app bundle on macOS:
`xpplab.app/Contents/MacOS/xpplab-batch`), runs models from a terminal,
a script or a scheduled job. Without commands it is XPPAUT's `-silent`
mode: it runs the model and writes `output.dat`, the same numbers
`xppaut -silent` writes.

```
xpplab-batch lecar.ode
xpplab-batch lecar.ode -outfile run.dat
```

With commands it speaks the command window's language, AUTO included.
Give them with `-c`, or one per line in a file (lines starting with `#`
are comments); `-c` commands run after those of the files.

```
xpplab-batch lecar.ode -c "set iapp=0.1" -c run -c "export run.csv"
xpplab-batch lecar.ode analysis.txt --script analysis.py
```

where `analysis.txt` could hold:

```
# a brute-force diagram, then AUTO's
use hopf
range iapp 0 0.5 50 V
export range.csv range
auto set parmax=0.6 nmax=220
auto steady iapp
auto from 2 periodic
export diagram.csv auto
export diagram.svg auto
```

The command window's text goes to the terminal. The first command that
fails stops the run, and `xpplab-batch` exits with status 1 (0 when all
went well, 2 for a mistake in the options), so a script can tell. A `set`
of a name the app does not know prints a note but goes on, since XPPAUT
accepts options the app does not list.

| Option | Action |
|---|---|
| `-c "command"` | Run a command. Repeatable. |
| `-outfile FILE` | Without commands, the file the run is written to (default `output.dat`). |
| `--script FILE.py` | Save the session as a Python script at the end, as *File > Save session as Python script* does. |
| `--echo` | Also print each action as Python (`>>> ...`). |
| `--quiet` | Print only the errors (to the error stream). |
| `--keep-going` | Run every command even after one fails; the exit status is still 1. |

`export FILE.dat` writes a run in XPPAUT's format (8 significant digits,
no header); `.csv` writes every number exactly. Figures are written as SVG,
the phase plane over the model file's window (`@ xlo=..., xhi=...`) and the
others fitted to the data. PNG figures and the Kinescope need the window.

