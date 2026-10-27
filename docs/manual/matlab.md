# Coming from MATLAB

XPP Lab looks like MATLAB: a plot window, a workspace of named values, a
command window. Its numerical work is done by XPPAUT 8.0's C engine, and the
Python package `xpplab` gives you that engine as NumPy arrays. This page
maps what you do in MATLAB (`ode45`, `fsolve`, `eig`, `ss`, `ctrb`) to what
you do here, and works one example, the van der Pol oscillator, in both.

## The main differences

- A model is a file of equations (an `.ode` file), not a function handle.
  The file holds the right-hand side, the parameters and the initial values;
  you change them later by name.
- Results are named columns, not matrices of unnamed columns:
  `sol["x"]`, not `y(:,1)`. Names are not case sensitive.
- Python counts from 0 and slices with `a[:5]`, not `a(1:5)`.
- The default integrator is a fixed-step Runge-Kutta, not an adaptive one.
  Choose `5dp` to get the Dormand-Prince pair that `ode45` uses.
- Bifurcation diagrams are part of the engine. They come from AUTO, see
  [Bifurcation analysis with AUTO](auto.md).

## MATLAB and XPP Lab side by side

| MATLAB | XPP Lab (Python) | Notes |
|---|---|---|
| `ode45(f, [0 T], y0)` | `m.set(meth="5dp")`, then `m.run(total=T)` | Returns a table with a `T` column and one per variable. The points are `dt` apart; the adaptive steps are not reported. |
| `odeset('RelTol', r, 'AbsTol', a)` | `m.set(tol=r, atol=a)` | Used by the adaptive methods. |
| `ode15s`, `ode23s` | `m.set(meth="stiff")` or `meth="cvode"` | See [Numerical methods](numerics.md) for all fifteen. |
| `y0`, parameters in a struct | `m.initial_conditions["x"]`, `m.parameters["mu"]` | Dictionaries of names; assignment changes the model. |
| `plot(t, y)` | `sol.plot("x")` | Matplotlib. `sol.to_numpy()` gives the array. |
| `fsolve(@(y) f(0,y), y0)` | `m.equilibrium(guess={"x": 0.1, "y": 0.1})` | Newton's method on the model's own right-hand side. Returns an `Equilibrium`; its `values` hold the point. |
| `eig(A)` | `eq.eigenvalues` or `lin.eigenvalues` | The eigenvalues of the Jacobian at the equilibrium. `eq.kind` names the type, such as "unstable focus". |
| `linearize`, `jacobian` | `m.linearize(inputs=["u"])` | Numerical A and B; B has one column for each parameter named in `inputs`. |
| `ss(A, B, C, D)` | `lin.A`, `lin.B` | NumPy arrays. There is no C or D and no state-space object; pass the arrays to a control library if you need one. |
| `ctrb(A, B)` | `lin.controllability` | The matrix [B, AB, ...]. |
| `rank(ctrb(A, B))` | `lin.rank`, `lin.controllable` | The rank is computed from singular values. |
| Add-on tools for bifurcations (MatCont, COCO) | `m.auto.equilibria("mu")` and `m.auto.continue_from(...)` | AUTO-07p, as in XPPAUT. Returns a `Diagram` with the special points: `d.special("HB")`. |

If you need something that is not in the table (event functions, DAE
solvers such as `ode15i`, `pdepe`), this package does not provide an
equivalent. For delay equations, noise and difference equations, see the
[`.ode` file format](odefile.md).

## Example: the van der Pol oscillator

The equations are

    x' = y
    y' = mu (1 - x^2) y - x + u

with `mu = 1`. The force `u` is zero; it is there so that it can be the input
of the linear system. Save this as `vdp.ode`, or open it with File > Open...:

```ode
# van der Pol oscillator; u is a force that can be used as an input
p mu=1,u=0
x'=y
y'=mu*(1-x^2)*y-x+u
x(0)=2
y(0)=0
@ total=20,xlo=-3,xhi=3,ylo=-3,yhi=3
d
```

In MATLAB you would write:

```matlab
mu = 1;
f = @(t,y) [y(2); mu*(1-y(1)^2)*y(2) - y(1)];
opts = odeset('RelTol', 1e-8, 'AbsTol', 1e-8);
[t, y] = ode45(f, [0 20], [2; 0], opts);
plot(t, y(:,1))

y0 = fsolve(@(y) f(0, y), [0.1; 0.1]);   % the equilibrium
A = [0 1; -1 mu];  B = [0; 1];           % linearised by hand
eig(A)                                   % 0.5 +/- 0.866i
rank(ctrb(A, B))                         % 2
```

With XPP Lab:

```python
import xpplab as xs

m = xs.load("vdp.ode")
m.set(meth="5dp", tol=1e-8, atol=1e-8)
sol = m.run(total=20)
sol["x"][-1]                  # about 2.008: x at t = 20
sol.plot("x")

eq = m.equilibrium(guess={"x": 0.1, "y": 0.1})
eq.values                     # {'X': 0.0, 'Y': 0.0}
eq.kind                       # 'unstable focus'
eq.eigenvalues                # 0.5 +/- 0.866i

lin = m.linearize(inputs=["u"])
lin.A                         # [[0, 1], [-1, 1]]
lin.B                         # [[0], [1]]
lin.rank, lin.controllable    # (2, True)
```

`m.linearize` computes A and B from the model itself, so you do not derive
the Jacobian by hand. The eigenvalues are those of `eq`, because both
linearise at the equilibrium.

For the bifurcation tools, make `mu` run through zero and follow the steady
state:

```python
m.set(mu=-1)
d = m.auto.equilibria("mu", parmin=-1, parmax=2)
hb = d.first("HB")            # a Hopf point
hb.value                      # 0 (to 1e-7): the oscillation appears at mu = 0
```

## The same in the app

Everything above is a button or a command in the app, and every action
prints the Python line it corresponds to in the command window, starting
with `>>>`. Type these commands (open the model first):

```commands
set total=30 meth=5dp
run
eq
linearize at eq inputs u
set mu=-1
auto set parmin=-1 parmax=2
auto steady mu
```

The command window answers with these lines (`xpplab-batch vdp.ode --echo -c ...` prints
the same; the file says `total=20`, so only changed values are written):

```text
>>> m.set(meth="5dp")
>>> m.set(total=30.0, dt=0.05, nout=1.0)
>>> sol = m.run()
>>> eq = m.equilibrium()
>>> lin = m.linearize(inputs=["u"])
>>> m.parameters["mu"] = -1.0
>>> m.auto.set(parmin=-1.0)
>>> m.auto.equilibria("mu")
```

The lines are the same calls as in the script above, with the app's own
defaults written out. Pasted into a script, they give the same numbers.
File > Save session as Python script saves the whole session as such a script.

## Where to go next

- [Tutorial: your first model](tutorial.md), if you have not made a model file.
- [Bifurcation analysis with AUTO](auto.md) for the diagrams.
- [Coming from XPPAUT](xppaut.md), for the names XPPAUT's own users know.
