# Learn by example

These pages teach the mathematics of dynamical systems through models from
five fields. Each page opens with the theory it needs, then works through
examples: a model you can save and run, what to expect and why, the Python
that reproduces it, and exercises with worked solutions. They are meant for
a course, or for self-study with the desktop app open beside the text.

The theory follows established textbooks and cites them by chapter and
section, so that every page can be read next to a book that treats the
subject in full:

- mechanics and the kite: Sanz Recio and Sánchez Arriaga, *Mecánica
  analítica: lagrangiana, hamiltoniana y sistemas dinámicos* (McGraw-Hill,
  2019), whose chapters 8 to 10 on dynamical systems are also the general
  theory the other pages refer to, and which follows the kite from its
  first chapter to its last;
- neuroscience: Izhikevich, *Dynamical Systems in Neuroscience* (MIT Press,
  2007);
- ecology: Murray, *Mathematical Biology I* (Springer, 2002), and Strogatz,
  *Nonlinear Dynamics and Chaos* (CRC Press, 2024);
- chemistry: Epstein and Pojman, *An Introduction to Nonlinear Chemical
  Dynamics* (Oxford University Press, 1998), with Murray, Keener and Sneyd,
  and Atkins for the kinetics.

The full references are at the end of each page.

| Page | Models | What you learn |
|------|--------|----------------|
| [Neuroscience](neuroscience.md) | passive membrane, theta neuron, FitzHugh-Nagumo | equilibria and eigenvalues, excitability, saddle-node and Hopf onset of firing, relaxation oscillations |
| [Ecology](ecology.md) | harvested population, Lotka-Volterra, Rosenzweig-MacArthur, competition | tipping points and hysteresis, conserved quantities, the paradox of enrichment, basins of attraction |
| [Chemistry](chemistry.md) | consecutive reactions, Michaelis-Menten, Schlögl, Brusselator | exact solutions as tests, time scales and stiffness, bistability, chemical oscillators |
| [Aerospace: a kite on a tether](kite.md) | single-line kite, from the Lagrangian to the lab | fold and subcritical Hopf, unstable orbits and Floquet multipliers, tether tension, invariant manifolds, which flights survive, scaling of the minimum wind, figures for a report |
| [Mechanics](mechanics.md) | pendulum, pendulum on a rotating hoop, pendulum with a vibrating support, van der Pol, forced pendulum | energy and Lagrange's theorem, pitchfork and Hopf bifurcations, averaging, parametric resonance, period doubling and chaos |

## How a page works

Every model is an `.ode` file shown in a code block. The first line is a
comment with the file name:

```ode
# fhn.ode
v' = v - v^3/3 - w + I
...
```

Save it under that name in the folder where you work. You can open it in
the desktop app (**Open** in the toolbar, or **File > Open...**), or load it in Python:

```python
import xpplab as xs

m = xs.load("fhn.ode")
```

The Python snippets of a page run one after the other in a single script,
so a name defined in an early snippet (`m`, `eq`, `d`) is available in the
later ones. The numbers in the comments (`# 0.3313`) are what the code printed when
the pages were written. The test suite runs every snippet, and checks the
numbers that the text and the solutions quote, so the pages stay correct as
the software changes.

The exercises are for you to do. Their solutions are folded below them:
open one only after you tried.

## What you need

- The Python package (`pip install "xpplab[all]"`), which brings NumPy,
  matplotlib and pandas, or the [desktop app](../install.md). Install and
  Python quick start: [Python quick start](../python/quickstart.md).
- A little calculus and linear algebra: derivatives, and eigenvalues of a
  2 by 2 matrix. Everything else is explained when it is first used.

## Where to look things up

- [The .ode file format](../manual/odefile.md): every statement a model can
  contain.
- [Numerical methods](../manual/numerics.md): the integrators and when to
  choose each.
- [Bifurcation analysis with AUTO](../manual/auto.md): the point types and
  what each continuation does.
- [Python API reference](../python/reference.md).
- [Examples gallery](../gallery/index.md): the example models that come with
  XPPAUT, with thumbnails.
