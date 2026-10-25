# Python quick start

## Install

```
pip install xpplab            # the engine (NumPy is the only dependency)
pip install "xpplab[all]"     # plus matplotlib and pandas
```

The wheel contains the compiled XPPAUT engine. Nothing needs a compiler,
and no X11 server is involved.

## Start from the desktop app

The desktop app writes each action as the xpplab code that does the same,
in its command window, and **File > Save session as Python script** turns a
session into a script that repeats it number for number. Explore with the
mouse, then carry on in Python from the saved script.

## Load a model and run it

Any XPPAUT `.ode` file works. You can also write the model inline:

```python
import xpplab as xs

m = xs.Model.from_text("""
v' = v - v^3/3 - w + I
w' = eps*(v + a - b*w)
par I=0.5, a=0.7, b=0.8, eps=0.08
init v=-1, w=1
@ total=100, dt=0.05
done
""")

sol = m.run()               # integrate with the file's settings
sol.t                       # times (NumPy array)
sol["V"]                    # a variable (names ignore case)
sol.to_pandas()             # a DataFrame indexed by time
sol.plot("V", "W")          # a quick matplotlib plot
```

## Change things

Parameters, initial values and numerical options behave like dictionaries:

```python
m.parameters["I"] = 0.2
m.initial_conditions["v"] = 0.5
m.set(total=300, meth="stiff")    # anything an ODE-file @ line can set
m.run(I=0.4)                      # set, then run
m.apply_set("fast")               # a named 'set' from the file
```

## Explore

```python
eq = m.equilibrium()       # Newton's method from the initial values
eq.kind, eq.eigenvalues    # 'unstable focus', array([...])

m.phase_plane("v", "w")    # trajectory, nullclines, flow, equilibrium
```

## Bifurcations

```python
m.parameters["I"] = 0
d = m.auto.equilibria("I", parmax=2)   # follow the rest state in I
d.special()                            # [<HB #2: ... I=0.331>, <HB #3: ...>]
orbits = m.auto.continue_from(d.first("HB"), "periodic")
m.auto.diagram.plot("V")
```

## Notebook display

In Jupyter, a `Model`, a run (`Trajectory`), equilibria and AUTO diagrams
display as HTML tables: variables with their initial values, parameters,
equilibria with a stable or unstable label, and the special points of a
diagram. Long tables are cut after 20 rows. Plain `repr` is unchanged in a
terminal.

## Several models at once

Each `Model` holds its own copy of the engine, so models are independent:

```python
a = xs.load("lecar.ode")
b = xs.load("lecar.ode")
a.parameters["iapp"] = 0.3     # b is unaffected
```

Use `with xs.load(...) as m:` or `m.close()` to free a model's memory
promptly.
