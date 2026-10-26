# Chemistry: kinetics, bistability and chemical clocks

Chemical kinetics gives differential equations directly: the rate of change
of a concentration is the sum of the rates of the reactions that make it,
minus those that consume it. The law of mass action says that a reaction
step proceeds at a rate proportional to the product of the concentrations of
its reactants. Mass action turns reaction schemes into polynomial ODEs, and
polynomial ODEs can have almost any behaviour: relaxation to equilibrium,
switching between two steady states, and sustained oscillations.

This page has four examples:

1. **consecutive reactions** `A -> B -> C`, which have an exact solution to
   check the numerics against;
2. an **enzyme reaction** (Michaelis-Menten), where a fast and a slow time
   scale give a reduced model;
3. a **bistable reaction** (Schlögl's model), with two folds and
   hysteresis;
4. the **Brusselator**, a chemical oscillator, born at a Hopf bifurcation.

Save each model under the name in its first line, in the folder where you
run the Python. The snippets of a page run one after the other, in a single
script. The ideas used (equilibria, eigenvalues, Hopf and fold
bifurcations) are summarised at the start of the
[neuroscience page](neuroscience.md#theory-in-five-paragraphs).

## Theory for reactions

**Mass action.** For the step `A + B -> C` with rate constant `k`, the rate
is `k [A][B]`; the concentrations of `A` and `B` fall at that rate and `C`
rises at it. A step with two molecules of `A` (`2A -> ...`) has the rate
`k [A]^2`. Writing each step this way gives the rate equations.

**Conservation laws.** Most reaction networks conserve something: the total
amount of an element, or the total enzyme, whether free or bound. Each
conservation law is a linear combination of concentrations that does not
change, so it removes one equation. Checking that the numerical solution
keeps the conserved quantity constant is the best test of a simulation of
kinetics.

**Time scales and stiffness.** The rate constants of a network may differ
by many orders of magnitude. Fast steps relax in a short time, after which
the slow ones go on alone. The equations are then **stiff**, and a method
made for them (`meth=stiff` in `.ode` files, XPPAUT's implicit solver) is
much faster than the default. The fast steps can often be replaced by an
algebraic relation: the **quasi-steady-state approximation**.

**Open systems.** A closed system relaxes to chemical equilibrium and
cannot oscillate for ever. Oscillations and switches need a flow of matter
through the system, and in the models the flow appears as constant feed
terms: concentrations of species held fixed by a reservoir.

## Example 1: consecutive reactions, A to B to C

An intermediate `B` is made from `A` and consumed to `C`, both by first
order steps:

```ode
# abc.ode
a' = -k1*a
b' = k1*a - k2*b
c' = k2*b
aux total = a + b + c
par k1=1, k2=0.3
init a=1, b=0, c=0
@ total=15, dt=0.01
done
```

The first equation decouples: `a(t) = exp(-k1 t)`. Then
`b(t) = k1/(k2 - k1) (exp(-k1 t) - exp(-k2 t))`, and `c = 1 - a - b`. The
intermediate rises, reaches a maximum and falls, at the time

```
t_max = ln(k1/k2) / (k1 - k2)
```

which is 1.72 for `k1 = 1`, `k2 = 0.3`, where `b` reaches 0.5969. The
`aux total` line adds `a + b + c` to the output so you can check that it
stays at 1.

```python
import numpy as np
import xpplab as xs

abc = xs.load("abc.ode")
sol = abc.run()
i = sol["b"].argmax()
sol.t[i], sol["b"][i]                  # 1.72, 0.5969
sol["total"].min(), sol["total"].max() # 1.0, 1.0 to 7 digits

exact_b = (1 / (0.3 - 1)) * (np.exp(-sol.t) - np.exp(-0.3 * sol.t))
abs(sol["b"] - exact_b).max()          # below 1e-6
```

!!! note "Exercises"
    1. Check the formula for `t_max` by setting `b' = 0`. What happens when
       `k1` and `k2` are equal? Find `t_max` for `k1 = k2 = 1` from the run.
    2. Make the second step much faster than the first (`k2 = 30`). The
       intermediate never accumulates. What is `b` compared with
       `(k1/k2) a`? This is the **steady-state approximation** for `B`.
    3. Reverse the roles (`k1 = 30`, `k2 = 1`). How long before `a` is
       gone? What limits the rate of formation of `c`?

??? success "Solutions"
    1. At the maximum `b' = 0`, so `k1 a = k2 b`. Solving with the exact
       `a` and `b` gives `t_max = ln(k1/k2)/(k1 - k2)`. For `k1 = k2 = k`
       the formula is 0/0, but then `b = k t exp(-k t)`, whose maximum is
       at `t = 1/k`: `t_max = 1` for `k1 = k2 = 1`.
    2. `b` stays near `(k1/k2) a`: with `k2 = 30`, `b` is about `a/30`.
       After a very short transient, `b' ~ 0` and `b` is "slaved" to `a`.
       The conversion to `c` then happens at the rate of the first step.
    3. `a` is down to 1% in `ln(100)/30 = 0.15` time units. The formation
       of `c` is then limited by the second, slow step: `c` approaches 1 as
       `1 - exp(-k2 t)`, with only a short delay.

## Example 2: an enzyme, and two time scales

An enzyme `E` binds a substrate `S` reversibly to form a complex `C`, and
the complex releases the product `P` and the enzyme:

```
E + S <-> C -> E + P         rate constants  k1 (forward), km1 (back), k2
```

With the free enzyme eliminated by the conservation law `e = e0 - c`:

```ode
# mm.ode
s' = -k1*e*s + km1*c
c' = k1*e*s - (km1 + k2)*c
p' = k2*c
e = e0 - c
km = (km1 + k2)/k1
aux qssa = e0*s/(km + s)
par k1=10, km1=1, k2=1, e0=0.1
init s=1, c=0, p=0
@ total=60, dt=0.01, meth=stiff
done
```

The substrate is converted to product, one molecule at a time, so
`s + c + p = 1` all the time. With little enzyme compared with substrate,
`c` is small and changes slowly once it has built up. Setting `c' = 0` gives
the **quasi-steady-state approximation**

```
c = e0 s / (Km + s),    Km = (km1 + k2)/k1
```

and the product forms at the rate `p' = k2 c = Vmax s/(Km + s)`, with
`Vmax = k2 e0`. This is the Michaelis-Menten law: linear in `s` when
`s << Km`, constant at `Vmax` when `s >> Km`. In this model `Km = 0.2`.

The reduced model has one equation. The approximation is good when the
enzyme is a small fraction of the substrate and `Km`, `e0/(Km + s0) << 1`.

```ode
# mmreduced.ode
s' = -k2*e0*s/(km + s)
km = (km1 + k2)/k1
par k1=10, km1=1, k2=1, e0=0.1
init s=1
@ total=60, dt=0.01, meth=stiff
done
```

```python
mm = xs.load("mm.ode")
red = xs.load("mmreduced.ode")

full = mm.run()
total = full["s"] + full["c"] + full["p"]
total.max() - total.min()                    # about 1e-7: mass is conserved
i = full["c"].argmax()
full.t[i], full["c"][i]                      # 0.58, 0.0815: the complex builds up fast

errors = {}
for e0 in (0.01, 0.05, 0.1, 0.5):
    mm.parameters["e0"] = e0
    red.parameters["e0"] = e0
    a, b = mm.run(), red.run()
    errors[e0] = abs(a["p"] - (1 - b["s"])).max()
# 0.0015, 0.013, 0.027, 0.14
```

The error of the reduced model, as a fraction of the 1 unit of substrate,
grows with `e0`, in step with the small parameter `e0/(Km + s0)`: the
approximation is excellent for `e0 = 0.01` and poor at `e0 = 0.5`, where the
enzyme is a large fraction of the substrate and binding the substrate takes
a visible part of it.

!!! note "Exercises"
    1. Run `mm.ode` with `k1 = 1000, km1 = 100, k2 = 100`, first with
       `meth=stiff` and then with `meth=rungekutta`, both with `dt=0.01`.
       What does each do? Is the same true for `k1 = 10`?
    2. In the reduced model, `red.rhs({"s": s0})` gives `s'` at the
       substrate `s0`. Tabulate it for `s0` from 0.05 to 5. What is `Vmax`?
       At what `s0` is the rate half of it, and at what `s0` is it 90%?
    3. Add a competitive inhibitor: write `E + I <-> EI` as an extra pair of
       equations and conservation law, and show that it raises the apparent
       `Km` but not `Vmax`.

??? success "Solutions"
    1. The stiff run finishes (6001 points, product 1.0 at the end). The
       Runge-Kutta run fails at once: with `k1 = 1000` the fastest rate is
       about `k1 s = 1000`, and a step of 0.01 is ten times too long for
       the method to be stable. With `k1 = 10`, `meth=rungekutta` is fine.
    2. The rate is `-Vmax s0/(Km + s0)` with `Vmax = k2 e0 = 0.1`: 0.0833
       at `s0 = 1` and 0.0962 at `s0 = 5`. Half of `Vmax` is at `s0 = Km`
       = 0.2; 90% is at `s0 = 9 Km = 1.8`.
    3. With the inhibitor binding at rate `ki` and releasing at `kmi`, and
       the conservation law `e = e0 - c - ci`, the quasi-steady state gives
       the same `Vmax`, and `Km` multiplied by `(1 + i/Ki)`, with
       `Ki = kmi/ki`. At high enough `s` the inhibitor is outcompeted.

## Example 3: a chemical switch (Schlögl's model)

Autocatalytic steps such as `A + 2X <-> 3X` (the product catalyses its own
formation) produce cubic rate laws. With a reservoir that feeds and drains
`X`, the rate equation takes the form

```
x' = mu - f(x),     f(x) = x^3 - 3 x^2 + c x
```

where `mu` is a constant net feed. The model below uses `c = 2`.

```ode
# schlogl.ode
x' = mu - (x^3 - 3*x^2 + 2*x)
par mu=0
init x=0
@ total=30, dt=0.01, xlo=0, xhi=30, ylo=-0.5, yhi=2.5
done
```

The equilibria solve `f(x) = mu`. The cubic `f` rises, falls and rises
again; it has a local maximum at `x = 1 - 1/sqrt(3) = 0.4226` and a
local minimum at `x = 1 + 1/sqrt(3) = 1.5774`, where its values are
`+0.3849` and `-0.3849` (that is, `2/(3 sqrt(3))`). So for `mu` between
`-0.3849` and `+0.3849` there are three equilibria: the outer two are
stable and the middle one is unstable; outside that range, there is one.
The two values of `mu` where an equilibrium appears or disappears are
saddle-node (fold) bifurcations.

```python
sch = xs.load("schlogl.ode")
sch.parameters["mu"] = 0.0
found = sch.find_equilibria(50, box={"x": (-1, 3)})
[(round(e["x"], 3), e.kind) for e in found]
# 0 (stable), 1 (unstable), 2 (stable)

d = sch.auto.equilibria("mu", parmin=-1, parmax=1)    # from x = 0, on the lower branch
[round(p.value, 4) for p in d.special("LP")]     # [0.3849, -0.3849]
```

**Hysteresis.** Change `mu` slowly, up and then down, always starting each
run where the last one ended (`continue_runs=True`):

```python
sch.set(total=60)
mus = np.linspace(-0.6, 0.6, 13)
up = sch.sweep("mu", mus, measure=lambda s: s["x"][-1], continue_runs=True)
down = sch.sweep("mu", mus[::-1], measure=lambda s: s["x"][-1],
                 continue_runs=True)[::-1]
```

Going up, `x` follows the lower branch until `mu` passes 0.3849 and then
jumps to the upper branch (`up` is 0.21 at `mu = 0.3` and 2.16 at 0.4).
Coming down, it stays on the upper branch until `mu` is below -0.3849
(`down` is 1.79 at -0.3 and -0.16 at -0.4). Between -0.3849 and 0.3849 the
state depends on the history: the system is a **memory**, a one-bit chemical
switch. Bistable switches of this kind are found in cells, for example in gene regulation.

!!! note "Exercises"
    1. Check the fold values: solve `f'(x) = 0` and evaluate `f`. Then
       change the `2` in the model's cubic to `3`. How many equilibria are
       there for `mu = 0`, and what happens to the folds?
    2. With `mu = 0.2`, where does the trajectory go from `x = 0.5`? From
       `x = 1.3`? Find the unstable equilibrium that separates them.
    3. For a general coefficient `c` the folds are at `x = 1 ± sqrt(1 - c/3)`.
       Find the value of `c` at which they merge, and the `mu` there.

??? success "Solutions"
    1. `f'(x) = 3x^2 - 6x + 2 = 0` gives `x = 1 ± 1/sqrt(3)`, and `f` there
       is `±0.3849`. With a 3 instead of the 2, `f'(x) = 3(x - 1)^2 >= 0`:
       `f` is monotonic, so there is exactly one equilibrium for every
       `mu`, and the two folds have merged into one point.
    2. With `mu = 0.2` there are three equilibria, at 0.1211 and 2.088
       (both stable) and 0.7909 (unstable). From `x = 0.5` the trajectory
       goes down to 0.1211; from 1.3 it goes up to 2.088. The starting
       point 0.7 also goes to 0.1211, because it is below 0.7909.
    3. The folds coincide when the square root vanishes: `c = 3`, at
       `x = 1`. Then `mu = f(1) = 1 - 3 + 3 = 1`. The point `(c, mu) =
       (3, 1)` is a **cusp**: for `c < 3` there is a range of `mu` with
       bistability, for `c > 3` there is none.

## Example 4: the Brusselator, a chemical clock

The Brusselator (Prigogine and Lefever) is the simplest model of an
oscillating chemical reaction that can be written with mass action. In
dimensionless form, with the reservoirs `A` and `B` held constant:

```ode
# bruss.ode
x' = A - (B+1)*x + x^2*y
y' = B*x - x^2*y
par A=1, B=1.5
init x=1, y=1.5
@ total=200, dt=0.02, xp=x, yp=y, xlo=0, xhi=4, ylo=0, yhi=6
done
```

The step `2X + Y -> 3X` is the autocatalytic one. The only equilibrium is
`(x, y) = (A, B/A)`. The Jacobian there is

```
[ B - 1    A^2  ]
[  -B     -A^2  ]
```

with trace `B - 1 - A^2` and determinant `A^2 > 0`. So the equilibrium is
stable for `B < 1 + A^2` and unstable above it. At `B = 1 + A^2` the trace
is zero and the eigenvalues are `± i A`: a **Hopf bifurcation**, with an
oscillation of period `2 pi/A` at its birth. For `A = 1` the Hopf point is
at `B = 2` and the period is 6.283.

```python
br = xs.load("bruss.ode")
for B in (1.5, 2.5):
    br.parameters["B"] = B
    eq = br.equilibrium({"x": 1, "y": B})
    print(B, eq.kind, eq.eigenvalues)
# 1.5: stable focus,   -0.249 +/- 0.968i
# 2.5: unstable focus, +0.251 +/- 0.968i

br.parameters["B"] = 1.5
d = br.auto.equilibria("B", parmax=4)
hb = d.first("HB")
hb.value                               # 2.0
```

Follow the oscillation that starts at the Hopf point:

```python
orbits = br.auto.continue_from(hb, "periodic")
t = orbits.to_pandas()
t[["B", "period", "X_max", "X_min"]].iloc[::4].head(5)
# B      period   X_max   X_min
# 2.000  6.2832   1.000   1.000      the orbit is a point at the Hopf value
# 2.013  6.2878   1.144   0.879
# 2.221  6.3813   1.806   0.609
# 2.687  6.7606   2.967   0.435
# 3.397  7.8030   4.774   0.313
```

The branch leaves the Hopf point to the right (`B` increases): the Hopf
bifurcation is **supercritical**, and the amplitude grows like the square
root of the distance from `B = 2`. Simulation agrees:

```python
br.set(total=300)
cycle = {}
for B in (1.8, 2.2, 3.0):
    s = br.run(B=B)
    x = s["x"][len(s.t) // 2:]
    cycle[B] = (x.min(), x.max())
# 1.8: (1.000, 1.000) the oscillation dies out
# 2.2: (0.623, 1.751)
# 3.0: (0.371, 3.752)
```

The concentration of `X` oscillates with a relaxation-like shape when `B` is
larger: a slow rise and a quick spike, the period longer, 7.15 at `B = 3`.
Matter flows in as `A` and `B` and out as products, and the oscillation
feeds on the flow.

!!! note "Exercises"
    1. Derive the equilibrium and the Jacobian, and check that the Hopf
       condition is `B = 1 + A^2`. Verify with `A = 2` that the Hopf point
       is at `B = 5` and the period at onset is `2 pi/2 = 3.14`.
    2. Follow the Hopf point in two parameters,
       `br.auto.continue_from(hb, "two-parameter", second="A")`, and compare
       the `B` of the curve with `1 + A^2`.
    3. Set `B = 3` and use `br.limit_cycle(variable="x")` to get the period
       and one turn of the orbit. Is the equilibrium `(1, 3)` inside the
       orbit?

??? success "Solutions"
    1. `x' = 0` and `y' = 0`: adding the two equations gives `x = A`, and
       then `y = B/A`. The Jacobian is the matrix above, whose trace is
       `B - 1 - A^2` and whose determinant is `A^2`. Hopf at trace 0,
       `B = 1 + A^2`, frequency `sqrt(det) = A`. With `A = 2`: `B = 5`, period
       3.14.
    2. The curve is the parabola `B = 1 + A^2`; the computed points agree
       with it to better than 1e-3. In the desktop app this is the
       **Two-parameter** button of the Bifurcation window with the HB point
       selected.
    3. The period is 7.157. `x` runs from 0.371 to 3.751 and `y` from
       0.853 to 4.722, so the orbit surrounds `(1, 3)`: the equilibrium is
       inside the cycle, and it is unstable, a source from which
       trajectories spiral out to the cycle.

## Where to go next

- The examples folder has `gill_bruss.ode`, a stochastic simulation of the
  Brusselator with Gillespie's algorithm, to compare with the deterministic
  limit cycle above, and `lo.ode`, a normal form of the Hopf bifurcation.
- [Bifurcation analysis with AUTO](../manual/auto.md) for the diagram,
  its special points and the continuation of folds and Hopf points in two
  parameters.
- [Data analysis](../python/analysis.md) for the Fourier transform and the
  period of a limit cycle.
