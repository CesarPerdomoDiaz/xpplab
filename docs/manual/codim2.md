# Codimension-two bifurcations

A bifurcation diagram with one parameter shows where a steady state loses
stability or disappears: at a fold (LP, the limit point) or at a Hopf point
(HB). With a second parameter, each of these points moves, and the points
of all the diagrams draw a curve in the plane of the two parameters. This
page explains those curves, computes them for one model in the app and in
Python, and shows what AUTO reports where they meet. The ideas are in
Kuznetsov, *Elements of Applied Bifurcation Theory* (chapters 3, 4 and 8),
and, for one-parameter bifurcations, in Strogatz, *Nonlinear Dynamics and
Chaos* (chapters 3 and 8).

## What a two-parameter curve is

A fold and a Hopf point are codimension-one bifurcations: they happen at
isolated values of one parameter, and when a second parameter is free they
happen along a curve.

- The **fold curve** is the set of parameter pairs where a steady state is
  born in a pair with another one (the Jacobian has a zero eigenvalue).
- The **Hopf curve** is the set of pairs where a steady state has a pair of
  imaginary eigenvalues, `+- i w` with `w > 0`, and a periodic orbit is born.

The curves divide the plane into regions. Inside a region the number of
steady states and their stability do not change, so the phase portrait has
the same kind of pieces everywhere in it. Crossing a fold curve changes the
number of steady states by two; crossing a Hopf curve changes the stability
of one of them and starts a periodic orbit.

A **codimension-two bifurcation** is a point where two conditions hold
together, so it needs two parameters. It lies on a curve, or at the meeting
of curves. Two of them matter for this page:

- a **cusp** is where two fold curves meet with a common tangent. The
  steady states come in threes on one side of the cusp and singly on the other.
- a **Bogdanov-Takens (BT) point** is where a Hopf curve ends on a fold
  curve. The Jacobian has a double zero eigenvalue there; the frequency `w`
  of the Hopf curve goes to zero when it comes near.

There are others (a zero eigenvalue with an imaginary pair, a degenerate Hopf
point, resonances); Kuznetsov, chapter 8, lists them with the curves that
leave each one.

## The example

The model is the bistable switch of [Coming from AUTO-07p](auto07p.md), with
the slow feedback `b` as a second parameter:

    x' = lam + a x - x^3 - y
    y' = x - b y

with `a = 3`. Everything below can be derived by hand. The steady states have
`y = x/b` and satisfy `lam = x^3 - (3 - 1/b) x`. The Jacobian is

    [ 3 - 3x^2   -1 ]
    [ 1          -b ]

with trace `3 - 3x^2 - b` and determinant `1 - b (3 - 3x^2)`.

- The **fold curve** is where the determinant is zero: `x^2 = 1 - 1/(3b)`.
  There `lam = -2 x^3`. It exists for `b > 1/3`.
- The **Hopf curve** is where the trace is zero and the determinant is
  positive: `x^2 = 1 - b/3` and `b < 1`. The eigenvalues are
  `+- i sqrt(1 - b^2)`. At `b = 1/2` they are `+- 0.866 i`, as on the AUTO-07p page.
- The **cusp** is where the fold curve has `x = 0`: `b = 1/3`, `lam = 0`.
- The **BT point** is where the Hopf curve meets the fold curve, which is at
  `b = 1`, where `x^2 = 2/3` and `lam = +-2 (2/3)^(3/2) = +-1.0887`.

The model has a symmetry, `(lam, x, y) -> (-lam, -x, -y)`, so everything
appears twice, mirrored in `lam`. Save this as `codim2.ode`, or open it
with File > Open...:

```ode
# the bistable switch with a slow feedback; lam and b are the two parameters
p lam=-2,a=3,b=0.5
x'=lam+a*x-x^3-y
y'=x-b*y
x(0)=-1
y(0)=-0.5
@ total=100,xlo=-3,xhi=3,ylo=-3,yhi=3
d
```

## Step 1: one parameter

At `b = 0.5` follow the steady states in `lam`, as on the AUTO-07p page:

```python
import xpplab as xs

m = xs.load("codim2.ode")
m.auto.set(parmin=-2, parmax=2, dsmax=0.05, nmax=500, npr=500)
d = m.auto.equilibria("lam")
[(p.label, p.type) for p in d.labelled()]
# [(1, 'EP'), (2, 'HB'), (3, 'LP'), (4, 'LP'), (5, 'HB'), (6, 'EP')]
d.first("HB").value, d.first("LP").value      # 0.1521, 0.3849
```

Label 2 is a Hopf point at `lam = 0.1521`, label 3 a fold at `lam = 0.3849`,
and labels 4 and 5 are their mirror images at `-0.3849` and `-0.1521`.
These four points are the starting points of the curves below.

The same scan at two other values of `b` shows why one curve is not enough.
At `b = 0.2` the branch has no fold and no Hopf point in the range, and at
`b = 1.5` it has two folds and no Hopf point:

```python
for b in (0.2, 1.5):
    other = xs.load("codim2.ode")
    other.set(b=b)
    other.auto.set(parmin=-2, parmax=2, dsmax=0.05, nmax=500, npr=500)
    print(b, [(p.type, round(p.value, 4))
              for p in other.auto.equilibria("lam").labelled()])
    other.close()
# 0.2 [('EP', -2.0), ('EP', 2.0177)]
# 1.5 [('EP', -2.0), ('LP', 1.3719), ('LP', -1.3719), ('EP', 2.0095)]
```

The fold exists only when `b > 1/3`, which is the cusp's `b`, and the Hopf
point only when `b < 1`, which is the BT point's.

## Step 2: the curves of folds and of Hopf points

Select a fold or a Hopf point and follow it in `lam` and `b`. The
continuation goes one way from the point; start again from the same point
with the opposite sign of `ds` to go the other way. Here `ds > 0` raises `b`
on both curves:

```python
m.auto.set(nmax=100)
fold_up = m.auto.continue_from(3, "two-parameter", second="b")
m.auto.set(ds=-0.02)
fold_down = m.auto.continue_from(3, "two-parameter", second="b")
m.auto.set(ds=0.02, nmax=400)
hopf_up = m.auto.continue_from(2, "two-parameter", second="b")
m.auto.set(ds=-0.02, nmax=100)
hopf_down = m.auto.continue_from(2, "two-parameter", second="b")
```

Each curve is a new branch of the diagram, labelled after the first one:
`fold_up` ends with label 7, `fold_down` with 8, `hopf_up` with 9 and
`hopf_down` with 10. Each point of a curve has `p.value` (`lam`) and
`p.value2` (`b`).

## The figure

```python
import numpy as np
import matplotlib.pyplot as plt

def curve(c):
    return np.array([(p.value, p.value2) for p in c])

fig, ax = plt.subplots(figsize=(5.5, 4.5))
for c in (fold_up, fold_down):
    ax.plot(*curve(c).T, "k-", label="folds" if c is fold_up else None)
    ax.plot(*(curve(c) * [-1, 1]).T, "k-")
for c in (hopf_up, hopf_down):
    ax.plot(*curve(c).T, "k--", label="Hopf points" if c is hopf_up else None)
    ax.plot(*(curve(c) * [-1, 1]).T, "k--")
ax.axhline(0.5, color="0.6", linestyle=":")
ax.plot([0, 1.0887, -1.0887], [1/3, 1, 1], "ko", mfc="none")
ax.set(xlim=(-2, 2), ylim=(0, 3), xlabel="lam", ylabel="b")
ax.legend(loc="upper center")
fig.savefig("codim2.png", dpi=150)
```

![Curves of folds (solid) and of Hopf points (dashed) of the switch in the
lam-b plane, with the cusp and the two Bogdanov-Takens points circled and the
scan at b = 0.5 dotted](../images/codim2.png)

The solid curves are the folds. They form a wedge that opens upwards from the
cusp at the bottom (`lam = 0`, `b = 1/3`). Inside the wedge there are three
steady states and outside it there is one. The dashed curves are the Hopf
points. The curve through the Hopf point at `lam = 0.1521` crosses the dotted
line `b = 0.5` there, runs up to the right, and ends on the right-hand fold
curve at the BT point (`lam = 1.0887`, `b = 1`). The other curve is its
mirror image. The two cross at `lam = 0`, `b = 0.464`: the symmetry gives a
Hopf point at `x` and another at `-x` for the same parameters. That is a
consequence of the symmetry, and not a codimension-two point of one steady
state.

## What AUTO reports at the codimension-two points

The AUTO of XPPAUT labels only these points: `LP`, `HB`, `BP`, `PD`, `TR`,
`EP`, `UZ` and `MX`. It has no label for a cusp, a BT point or any other
codimension-two point, and it does not look for them. Along the four curves
above the only labels are the end points, `EP`. The codimension-two points
show in the shape of the curves and in the numbers, which you read yourself:

```python
fu, fd = curve(fold_up), curve(fold_down)
fd[fd[:, 1].argmin()]               # the cusp: lam = 0.000, b = 0.333
np.interp(1.0, fu[:, 1], fu[:, 0])  # lam of the fold curve at b = 1: 1.089
hopf_up[-1].value, hopf_up[-1].value2   # (1.0877, 0.9988) after 400 steps
hd = curve(hopf_down)[::-1]
np.interp(0.0, hd[:, 0], hd[:, 1])  # b where the Hopf curves cross: 0.464
```

- **Cusp.** The fold curve `fold_down` starts at the fold, goes down to
  smaller `b`, turns at the cusp and goes on as the other fold curve, to
  negative `lam`. It is a single smooth curve for AUTO, because the cusp is a
  turn of the curve in `(lam, b)` and not a singularity of the continuation.
  The lowest `b` on it, 0.333, and the `lam` there, 0, agree with
  `(0, 1/3)` to the accuracy of AUTO's steps.
- **Bogdanov-Takens.** The Hopf curve runs towards `(1.0887, 1)` and does not
  stop there or get a label. After 400 steps it is at `lam = 1.0877`,
  `b = 0.9988`, and each step gains less (after 3000 steps `b` is still below 1). The fold
  curve, followed from the fold, passes `b = 1` at `lam = 1.089`. The
  two curves meet where the Hopf curve ends, which you see in the figure and
  from the formulas, and not in a label. Stop the Hopf run with `nmax` when it
  comes close.

What is not computed here: the curves that also leave a BT point (a curve of
homoclinic orbits and one of folds of periodic orbits; Kuznetsov, chapter 8).
They are not codimension-two points of the steady states, and XPP Lab does
not follow them from the BT point. The tools to follow a homoclinic orbit
are in [AUTO](auto.md).

## Advice from the example

- **Choose the second parameter so that the conditions can meet.** The
  fold needs `3 - 3x^2 = 1/b` and the Hopf point `3 - 3x^2 = b`. Moving
  `a` instead of `b` moves both through `a - 3x^2` and never makes the two
  values equal at `b = 0.5`, so no BT point. A parameter that enters the
  steady states and the trace in different ways does. Try the one-parameter
  scans at a few values of the second parameter first, as above: where a
  fold or Hopf point appears or disappears, a codimension-two point is near.
- **Use `ds` for the direction.** One run goes one way from the point. Run
  again from the same label with the opposite sign of `ds` for the other way.
  The sign that means "up" depends on the curve; the first points tell.
- **`parmin` and `parmax` limit the first parameter only.** `hopf_down` ends
  after 63 of its 100 steps because `lam` reached the limit -2 (at
  `lam = -2.002`, `b = 0.240`). The second parameter has no limit, so the
  fold curves would run to large `b`: use `nmax` to stop them.
- **Read the points from the data.** Use `p.value` and `p.value2` of the
  curve and the formulas for the points AUTO does not label, as above.

## The same in the app

Every action prints the Python line it corresponds to in the command window,
starting with `>>>`. Open `codim2.ode`, then type these commands (they are
what the buttons do):

```commands
auto set parmin=-2 parmax=2 dsmax=0.05 nmax=500 npr=500
auto steady lam
auto set nmax=100
auto from 3 two b
auto set ds=-0.02
auto from 3 two b
auto set ds=0.02 nmax=400
auto from 2 two b
auto set ds=-0.02 nmax=100
auto from 2 two b
```

The command window answers with these lines (`xpplab-batch codim2.ode --echo
-c ...` prints the same):

```text
>>> m.auto.set(dsmax=0.05, parmin=-2.0, nmax=500, npr=500)
>>> m.auto.equilibria("lam")
>>> m.auto.parameters("lam", "b")
>>> m.auto.set(nmax=100)
>>> m.auto.continue_from(3, "two-parameter", parameter="lam", second="b")
>>> m.auto.set(ds=-0.02)
>>> m.auto.continue_from(3, "two-parameter", parameter="lam", second="b")
>>> m.auto.set(ds=0.02, nmax=400)
>>> m.auto.continue_from(2, "two-parameter", parameter="lam", second="b")
>>> m.auto.set(ds=-0.02, nmax=100)
>>> m.auto.continue_from(2, "two-parameter", parameter="lam", second="b")
```

In the window, fill in **Par min**, **Par max**, **DS max**, **Max steps** and
**Label every**; press **Follow steady states**; choose `b` in **2nd param.**;
select the row `3 LP` and press the two-parameter action; set **Step DS** to
`-0.02` and press it again; and do the same from the row `2 HB` with the
steps and the signs of the commands above. The curves appear in the
**Two parameters** tab. The fold has no other action: from a fold, AUTO
always follows it in two parameters.

## Where to go next

- [Bifurcation analysis with AUTO](auto.md), for the window and the settings.
- [Coming from AUTO-07p](auto07p.md), for the same model followed in one
  parameter and for the periodic orbits born at the Hopf point.
- Kuznetsov, *Elements of Applied Bifurcation Theory*, chapter 8, for the
  cusp and the Bogdanov-Takens bifurcations.
- Strogatz, *Nonlinear Dynamics and Chaos*, chapters 3 and 8, for folds, the
  cusp catastrophe and Hopf bifurcations.
