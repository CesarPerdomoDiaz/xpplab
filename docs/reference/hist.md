# hist

A histogram of a column of the last run: how much of the run is spent at
each value.

## Syntax

```
hist COLUMN
hist COLUMN BINS
hist COLUMN BINS LO HI
```

- **In the app:** Run > Analyse the run > **Histogram** (XPPAUT's
  *Numerics > stocHastic > Histogram*), where a condition can also be set.
- **In Python:** `m.histogram("V", bins=10)`, with `range=(lo, hi)` and
  `condition="W>0.2"`.

## Description

`hist COLUMN BINS` divides the range of `COLUMN` into `BINS` equal bins and
counts the stored points of the last run in each. Without `BINS` the app
uses the number given last, 50 at first; Python's `bins` is 100 unless
given. Without
`LO HI` the range is the column's smallest to largest value in the run.

Since the points are stored at equal times, the count in a bin is
proportional to the time the run spends there.

The result is a table with a row per bin: its left edge and its count, and
one more row at `HI` (XPPAUT's convention). With the column's own range
that last row holds just the largest value. In the app it is drawn as bars in the Analysis window.

A **condition**, in the app's Histogram tool or in Python, counts only the
points where an expression of the model's quantities is true, such as
`W>0.2`.

## Examples

### Where a firing neuron spends its time

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Make it fire for longer](cmd:set iapp=0.1 total=200) and [run](cmd:run).
3. [Take the histogram of V in 10 bins](cmd:hist V 10).

```
set iapp=0.1 total=200
run
hist V 10
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.run(iapp=0.1, total=200)
h = m.histogram("V", bins=10)
print(h["bin"], h["count"])
```

The command window says "Histogram of V: 11 rows." The range is −0.3804
to 0.2615, so the bins are 0.0642 wide. The counts, from the lowest bin to
the highest, are 1020, 858, 750, 349, 173, 138, 144, 142, 151, 275, and 1
point at the top.

How to read it. Of the 4001 points, 1020, a quarter, lie in the lowest
bin, between −0.380 and −0.316: the neuron spends most of its time slowly
recovering near rest. The middle bins hold about 140 points each: the
upstroke and the downstroke of a spike are fast. The 275 in the top bin
are the peaks, where `V` turns.

### Counting only part of the run

Count only the points where the recovery variable is high:

```python
w = m.histogram("V", bins=10, condition="W>0.2")
print(w["count"].sum())
```

857 of the 4001 points have `W` > 0.2: a fifth of the run.

How to read it. A condition picks a part of the run by a property other
than the column itself, such as one phase of an oscillation, and shows
the distribution of the column there.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `BINS` | the command; `bins=` in Python | The number of bins. | 50 at first in the app, then the last number given; 100 in Python. Fewer for a short run, so that each bin holds enough points. |
| `LO`, `HI` | the command; `range=` in Python | The range divided into bins. | The column's range by default. Give it to compare runs on the same bins. |
| **Condition** | the Histogram tool; `condition=` in Python | Only points where this is true count. | Any expression in the model's quantities, such as `W>0.2`. |
| **Total time**, **Transient** | `set total=... trans=...` | Which part of the run is counted. | Leave out the transient for the distribution on an attractor. |

## Results

- **The Analysis window** draws the bars.
- **The table** has the columns `bin` (the left edge) and `count`, with
  `BINS` + 1 rows; `export FILE.csv analysis` writes it.
- **In Python** `m.histogram()` returns the table, with `h["bin"]` and
  `h["count"]`.

## More about

**Histograms and time.** For a run stored at equal times, a histogram is
an estimate of the fraction of time spent at each value. On a chaotic
attractor, over a long run, it approaches the attractor's invariant
density; on a limit cycle it is large where the motion is slow.

**The edges.** A bin covers its left edge but not its right edge, so the
largest value would fall outside the last one; XPPAUT gives it a row of
its own at `HI`, which counts the points from `HI` up to one bin width
above it. `numpy.histogram` puts the largest value in the last bin
instead, so its last count is the sum of XPPAUT's last two (276 here).

**A given range.** XPPAUT finds a point's bin by cutting off the fraction
of (value − LO) / width, so a point less than one bin width below `LO`
also counts in the first bin, and one less than a width above `HI` in the
last row. With the column's own range this never happens; with a range of
your own, make it a little wider than the values you want to count.

## Tips

- For a noisy model, run long and use many bins; for a short run, few.
- To compare two runs, give both the same `LO HI`.

## Algorithms

XPPAUT's histogram: the width is (HI − LO) / BINS, the row of a point is
(value − LO) / width with its fraction cut off, and a point is counted if
that row is between 0 and BINS and the condition, if any, is true there.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: give a column, e.g. hist V` | No column was named. | Name one; `whos` lists them. |
| A single tall bar | The column hardly varies over the run. | Narrow the range with `LO HI`, or check that the run is the one you meant. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [mean](mean.md): the mean and spread of a column.
- [run](run.md): the run that is counted.

## Version history

Introduced in 0.1.0.
