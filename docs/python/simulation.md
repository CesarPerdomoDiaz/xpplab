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

### Continuing a run, and saving it

`m.continue_run(sol, 50)` is XPPAUT's Continue: it integrates 50 more time
units from where `sol` ended and returns `sol` with the new points
appended, leaving the initial conditions as they were. `m.last_run()` is
the engine's most recent run, such as the one period of the limit cycle
that `m.phase_response()` leaves behind.

### Initial conditions

```python
m.ics["V"] = -0.3                       # one value (m.initial_conditions)
m.ic_last()                             # from the end of the last run (I L)
m.ic_last(shift=True)                   # and T0 from its end time (I S)
m.write_ics("ics.dat")                  # one per line, exactly
m.read_ics("ics.dat")                   # one number per variable (I F)
m.ic_formula("V", "vk + 0.1")           # a formula, evaluated by XPPAUT
m.ic_formula("u[0..199]", "0.8*exp(-([j]-100)^2/50)")   # [j]: the index
```

`read_ics` takes the first numbers in a text file, in the order of
`m.variables`; a file with too few raises `EngineError` and changes
nothing. `ic_formula` is XPPAUT's *Initial conds > formUla*: the formula
may use the parameters and the model's functions, and a numbered range of
variables gets one value per index.

`sol.to_csv("run.csv")` writes a run with a header and every number exact;
`Table` results and bifurcation diagrams (`m.auto.diagram.to_csv(...)`)
have the same method. The files match the desktop app's CSV exports.

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

Two parameters move together when `sweep` gets a pair of names and a list
of pairs, or over every combination with `sweep_grid` (XPPAUT's 2par
range):

```python
peak = lambda s: s["V"][len(s) // 2:].max()
line = m.sweep(("I", "gk"), [(0.1, 1.0), (0.2, 1.5)], measure=peak)
grid = m.sweep_grid("I", np.linspace(0, 2, 40),     # across
                    "gk", np.linspace(1, 3, 20),    # up
                    measure=peak)                   # shape (20, 40)
xs.Table.from_grid("I", np.linspace(0, 2, 40), "gk", np.linspace(1, 3, 20),
                   {"peak": grid}).to_csv("grid.csv")
```

Row `j` of the grid holds the runs at the second parameter's `j`-th value.
A measure that returns several numbers adds a last axis. Without a
measure, `sweep_grid` returns the trajectories as a list of rows.
`continue_runs=True` starts each run where the previous one ended, going
through the grid row by row. Both parameters are restored afterwards.

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
