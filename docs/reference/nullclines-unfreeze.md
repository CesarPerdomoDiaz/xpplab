# nullclines unfreeze

Remove the frozen nullclines from the Phase Plane.

## Syntax

```
nullclines unfreeze
```

- **In the app:** Phase Plane > **Freeze...** > **Delete them**.
- **In Python:** forget the list: `frozen = []`.

## Description

`nullclines unfreeze` deletes every set of nullclines kept by
[nullclines freeze](nullclines-freeze.md). The live nullclines, the runs
and the kept curves stay.

## Examples

### Clearing a family

1. [Open the Morris-Lecar model](example:lecar.ode) and open the **Phase
   Plane** tab.
2. [Freeze five currents](cmd:nullclines freeze iapp 0 0.1 5), then
   [remove them](cmd:nullclines unfreeze).

```
nullclines freeze iapp 0 0.1 5
nullclines unfreeze
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
frozen = m.nullcline_sweep("iapp", [0, 0.025, 0.05, 0.075, 0.1], "V", "W",
                           (-0.6, 1.2), (-0.25, 1.2))
frozen = []
```

After `nullclines unfreeze` only the live nullclines remain, for the
present `iapp`, 0.05.

How to read it. The frozen sets were copies; removing them changes nothing
in the model.

## Settings

`nullclines unfreeze` has no settings.

## Results

- **The Phase Plane** shows only the live nullclines.

## More about

**What else clears the plots.** [erase](erase.md) removes the kept runs
and the marked equilibria, but not the frozen nullclines; `nullclines
unfreeze` removes only those. To start afresh, use both.

## Tips

- Unfreeze before freezing a new family, so that two families do not mix.

## Algorithms

The frozen sets are deleted; nothing is computed.

## When it fails

`nullclines unfreeze` does not fail: with nothing frozen, it does nothing.

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [nullclines freeze](nullclines-freeze.md): keep nullclines.
- [erase](erase.md): remove kept runs.

## Version history

Introduced in 0.1.0.
