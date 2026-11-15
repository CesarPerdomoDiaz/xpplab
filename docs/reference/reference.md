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
| [ic read](ic-read.md) | Read the initial values from a text file. |
| [ic write](ic-write.md) | Write the initial values to a text file, one per line. |
| [whos](whos.md) | List the variables, parameters and main options, with their values. |
| [continue](continue.md) | Integrate further from the end of the last run and add the new part to it. |
| [range](range.md) | Run once for each value of a parameter or initial value: a brute-force diagram, hysteresis, Liapunov exponents. |

## Equilibria

| Page | What it does |
|---|---|
| [eq](eq.md) | Find an equilibrium near the initial values, with its eigenvalues and its stability. |
| [eq all](eq-all.md) | Find every equilibrium in a region, from many starting points. |
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
