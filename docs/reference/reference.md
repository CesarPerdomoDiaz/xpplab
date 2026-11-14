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
