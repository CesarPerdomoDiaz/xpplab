# Ecology: populations that grow, collapse and cycle

Population models are the other classical home of phase-plane analysis.
The state is a set of population sizes; the parameters are growth rates,
harvest, the carrying capacity of the environment. The questions are the
ones the equations answer well: which populations coexist, which collapse,
which oscillate, and how a small change in the environment can turn one
outcome into another.

This page has four examples:

1. a **harvested population**, where a saddle-node bifurcation is the
   difference between a sustainable fishery and a collapse;
2. the **Lotka-Volterra predator-prey model**, a centre with a conserved
   quantity, and why that makes it a poor model;
3. the **Rosenzweig-MacArthur model**, where enriching the food supply
   destabilises the system (the paradox of enrichment) through a Hopf
   bifurcation;
4. **two competing species**, where the phase plane decides who wins.

Save each model under the name in its first line, in the folder where you
run the Python. The snippets of a page run one after the other in a single
script. A reminder of the theory of equilibria, stability and bifurcations
is at the start of the [neuroscience page](neuroscience.md#theory-in-five-paragraphs);
the ecological reading of it follows.

## Theory for populations

**Population models.** A population `x` has a per-capita growth rate
`g(x)`, so `x' = x g(x)`. Exponential growth is `g = r`; the **logistic
equation** `x' = r x (1 - x/K)` lets `g` fall to zero at the carrying
capacity `K`. Two species add interaction terms: predation takes from the
prey and feeds the predator, competition takes from both.

**Equilibria are coexistence states.** An equilibrium with every population
positive is a coexistence state. One with a population at zero is an
extinction or exclusion state. Its stability tells you whether the missing
species can invade: if it is unstable in the direction of the missing
species, a few individuals will grow.

**Functional response.** How much one predator eats as a function of prey
density `x` is the functional response. Type I is linear (`a x`). Type II
saturates (`a x/(1 + a h x)`) because the predator needs handling time `h`.
Saturation is destabilising, and the Rosenzweig-MacArthur model below is
the textbook example.

**Structural stability.** A model is structurally stable if small changes in
its equations leave the phase portrait qualitatively the same. A model with
a centre (closed orbits all around an equilibrium, as in Lotka-Volterra) is
not: an arbitrarily small extra term turns the centre into a stable or an
unstable focus. Be wary of ecological conclusions from such models.

**Bifurcations in ecology.** A saddle-node is a **tipping point**: a
parameter crosses a threshold, a stable state disappears, and the
population drops to the other state. A Hopf bifurcation is the onset of
cycles. In both cases the key fact for management is that the system moves
at a critical value of a parameter that changes slowly and smoothly.

## Example 1: the harvested population

A population grows logistically (carrying capacity 1) and a fixed amount `h`
is removed per unit time, as in a fishery with a quota:

```
x' = x (1 - x) - h
```

```ode
# fish.ode
x' = x*(1-x) - h
par h=0.2
init x=0.5
@ total=60, dt=0.05, xlo=0, xhi=60, ylo=-0.5, yhi=1.1
done
```

The equilibria solve `x^2 - x + h = 0`, so
`x* = (1 ± sqrt(1 - 4h))/2`. There are two for `h < 1/4`, one at `h = 1/4`
and none above it. For `h = 0.2` they are 0.7236, which is stable, and
0.2764, which is unstable: it is the **threshold** below which the harvest
exceeds the growth, and the population collapses. At `h = 1/4` the two
meet and disappear in a saddle-node bifurcation. The largest sustainable
yield is `1/4` of the carrying capacity per unit time, but it is a bad
place to operate: any further push collapses the population, and so does a
small drop in the stock.

```python
import numpy as np
import xpplab as xs

fish = xs.load("fish.ode")
high = fish.equilibrium({"x": 0.9})
low = fish.equilibrium({"x": 0.1})
high.kind, high["x"]                   # 'stable node', 0.7236
low.kind, low["x"]                     # 'unstable node', 0.2764

ok = fish.run(x=0.5)                   # settles at 0.7236
gone = fish.run(x=0.1)                 # below the threshold: goes through zero
ok["x"][-1], gone["x"].min() < 0
```

Now let AUTO follow the stable equilibrium as the harvest rises:

```python
fish.initial_conditions["x"] = 1.0
fish.parameters["h"] = 0.0
d = fish.auto.equilibria("h", parmax=0.4)
d.special("LP")                        # [<LP #2: ... h=0.25 ...>]
d.special("LP")[0].value               # 0.25
```

There is one limit point (LP), at `h = 0.25`, and the diagram folds back:
the stable branch, with the large population, meets the unstable branch,
the threshold. This is the picture of a **tipping point**: no sign of
trouble on the way up (the population falls gently as `h` rises, to 0.5 at
the fold), and then nothing.

!!! note "Exercises"
    1. For `h = 0.3`, start at `x = 1`. When does the population go through
       zero? What does that mean for the model?
    2. A manager lowers the harvest after a collapse is noticed. At
       `h = 0.2` the population is at 0.2 (below the threshold 0.2764).
       Does the population recover? At what `h` does it recover, starting
       from `x = 0.2`? This is **hysteresis**.
    3. Change the harvest to be proportional to the stock, `h*x`. Show with
       the equilibria that there is no tipping point and give the
       sustainable yield.

??? success "Solutions"
    1. There is no equilibrium for `h > 1/4`, and the population falls at
       every `x`. Starting at 1 it crosses zero at `t = 10.3`, and `x`
       becomes negative: in the model this is extinction, and the equation
       is no longer meaningful afterwards.
    2. No: with `h = 0.2`, `x = 0.2` is below the unstable equilibrium
       0.2764, so the population still collapses. It recovers only when the
       unstable equilibrium drops below 0.2: `(1 - sqrt(1 - 4h))/2 < 0.2`,
       which gives `h < 0.16`. At `h = 0.16` the population stays at 0.2 (it
       is the unstable equilibrium itself); at `h = 0.15` it grows to
       0.8162. Restoring the stock needs a harvest well below the one that
       caused the collapse.
    3. `x' = x(1 - x - h)` has the equilibria `x = 0` and `x = 1 - h`: a
       transcritical exchange of stability at `h = 1`, no fold. The yield
       is `h x* = h (1 - h)`, with the maximum 1/4 at `h = 1/2`, where the
       population is at half the carrying capacity and nothing is near a
       cliff. A proportional harvest is safer than a fixed quota.

## Example 2: Lotka-Volterra, a centre

The classical predator-prey model has prey `x` that grows by itself and
predators `y` that only grow by eating prey:

```
x' = a x - b x y
y' = -c y + d x y
```

```ode
# lv.ode
x' = a*x - b*x*y
y' = -c*y + d*x*y
par a=1, b=1, c=1, d=1
init x=1.5, y=1
@ total=100, dt=0.01, xp=x, yp=y, xlo=0, xhi=4, ylo=0, yhi=4
done
```

The coexistence equilibrium is at `(c/d, a/b)` = `(1, 1)`. Its Jacobian has
trace 0 and determinant `a c`, so the eigenvalues are `± i sqrt(a c)`:
exactly on the imaginary axis. Linear theory cannot say whether the
equilibrium is stable. But the model has a **conserved quantity**,

```
H = d x - c ln(x) + b y - a ln(y)
```

which is constant along every trajectory; the orbits are its level curves
and are all closed. The populations oscillate for ever, with a period that
depends on the amplitude: it is `2 pi/sqrt(a c)` = 6.28 for small
oscillations and longer for large ones (6.38 for the orbit that starts at
`x = 1.5`).

```python
lv = xs.load("lv.ode")
eq = lv.equilibrium({"x": 1.1, "y": 0.9})
eq["x"], eq["y"], eq.eigenvalues       # 1, 1, +/- 1i

def H(s):
    return s["x"] - np.log(s["x"]) + s["y"] - np.log(s["y"])

rk = lv.run()
H(rk).max() - H(rk).min()              # about 1e-7: conserved by the integrator

lv.set(meth="euler")
eu = lv.run()
H(eu).max() - H(eu).min()              # 0.16: Euler's method adds energy
lv.set(meth="rungekutta")
```

The second run shows why the choice of method matters when a model has a
conserved quantity: Euler's method makes the orbit spiral outwards, and the
numerical error looks like a biological effect.

!!! note "Exercises"
    1. Add a logistic term for the prey, `x' = a*x*(1 - x/K) - b*x*y`, with
       `K = 5`. Find the equilibrium and its eigenvalues. What happened to
       the centre?
    2. Plot `x` and `y` against time for the original model. It starts
       with the prey at its peak. When does the predator peak?
    3. Start from `x = 0.1, y = 0.1` instead. Read the smallest and the
       largest densities. What would they mean for a population of a few
       hundred animals?

??? success "Solutions"
    1. The coexistence equilibrium is now `(1, 0.8)` (`x* = c/d` and
       `y* = (a/b)(1 - x*/K)`). The trace of the Jacobian there is
       `-a x*/K = -0.2`, and the eigenvalues are `-0.100 ± 0.889i`: the
       centre has become a stable focus, and the oscillations die out.
       Lotka-Volterra's neutral cycles are not robust.
    2. The prey is at its maximum, 1.5, at `t = 0`; the predator reaches
       its maximum, also 1.5, at `t = 1.31`.
    3. The densities swing between 0.023 and 5.5. With a few hundred
       animals, 0.023 of the equilibrium density is a handful of
       individuals, and in a real population chance would extinguish it.
       The neutral oscillations of this model are not a safe description of
       small populations.

## Example 3: the paradox of enrichment

Rosenzweig and MacArthur gave the prey logistic growth and the predator a
saturating functional response:

```
x' = x (1 - x/K) - x y/(1 + x)
y' = y (x/(1 + x) - d)
```

Prey `x` is scaled to its half-saturation density, `K` is the carrying
capacity (the richness of the environment) and `d` is the predator
mortality, scaled by the maximum predator growth rate.

```ode
# rm.ode
x' = x*(1-x/K) - x*y/(1+x)
y' = y*(x/(1+x) - d)
par K=1.2, d=0.25
init x=0.5, y=0.3
@ total=400, dt=0.05, xp=x, yp=y, xlo=0, xhi=3, ylo=0, yhi=2
done
```

**The coexistence equilibrium.** The predator nullcline gives
`x* = d/(1 - d)` = 1/3, independent of `K`. The prey nullcline then gives
`y* = (1 - x*/K)(1 + x*)`: 0.963 for `K = 1.2`. The predator needs a
minimum prey density to survive, and sets it; enrichment only feeds the
predator.

**Stability.** The trace of the Jacobian at the equilibrium changes sign at

```
K_H = 1 + 2 x* = (1 + d)/(1 - d)
```

which is 5/3 = 1.667 here. For `K < K_H` the equilibrium is a stable
focus. For `K > K_H` it is an unstable focus and the populations cycle.
Making the environment richer does not make the populations more stable:
it makes them swing, and the swings take the prey and predator to ever
smaller densities. That is the paradox of enrichment.

```python
rm = xs.load("rm.ode")
eq = rm.equilibrium({"x": 0.33, "y": 0.5})
eq.kind, eq["x"], eq["y"]              # 'stable focus', 1/3, 0.963
eq.eigenvalues                         # -0.049 +/- 0.365i

rm.initial_conditions.update(x=eq["x"], y=eq["y"])
d = rm.auto.equilibria("K", parmax=3)
hb = d.first("HB")
hb.value                               # 1.667 = (1 + d)/(1 - d)
```

Below the Hopf point a disturbance spirals into the equilibrium (slowly,
with a decay rate of 0.049 at `K = 1.2`). Above it the orbit grows to a
limit cycle. Look at its extent as `K` rises, with `sweep`:

```python
rm.set(total=800)
Ks = [1.2, 2.0, 3.0, 4.0]
lows = rm.sweep("K", Ks, measure=lambda s: s["x"][len(s.t) // 2:].min())
highs = rm.sweep("K", Ks, measure=lambda s: s["x"][len(s.t) // 2:].max())
```

| `K` | smallest prey density | largest prey density |
|-----|-----------------------|----------------------|
| 1.2 | 0.333 | 0.333 |
| 2.0 | 0.052 | 1.05 |
| 3.0 | 0.0017 | 2.36 |
| 4.0 | 0.00005 | 3.55 |

At `K = 4` the prey falls to 5 in 100 000 of the half-saturation density.
In a real population that is extinction: the cycle is "stable" in the model
and fatal in the field.

!!! note "Exercises"
    1. Derive `y* = (1 - x*/K)(1 + x*)` and the Jacobian at the
       equilibrium. Check that the trace is zero when `K = 1 + 2 x*`.
    2. Make the predators less efficient by raising `d` to 0.4. Where does
       the Hopf point move? What is `x*`? What happens to the cycle for the
       same `K = 3`?
    3. For `K` below `d/(1 - d)` (here 1/3) the predator cannot survive at
       all: the prey's equilibrium is below the threshold. Run `K = 0.3` and
       confirm what happens.
    4. Open `rm.ode` in the app, drag the `K` slider and watch the phase
       plane with **Nullclines** on. What happens to the prey nullcline
       (a hump) as `K` grows? Where is the equilibrium relative to the top
       of the hump when the Hopf bifurcation happens?

??? success "Solutions"
    1. On the prey nullcline, `x(1 - x/K) = xy/(1 + x)`, so
       `y = (1 - x/K)(1 + x)`. The Jacobian at the equilibrium has
       `df/dx = 1 - 2x/K - y/(1+x)^2` and `dg/dy = x/(1+x) - d = 0`, so the
       trace is `df/dx`. With `y*` substituted, `y*/(1+x*)^2 =
       (1 - x*/K)/(1 + x*)`, and the trace becomes
       `x*(1 - (1 + 2x*)/K)`. It is zero at `K = 1 + 2x*`, which is 5/3 for
       `d = 1/4`. It is negative (stable) below that value.
    2. `K_H = (1 + d)/(1 - d)` = 2.333, and `x* = d/(1 - d)` = 0.667. At
       `K = 3`, past the Hopf point, the cycle is much milder than for
       `d = 0.25`: the prey stays between 0.100 and 2.10, against 0.0017 and
       2.36. A predator that needs more prey to live does not drive it as
       low.
    3. With `K = 0.3` the predator has no coexistence equilibrium (it would
       need `x* = 1/3 > K`). The predator dies out (`y` ends near 1e-8) and
       the prey settles at `K = 0.3`: the equilibrium `(K, 0)` is stable.
    4. The prey nullcline is a hump with its maximum at `x = (K - 1)/2`,
       which moves right as `K` grows, while the predator nullcline is the
       vertical line `x = x*`. For `K < K_H` the top of the hump is to the
       left of the line, so the equilibrium sits on the falling side and is
       stable. The Hopf bifurcation is where the top of the hump passes the
       line, `(K - 1)/2 = x*`. Beyond it the equilibrium is on the rising
       side and is unstable.

## Example 4: two competing species

Two species compete for the same resource. Each limits itself (the `1` in
the brackets) and the other (the coefficients `a` and `b`):

```
x' = x (1 - x - a y)
y' = r y (1 - y - b x)
```

```ode
# comp.ode
x' = x*(1 - x - a*y)
y' = r*y*(1 - y - b*x)
par a=0.5, b=0.5, r=1
init x=0.1, y=0.1
@ total=60, dt=0.05, xp=x, yp=y, xlo=0, xhi=1.2, ylo=0, yhi=1.2
done
```

There are four equilibria: both extinct `(0, 0)`, only `x` at `(1, 0)`, only
`y` at `(0, 1)`, and coexistence at `x = y = 1/(1 + a)` when `a = b`. The
outcome depends on whether competition between species is weaker or
stronger than competition within a species:

- `a = b < 1` (weak): coexistence is a stable node and the boundary states
  are saddles. Both species survive, from any start with both present.
- `a = b > 1` (strong): coexistence is a saddle, and `(1, 0)` and `(0, 1)`
  are stable nodes. **Winner takes all**, and which one wins depends on
  where you start. The stable manifold of the saddle separates the two
  basins.

```python
comp = xs.load("comp.ode")
box = {"x": (0, 1.5), "y": (0, 1.5)}
for a in (0.5, 1.5):
    comp.set(a=a, b=a)
    found = comp.find_equilibria(200, box=box)
    print(a, [(round(e["x"], 3), round(e["y"], 3), e.kind) for e in found])
# 0.5: (0,0) unstable node, (1,0) saddle, (0,1) saddle, (0.667,0.667) stable node
# 1.5: (0,0) unstable node, (1,0) stable node, (0,1) stable node, (0.4,0.4) saddle

comp.set(a=1.5, b=1.5)
x_ahead = comp.run(x=0.6, y=0.5)       # ends near (1, 0)
y_ahead = comp.run(x=0.5, y=0.6)       # ends near (0, 1)
x_ahead["x"][-1], y_ahead["y"][-1]     # both 1.0
```

The starting points differ by 0.1 in each variable and end at opposite
states. The line `x = y` is the stable manifold of the saddle in this
symmetric case: the species that starts ahead wins.

You can see the basins by drawing the nullclines and the flow in the app
(`nullclines`, `dirfield`), or in Python with `comp.phase_plane("x", "y")`.

!!! note "Exercises"
    1. With `a = b = 0.5` compute the eigenvalues at the coexistence point
       and show that it is a node, not a focus. Does competition ever
       produce cycles in two species?
    2. Take `a = 0.5` and `b = 1.5` (species `x` is weakly affected, `y`
       strongly). What is the outcome from random starts? Find the
       equilibria.
    3. Where is the saddle for `a = b = 1.5`? Use `comp.manifolds()` to
       compute its stable manifold and compare it with the line `x = y`.

??? success "Solutions"
    1. The Jacobian at `(2/3, 2/3)` is `-(2/3) [[1, a], [b, 1]]` for `r = 1`:
       eigenvalues `-(2/3)(1 ± sqrt(ab))`, real and negative, so the point
       is a node. In fact the Lotka-Volterra competition model in two
       species has no periodic orbits at all; cycles need three species or
       another mechanism.
    2. `x` wins. The equilibrium with both species would be at `x = 2`,
       `y = -2`, outside the quadrant, so `find_equilibria` finds only
       `(0, 0)` (unstable), `(0, 1)` (a saddle) and `(1, 0)` (stable). From
       any start with `x > 0`, the trajectory ends at `(1, 0)`.
    3. At `(0.4, 0.4)`. `comp.manifolds()` returns the saddle and the
       branches of its invariant manifolds. The two stable branches lie on
       the line `x = y`, which is the boundary between the basins.

## Where to go next

- The examples folder has `ecofilip.ode` (a population with a harvest that
  switches on above a threshold) and `logbif.ode` (the logistic map and
  its period-doubling route to chaos). Open them in the app.
- [Bifurcation analysis with AUTO](../manual/auto.md) for two-parameter
  continuation of the Hopf point in `(K, d)`.
- [Phase plane](../python/phase-plane.md) for nullclines, the flow and
  manifolds from Python.
