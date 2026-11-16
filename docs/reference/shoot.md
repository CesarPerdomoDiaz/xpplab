# shoot

Run away from an equilibrium along its eigenvectors: the start of its
unstable, or stable, manifolds.

## Syntax

```
shoot
shoot eps E time T
shoot unstable
shoot stable
shoot both
```

- **In the app:** Workspace > **Many runs** > *Shoot from an
  equilibrium*: **step off**, **time**, the directions, then **Shoot**
  (XPPAUT's *Initial conds > sHoot*, which XPPAUT offers after *Sing
  pts*).
- **In Python:** `runs = m.shoot(eq)`, with `directions="stable"` or
  `"all"`, `eps=` and `total=`.

## Description

`shoot` finds the equilibrium that Newton's method reaches from the
initial values, and for each real eigenvector `v` of its Jacobian starts
two runs, from x + `eps`·v and x − `eps`·v. For the unstable directions
(eigenvalue > 0, the default) the runs go forward in time; for the stable
ones (eigenvalue < 0) backward. Each pair traces the two branches of a
one-dimensional manifold of the equilibrium.

`eps` is the distance of the starts from the equilibrium (1e-5 at first,
then the value given last) and `time` the length of each run (the total
time unless given). The initial values are left as they were.

## Examples

### The two ways out of the neuron's saddle

The Morris-Lecar model has a saddle at `V` = −0.1425, `W` = 0.0340, between
its rest state and its threshold.

1. [Open the Morris-Lecar model](example:lecar.ode).
2. [Start near the saddle](cmd:set v=-0.14 w=0.035) and
   [shoot](cmd:shoot).

```
set v=-0.14 w=0.035
shoot
```

```python
import xpplab as xs

m = xs.load(xs.example("lecar.ode"))
eq = m.equilibrium({"V": -0.14, "W": 0.035})
runs = m.shoot(eq)
for run in runs:
    print(run["V"].max(), run["V"][-1])
```

The command window says "Shot 2 runs from the equilibrium along its
eigenvectors: 2 ran to the end." The saddle's unstable eigenvalue is
1.0289. One run leaves towards higher `V`, fires a full spike, reaching
`V` = 0.2273, and returns to rest at −0.3825. The other leaves towards
lower `V` and goes straight to rest.

How to read it. Both branches of the unstable manifold end at the rest
state, one directly and one round a spike: together they form a loop
through the saddle and the rest state, the frame of the neuron's
excitability.

### The stable manifold is the threshold

```
shoot stable
```

```python
stable = m.shoot(eq, directions="stable")
```

Run backward in time, the two branches of the stable manifold spread out
from the saddle; one of them crosses `W` = 0.0013, the rest state's `W`,
at `V` = −0.168228.

How to read it. That is the threshold that halving the gap between starts
found on the [click](click.md) page, −0.1682284: starts on one side of the
stable manifold return to rest at once, starts on the other fire. The
manifold computes the threshold curve in one go.

## Settings

| Setting | Where | Meaning | Advice |
|---|---|---|---|
| `eps` | `shoot eps ...`; **step off** in the tab; `eps=` in Python | How far from the equilibrium the runs start. | 1e-5. Smaller follows the manifold more closely at the start, at the cost of a longer run to leave. |
| `time` | `shoot time ...`; **time** in the tab (0 keeps the model's total time); `total=` in Python | The length of each run. | Long enough to see where each branch goes. |
| `unstable`, `stable`, `both` | the command; the directions box in the tab; `directions=` in Python (`"all"` for both) | Which manifolds: forward in time along the unstable directions, backward along the stable ones, or both. | `unstable` by default. |

## Results

- **The runs**, one per direction and sign, are drawn on the plots as a
  family, as the Workspace's **Many runs** tab draws its runs.
- **The command window** says how many runs there were, and how many ran
  to the end.
- **`export FILE.csv many`** writes the runs, as for [many](many.md).
- **In Python** `m.shoot()` returns the list of runs; `runs.directions`
  gives the eigenvalue and the sign of each.

## More about

**Manifolds.** Near a saddle the flow is that of its linearisation: away
along the unstable eigenvector, in along the stable one. The manifolds are
the curves that continue those directions: everything on the stable
manifold tends to the saddle, everything on the unstable one came from it.
Starting a little way along an eigenvector and running is the simplest way
to draw them.

**Eigenvalues.** `shoot` takes the eigenvectors from the Jacobian that the
linearisation extrapolates to zero step, and finds the unstable eigenvalue
1.0289065. [eq](eq.md) reports 1.02997, because XPPAUT's own Jacobian uses
differences of step `jac_eps`, 0.001; with `set jac_eps=1e-6` it reports
1.028907. The direction of the eigenvector, and so the manifold, hardly
changes.

## Tips

- Run [eq](eq.md) first to see the equilibrium and its eigenvalues, then
  `shoot` from the same start.
- A node has two unstable (or two stable) directions; `shoot` follows both,
  unlike XPPAUT's invariant sets, which are drawn for saddles only.

## Algorithms

Newton's method finds the equilibrium; the Jacobian is that of the
linearisation (central differences extrapolated to zero step); each real
eigenvector of the chosen sign gives two runs with the model's method,
forward for unstable directions and backward for stable ones.

## When it fails

| Message | Why | What to do |
|---|---|---|
| `The equilibrium has no real unstable directions to shoot along.` | No real eigenvalue of the chosen sign, as at a stable node or a focus. | Choose the other directions, or another equilibrium. |
| `error: no equilibrium found from the initial values: ...` | Newton's method found no equilibrium from the start. | Start nearer one; see [eq all](eq-all.md). |
| `error: shoot [eps E] [time T] [unstable\|stable\|both]` | A word was not understood. | Use the words of the syntax. |

## References

- Strogatz, S. H. (2015). *Nonlinear Dynamics and Chaos*, 2nd edition.
  Westview Press. Chapter 6.
- Ermentrout, B. (2002). *Simulating, Analyzing, and Animating Dynamical
  Systems: A Guide to XPPAUT for Researchers and Students*. SIAM.

## See also

- [eq](eq.md): the equilibrium and its eigenvalues.
- [click](click.md): the threshold found by trial.

## Version history

Introduced in 0.1.0.
