# xppstudio

XPPAUT's simulation and AUTO bifurcation engine for Python. Install it with
pip; there is nothing to compile and no X11 server is needed.

```python
import xppstudio as xs

m = xs.load("lecar.ode")              # any XPPAUT .ode file
sol = m.run(total=200, iapp=0.1)     # NumPy arrays: sol.t, sol["V"]
eq = m.equilibrium()                 # eigenvalues and stability

d = m.auto.equilibria("iapp")        # AUTO continuation of steady states
hb = d.first("HB")                   # the first Hopf bifurcation
orbits = m.auto.continue_from(hb, "periodic")
m.auto.diagram.plot("V")             # matplotlib, XPPAUT's colours
```

- **Models**: every XPPAUT `.ode` file works unchanged. Parameters, initial
  values and options are live dictionaries, and named `set`s apply with one
  call.
- **Simulation**: all fifteen XPPAUT integrators (`meth="stiff"`,
  `"cvode"`, `"5dp"`, ...), delays, noise, maps, DAEs and Volterra
  equations.
- **AUTO**: steady states, periodic orbits from Hopf points, two-parameter
  curves of folds and Hopf points, branch switching, and period doubling.
  Results come back as Python objects, a pandas DataFrame and a
  ready-made plot.
- **Fidelity**: the numerics are XPPAUT 8.0's own C code. The test suite
  checks the output against unmodified XPPAUT bit for bit.

Part of [XPP Studio](https://github.com/xppstudio/xppstudio). GPL-2.0-or-later,
like XPPAUT.
