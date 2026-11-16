# What's new

What changed in each version of XPP Lab, newest first. Each [release's page](https://github.com/CesarPerdomoDiaz/xpplab/releases)
on GitHub has the downloads; `pip install xpplab==0.1.3` installs a given
version of the Python package.

## Not released yet

In the source on GitHub, for the next release.

**Documentation to the standard of MATLAB's.**

- Every command of the app has a reference page: every form, worked
  examples with their numbers, each setting, the mathematics, and what to
  do when it fails. `doc NAME` in the command window opens one, as does
  the **?** beside each action and F1 over a button; `xs.doc()` opens it
  from Python.
- The Python package's reference gives every function and method its
  arguments, what it returns, an example with its output, and the command
  it matches. The examples are run by the tests.
- The manual's chapters are short guides that link the reference pages.
  The Help window typesets mathematics, as the website does.
- A glossary, whose terms are explained on hover on every page of the
  site; search finds the reference pages first.

**New in the app and in Python.**

- **The command palette** (Ctrl+Shift+P): find and run any action or
  command by name.
- **Annotations:** text, arrows and markers on the plots and on the
  panels of a figure for a paper.
- **MATLAB files:** `export` writes `.mat` files of runs, diagrams and
  tables, and `to_mat()` does so in Python.
- **A gallery of 21 classic models** (Lorenz, Chua, van der Pol, the heat
  equation, Hodgkin-Huxley, ...), in the app's **Models > Classics** and
  as `xs.example("classics/lorenz")`, and Jupyter notebooks to start
  from.
- **The Animator** draws an animation's nullclines and lets its grab points
  be dragged, as XPPAUT's does.
- **Control design** in the MCP server for coding agents: linearise, place
  poles, LQR, root locus and sensitivity.

**AUTO.**

- A second continuation numbers its labels on from the first, and the
  first run's points can still be continued from.
- `auto load` refuses a diagram saved from another model, naming its
  variables and parameters.
- Switching at a branch point of steady states says why it would only
  retrace the branch, and what to do instead.
- Heteroclinic starts are tested against a known answer.

**Mistakes are reported instead of ignored.**

- `set` refuses a value that is not a number and a name that is no
  parameter, variable or option; commands given a wrong word say so.
- An adjoint from an earlier run is refused.
- Files with names in any alphabet open.

**Fixes.**

- The Liapunov exponent with fixed-step methods such as Adams.
- The engine leaked a file stream on each load of a model, so that a
  session that loaded about 500 models stopped.

## 0.1.3

- **Control design**, in the app (the *Linearise* tool) and in Python,
  with the same numbers in both: pole placement, with the closed loop
  written as a model and run; optimal control (LQR); and the root locus
  over a gain or a parameter, with the values where a pole crosses into or
  out of stability.
- **Figures for papers:** the *Figure* window puts several plots, insets
  and labels with mathematical notation on one page, and exports it as
  PDF, SVG, EPS or PNG at any size. `xpplab.Figure` draws the same figure
  from Python.
- **Many runs and shooting:** a batch of starting points with the flag
  that stopped each run, and runs along the eigenvectors of an
  equilibrium. Auxiliary quantities at a state and along a bifurcation
  diagram, and Floquet multipliers along a branch of periodic orbits.
- **Sensitivity** to parameters along a trajectory, from the variational
  equations (`Model.sensitivity`).
- **The Liapunov exponent is fixed.** XPPAUT computes it from the stored
  run, in single precision, so long runs went wrong: Lorenz to t = 1000
  gave 7.6 for an exponent of about 0.9. XPP Lab computes it during the
  integration in double precision; XPPAUT's number is still there as
  `Model.lyapunov(stored=True)`.
- **Learn by example:** a kite on a tether, from its Lagrangian to its
  bifurcations, and the neuron tutorial with the theory behind it. A
  *Coming from MATLAB* guide.

## 0.1.2

- **The app and Python together.** Each action in the app appears in the
  command window as the line of Python that does the same, and *File >
  Save session as Python script* saves a script that repeats the session
  exactly. The command window also takes Python, and **Install Python**
  downloads one for XPP Lab on a computer without it.
- **Drive the open app from outside:** `xpplab.app.connect()` in a
  notebook or a script, or a coding agent through the MCP tools.
- **The rest of XPPAUT's interface:** equilibria over a parameter and
  every equilibrium in a box, Range over two parameters, initial values
  from a file or a formula, frozen and saved nullclines, the Kinescope,
  and statistics over many noisy runs. `xpplab-batch` is XPPAUT's
  `-silent` mode, and also runs the app's commands without a window.
- **AUTO:** diagrams save and load exactly, an orbit can be imported, and
  boundary-value and homoclinic starts are tested.
- **Linearise:** the A and B matrices at an equilibrium, the eigenvalues
  in the complex plane, and whether the inputs can steer the system.
- **Plots keep your zoom** until a double-click or *Fit*.
- **Learn by example** on the website: neuroscience, ecology, chemistry
  and mechanics.

## 0.1.1

- **A new name: XPP Lab** (it was XPP Studio). The Python package is
  `xpplab`, the MCP server `xpplab-mcp`, and the app `xpplab`.
- **AUTO no longer leaves a model broken.** After a failed continuation
  XPPAUT left the parameter at AUTO's last trial value, so every later run
  was wrong; the parameters now return to their values after every run.
- **A singular matrix ends only the continuation,** and the points
  computed so far stay.
- **The bifurcation plot** fits its axes to the window AUTO was asked to
  cover.

## 0.1.0

The first release, as XPP Studio: XPPAUT 8.0's engine, giving the same
numbers as XPPAUT, with a desktop app for Windows, macOS and Linux and a
Python package, needing no X server, compiler or terminal. The phase
plane, AUTO without the dialogs, a slider for every parameter, the 3D view
and the Animator, XPPAUT's analysis tools, and an editor whose errors
point at the line.
