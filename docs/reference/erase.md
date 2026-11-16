# erase

Remove the kept curves and the marked equilibria from the plots.

## Syntax

```
erase
```

`e` is another name for `erase`.

- **In the app:** XPPAUT's key `E`.
- **In Python:** there is nothing to erase: a plot shows only the runs it
  is given.

## Description

`erase` removes from the plots every curve that was kept: by
[freeze](freeze.md), by `click keep`, or by a [range](range.md) sweep that
draws its runs. It also removes the equilibria marked by [eq](eq.md) and
[eq all](eq-all.md), with their manifolds. The current run stays, and so
do the initial values, the parameters and the bifurcation diagram.

## Examples

### Clearing a phase portrait

1. [Open the Morris-Lecar model](example:lecar.ode) and open the **Phase
   Plane** tab.
2. [Keep the runs](cmd:click keep), then
   [start at V = −0.17](cmd:click -0.17 0.0013) and
   [at V = −0.16](cmd:click -0.16 0.0013).
3. [Find the equilibria](cmd:eq all), then [erase](cmd:erase).

```
click keep
click -0.17 0.0013
click -0.16 0.0013
eq all
erase
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
runs = [m.run(V=v, W=0.0013) for v in (-0.17, -0.16)]
found = m.find_equilibria(box={"V": (-0.6, 1.2), "W": (-0.25, 1.2)})
print(len(runs), len(found))
```

Before `erase` the Phase Plane shows the two runs and the three equilibria.
After it, only the last run, from −0.16; the status line says "Erased kept
curves and equilibria."

How to read it. Nothing is computed again or lost: the runs and equilibria
can be found again in the same way, and in Python they are still in `runs`
and `found`.

## Settings

`erase` has no settings.

## Results

- **The plots** show only the current run, and the nullclines and the
  direction field if they are on.
- **The status line** says what was erased.

## More about

**What is kept, and where.** Curves on the plots come from three places:
the current run, which the next run replaces; the kept curves, which stay
until `erase`; and frozen nullclines, which `nullclines unfreeze` removes.
`erase` clears only the second, and the marked equilibria.

## Tips

- Erase before a new phase portrait, so that old curves do not mix with
  new ones.
- To remove frozen nullclines too, use `nullclines unfreeze`.

## Algorithms

`erase` deletes the kept curves and equilibria from the app's memory; it
computes nothing.

## When it fails

`erase` does not fail: with nothing kept, it does nothing.

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [freeze](freeze.md) and [click](click.md): what keeps curves.
- [eq all](eq-all.md): what marks equilibria.

## Version history

Introduced in 0.1.0.
