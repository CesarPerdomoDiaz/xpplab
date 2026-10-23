# Welcome to XPP Studio

XPP Studio is a workbench for dynamical systems: you write the equations,
then simulate them, explore their phase plane and compute bifurcation
diagrams with AUTO. Under the hood it runs **XPPAUT 8.0**, the program by
Bard Ermentrout that a generation of scientists has used, with its
numerical code unchanged. Any XPPAUT `.ode` file works here as it is.

## A first look in one minute

1. [Open the Morris-Lecar neuron model](example:lecar.ode). It is a classic
   two-variable model of a nerve cell.
2. [Run it](cmd:run). The **Time Series** tab shows V and W over time.
3. Open the **Phase Plane** tab and [draw the nullclines](cmd:nullclines).
   Click anywhere in the plot to start a new trajectory from that point.
4. In **Workspace > Parameters**, drag the `iapp` slider (the injected
   current). The solution updates as you drag.
5. [Find the equilibrium](cmd:eq) near the current state. The command window
   reports its stability and eigenvalues.

## The windows

| Window | What it is for |
|---|---|
| Models | Example models and recent files. Hover a name for a description. |
| Workspace | Parameters (with sliders), initial values, numerics, parameter sets. |
| Editor | The `.ode` file. Press Ctrl+Enter to apply changes. |
| Time Series | Variables against time. |
| Phase Plane | One variable against another, with nullclines and flow. |
| Bifurcation | AUTO: follow equilibria and periodic orbits as a parameter varies. |
| Command Window | A log of what happened, and a command line. |
| Help | This manual. Press F1 any time. |

Every window can be dragged, docked, stacked in tabs or closed. To get the
original arrangement back, use **View > Reset window layout**.

## Saving your work

- **Figures**: every plot has an **Export...** button. Choose *PNG* for an
  image exactly as shown, or *SVG* for a vector figure that stays sharp at
  any size, for papers and posters.
- **Data**: **File > Export data as CSV** saves the last run for
  spreadsheets, MATLAB or Python.
- **Models**: **File > Save** (Ctrl+S) writes the `.ode` file. Parameters
  you changed in the Workspace are not written into the file; put values you
  want to keep in the file, or in a named `set`.

## Where to go next

- [Tutorial: your first model](tutorial.md), from writing the equations to a
  bifurcation diagram, step by step.
- [Bifurcation analysis with AUTO](auto.md).
- [The .ode file format](odefile.md), the complete reference.
- [Coming from XPPAUT](xppaut.md), if you already know XPPAUT.

> Tip: if you know XPPAUT's keyboard commands, they still work: I G
> integrates, N N draws nullclines, S G finds an equilibrium. See
> [Commands and keyboard shortcuts](commands.md).
