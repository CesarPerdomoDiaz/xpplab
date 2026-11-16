# save

Save the model in the editor and open it again, so that the changes take
effect.

## Syntax

```
save
save FILE.ode
```

- **In the app:** File > **Save** (Ctrl+S; ⌘S on a Mac), **Save as...**,
  or the editor's **Apply** (Ctrl+Enter).
- **In Python:** write the text to a file and `xs.load()` it.

## Description

The editor holds the model's text. `save` writes it to the model's file
and opens it again: the new equations, parameters and options are then
the model's. `save FILE.ode` writes it to another file and opens that
one, which becomes the model from then on.

## Examples

### Saving the template under a name

1. Start a new model with `new`, then save it with `save`.

```
new
save
```

```python
import xpplab as xs
from pathlib import Path

text = """v'=v-v^3/3-w+I
w'=eps*(v+a-b*w)
par I=0.5,a=0.7,b=0.8,eps=0.08
init v=-1,w=1
@ total=200,dt=0.05
done
"""
Path("fhn.ode").write_text(text)
m = xs.load("fhn.ode")
print(m.variables)
```

The command window answers "Saved .../untitled.ode" with the full path
of the app's settings folder, then "Reloaded untitled.ode."

How to read it. Saving and opening again is how an edit becomes the
model: the engine reads the file anew. In Python the same is done by
writing the file and loading it.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `FILE.ode` | the command; Save as... | Where to save. | Without it, the model's own file. |

## Results

- **The file** is written and the model opened again from it.
- **The command window** says where it was saved.

## More about

**What is lost.** Opening the model again starts it afresh: runs, kept
curves, equilibria and diagrams go, and parameters return to the values
in the file. Save before a long analysis, not in the middle of one.

## Tips

- Use Save as... to keep the original and try a variant.
- If the file has a mistake, the error names the line; the editor marks it.

## Algorithms

The editor's text is written to the file, which is then opened as by
[open](open.md).

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: cannot write FILE` | The folder cannot be written. | Save as... somewhere else. |
| An error with a line number | The edited text has a mistake. | Fix the line in the editor and save again. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [new](new.md), [open](open.md).

## Version history

Introduced in 0.1.0.
