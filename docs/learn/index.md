# Learn by example

These pages teach the mathematics of dynamical systems through models from
four fields. Each page opens with the theory it needs, then works through
examples: a model you can save and run, what to expect and why, the Python
that reproduces it, and exercises with worked solutions. They are meant for
a course, or for self-study with the desktop app open beside the text.

| Page | Models | What you learn |
|------|--------|----------------|
| [Neuroscience](neuroscience.md) | passive membrane, theta neuron, FitzHugh-Nagumo | equilibria and eigenvalues, excitability, saddle-node and Hopf onset of firing, relaxation oscillations |
| [Ecology](ecology.md) | harvested population, Lotka-Volterra, Rosenzweig-MacArthur, competition | tipping points and hysteresis, conserved quantities, the paradox of enrichment, basins of attraction |
| [Chemistry](chemistry.md) | consecutive reactions, Michaelis-Menten, Schlögl, Brusselator | exact solutions as tests, time scales and stiffness, bistability, chemical oscillators |
| [Mechanics](mechanics.md) | damped pendulum, buckling beam, van der Pol, Lorenz | energy, pitchfork bifurcation, limit cycles and relaxation oscillations, chaos |

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
