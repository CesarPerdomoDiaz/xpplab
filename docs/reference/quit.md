# quit

Leave the app.

## Syntax

```
quit
```

`exit` is another name for `quit`.

- **In the app:** File > **Quit** (Ctrl+Q; ⌘Q on a Mac), or XPPAUT's keys
  `F Q`.
- **In Python:** `m.close()` releases a model; the script ends as any
  Python script does.

## Description

`quit` closes the app. In a script of commands run by the batch tool, it
ends the script there: the commands after it are not run.

## Examples

### Ending a script early

A script of commands, run by `xpplab-batch`, that stops after the first
part:

```
run
eq
quit
range iapp 0 0.3 31 V
```

The run and the equilibrium are computed; the range is not.

How to read it. `quit` is a clean end: what was computed before it is
written and kept, and the tool exits normally.

## Settings

`quit` has no settings.

## Results

- **The app** closes; **the batch tool** stops running commands.

## More about

**What to save first.** Runs, diagrams and results live in the app's
memory. Export what you need ([export](export.md), [auto save](auto-save.md),
[script](script.md)) before quitting.

## Tips

- Save the session with [script](script.md) before quitting, to be able
  to repeat it.

## Algorithms

The app closes; nothing is computed.

## When it fails

`quit` does not fail.

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [script](script.md), [export](export.md).

## Version history

Introduced in 0.1.0. Ends a batch script since 0.1.4.
