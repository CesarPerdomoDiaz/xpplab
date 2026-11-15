# get

Show the value of a parameter, a variable's initial value or a numerical
option.

## Syntax

```
get NAME
```

- **In the app:** the Workspace shows the same values in its tabs.
- **In Python:** `m.get("iapp")`.

## Description

`get NAME` prints `NAME = VALUE` in the command window: for a parameter
its value, for a variable its initial value, for a numerical option such
as `total`, `dt`, `t0` or `maxstor` its setting. For `meth` it prints the
method's name.

Names ignore case. Values are printed with ten significant digits.

## Examples

### Reading back what was set

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Change three values](cmd:set iapp=0.1 total=100 meth=stiff), then read
   them with `get iapp`, `get total` and `get meth`.

```
set iapp=0.1 total=100 meth=stiff
get iapp
get total
get meth
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
m.set(iapp=0.1, total=100, meth="stiff")
print(m.get("iapp"), m.get("total"), m.get("meth"))
```

The command window answers `iapp = 0.1`, `total = 100` and
`meth = stiff`; Python gives 0.1, 100.0 and `"stiff"`.

How to read it. `get` reads the engine's value, the one the next
computation will use, so it shows what a `set`, a `use` or an `ic last`
really did.

### An initial value after a run

In a freshly opened Morris-Lecar model:

```
set iapp=0.1
run
ic last
get v
```

```python
n = xs.load(xs.example("lecar.ode"))
n.set(iapp=0.1)
n.run()
n.ic_last()
print(n.get("V"))
```

The app answers `v = -0.2384222746`; Python prints −0.23842227676.

How to read it. After `ic last` the initial value is the end of the run.
The app takes it from the stored run, in single precision; Python from
the integrator, in full precision. See [ic last](ic-last.md).

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `NAME` | the command; the name in Python | A parameter, a variable or a numerical option. | `whos` lists the parameters and variables. |

## Results

- **The command window** prints the value.
- **In Python** `m.get()` returns a number, or the method's name for
  `meth`.

## More about

**Options.** Besides `total`, `dt`, `t0`, `trans`, `bound`, `maxstor`
and `meth`, the options include `tol`, `atol`, `dtmin`, `dtmax`, `nout`,
`newt_tol`, `newt_iter` and `jac_eps`: Python's `m.options` lists them
all with their values.

## Tips

- After an action that changes values for you (`use`, `ic last`,
  `fitdata`, `bvp`), `get` shows the result.

## Algorithms

`get` reads the value from the engine; nothing is computed.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `error: no value named NAME` | No parameter, variable or option has that name. | Check the name with `whos`. |

## References

- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [set](set.md): change a value.
- [whos](whos.md): every parameter and variable at once.

## Version history

Introduced in 0.1.0. `get meth` names the method since 0.1.4.
