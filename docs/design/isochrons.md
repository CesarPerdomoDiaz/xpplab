# Design: isochrons and phase reduction

Status: proposal. This note describes the work; no code exists yet. The
roadmap entry is *Isochrons and phase reduction* under M7.


## Conventions

Fixed here once; every formula below and every piece of code uses them. They
are XPPAUT's, because Z and H come from `adjoint` and `make_h` in
`engine/xppaut/adj2.c`.

- **The phase θ is in time units**, 0 ≤ θ < T, where T is the period. The
  cycle is γ(θ), and a point of the cycle moves as dθ/dt = 1.
- **The iPRC Z(θ)** is the gradient of the asymptotic phase on the cycle,
  normalised so that Z(θ) · f(γ(θ)) = 1 (XPPAUT's adjoint). It is in time
  units per unit of state.
- **One oscillator with a weak input p** obeys dθ/dt = 1 + ε Z(θ) · p(θ, t).
- **Weak coupling.** For the coupling term G(x_i, x_j) that the user writes
  (unprimed names are oscillator i, primed names oscillator j, as in
  `xppe_hfunction`), N identical oscillators obey
  dθ_i/dt = 1 + ε Σ_j H(θ_j − θ_i), with
  H(φ) = (1/T) ∫_0^T Z(t) · G(γ(t), γ(t + φ)) dt, and φ in time units, as
  `make_h` computes it (a mean over the stored points of the cycle).
- **H_odd and H_even** are the columns `Hodd` and `Heven` that XPPAUT
  returns: H_odd(φ) = (H(φ) − H(−φ))/2 and H_even(φ) = (H(φ) + H(−φ))/2.
  The phase model takes them from `xppe_hfunction`; it does not compute
  them again.
- **A pair.** With φ = θ_2 − θ_1, dφ/dt = ε (H(−φ) − H(φ)) = −2ε H_odd(φ).
  A zero of H_odd is a locked state; it is stable if H_odd'(φ) > 0 (for
  ε > 0). Zeros at 0 and T/2 always exist, by symmetry.
- **What users see as the asymptotic phase is θ/T in [0, 1).** To convert to
  radians, use 2πθ/T, and ω = 2π/T; this is the only place radians appear
  (except in models, such as the lambda-omega system, whose own equations use
  an angle).

## What users get

A stable limit cycle has a period T. Every point in its basin of attraction
has an *asymptotic phase*: the point of the cycle that the trajectory from it
ends up synchronised with. A level set of the asymptotic phase is an
*isochron*. Isochrons let a researcher read off, for any perturbation of any
size, how far the oscillator is shifted in phase.

XPP Lab already computes the adjoint along a cycle (`xppe_adjoint`,
`Model.adjoint`, `Model.phase_response`): its columns are the infinitesimal
phase response curves (iPRC) Z. It also computes the averaged interaction
function H for two coupled oscillators (`xppe_hfunction`, `Model.h_function`).
This note adds three things:

1. **Asymptotic phase of a point.** Given a state x, the phase of the cycle
   point it converges to, and the time it takes to get close.
2. **Isochrons in the phase plane.** For a two-variable model, curves of equal
   phase drawn with the cycle and the nullclines, a chosen number of them
   (for example 12 or 24 equally spaced in phase).
3. **The phase-reduced model.** The period T, the iPRC Z(θ) and the functions
   H, H_odd and H_even, put together as one object, with the equations they
   stand for (see Conventions): one oscillator with an input, N coupled
   oscillators, and for a pair the locked states and their stability.

The textbook background is Ermentrout & Terman, *Mathematical Foundations of
Neuroscience* (chapters on phase response curves and on weakly coupled
oscillators), and Izhikevich, *Dynamical Systems in Neuroscience* (chapter 10,
phase models and isochrons).

## Methods and their limits

Throughout, the cycle must be a **stable** hyperbolic limit cycle of an
ordinary differential equation, with the other Floquet multipliers inside the
unit circle. Delays, Volterra integrals, maps and noise are refused, as for
`xppe_lyapunov_integrate`. Phase reduction is a first-order theory in the
size of the perturbation: it says nothing about perturbations that throw the
state to another attractor, and it is not a replacement for simulating the
full model when the coupling is strong.

### A. Asymptotic phase by forward integration

For a point x, integrate forward with the current method until the state
φ_s(x) (the flow for the time s) is within a tolerance δ of the cycle. Let
γ(θ_c) be the nearest cycle point (nearest stored point, refined along the
cycle). Then, with θ_c in time units and the result mod 1,

    phase(x) = θ_c/T − s/T,   with error of order δ.

With the iPRC the error is of order δ². The correction belongs to the *end
state* φ_s(x), which is within δ of the cycle, not to the starting state x,
which can be far from it:

    phase(x) = θ_c/T + Z(θ_c) · (φ_s(x) − γ(θ_c))/T − s/T   (mod 1).

Z is the adjoint with the normalisation above, so Z · δx is a time shift.

*Limits.* It is the most robust method, because it only integrates forward
and uses whatever method and tolerances the model has, stiff solvers
included. It costs one integration per point. Near the boundary of the basin
(a saddle or an unstable focus inside the cycle) the time s grows without
bound; the call stops at a time limit and reports that the phase is
undefined. Where the cycle is only weakly attracting, s is long and the
result is expensive.

### B. Isochrons by integrating backward from the cycle

In the plane (n = 2) the cycle has one nontrivial Floquet multiplier λ, with
0 < |λ| < 1. It follows from the divergence of the vector field along the
cycle:

    λ = exp(∫_0^T div f(γ(t)) dt),

which is computed from the stored orbit by the trapezoid rule on the points
the adjoint already uses (or read from the Floquet multipliers of the
monodromy matrix that `adjoint` integrates). A single backward trajectory
from a point near the cycle is *not* an isochron: it crosses every isochron
as it goes, because its phase falls by s/T after the backward time s. The
construction below uses backward times that are whole periods, which keep the
phase exactly.

For each requested phase θ (in time units):

1. At γ(θ), the *linear isochron* is the line {v : Z(θ) · v = 0}, with
   direction v (a unit vector). Take a segment of starting points
   γ(θ) + d v with d running from ε to ε/|λ|, and the same on the other side
   (d from −ε to −ε/|λ|), one segment on each side.
2. Integrate every starting point *backward for exactly T, 2T, …, kT*. The
   backward flow for a whole period keeps the phase, so every image lies on
   the isochron of phase θ. Backward in time the transverse distance grows
   by 1/|λ| per period, so the image of the segment for the time T is the
   segment from ε/|λ| to ε/|λ|², which continues the first one. Joined in
   order, the segment and its images for T, 2T, … make up the isochron.
3. Where neighbouring images separate by more than a set arc length, add
   starting points to the segment (by integrating from the interpolated
   starting distance), and repeat until the arc length is respected.
4. Stop at a maximum arc length, at a bound on the variables, and at a
   maximum number of periods k.

Repeat for each θ. The method uses the Floquet multiplier only to choose the
segment; the phase of a start point on the linear isochron is exact up to
O(ε²), and that error does not grow in the backward integration. The error
*along* the isochron is stretched by 1/|λ| every period, so ε must be
small, and the refinement of step 3 is essential, not cosmetic.

*Limits.* (1) In the plane this gives curves. For n > 2 an isochron is an
(n − 1)-dimensional surface; the first version refuses n > 2 (the asymptotic
phase of a point, method A, works in any dimension). (2) The backward flow
can blow up or run to the unstable equilibrium in finite time, so the
integration stops at a bound and at a maximum arc length. (3) If the cycle
is strongly contracting (|λ| small), the isochrons wind up tightly and
spiral near the cycle, and the segment and the refinement must follow them.
(4) Backward integration of a stiff model is itself stiff in the other
sense: the fast stable direction becomes the fast unstable one, so a stiff
model needs small steps and may fail. Methods A and C are the fallback there.

### C. Isochrons as level sets of the phase on a grid

Method A applied on a grid in a window of the phase plane, then contours of
the phase at the chosen levels. It needs no backward integration, so it is
safe for stiff models, and it also shows where the phase is undefined. It is
slower (a grid of 100 by 100 is 10 000 integrations) and the contours are
only as sharp as the grid. It serves as an independent check of method B and
as the fallback, not as the default.

### D. The phase-reduced model

Z comes from `xppe_adjoint`, and H, H_odd and H_even from `xppe_hfunction`;
both exist and are tested against XPPAUT. The new part is to collect them:
the period T, the tables Z, H, H_odd and H_even on a common phase grid in
time units, and the roots of H_odd with their stability. For the roots, the C
routine finds sign changes of the XPPAUT `Hodd` column and refines each by a
few secant steps on a periodic cubic interpolation of it, so the app and
Python print the same numbers. The interpolation is the only new numerics;
the integration and the averages are XPPAUT's.

*Limits.* H is the first-order average. It is valid for weak coupling (small
ε compared with the attraction rate to the cycle) and for oscillators with
close frequencies; with strong coupling, the locked states and their
stability can differ from the full model. The documentation says so, and the
tests compare the locked phase with the full model for small ε.

### Not in the first version

Second-order phase reduction (phase-dependent corrections to Z), isochrons
of non-smooth models and of cycles with a nearly neutral second multiplier,
and periodically forced oscillators with a general input. They can be
added after the pieces below, on the same API.

## Where each part goes

CLAUDE.md requires that numbers the app and Python share are computed in the
C API; the app commands and the Python they echo call the same functions.

### C API (`engine/api/xppe_data.c`, declared in `xppe.h` and `xpp_instance.h`)

Following `xppe_adjoint` and `xppe_hfunction`, the new functions work on the
cycle and adjoint of the last run (one period of a stable cycle), and
return their results in the result table (`xppe_result_*`) or through
output arguments:

- `xppe_asymptotic_phase(const double *x, double delta, double tmax,
  double *phase, double *elapsed)`: method A for one state.
- `xppe_isochrons(int count, double length, int max_points, ...)`: method B;
  a table with columns `phase`, then the plotted pair of variables, one
  polyline per phase separated by a row of NaN. Method C as an option
  (`xppe_phase_grid`) in a later piece.
- `xppe_phase_model(int n, const char *const *coupling)`: T, then the
  table of phase (time units), Z for each variable, H, H_odd, H_even (the
  columns of `xppe_hfunction`), and the locked states with their stability.

Each function that integrates calls XPPAUT's own step routines for the
current method, as `xppe_lyapunov_integrate` does, and does not copy the
numerics. Each addition bumps `XPPE_API_VERSION` and `API_VERSION` in
`python/src/xpplab/_engine.py`. No file is opened by these functions, so
no new UTF-8 path handling is needed.

### Python (`python/src/xpplab/analysis.py`)

`Model.asymptotic_phase(point)`, `Model.isochrons(count=12, ...)` returning
a `Table` ready for `xpplab.Figure`, and `Model.phase_model(coupling)`
returning an object with `period`, `z`, `h`, `h_odd`, `locked`.
These are thin ctypes wrappers like `adjoint` and `h_function`; they do
not compute, apart from reshaping tables. An MCP tool in
`python/src/xpplab/mcp/server.py` summarises the phase model for a coding
agent, as the existing phase-response tool does.

### App (`app/src/app_analysis.c`, `ui_analysis.c`)

- *Averaging* gains **Isochrons**: draws the curves over the phase plane
  in the same plot as the cycle, with a count and a time limit;
  the command is `isochrons`. The asymptotic phase of a point is the
  command `phase x y`, and a separate tool mode in the plot (off by
  default) lets the user click a point to read its phase. A click in the
  phase plane already starts a trajectory from that point (XPPAUT's mouse
  initial conditions, `app/src/ui_plots.c`); that behaviour does not
  change, and the tool mode is a different, explicit choice.
- *Averaging > Phase model* shows T, Z, H, H_odd and the locked states; the
  command is `phasemodel`.

Each action is wrapped in `echo_begin()`/`echo_end()` and writes the line
(`m.isochrons(count=12)`, `m.asymptotic_phase([x, y])`,
`m.phase_model({...})`) when it succeeds, so `tests/replay.py` can check
that the saved script writes the same CSV bit for bit.

### Documentation

A section in `docs/manual/analysis.md` and `docs/python/analysis.md`, an
entry in the API reference, and a notebook after the existing three
(examples: the Morris-Lecar cycle in `engine/xppaut/ode/lecar.ode` and a neural pair, with H).
The ROADMAP line links to this note and is ticked when the last piece lands.

## How it will be tested

Goldens must not change: `golden.py check` still reports
`{"nooutput": 10, "ok": 187}`, since all the work adds functions and touches
no existing numerics, and no file in `engine/xppaut/` needs an edit.

### Models with a closed-form answer

All in `tests/models/`, with tolerances that follow from the integration
tolerance and are written in the test.

1. **The radial isochron clock**, r' = r(1 − r), θ' = 1 (in Cartesian
   coordinates x = r cos θ, y = r sin θ). The cycle is r = 1, T = 2π, so the
   phase in time units is Θ = θ (mod 2π) and the asymptotic phase seen by
   the user is θ/2π. The isochrons are the rays θ = const. The Floquet
   multiplier is λ = exp(−2π) (at r = 1, div f = −1 along the cycle). Z on
   the cycle is (−sin θ, cos θ), with Z · f = 1. The test checks the
   asymptotic phase at points inside and outside the cycle, that every point
   of an isochron curve has the phase of its ray, and Z.
2. **A lambda-omega system**, r' = r(1 − r²), θ' = ω_0 + q(1 − r²), so the
   cycle is r = 1 and T = 2π/ω_0. Derivation: if r' = r g(r) and θ' = w(r),
   then Θ = θ − ∫_1^r (w(ρ) − ω_0)/(ρ g(ρ)) dρ has dΘ/dt = ω_0 along every
   trajectory. Here (w − ω_0)/(ρ g) = q/ρ, so Θ = θ − q ln r. In time units
   the phase is Θ/ω_0, and the user's asymptotic phase is
   (θ − q ln r)/2π (mod 1). The isochrons are the logarithmic spirals
   θ = Θ_0 + q ln r. The gradient of Θ at r = 1 is −q r̂ + θ̂, and f = ω_0 θ̂
   there, so Z = (−q r̂ + θ̂)/ω_0, with Z · f = 1. The Floquet multiplier is
   λ = exp(−2T) = exp(−4π/ω_0) (div f = −2 at r = 1). The test checks phases
   at several radii (the curved isochrons are what the radial clock cannot
   show), the curves of method B, and Z.
3. **Weak coupling of two lambda-omega systems**, coupled through both
   variables, G = x_j − x_i (primed names are oscillator j, so the coupling
   formulas are `x'-x` and `y'-y`). On the cycle γ(t) = (cos ω_0 t,
   sin ω_0 t), with a = ω_0 t and ψ = ω_0 φ,

       ω_0 Z · G = −q [cos a (cos(a+ψ) − cos a) + sin a (sin(a+ψ) − sin a)]
                   + [−sin a (cos(a+ψ) − cos a) + cos a (sin(a+ψ) − sin a)]
                 = −q (cos ψ − 1) + sin ψ,

   which does not depend on a, so the mean over the period is the same:

       H(φ) = [sin(ω_0 φ) − q (cos(ω_0 φ) − 1)] / ω_0,
       H_odd(φ) = sin(ω_0 φ) / ω_0,  H_even(φ) = −q (cos(ω_0 φ) − 1)/ω_0.

   H_odd has zeros at 0 and T/2. H_odd'(0) = 1 > 0, so synchrony is stable
   for ε > 0; H_odd'(T/2) = −1, so antiphase is unstable. The test checks H,
   H_odd, the locked states, and that a full simulation of the coupled pair
   with small ε settles at φ = 0. These two locked states are only
   synchrony and antiphase, which exist by symmetry in any model, so they do
   not show that the roots are found in general.
4. **A pair of Morris-Lecar cells** (`engine/xppaut/ode/lecar.ode`) is the
   test of a locked state that is neither 0 nor T/2. **It is not specified
   yet.** The cycle parameters and the coupling formulas (for example
   `V'-V` for V and `0` for the other variable) are chosen in piece 3, which
   runs `Model.h_function` on candidate sets until H_odd has an extra zero;
   this note does not claim that one exists. Piece 3 then writes the
   parameters and formulas into the test and into this note, and the test
   compares the predicted locked phase and its stability with a full
   simulation of the pair for small ε.

### Consistency with the engine

- Z from `xppe_adjoint` agrees with the gradient of the asymptotic phase
  from method A, by finite differences across the cycle, on Morris-Lecar
  and on the lambda-omega model.
- The phase advances by the time elapsed: for a point x and its flow
  φ_t(x), the phase of φ_t(x) equals the phase of x plus t/T, modulo 1.
- Method B and method C draw the same curves where both work, to within the
  grid spacing.
- The phase model's H, H_odd and H_even equal the `xppe_hfunction` columns
  for the same coupling, bit for bit, and the locked-state stability agrees with a simulation of the pair.
- Refusals: delays, noise, a run that is not one period, and an unstable
  cycle give an error and not a number.

### Python, app and replay

Python tests in `python/tests` check the wrappers against the C numbers.
`tests/app_check.py` runs the commands `isochrons`, `phase` and
`phasemodel`, and `tests/replay.py` covers the echo. The sanitizer and
valgrind jobs run the C tests as before.

## The pieces, one pull request each

1. **C API: asymptotic phase** (`xppe_asymptotic_phase`), with the radial
   clock and lambda-omega tests, the API version bump and the Python
   signature.
2. **C API: isochrons in the plane** (method B), with the closed-form
   tests; refuses n > 2 for now.
3. **C API: the phase model** (`xppe_phase_model`: T, Z, H, locked states),
   with the coupled lambda-omega test and the comparison with a full
   simulation.
4. **Python and MCP**: `asymptotic_phase`, `isochrons`, `phase_model`, the
   MCP tool, the reference docs.
5. **App**: the commands, the plot, the echo lines and the replay and
   `app_check.py` tests.
6. **Documentation**: the manual section, the Python page and a notebook;
   ticks the roadmap line.
7. **Later, optional**: isochrons on a grid (method C) as the stiff-model
   fallback, and isochrons of cycles in more than two variables.

Pieces 1 to 3 do not depend on each other except for the shared table
layout, which piece 1 fixes; 4 needs 1 to 3, 5 needs 4, and 6 comes last.
