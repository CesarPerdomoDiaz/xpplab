# Simulation

## Running

`Model.run()` integrates from the current initial values, exactly as
XPPAUT's batch mode (`xppaut -silent`) does, and returns a `Trajectory`.
Keyword arguments are applied with `set()` first:

```python
sol = m.run(total=500, dt=0.01, meth="5dp")
```

A `Trajectory` holds time (`sol.t`) and every output column: the state
variables, then the auxiliary quantities declared with `aux`.

| Method | Result |
|---|---|
| `sol["V"]` | one column as a NumPy array |
| `sol.columns` | the column names |
| `sol.to_numpy()` | all columns, one row per output time |
| `sol.to_pandas()` | a DataFrame indexed by time |
| `sol.save("out.dat")` | XPPAUT's `output.dat` text format |
| `sol.plot("V", "W")` | a matplotlib plot against time |
| `sol.plot("W", x="V")` | one column against another |

The values are XPPAUT's stored data (single precision), the same numbers
XPPAUT writes to `output.dat`.

## Integrators

`meth=` takes XPPAUT's names or friendlier aliases:

| XPPAUT name | Alias | Kind |
|---|---|---|
| `rungekutta` | `rk4` | fixed-step RK4 |
| `5dp` | `dopri5`, `dormand-prince` | adaptive Dormand-Prince 5 |
| `83dp` | `dopri8`, `dopri853` | adaptive Dormand-Prince 8 |
| `stiff` | | adaptive stiff Rosenbrock |
| `cvode` | | CVODE |
| `2rb` | `rosenbrock` | Rosenbrock 2(3) |
| `euler`, `modeuler`, `backeul` | `backward-euler` | for noise |
| `gear`, `adams`, `qualrk`, `volterra`, `ymp` | `symplectic` | |
| `discrete` | `map` | maps |

`xs.METHODS` lists them all.

## Options

Any option an ODE file's `@` line accepts can be set from Python:

```python
m.set(total=200, dt=0.05, trans=50, bound=1e6, nout=10, tol=1e-8)
m.options          # a snapshot: {'TOTAL': 200.0, 'DT': 0.05, 'METH': ...}
m.get("total")
```

## Parameter sweeps

```python
runs = m.sweep("I", [0.1, 0.2, 0.3])          # a list of Trajectory
amp = m.sweep("I", np.linspace(0, 2, 100),    # an array: a brute-force
              measure=lambda s: np.ptp(s["V"][len(s)//2:]),
              total=400)                      # bifurcation diagram
```

The parameter is restored after the sweep.

## Interactive sliders in Jupyter

```python
m.interact("I", "eps")          # sliders around the current values
m.interact(I=(0, 2), y="V")     # explicit range, plot only V
```

This needs `ipywidgets` and `matplotlib`.

## Errors

A model with a mistake raises `xs.EngineError`. The message quotes
XPPAUT's parser, and `err.log` holds everything the engine printed:

```python
try:
    xs.load("broken.ode")
except xs.EngineError as err:
    print(err)        # could not load broken.ode: Y is a duplicate name
```
