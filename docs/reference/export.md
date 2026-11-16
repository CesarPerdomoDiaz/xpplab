# export

Save the last run, a result or a figure to a file: CSV or MATLAB for
data, SVG or PNG for figures.

## Syntax

```
export FILE.csv
export FILE.mat
export FILE.csv WHAT
export FILE.svg FIGURE
export FILE.png FIGURE
```

- **In the app:** File > **Export data as CSV or MAT...**.
- **In Python:** `sol.to_csv("run.csv")`, `sol.to_mat("run.mat")`; results
  such as tables and diagrams have the same two methods.

## Description

`export FILE.csv` writes the last run: a header with the column names,
then one row per stored point, every number written with all the digits
needed to read it back exactly. `export FILE.mat` writes the same columns
as a MATLAB file: `t` and one column vector per variable and auxiliary
quantity, and a struct `params` with the parameter values the run used.
Any other ending, such as `.txt` or `.dat`, gets the CSV text.

`WHAT` chooses another table instead of the run:

| `WHAT` | What is written |
|---|---|
| `auto` | The bifurcation diagram: one row per point. |
| `range` | The table of a [range](range.md) that measures a variable or the Liapunov exponent. |
| `analysis` | The result in the Analysis window: a histogram, spectrum, correlation and so on. |
| `equilibria` | The equilibria the last [eq all](eq-all.md) found, with their eigenvalues. |
| `nullclines` | The nullclines shown and frozen, as line segments. |
| `locus` | The root locus of the control tools. |

`export FILE.svg FIGURE` saves a figure as a vector drawing: `time` (the
Time Series), `phase`, `auto` or `auto2`. `.png` saves it as it looks on
the screen, so it needs the app's window.

## Examples

### The run, for another program

1. [Open the Morris-Lecar model](example:lecar.ode).
2. Write the run with `export run.csv`, and again with `export run.mat`.

```
export run.csv
export run.mat
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
sol = m.run()
sol.to_csv("run.csv")
sol.to_mat("run.mat")
```

The command window says "Exported 601 rows to run.csv". The file begins

```
T,V,W,CUR,ICA,IK,RATE
0,-0.14399999380111694,0.029999999329447746,0.05000000074505806,...
```

and in MATLAB, `load run.mat` gives `t`, `V`, `W`, `CUR`, `ICA`, `IK`,
`RATE` as 601-by-1 vectors and `params.iapp` = 0.05.

How to read it. Each number is the stored value exactly: −0.144 was stored
in single precision as −0.14399999380111694, and that is what the file
says. The `.mat` file the app writes and the one Python writes are the
same, byte for byte. The CSV files hold the same numbers; the app ends its
lines as the system does (CR LF on Windows) and writes a round number as
`0`, where Python writes `0.0`.

### The equilibria, as a table

```
eq all
export eq.csv equilibria
```

The command window says "Exported 3 equilibria to eq.csv". The file has a
row per equilibrium: the values of `V` and `W`, the number of unstable
directions, and the real and imaginary part of each eigenvalue.

How to read it. The first row,
`0.0276...,0.2692...,2,0.5204...,0.7811...,0.5204...,-0.7811...`, is the
unstable focus: two unstable directions, eigenvalues 0.5204 ± 0.7811 i.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `FILE` | the command; the path in Python | The file to write; it is replaced if it exists. | The ending chooses the format: `.mat` for MATLAB, `.svg` or `.png` for a figure, anything else for CSV. In the app a relative name is relative to the model's folder; in Python, to the working folder. |
| `WHAT` | the command | Which table, for data. | Leave it out for the run. |
| `FIGURE` | the command | Which figure, for `.svg` and `.png`. | `time`, `phase`, `auto` or `auto2`. |

## Results

- **The file**, and the command window says how many rows it holds, or
  where the figure went.
- **In Python** `to_csv()` and `to_mat()` write the same numbers; a
  figure is saved with matplotlib's `savefig`.

## More about

**Exact numbers.** The CSV holds, for each value, the shortest text that
reads back as the same double-precision number. Reading the file gives
back exactly the numbers XPP Lab computed, which is what makes a session
and its Python script comparable to the last bit.

**MATLAB files.** A name ending in `.mat` gives a MATLAB file for
everything that `export` writes as CSV. MATLAB's `load` reads it with no
toolbox: `s = load("run.mat"); plot(s.t, s.V); s.params.iapp`. It is a
Level 5 MAT-file, little-endian and uncompressed, and every column is a
column vector of doubles:

| Export | Variables |
|---|---|
| a run | `t`, then one vector per variable and auxiliary quantity, named as in the model, and the struct `params` with the parameter values the run used |
| `range` | the swept value or values, then the measure (`V_max`, `V_min`, or `lyapunov`), and `params`; a Range that follows equilibria has the columns of the CSV and no `params` |
| `auto` | `branch`, `point`, `label`, `stable`, `periodic`, `two_parameter`, `value`, `value2` (NaN for a one-parameter point), `norm`, `period`, then `V_max` and `V_min` for every variable. The text columns of the CSV (`type`, `parameter`, `parameter2`) are left out. |
| `equilibria`, `nullclines`, `analysis`, `many`, `locus` | the columns of the CSV file |

`export lin.mat analysis` after `linearize` is not available, because the
table has a column of state names: use CSV.

**MATLAB names.** MATLAB names start with a letter and hold only letters,
digits and underscores, up to 63 characters. Each column's name is
changed by these rules, in this order:

1. every character that is not a letter, digit or underscore becomes `_`
   (a non-ASCII character becomes one `_`);
2. if the result does not start with a letter, `x` goes in front (`_u`
   becomes `x_u`, `1x` becomes `x1x`, and an empty name becomes `x`);
3. the name is cut to 63 characters;
4. a MATLAB keyword (`end`, `for`, `if`, `function`, `global`, `while`,
   ...: the names `iskeyword` lists) gets `_` added, so `end` becomes
   `end_`;
5. a name already used earlier in the file gets `_2`, `_3`, ... added until
   it is new, cut further so that the ending fits. With a `params` struct,
   `params` counts as used. The parameters' names are made unique among
   themselves in the same way.

So the column `V max` is `V_max`, and a second `V_max` is `V_max_2`.

**The same bytes.** A MATLAB file holds no date, host or version: its
header text is always `MATLAB 5.0 MAT-file, written by XPP Lab`. The same
data gives the same file every time, and the file the app writes and the
one the session's Python script writes are identical.

## Tips

- Export the run before changing anything, if you will want to compare.
- For a figure in a paper, export SVG: it stays sharp at any size.
- `export FILE.csv analysis` right after an analysis, before running
  another.

## Algorithms

The run is written as stored; MATLAB files are Level 5 .mat files written
by the engine, so the app and Python produce the same bytes.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: there is no run to export` | No run is stored. | `run` first. |
| `error: the bifurcation diagram has no points yet` | `export ... auto` without a diagram. | Compute one with [auto steady](auto-steady.md). |
| `error: there is no Analysis result to export (statistics and fits are shown in the command window)` | `export ... analysis` after `mean` or a fit, or before any analysis. | Run an analysis that makes a table. |
| `error: export a Range that measures a variable or the Liapunov exponent (...)` | `export ... range` after a range that keeps the runs. | Run the range with a `VAR` or `lyap`. |
| `error: a PNG is a picture of the window; without one, export the figure as SVG` | `.png` from the batch tool. | Use `.svg`. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [run](run.md): the run that is written.
- [ic write](ic-write.md): write only the initial values.
- [auto save](auto-save.md): keep a diagram to load again.

## Version history

Introduced in 0.1.0.
