# fitdata

Fit parameters, and initial values, so that the model's run matches data,
by least squares.

## Syntax

```
fitdata FILE VARIABLES COLUMNS PARAMETERS
```

`VARIABLES`, `COLUMNS` and `PARAMETERS` are comma-separated lists, without
spaces: `fitdata data.csv V,W 2,3 gca,gk`.

- **In the app:** Run > Analyse the run > **Fit to data** (XPPAUT's
  *Numerics > stocHastic > fIt data*).
- **In Python:** `m.fit("data.csv", variables="V", parameters=["gca", "gk"])`,
  with `columns=`, `tolerance=`, `epsilon=` and `max_iterations=`. The data
  may also be an array, time in its first column.

## Description

`fitdata` reads a data file whose first column is time and whose other
columns are measurements, and changes `PARAMETERS` until the model's
`VARIABLES`, run from the data's first time, match the data's `COLUMNS`
in the least-squares sense. It uses XPPAUT's Levenberg-Marquardt method,
starting from the present values. On success the model keeps the fitted
values, and the app runs it again with them.

`COLUMNS` count from 1 for time, so the first measurement is column 2.
`PARAMETERS` may include variables: their initial values are then fitted.

The file may be the CSV that [export](export.md) writes, with its header
and commas, or plain rows of numbers separated by spaces.

## Examples

### Recover two conductances

Make data from a run, spoil two parameters, and fit them back:

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Set the current](cmd:set iapp=0.1), [run](cmd:run), and write the run
   with `export data.csv`.
3. [Spoil the conductances](cmd:set gca=1.2 gk=1.8), then fit them with
   `fitdata data.csv V 2 gca,gk`.

```
set iapp=0.1
run
export data.csv
set gca=1.2 gk=1.8
fitdata data.csv V 2 gca,gk
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.run(iapp=0.1).to_csv("data.csv")
m.set(gca=1.2, gk=1.8)
print(m.fit("data.csv", variables="V", parameters=["gca", "gk"]))
```

The command window answers "Fitted: gca = 1 gk = 2" (to six digits), and `get gca` gives
0.9999999999, `get gk` 2.000000006: the values the data were made with.

How to read it. The fit only saw `V`, yet found both conductances: the
calcium and potassium currents shape the spikes differently, so the
voltage alone pins them down. With real data the fit stops at the values
that explain the data best; whether they are the true ones depends on how
much the data constrain them.

### Fit an initial value

The initial value of `W` can be fitted too, here from both columns:

```python
m.set(gca=1, gk=2, W=0.1)
print(m.fit("data.csv", variables=["V", "W"], columns=[2, 3], parameters=["W"]))
```

From 0.1 the fit returns `W` = 0.03, the model's own initial value.

How to read it. A variable among `PARAMETERS` stands for its initial value
at the data's first time. Fitting it is how to recover a state that was
not measured at the start.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `VARIABLES` | the command; `variables=` in Python | The model's variables to compare with the data. | ODE variables only. |
| `COLUMNS` | the command; `columns=` in Python | Their columns in the file, from 2. | One per variable. Python's default is 2, 3, ... |
| `PARAMETERS` | the command; `parameters=` in Python | What to vary: parameters, and variables for their initial values. | Start them near plausible values. |
| `tolerance` | `tolerance=` in Python | When to stop. | 1e-3 by default. |
| `epsilon` | `epsilon=` in Python | The step of the finite differences. | 1e-5 by default. |
| `max_iterations` | `max_iterations=` in Python | The most iterations. | 20 by default. |

`tolerance`, `epsilon` and `max_iterations` keep a value given until it is
changed, for the model's later fits too.

## Results

- **The parameters** hold the fitted values, and the command window lists
  them. The model is run again to show the fitted solution.
- **The log** shows each iteration's error and, at the end, the
  covariance of the fitted values.
- **In Python** `m.fit()` returns the fitted values as a dictionary.

## More about

**Least squares.** The fit minimises the sum of the squared differences
between the model and the data at the data's times. Levenberg-Marquardt
moves between steepest descent, safe far from the minimum, and
Gauss-Newton, fast near it.

**Identifiability.** Two parameters that change the run in the same way
cannot be told apart: the fit wanders along a valley. The covariance in
the log shows it, with a large variance and a strong correlation. Fit
fewer parameters, or more of the variables.

## Tips

- Run the model with the starting values first, and check that it looks
  roughly like the data: a fit started far away may stop at a wrong
  minimum.
- Fit one or two parameters at a time, then all of them together.

## Algorithms

XPPAUT's fit: Levenberg-Marquardt on the squared differences between the
model, integrated with its method to each data time, and the data, with
the Jacobian from finite differences of step `epsilon`.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `cannot read the data file FILE` | No such file. | Check the name and the folder. |
| `the data file FILE holds no rows of numbers` | Every line starts with a word. | Check the file. |
| `the data needs a column of times and at least one more; ...` | The file has a single column. | Add the measurements. |
| `# columns != # fitted variables` | `VARIABLES` and `COLUMNS` have different lengths. | One column per variable. |
| `the fit did not converge; see the log` | The iterations ran out, or the run failed. | Start nearer, fit fewer parameters, or raise `max_iterations`. |

## References

- Press, W. H., Teukolsky, S. A., Vetterling, W. T., and Flannery, B. P.
  (2007). *Numerical Recipes*, 3rd edition. Cambridge University Press.
  Chapter 15 (modelling of data).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [export](export.md): write a run as data.
- [run](run.md): the model's run that is compared.

## Version history

Introduced in 0.1.0. Reads the CSV that `export` writes since 0.1.4.
