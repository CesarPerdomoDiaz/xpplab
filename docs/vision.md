# Vision

**XPP Studio brings the self-contained XPPAUT experience back to more
people.** You write a model, simulate it, look at its phase plane, follow
its bifurcations with AUTO, and understand it, all in one program that
installs in a minute on any computer. XPPAUT has offered this for thirty
years to the people who could get it running. XPP Studio makes it
approachable for everyone else: students meeting differential equations
for the first time, teachers, and researchers in neuroscience, biology,
chemistry and engineering.

This page says where the project is going. [The roadmap](https://github.com/CesarPerdomoDiaz/xppstudio/blob/main/ROADMAP.md)
says how far it has got. The maintainer agent reads both when it proposes
work.

## Five themes

### 1. An app anyone can install and use

- One download on Windows, macOS and Linux; no X server, no compiler, no
  terminal.
- An interface that shows what each button does and why, with the equivalent
  command for every action, so the app teaches its own scripting.
- XPPAUT users keep their `.ode` files, their keyboard shortcuts and their
  workflow.

### 2. Learn by doing: documentation and examples

- A manual that teaches: tutorials that start from a question ("why does this
  neuron fire?") and end with a bifurcation diagram.
- Many worked examples, from field to field, each runnable in one click.
- Every feature documented where it is used: in the app, on the website and
  in Python.

### 3. The engine, extended

- XPPAUT's numerics stay exactly as they are; new analyses sit on top of them.
- More cases: larger models, more kinds of equations, and the XPPAUT features
  still missing.
- Deeper analysis: sensitivity to parameters, Floquet multipliers, isochrons
  and phase reduction, codimension-two bifurcations, parameter fitting.

### 4. Beyond XPPAUT: control design

Most tools that analyse a system stop at describing it. XPP Studio should
also let you change it:

- linearise a model at an equilibrium, with chosen inputs;
- see its eigenvalues in the complex plane, and whether it is controllable;
- drag an eigenvalue to where you want it, get the feedback gain that puts it
  there, and simulate the controlled system;
- root loci over a gain or a parameter, and optimal control (LQR) later.

### 5. Reach: open, citable, and ready for coding agents

- Open source and easy to cite, with a paper, releases and a Python package
  on PyPI.
- Ready for coding agents. Many researchers now work with assistants such as
  Claude Code. XPP Studio gives those agents their own tools: an MCP server,
  a skill and machine-readable results. An agent can then take an `.ode` model
  and run a whole analysis, while the researcher checks every step.

## Rules that do not bend

- **The numbers do not change.** Every example gives bit-identical output to
  unmodified XPPAUT 8.0.
- **Every XPPAUT model works unchanged.**
- **Everything the app does can be done from a script**, and the other way
  round where it makes sense.
- **Credit where it is due:** XPPAUT is Bard Ermentrout's work, and AUTO is
  Eusebius Doedel's.

## How work is chosen

The maintainer agent, The Limit Cyclist, proposes the next steps: a few at a
time, each tied to a theme and a roadmap milestone, with why it matters and
what done means. The project's maintainer approves or declines each
proposal, and the agent then plans it, builds it, tests it and opens a pull
request. Large features start with a short design note, which is agreed
before any code is written. Small, finished steps are preferred to large,
half-done ones.
