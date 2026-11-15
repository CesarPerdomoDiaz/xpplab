# ic write

Write the initial values to a text file, one per line.

## Syntax

```
ic write FILE
```

- **In the app:** Workspace > **Initial values** > **Save to file...**.
- **In Python:** `m.write_ics("state.txt")`.

## Description

`ic write FILE` writes the model's present initial values to `FILE`, one
number per line, in the order of the variables (the order `whos` lists
them). The numbers are written exactly: [ic read](ic-read.md) reads them
back to the last digit.

Use it to keep a state worth coming back to: a point on an attractor, the
end of a long transient, the starting point of a figure.

## Examples

### Keep a state on the oscillation

The Morris-Lecar model with `iapp` = 0.1 fires repetitively. Run it, take
the end of the run as the initial values, and write them out:

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Set the current](cmd:set iapp=0.1), [run](cmd:run) and
   [start from the end](cmd:ic last).
3. Write the initial values with `ic write firing.txt`.

```
set iapp=0.1
run
ic last
ic write firing.txt
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.set(iapp=0.1)
m.run()
m.ic_last()
path = m.write_ics("firing.txt")
print(path.read_text())
```

The file has two lines, `V` and then `W`. From the app:

```
-0.23842227458953857
0.007057134062051773
```

How to read it. These are the values at the end of the run, as the run
stores them, written with every digit needed to read them back exactly.
From Python the numbers differ in the ninth digit (−0.23842227676...),
because Python's `ic_last()` takes the state from the integrator in full
precision; see [ic last](ic-last.md).

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `FILE` | the command; the path in Python | The file to write. It is replaced if it exists. | In the app a relative name is relative to the model's folder; in Python, to the working folder. |

## Results

- **The file** holds one number per line, nothing else.
- **The command window** says "Initial values written to FILE."
- **In Python** `m.write_ics()` returns the path, as a `pathlib.Path`.

## More about

**Exact numbers.** Each value is written with the fewest digits that read
back as the same double-precision number, so a write followed by a read
changes nothing. Seventeen significant digits always suffice; most values
need fewer.

**The order.** The lines follow the order of the variables in the model
file. A file written for one model fits another model only if it has the
same variables in the same order.

## Tips

- Write the initial values before experimenting with `ic last` or
  `ic NAME = FORMULA`, so that you can go back with `ic read`.
- The file is plain text: you can edit it, or write one by hand.

## Algorithms

The values are written as text with the shortest exact representation of
each number; nothing is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: cannot write FILE` | The folder does not exist, or it cannot be written. | Choose another place. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [ic read](ic-read.md): read the file back.
- [ic last](ic-last.md): take the end of the last run as the initial
  values.
- [whos](whos.md): the order of the variables.

## Version history

Introduced in 0.1.0.
