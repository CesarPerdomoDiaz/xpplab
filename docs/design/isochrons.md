# Design: isochrons and phase reduction

Status: proposal. This note describes the work; no code exists yet. The
roadmap entry is *Isochrons and phase reduction* under M7.

## What users get

A stable limit cycle has a period T. Every point in its basin of attraction
has an *asymptotic phase*: the point of the cycle that the trajectory from it
ends up synchronised with. A level set of the asymptotic phase is an
*isochron*. Isochrons let a researcher read off, for any perturbation of any
size, how far the oscillator is shifted in phase.

XPP Lab already computes the adjoint along a cycle (`xppe_adjoint`,
`Model.adjoint`, `Model.phase_response`): its columns are the infinitesimal
phase response curves (iPRC) Z, the gradient of the asymptotic phase on the
cycle. It also computes the averaged interaction function H for two coupled
oscillators (`xppe_hfunction`, `Model.h_function`). This note adds three things:

1. **Asymptotic phase of a point.** Given a state x, the phase in [0, 1) of the
   cycle point it converges to, and the time it takes to get close.
2. **Isochrons in the phase plane.** For a two-variable model, curves of equal
   phase drawn with the cycle and the nullclines, a chosen number of them
   (for example 12 or 24 equally spaced in phase).
3. **The phase-reduced model.** The natural frequency ω = 2π/T, the iPRC Z(θ)
   and H(φ), put together as
   one object, with the equations they stand for:

    - one oscillator with a weak input p: dθ/dt = ω + ε Z(θ) · p(θ, t);
    - N weakly coupled identical oscillators:
      dθ_i/dt = ω + ε Σ_j H(θ_j − θ_i), and for two oscillators the phase
      difference φ = θ_2 − θ_1 obeys dφ/dt = ε (H(−φ) − H(φ)) = −2ε H_odd(φ);
    - the phase-locked states of a pair: the zeros of H_odd, and which of them
      are stable (H_odd'(φ) > 0 for a stable one with this sign convention).

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

For a point x, integrate forward with the current method until the state is
within a tolerance δ of the cycle (the distance to the stored cycle points,
refined by the nearest point along the cycle). Let s be the elapsed time and
θ_c the phase of the nearest cycle point. Then the phase of x is
θ_c − s/T, taken modulo 1. The error is of order δ.

With the iPRC the error can be made of order δ²: if γ(θ_c) is the cycle point,
then Θ(x) ≈ θ_c + Z(θ_c) · (x − γ(θ_c)) / T, where Z is the adjoint with
XPPAUT's normalisation (its dot product with the vector field is 1, in time
units). This uses the adjoint the engine already computes.

*Limits.* It is the most robust method, because it only integrates forward
and uses whatever method and tolerances the model has, stiff solvers
included. It costs one integration per point. Near the boundary of the basin
(a saddle or an unstable focus inside the cycle) the time s grows without
bound; the call stops at a time limit and reports that the phase is
undefined. Where the cycle is only weakly attracting, s is long and the
result is expensive.

### B. Isochrons by integrating backward from the cycle

The isochron through a cycle point γ(θ) is the stable manifold of that point
for the time-T map. Its tangent at γ is the hyperplane of the adjoint,
{v : Z(θ) · v = 0}; in a plane it is one direction. Take a small displacement
ε from γ(θ) along that tangent, on both sides, and integrate the *backward*
flow. Backward in time the isochron direction expands, so the computed curve
is attracted to the isochron and the method is stable in the direction that
matters; the cycle direction is neutral. A point reached after backward time
s from the isochron of phase θ lies on the isochron of phase θ − s/T, so a
few starting points on the cycle, each integrated for a long enough
backward time, cover all phases. The curves are drawn as a polyline,
adding points by arc length so that the stretching by the backward flow
(at the rate of the strongest transverse multiplier) does not leave gaps.

*Limits.* (1) In the plane this gives curves. For n > 2 an isochron is an
(n − 1)-dimensional surface; the same idea gives a one-parameter family of
curves, but not the surface, and the first version refuses n > 2 (the
asymptotic phase of a point, method A, works in any dimension). (2) The
backward flow can blow up or run to the unstable equilibrium in finite
time, so the integration stops at a bound and at a maximum arc length. (3)
If the cycle is strongly contracting (Floquet exponent large and negative),
the isochrons wind up tightly and spiral near the cycle; the arc-length
control must follow them or they alias. (4) Backward integration of a stiff
model is itself stiff in the other sense: the fast stable direction becomes
the fast unstable one, so a stiff model needs small steps and may fail.
Method A is the fallback there.

### C. Isochrons as level sets of the phase on a grid

Method A applied on a grid in a window of the phase plane, then contours of
the phase at the chosen levels. It needs no backward integration, so it is
safe for stiff models, and it also shows where the phase is undefined. It is
slower (a grid of 100 by 100 is 10 000 integrations) and the contours are
only as sharp as the grid. It serves as an independent check of method B and
as the fallback, not as the default.

### D. The phase-reduced model

Z comes from `xppe_adjoint` and H from `xppe_hfunction`; both exist and are
tested against XPPAUT. The new part is to collect them: ω from the period,
the tables Z and H on a common phase grid, H_odd and H_even, and the roots of
H_odd with their stability. For the roots, the C routine finds sign changes of
H_odd on the grid and refines each by a few secant steps on a periodic cubic
interpolation of H, so the app and Python print the same numbers.
Interpolation is the only new numerics; the integration is XPPAUT's.

*Limits.* H is the first-order average. It is valid for weak coupling (small
ε compared with the attraction rate to the cycle) and for oscillators with
close frequencies; with the coupling variable's own dynamics slow, or
with strong coupling, the locked states and their stability can differ from
the full model. The note and the documentation say so, and the tests compare
the locked phase with the full model for small ε.

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
- `xppe_phase_model(int n, const char *const *coupling)`: ω, then the
  table of phase, Z for each variable, H, H_odd, H_even, and the locked
  states with their stability.

Each function that integrates calls XPPAUT's own step routines for the
current method, as `xppe_lyapunov_integrate` does, and does not copy the
numerics. Each addition bumps `XPPE_API_VERSION` and `API_VERSION` in
`python/src/xpplab/_engine.py`. No file is opened by these functions, so
no new UTF-8 path handling is needed.

### Python (`python/src/xpplab/analysis.py`)

`Model.asymptotic_phase(point)`, `Model.isochrons(count=12, ...)` returning
a `Table` ready for `xpplab.Figure`, and `Model.phase_model(coupling)`
returning an object with `omega`, `period`, `z`, `h`, `h_odd`, `locked`.
These are thin ctypes wrappers like `adjoint` and `h_function`; they do
not compute, apart from reshaping tables. An MCP tool in
`python/src/xpplab/mcp/server.py` summarises the phase model for a coding
agent, as the existing phase-response tool does.

### App (`app/src/app_analysis.c`, `ui_analysis.c`)

- *Averaging* gains **Isochrons**: draws the curves over the phase plane
  in the same plot as the cycle, with a count and a time limit;
  the command is `isochrons`. Clicking a point in the plane shows its
  asymptotic phase (`phase x y` in the command line).
- *Averaging > Phase model* shows ω, Z and H and the locked states; the
  command is `phasemodel`.

Each action is wrapped in `echo_begin()`/`echo_end()` and writes the line
(`m.isochrons(count=12)`, `m.asymptotic_phase([x, y])`,
`m.phase_model({...})`) when it succeeds, so `tests/replay.py` can check
that the saved script writes the same CSV bit for bit.

### Documentation

A section in `docs/manual/analysis.md` and `docs/python/analysis.md`, an
entry in the API reference, and a notebook after the existing three
(examples: the Morris-Lecar cycle in `examples/` and a neural pair, with H).
The ROADMAP line links to this note and is ticked when the last piece lands.

## How it will be tested

Goldens must not change: `golden.py check` still reports
`{"nooutput": 10, "ok": 187}`, since all the work adds functions and touches
no existing numerics, and no file in `engine/xppaut/` needs an edit.

### Models with a closed-form answer

All in `tests/models/`, with tolerances that follow from the integration
tolerance and are written in the test.

1. **The radial isochron clock**, r' = r(1 − r), θ' = 1 (in Cartesian
   coordinates x = r cos θ, y = r sin θ). The cycle is r = 1, T = 2π, the
   asymptotic phase is θ/2π (mod 1), and the isochrons are the rays
   θ = const. The test checks the asymptotic phase at points inside and
   outside the cycle, that every point of an isochron curve has the phase of
   its ray, and that Z on the cycle is (−sin θ, cos θ).
2. **A lambda-omega system**, r' = r(1 − r²), θ' = ω_0 + q(1 − r²), with
   ω_0 and q chosen so the cycle is r = 1, T = 2π/ω_0. The asymptotic phase
   is Θ = θ − q ln r (in radians, rate ω_0), so the isochrons are the
   logarithmic spirals θ = Θ_0 + q ln r. The iPRC in XPPAUT's normalisation
   is Z = (−q r̂ + θ̂)/ω_0, which has the dot product 1 with the vector field
   ω_0 θ̂ on the cycle. The test checks phases at several radii (the
   curved isochrons are what the radial clock cannot show), the curve of
   method B, and Z.
3. **Weak coupling of two such systems**, with diffusive coupling. H is known
   in closed form (a sine plus a cosine of the phase difference, with
   amplitudes from q), so the test checks H, its odd part, the locked
   states, and that a full simulation of the coupled pair with small ε
   settles at the predicted phase difference.

### Consistency with the engine

- Z from `xppe_adjoint` agrees with the gradient of the asymptotic phase
  from method A, by finite differences across the cycle, on Morris-Lecar
  and on the lambda-omega model.
- The phase advances by the time elapsed: for a point x and its flow
  φ_t(x), the phase of φ_t(x) equals the phase of x plus t/T, modulo 1.
- Method B and method C draw the same curves where both work, to within the
  grid spacing.
- The phase model's H equals `xppe_hfunction` for the same coupling, bit for
  bit, and the locked-state stability agrees with a simulation of the pair.
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
3. **C API: the phase model** (`xppe_phase_model`: ω, Z, H, locked states),
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
