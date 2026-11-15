# clear

Clear the command window.

## Syntax

```
clear
```

`clc` is another name for `clear`.

- **In the app:** the command window.
- **In Python:** nothing to clear; the console's own commands do it.

## Description

`clear` empties the command window: the commands typed and their
answers. Nothing else changes: the model, its runs and results, and the
record that [script](script.md) saves stay.

## Examples

### A fresh page

1. [Open the Morris-Lecar model](example:lecar.ode),
   [list it](cmd:whos), then [clear the window](cmd:clear).

```
whos
clear
```

The command window is empty again.

How to read it. Only what is shown goes; the record of the session for
[script](script.md) is kept, so nothing done is lost.

## Settings

`clear` has no settings.

## Results

- **The command window** is empty.

## More about

**`clc` and `clear`.** Both names come from MATLAB, where `clc` clears the
command window; in XPP Lab `clear` does the same and removes no values.

## Tips

- Clear the window before a long command whose output you want to read
  on its own.

## Algorithms

The command window's text is deleted; nothing is computed.

## When it fails

`clear` does not fail.

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [script](script.md): the session's record, which `clear` keeps.

## Version history

Introduced in 0.1.0.
