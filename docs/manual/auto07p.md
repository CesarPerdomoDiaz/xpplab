# Coming from AUTO-07p

XPP Lab runs AUTO underneath: the continuation code is the one XPPAUT 8.0
embeds, with its own set of constants and menus, not the AUTO-07p
distribution. If you know AUTO-07p, you know the ideas: branches, labelled points, `IPS`, `ICP`, `NMX`, `DS`.
This page maps its constants and calls to the AUTO window of the app and to
the `xpplab.Auto` object in Python, says what is not here, and works one
example, in the way of AUTO-07p's tutorials: a fold and a Hopf point on one
branch of steady states, then the periodic branch from the Hopf point.

## The main differences

- You write an `.ode` file, not a Fortran or C equation file and a `c.`
  constants file. The file holds the equations, the parameters and the start
  values; the AUTO constants are set by name with `m.auto.set(...)` or in the
  window. Parameters have names (`lam`), not numbers (`PAR(1)`).
- There is no `run()` that reads a start solution from a file. A branch of
  steady states starts at the model's initial values after Newton's method
  has moved them onto the nearest equilibrium (`m.auto.equilibria`). Other
  branches start at a labelled point of the current diagram
  (`m.auto.continue_from`, the equivalent of `IRS` with a label) or, for a
  periodic orbit, at the model's current run (`m.auto.start("periodic", ...)`,
  which needs the run to lie close to an orbit). `m.auto.orbit(label)`
  turns a periodic point of the diagram into the current run.
- The diagram is kept in the model, as one list of points: `m.auto.diagram`.
  Each point knows its branch, its number on the branch, its label, its type
  and whether it is stable.
- Labels count 1, 2, 3, ... through the whole diagram, in the order the points
  were found, over all branches. The first point of a run and its last point
  are labelled `EP`, as in AUTO-07p. Continuing from a point adds a branch
  and keeps the labels going.
- Branch numbers are AUTO's own: the first branch is 1 and a periodic branch
  has a negative number. The point number on a branch is negative where the
  solution is stable. Use `p.stable` rather than these signs.
- Stable parts are drawn solid and unstable parts dashed, with XPPAUT's colours.
- A diagram is saved to one file, `.auto`, XPPAUT's format, with
  `m.auto.save(path)` and `m.auto.load(path)`. The file holds the settings,
  every point and the restart data, so you can continue from its labels
  after loading it. AUTO-07p's `b.`, `s.` and `d.` files are not read or
  written.

## AUTO-07p and XPP Lab side by side

| AUTO-07p | XPP Lab app (AUTO window) | XPP Lab Python | Notes |
|---|---|---|---|
| equation file and `c.` file | the `.ode` file; File > Open... | `m = xs.load("switch.ode")` | The model is written in XPPAUT's language, see [the `.ode` file format](odefile.md). |
| `run(...)` of a steady branch, `IPS=1` | **Follow steady states** | `m.auto.equilibria("lam")` | Newton's method first moves the initial values onto an equilibrium. |
| `IPS=2` from a Hopf point, with `IRS` the label | select the point, **Periodic orbits from this Hopf point** | `m.auto.continue_from(hb, "periodic")` | `hb` is a point or its label. |
| `IPS=2` from an orbit | | `m.auto.start("periodic", "lam")` | Continues the current run as an orbit. |
| `IPS=4` (boundary value problems) | not in the app | `m.auto.start("bvp", "l")` | The conditions are the `b` lines of the `.ode` file; see [AUTO](auto.md). |
| `IPS=9` (HomCont) | not in the app | `m.auto.start("homoclinic", "a", second="b", left=[0, 0])` | Also `"heteroclinic"`. |
| `ICP=['lam']` | **Parameter** | `m.auto.equilibria("lam")` | One parameter, by name. |
| `ICP=['lam', 'a']` (two parameters) | **2nd param.**, then a two-parameter action | `m.auto.continue_from(lp, "two-parameter", second="a")` | From a fold, a Hopf, a branch, period-doubling or torus point. |
| `NMX` | **Max steps** | `m.auto.set(nmax=...)` | |
| `NPR` | **Label every** | `m.auto.set(npr=...)` | Special points are always labelled. |
| `DS` | **Step DS** | `m.auto.set(ds=...)` | A negative sign goes towards smaller values. |
| `DSMIN`, `DSMAX` | **DS min**, **DS max** | `m.auto.set(dsmin=..., dsmax=...)` | |
| `RL0`, `RL1` | **Par min**, **Par max** | `m.auto.set(parmin=..., parmax=...)` | |
| `A0`, `A1` | not in the app | `m.auto.set(normmin=..., normmax=...)` | |
| `NTST` | **Mesh NTST** | `m.auto.set(ntst=...)` | |
| `NCOL`, `IAD`, `MXBF`, `IID`, `ITMX`, `ITNW`, `NWTN`, `IADS`, `EPSL`, `EPSU`, `EPSS` | not in the app | `m.auto.set(ncol=..., iad=..., ...)` | The same names in lower case. |
| `ISP` | not in the app | `m.auto.set(suppbp=1)` | Only the one thing: it skips branch point detection. |
| `ISW=-1` at a branch point | select the BP, **Switch to the other branch** | `m.auto.continue_from(bp, "switch")` | |
| `ISW=2` at LP, HB, PD, TR | select the point, a two-parameter action | `m.auto.continue_from(pt, "two-parameter", second=...)` | |
| `IRS` with a label | select the point in the table or on the plot | `m.auto.continue_from(5, ...)` | The label is one of the current diagram. |
| the labels `LP`, `HB`, `BP`, `PD`, `TR`, `EP`, `MX` | the **Type** column | `p.type`, `d.special("HB")`, `d.first("LP")`, `d.label(5)` | A regular point has the type `""`. |
| `sv('name')`, `ld('name')` | **Save...**, **Open...**; `auto save`, `auto load` | `m.auto.save(path)`, `m.auto.load(path)` | One `.auto` file. |
| `plot('name')` | the plot in the AUTO window | `m.auto.diagram.plot()` | `d.plot("norm")` or a variable name; `d.to_csv(path)` writes the points. |
| `cl()`, starting a new diagram | **Clear diagram**; `auto reset` | `m.auto.reset()` | Forgets the diagram; there are no files to clean. |
| the orbit stored at a label in `s.` | **Show this orbit as the run**; `auto orbit 7` | `m.auto.orbit(7)` | One period of a periodic point, as a run. |

The defaults are `ntst` 15, `nmax` 200, `npr` 50, `ds` 0.02, `dsmin` 0.001,
`dsmax` 0.5, `parmin` 0 and `parmax` 2. `m.auto.settings` lists all of them.

## What has no equivalent here

- Equation files in Fortran or C, `c.` files, and the `r.`, `s.`, `b.`, `d.`
  files of AUTO-07p. There is no `run()` call with `e=`, `c=`, `s=` and `IRS`
  as a file name, and no AUTO command-line interpreter.
- Any `IPS` other than the starts in the table: the starts are steady
  states, periodic orbits, boundary value problems and homoclinic or
  heteroclinic orbits. There is no start for parabolic PDEs or for
  optimisation; time integration is `m.run`.
- More than two continuation parameters. `ICP` here is a parameter and an
  optional second one.
- `UZR`, `NUZR` and `UZSTOP`: no setting asks AUTO to stop or to label at given
  parameter values. Use `parmin` and `parmax`, or `npr` for regular labels.
- `THL` and `THU`, the weights of the norm; `ILP` and `IPLT`; the choice of
  what goes in the diagram (the plot shows the norm, or the maximum and
  minimum of one variable).
- The app's AUTO window and its `auto set` command have only `ds`, `dsmin`,
  `dsmax`, `nmax`, `npr`, `ntst`, `parmin` and `parmax`. The rest is Python.
- Boundary value problems and homoclinic starts are Python only, and
  tested on small examples; see [AUTO](auto.md) for their limits.

## Example: a fold and a Hopf point on one branch

The model is a bistable switch with a slow feedback. The variable `x` is
driven by the parameter `lam` and has a cubic nonlinearity, `y` follows `x`
slowly:

    x' = lam + a x - x^3 - y
    y' = eps (x - y/2)

with `a = 3` and `eps = 1`. The steady states satisfy `lam = x^3 - x`. Folds
are where `3 x^2 = 1`, so at `lam = +-2/(3 sqrt 3) = +-0.3849`. The trace of
the Jacobian is `a - 3 x^2 - eps/2`; it is zero where `x^2 = 5/6`, at
`lam = +-0.1521`, and the eigenvalues there are `+-i sqrt(3)/2 = +-0.866 i`.
Save this as `switch.ode`, or open it with File > Open...:

```ode
# a bistable switch with a slow feedback; lam is the continuation parameter
p lam=-2,a=3,eps=1
x'=lam+a*x-x^3-y
y'=eps*(x-0.5*y)
x(0)=-1
y(0)=-0.5
@ total=100,xlo=-3,xhi=3,ylo=-3,yhi=3
d
```

In AUTO-07p you would write a `c.switch` with `IPS=1`, `ICP=['lam']`,
`RL0=-2`, `RL1=2`, `DSMAX=0.05`, `NMX=500`, `NPR=500` and call
`run(...)`. With XPP Lab the settings are arguments:

```python
import xpplab as xs

m = xs.load("switch.ode")
m.auto.set(parmin=-2, parmax=2, dsmax=0.05, nmax=500, npr=500)
d = m.auto.equilibria("lam")
d                              # 193 points; special: HB#2, LP#3, LP#4, HB#5
[(p.label, p.type) for p in d.labelled()]
# [(1, 'EP'), (2, 'HB'), (3, 'LP'), (4, 'LP'), (5, 'HB'), (6, 'EP')]
hb, lp = d.first("HB"), d.first("LP")
hb.value                       # 0.15211: lam at the Hopf point
lp.value                       # 0.3849: lam at the fold
hb.eigenvalues                 # 0 +/- 0.866i
```

The branch starts at `lam = -2` with label 1 and runs to the end of the range
with label 6. Label 2 is the Hopf point at `lam = 0.1521` (the continuation goes from the
equilibrium at `x = -1.52` through `x = -0.91`), label 3 the fold at
`lam = 0.3849`, label 4 the other fold, label 5 the second Hopf point. The
numbers agree with the formulas above to the accuracy of AUTO's tolerances.

Now the periodic branch from the Hopf point, the way AUTO-07p would do it
with `IPS=2`, `IRS=2`, `ICP=['lam', 'T']`. The orbits are born with a period
of `2 pi / 0.866 = 7.26` and the branch turns towards smaller `lam`, so the
Hopf point is subcritical and the orbits are unstable. Then save the
diagram, clear it, load it again and follow the fold in two parameters:

```python
orb = m.auto.continue_from(hb, "periodic", nmax=30)
orb[0].period                  # 7.2553: the period at the Hopf point
orb[-1].period                 # 25.648 after 30 steps, and growing
orb[-1].label                  # 7: the end point
sol = m.auto.orbit(7)          # the orbit at label 7 as a run, one period
sol.t[-1]                      # 25.648

m.auto.save("switch.auto")
m.auto.reset()
d2 = m.auto.load("switch.auto")
len(d2)                        # 223: the points come back as they were
fold = m.auto.continue_from(d2.first("LP"), "two-parameter", second="a",
                            nmax=30)
fold[-1].value, fold[-1].value2   # lam = 1.144, a = 4.068
```

The label counting goes on from the first branch: the periodic branch has the
end point 7 and the fold curve the end point 8. The periodic branch has
the branch number `-2`, the steady branch `1`.

## The same in the app

Every action prints the Python line it corresponds to in the command window,
starting with `>>>`. Open `switch.ode`, then type these commands (they are
what the buttons do):

```commands
auto set parmin=-2 parmax=2 dsmax=0.05 nmax=500 npr=500
auto steady lam
auto set nmax=30
auto from 2 periodic
auto orbit 7
```

The command window answers with these lines (`xpplab-batch switch.ode --echo
-c ...` prints the same; only the settings that differ from the defaults are
written):

```text
>>> m.auto.set(dsmax=0.05, parmin=-2.0, nmax=500.0, npr=500.0)
>>> m.auto.equilibria("lam")
>>> m.auto.set(nmax=30.0)
>>> m.auto.continue_from(2, "periodic")
>>> sol = m.auto.orbit(7)
```

In the window, the same steps are: fill in **Par min**, **Par max**, **DS max**,
**Max steps** and **Label every**; press **Follow steady states**; select the
row `2 HB` in the table and press **Periodic orbits from this Hopf point**;
select the last point and press **Show this orbit as the run**. The commands
`auto save switch.auto` and `auto load switch.auto` are **Save...** and
**Open...**, and `auto from 3 two a` follows the fold (label 3) in `lam` and
`a`. The echoed lines of those two commands carry the full path of the file.
File > Save session as Python script saves the whole session as a script
that repeats it with the package.

## Where to go next

- [Bifurcation analysis with AUTO](auto.md) for the window, the settings and
  the limits of the boundary value and homoclinic starts.
- [Coming from XPPAUT](xppaut.md), because the AUTO here is XPPAUT's.
- [The `.ode` file format](odefile.md), for writing the model.
