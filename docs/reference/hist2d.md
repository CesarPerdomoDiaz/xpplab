# hist2d

A two-dimensional histogram of two columns of the last run: the fraction
of the run spent in each cell of a grid.

## Syntax

```
hist2d X Y
hist2d X Y BINS
```

- **In the app:** Run > Analyse the run > **2-D histogram** (XPPAUT's
  *Numerics > stocHastic > 2D-hist*).
- **In Python:** `m.histogram2d("V", "W", bins=10)`, or `bins=(nx, ny)`
  and `range=((xlo, xhi), (ylo, yhi))`.

## Description

`hist2d X Y BINS` divides the range of `X` and the range of `Y` over the
last run into `BINS` equal parts each, and counts the fraction of the
run's stored points in each cell of the grid. The result is a table with a
row per cell: the cell's centre `x`, `y` and the fraction `count`. In the
app it is drawn as a coloured grid.

Without `BINS` the app uses 50 cells across and 20 up at first, and after
that the numbers given last; the across number is shared with
[hist](hist.md). Python's default is 20 by 20.

## Examples

### Where the firing neuron's orbit lingers

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Make it fire for longer](cmd:set iapp=0.1 total=200) and [run](cmd:run).
3. [Take the 2-D histogram of V and W](cmd:hist2d V W 10).

```
set iapp=0.1 total=200
run
hist2d V W 10
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.run(iapp=0.1, total=200)
h = m.histogram2d("V", "W", bins=10)
k = np.argmax(h["count"])
print(h["x"][k], h["y"][k], h["count"][k])
```

The command window says "2-D histogram of V and W: 100 rows." 36 of the 100
cells hold some of the run. The fullest, with 19.3% of the points, is the
cell centred at `V` = −0.284, `W` = 0.0265.

How to read it. The orbit is a closed curve, so most cells are empty and
the full ones trace the loop. The fullest cell is the slow part of the
loop, near rest, where the neuron recovers before the next spike; the
spike itself crosses its cells quickly and leaves few points in each.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `BINS` | the command; `bins=` in Python | The number of cells across and up. | Fewer for a short run, so that cells hold enough points. |
| **The ranges** | `range=` in Python | The region the grid covers. | The two columns' ranges in the run by default. |
| **Total time**, **Transient** | `set total=... trans=...` | Which part of the run is counted. | Leave out the transient for the distribution on an attractor. |

## Results

- **The Analysis window** draws the grid, coloured by the fraction.
- **The table** has the columns `x`, `y` and `count`, one row per cell;
  `export FILE.csv analysis` writes it.
- **In Python** `m.histogram2d()` returns the table.

## More about

**Fractions, not counts.** The `count` column is the fraction of the
stored points in each cell, so it adds up to about 1: here to 0.9995, since
points exactly on the upper edges of the ranges fall outside the grid.

**Invariant densities.** On a chaotic attractor, over a long run, the 2-D
histogram approaches the attractor's density in the plane: where the
trajectory spends its time.

## Tips

- Use few cells for a short run, many for a long one.
- After `hist2d ... BINS`, `hist` uses the same `BINS` until you give
  another.

## Algorithms

XPPAUT's 2D-hist: the cell of each stored point is found from its two
values, and each cell's count is divided by the number of points.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: write hist2d X Y [BINS]` | The second column is missing or unknown. | Name two columns; `whos` lists them. |
| `Need more data and at least 3 columns` | The run is too short. | Run longer. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [hist](hist.md): the histogram of one column.
- [mean](mean.md): the mean and spread of a column.

## Version history

Introduced in 0.1.0.
