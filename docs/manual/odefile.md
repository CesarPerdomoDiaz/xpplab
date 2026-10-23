# The .ode file format

A model is a plain text file, usually ending in `.ode`. XPP Studio reads
exactly the format of XPPAUT 8.0, so files written for XPPAUT work
unchanged. Each statement goes on its own line, and the file ends with
`done`. Upper and lower case are the same in names.

## A complete example

```
# Lotka-Volterra predator-prey model
x' = a*x - b*x*y
y' = -c*y + d*x*y
par a=1, b=0.5, c=0.75, d=0.25
init x=2, y=1
aux total = x + y
@ total=50, dt=0.02, xp=x, yp=y, xlo=0, xhi=8, ylo=0, yhi=6
done
```

## Equations

| Form | Meaning |
|---|---|
| `x' = f` or `dx/dt = f` | A differential equation. |
| `x(t+1) = f` | A map (difference equation); use method `discrete`. |
| `x(t) = f` | A Volterra integral equation (`f` may contain `int{...}`). |
| `0 = f` | An algebraic constraint (with `solv` variables: a DAE). |
| `s = expression` | A fixed (intermediate) quantity, computed at each step. |
| `aux name = expression` | An auxiliary output: plotted and saved, but not integrated. |

## Declarations

| Statement | Meaning |
|---|---|
| `par a=1, b=2` | Parameters. |
| `init x=0.1, y=0` | Initial values (`x(0)=0.1` also works). |
| `number pi2=6.2832` | A constant that cannot be changed while running. |
| `f(u,v) = u^2 - v` | A user function, with up to 9 arguments. |
| `!d = 2*a` | A derived parameter, recomputed when parameters change. |
| `table g g.tab` | A function tabulated in a file. |
| `wiener w` | A Wiener process for noise; use `euler` or `backeul`. |
| `markov z 2` | A Markov chain variable with 2 states (transition table follows). |
| `global 1 x-1 {x=0}` | An event: when `x-1` crosses zero upwards, reset `x=0`. |
| `bdry x-1` | A boundary condition for boundary-value problems. |
| `set name {a=2, total=50}` | A named parameter set. |
| `#include file.inc` | Insert another file. |

Arrays save typing: `x[1..10]' = -x[j] + x[j-1]` defines ten equations,
with `j` running from 1 to 10.

## Expressions

Arithmetic: `+ - * / ^` and parentheses. Comparisons and logic
(`< > <= >= == != & |`) give 1 or 0, so `(x>0)*x` is max(x, 0).

Functions: `sin cos tan asin acos atan atan2 sinh cosh tanh exp ln log
log10 sqrt abs heav sign mod max min flr ran normal besselj bessely erf erfc
hom_bcs`, and `if(cond)then(a)else(b)`.

Special terms: `t` is time. `delay(x, tau)` or `x(t-tau)` is a delayed
value. `pi` is 3.14159...

## Options (@ lines)

Options set how the model runs. They can be combined on one line:
`@ total=100, dt=0.05, meth=stiff`.

| Option | Meaning |
|---|---|
| `total`, `dt`, `t0`, `trans` | Integration time, step, start time, transient to discard. |
| `meth` | Method: `rungekutta`, `euler`, `modeuler`, `adams`, `gear`, `stiff`, `cvode`, `5dp`, `83dp`, `2rb`, `backeul`, `qualrk`, `volterra`, `ymp`, `discrete`. |
| `tol`, `atol` | Relative and absolute tolerances of adaptive methods. |
| `dtmin`, `dtmax` | Limits on the adaptive step. |
| `bound` | Stop if a variable exceeds this size. |
| `nout` | Store every n-th step. |
| `maxstor` | Maximum number of stored points. |
| `delay` | Maximum delay, for delay equations. |
| `xp`, `yp`, `xlo`, `xhi`, `ylo`, `yhi` | The phase-plane variables and window. |
| `nmesh` | Grid for nullclines. |
| `poimap`, `poivar`, `poipln`, `poisgn` | Poincare sections. |
| `ds`, `dsmin`, `dsmax`, `parmin`, `parmax`, `nmax`, `npr`, `ntst` | AUTO settings. |

## Comments

Lines starting with `#` are comments. The first comment lines of a file are
shown as its description in the Models window, so describe your model there.
Lines starting with `"` are XPPAUT's interactive tutorial text.
