# new

Start a new model from a template, ready to edit.

## Syntax

```
new
```

- **In the app:** File > **New model** (Ctrl+N; ⌘N on a Mac).
- **In Python:** `m = xs.Model.from_text("...")`, with the model's text.

## Description

`new` opens a small working model, the FitzHugh-Nagumo neuron, as
`untitled.ode`, and shows it in the editor. Change its equations into your
own, apply them with **Apply** (Ctrl+Enter) or [save](save.md), and save it
under a name of yours. Starting from a model
that already runs saves looking up the format.

## Examples

### The template

1. Start a new model with `new`, and list it with `whos`.

```
new
whos
```

```python
import xpplab as xs

m = xs.Model.from_text("""
v' = v - v^3/3 - w + I
w' = eps*(v + a - b*w)
par I=0.5, a=0.7, b=0.8, eps=0.08
init v=-1, w=1
@ total=200, dt=0.05, xp=v, yp=w
@ xlo=-2.5, xhi=2.5, ylo=-1, yhi=2.5
done
""")
print(m.variables, dict(m.parameters))
```

The command window answers:

```
Loaded untitled.ode: 2 variables, 4 parameters.
Variables (initial value): V=-1 W=1
Parameters: I=0.5 a=0.7 b=0.8 eps=0.08
Output columns: 3. Method rungekutta, total 200, dt 0.05.
```

How to read it. The template has every part a model needs: equations,
parameters, initial values, options and `done`. The Python block writes
the same model as text.

## Settings

`new` has no settings.

## Results

- **The model** `untitled.ode` is open and in the editor.
- **In Python** `xs.Model.from_text()` loads a model from its text.

## More about

**Where it is kept.** Until saved under another name, `untitled.ode`
lives in the app's own settings folder; [save](save.md) without a name
writes it there.

## Tips

- Save the new model under its own name, in its own folder, before
  building on it.
- [The ODE file](../manual/odefile.md) lists everything a model file can
  hold.

## Algorithms

The template is written and opened as any model; nothing is computed
beyond its first run.

## When it fails

`new` does not fail.

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [save](save.md): save it under a name.
- [open](open.md): open an existing model.

## Version history

Introduced in 0.1.0.
