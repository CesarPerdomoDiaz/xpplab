# Notebooks

Three short guided notebooks. Each one is a task to follow, cell by cell,
not a copy of a manual page.

| Notebook | You will |
|---|---|
| [01_first_steps.ipynb](01_first_steps.ipynb) | load an example model, run it, plot it, change a parameter, find the equilibria |
| [02_bifurcations.ipynb](02_bifurcations.ipynb) | follow a branch of equilibria with AUTO, find its Hopf point, follow the periodic branch from it |
| [03_control.ipynb](03_control.ipynb) | linearise, check controllability, place the poles, run the closed loop, compare with LQR |

## Open them

```
pip install "xpplab[all]" jupyterlab
jupyter lab notebooks/
```

The notebooks load their models with `xs.example("lecar.ode")`, which finds
the examples that ship with the package, so they run wherever `xpplab` is
installed. They are committed without outputs; run them to see the plots.

`python/tests/test_notebooks.py` runs each notebook in a fresh kernel and
fails on any error. It needs `nbclient`, `nbformat` and `ipykernel`, and is
skipped where they are missing.
