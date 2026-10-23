# Numerical methods

XPP Studio offers XPPAUT's fifteen integrators. Choose one in the toolbar,
in **Workspace > Numerics**, or with `@ meth=...` in the model file.

| Method | Kind | Use it for |
|---|---|---|
| rungekutta | fixed-step RK4 | A good default for smooth, non-stiff systems. |
| 5dp | adaptive Dormand-Prince 5(4) | Accurate results without tuning dt. |
| 83dp | adaptive Dormand-Prince 8(5,3) | High accuracy. |
| stiff | adaptive Rosenbrock | Stiff systems (fast and slow time scales). |
| cvode | CVODE (BDF or Adams) | Large or stiff systems. |
| 2rb | Rosenbrock 2(3) | Stiff systems, with a banded Jacobian option. |
| gear | Gear | Older stiff solver. |
| adams | Adams-Bashforth 4 | Smooth problems, fixed step. |
| euler | forward Euler | Noise (wiener), quick checks. |
| modeuler | modified Euler (Heun) | Noise, a little more accurate. |
| backeul | backward Euler | Very stiff problems, noise. |
| qualrk | quality-controlled RK4 | Adaptive RK4. |
| volterra | for integral equations | Models with `int{...}`. |
| ymp | symplectic | Mechanical (Hamiltonian) systems, long runs. |
| discrete | maps | Difference equations x(t+1) = f(x(t)). |

## The settings that matter

- **Total**: how long to run.
- **dt**: the step of fixed-step methods. For adaptive methods it is the
  interval between stored points.
- **Output every** (nout): store one point every n steps. Use it to keep long
  runs small.
- **Transient**: drop output before this time.
- **Bound**: stop when a variable grows larger than this, so a blow-up is
  caught instead of filling the screen with infinities.
- **Tolerances** (tol, atol): accuracy of adaptive methods.

## Is my system stiff?

If the solution needs a tiny dt to stay stable, or an adaptive method
crawls, the system is probably stiff: it mixes very fast and very slow
processes. Switch to `stiff` or `cvode`. Neuron models with fast spikes are
a common example.

## Delays and noise

Delay equations (`delay(x,tau)` or `x(t-tau)`) need `@ delay=` at least as
large as the longest delay. For noise (`wiener`), use `euler`, `modeuler` or
`backeul`: the adaptive methods do not handle random terms.
