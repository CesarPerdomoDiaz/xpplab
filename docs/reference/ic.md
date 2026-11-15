# ic

Set initial values from a formula: one variable, or a whole range of
numbered variables with the index in the formula.

## Syntax

```
ic NAME = FORMULA
ic NAME[FROM..TO] = FORMULA
```

In the range form, `[j]` in the formula stands for the index, and `[j+1]`,
`[j-1]` work too.

- **In the app:** Workspace > **Initial values**, the formula fields
  (XPPAUT's *Initial conds > formUla*).
- **In Python:** `m.ic_formula("u[1..49]", "sin(pi*[j]/50)")`.

## Description

`ic NAME = FORMULA` gives the variable `NAME` the value of `FORMULA` as
its initial value. The formula may use numbers, the model's parameters,
its functions, `pi`, and the other variables, which stand for their
initial values as they were before the command.

For a model with numbered variables, such as the 49 temperatures
`u1` ... `u49` of a rod, `ic u[1..49] = FORMULA` sets them all at once,
`[j]` taking the values 1 to 49 in turn. It is how to give a spatial
model an initial shape.

## Examples

### The smoothest shape on a rod

The heat equation on a rod with its ends held at zero, solved on 49 inner
points, starts with a hot middle. Give it instead the shape sin(πx), the
slowest mode, which should keep its shape and decay as e^(−π²t):

1. [Open the heat equation](example:classics/heat.ode).
2. [Set the shape](cmd:ic u[1..49] = sin(pi*[j]/50)) and [run](cmd:run).

```
ic u[1..49] = sin(pi*[j]/50)
run
```

```python
import numpy as np
import xpplab as xs

m = xs.load(xs.example("classics/heat"))
m.ic_formula("u[1..49]", "sin(pi*[j]/50)")
sol = m.run()
print(m.get("u25"), sol["U25"][-1])
```

The command window says "49 initial values set from sin(pi*[j]/50)."; `get
u25` answers 1 and `get u1` 0.06279051953, sin(π/50). After the run, to
`t` = 0.2, the middle `u25` is 0.1390014.

How to read it. The exact solution of the heat equation decays as
e^(−π² · 0.2) = 0.13891. The model, with 49 points, decays at the rate
4n² sin²(π/2n) = 9.8664 instead of π² = 9.8696 (n = 50), which gives
e^(−9.8664 · 0.2) = 0.1390014: the run agrees with it to seven digits.
The 0.07% between the two is the error of the grid, not of the
integration.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `NAME` or `NAME[FROM..TO]` | the command; the first argument in Python | The variable, or the range of numbered variables. | The range's names must exist: `u1` ... `u49` here. |
| `FORMULA` | the command; the second argument in Python | An expression in numbers, parameters, functions, `pi`, variables and `[j]`. | Write it as in the model file. |

## Results

- **The initial values** are set; the **Initial values** tab shows them,
  and the command window says how many.
- **In Python** `m.ic_formula()` returns the model.

## More about

**Shapes for spatial models.** A model of a rod, a cable or a ring
written with `u[1..N]` is a partial differential equation on a grid
(the method of lines). Its initial shape is a function of the position,
and `[j]` is the position's index: with N + 1 intervals on a length L,
x = j L/(N + 1).

**Variables in a formula.** A variable in the formula is its initial value
before the command: `ic w = v/10` makes `w` a tenth of `v`'s initial value,
whatever the last run or equilibrium left behind.

## Tips

- Check a few values with [get](get.md) before running.
- To start from the end of a run instead, use [ic last](ic-last.md).

## Algorithms

XPPAUT's own expression evaluator computes the formula for each index,
with the model's parameters, functions and initial values.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: no variable called NAME` | The name, or one in the range, is not a variable. | Check the names with `whos`. |
| `error: cannot evaluate FORMULA` | The formula uses an unknown name or is not well formed. | Check it as you would a line of the model. |
| `Write: ic last (...), ..., or ic NAME = FORMULA (...)` | No `=`, or a range without its `]`. | Write `NAME = FORMULA`, `u[1..20]` with both brackets. |

## References

- Strang, G. (2007). *Computational Science and Engineering*.
  Wellesley-Cambridge Press. Chapter 6 (the heat equation).
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [set](set.md): one value at a time.
- [ic last](ic-last.md), [ic read](ic-read.md): other ways to set
  initial values.

## Version history

Introduced in 0.1.0. Variables in a formula are their initial values
since 0.1.4.
