# Design: a compiled engine for large models

Status: proposal. No code exists. The roadmap entry is *A compiled engine
for large models, checked against XPPAUT's* under M11.

**Recommendation, in one paragraph.** Build a narrow version, in stages, and
stop at the first stage whose test fails. A compiled right-hand side makes
the right-hand side about 4 to 7 times faster, so it pays where the
right-hand side is most of the run: explicit methods (Runge-Kutta, Dormand-Prince, ...) on networks, and
any model with `sum` coupling (4 to 5 times faster in total). It does not pay with
CVODE or Gear on large models: there the integrator's linear algebra takes
87 to 93% of the time, and a compiled right-hand side would give at most
1.08 times. For those, turn on CVODE's banded solver and order the variables
so that the Jacobian is banded; that gave a 30-fold saving in the integrator
in the measurement below. The compiled path is opt-in, calls XPPAUT's own
math functions, and is accepted only when it reproduces the interpreter bit
for bit. Use the system's C compiler when there is one; embed no compiler
until a prototype shows that the criterion holds on all three platforms.

## Conventions

- **RHS** is the right-hand side: `my_rhs` in `engine/xppaut/my_rhs.c`, which
  sets the variables, evaluates the fixed variables, the networks, the DAEs
  and the external code, and then each equation's postfix form.
- **Time is wall-clock seconds** on the machine below. **µs/call** is the
  mean time of one call of `my_rhs`.
- **f** is the share of the whole run (process start to exit, including
  parsing the file and writing the output file) spent in the RHS.
  **f_loop** is its share of the integration loop only.
- **Amdahl bound** is 1/(1 − f): the speed-up of the whole run if the RHS
  cost nothing. With a RHS that is s times faster, the speed-up is
  1/((1 − f) + f/s).

## 1. Is it worth it? Measure first

### How it was measured

- **Machine:** Intel Xeon E5-2699 v4, 2.2 GHz, 22 cores (one used),
  Windows Server 2025, MSYS2 UCRT64 GCC 16.2.0, `cmake -DCMAKE_BUILD_TYPE=Release`
  (the engine's own flags: `-O2 -ffp-contract=off`). One machine and one
  compiler: the shares carry over to other machines, the absolute times do not.
- **Timers.** A scratch copy of the engine, not committed, read
  `clock_gettime(CLOCK_MONOTONIC)` (58 ns per call here) around
  - `my_rhs` (the RHS),
  - the integrator call in `ode_int` (`engine/xppaut/integrate.c`, from
    `switch(METHOD)` to the line "START POST INTEGRATE STUFF"; it contains
    the RHS time, so the integrator's own share is that minus the RHS),
  - the per-step block after it (`extra` for the auxiliaries, NaN and bound
    checks, storage).

  Everything else in the process, which is parsing the `.ode` file, start-up
  and writing the output file, is the wall time of the process minus these.
  Each model was run three times with `xppaut-headless model.ode -silent
  -outfile model.dat`; the median run by wall time is shown. For the 2-cell
  model the two timer calls per RHS call (about 0.1 µs) are a large part of
  its 0.50 µs.
- **A fixed cost** to remember: with `total=0.05` (one step) the non-stiff
  2-cell, 100-cell and Kuramoto-100 runs took 0.02 to 0.03 s. That is
  parsing and start-up; the rest of "elsewhere" in the table is the output
  file. For the 1000-cell models the one-step runs took 0.25 s
  (non-stiff) and 0.5 to 0.8 s (Kuramoto, CVODE), but they include the first
  steps, so the table's "elsewhere" column (0.1 to 0.6 s) is the better
  figure.

### The models

Built by a throwaway script, not part of the repository. The cell is a
FitzHugh-Nagumo oscillator, `v' = v − v^3/3 − w + I + coupling`,
`w' = eps (v + a − b w)`, with nearest-neighbour coupling through `v[j+1]`
and `v[j-1]`, written with arrays (`v[1..98]'=...`).

| Name | Model | Method | Run |
|---|---|---|---|
| fhn2 | 2 cells | Runge-Kutta, dt 0.05 | to t = 1000 |
| ns100 | 100 cells (200 variables), eps 0.08, d 0.5 | Runge-Kutta | t = 100 |
| st100 | 100 cells, eps 0.001, d 50 (stiff) | CVODE | t = 100 |
| sg100 | the same | Gear | t = 100 |
| ns1000 | 1000 cells (2000 variables), non-stiff | Runge-Kutta | t = 20 |
| st1000 | 1000 cells, stiff | CVODE | t = 20 |
| sg1000 | 1000 cells, stiff | Gear | t = 2 |
| ku100 | `kuramot100.ode` of XPPAUT's examples: 100 phases, `sum(0,99)of(...)` coupling, table | Runge-Kutta | t = 10 |
| ku1000 | the same with 1000 phases | Runge-Kutta | t = 1 |

Tolerances 1e-6, `bound=1000`. All runs finished without errors.

### Results

| Model | Wall (s) | RHS (s) | f | Integrator, not RHS (s) | Per step: aux, checks, storage (s) | Elsewhere: parse, output file (s) | µs/call | Amdahl bound |
|---|---|---|---|---|---|---|---|---|
| fhn2 | 0.160 | 0.040 | 25% | 0.010 | 0.006 | 0.105 | 0.50 | 1.33 |
| ns100 | 0.401 | 0.159 | 40% | 0.005 | 0.005 | 0.232 | 19.8 | 1.66 |
| st100 | 0.322 | 0.016 | 5% | 0.028 | 0.005 | 0.274 | 21.9 | 1.05 |
| sg100 | 1.085 | 0.498 | 46% | 0.388 | 0.004 | 0.195 | 15.8 | 1.85 |
| ns1000 | 0.973 | 0.326 | 34% | 0.008 | 0.018 | 0.620 | 204 | 1.50 |
| st1000 | 9.477 | 0.682 | 7% | 8.172 | 0.012 | 0.611 | 165 | 1.08 |
| sg1000 | 76.1 (one run) | 5.226 | 7% | 70.614 | 0.003 | 0.232 | 161 | 1.07 |
| ku100 | 0.736 | 0.704 | 96% | 0.000 | 0.000 | 0.032 | 880 | 23 |
| ku1000 | 7.215 | 7.094 | 98% | 0.000 | 0.000 | 0.120 | 88 670 | 60 |

Inside the integration loop (RHS, integrator and per-step block, without
parsing and output) the RHS share f_loop is: fhn2 71%, ns100 94%, st100 33%,
sg100 56%, ns1000 93%, st1000 8%, sg1000 7%, ku100 and ku1000 above 99%.

What the table says:

1. **Short runs of small and mid-size models are not RHS-bound at all.**
   For 100 cells with an explicit method 58% of the 0.4 s is parsing and
   writing 2000 rows of 201 numbers. Compiling the RHS cannot touch that.
2. **With an explicit method the loop is the RHS** (f_loop 93 to 94%), and
   the share of the whole run grows with the run length.
3. **With `sum` coupling the RHS is everything** (96 to 98%): each call
   evaluates N² terms.
4. **With CVODE or Gear on 1000 cells, the integrator takes 86 to 93%.**
   The RHS is 7%. The dense Jacobian and its LU factorisation cost
   O(N³) at 2000 variables. A faster RHS would save 7% at most.

### How much faster is a compiled RHS?

A throwaway C program with the same expressions in the same order, built
with `gcc -O2 -ffp-contract=off -fno-builtin`, with `pow` and `sin` as
library calls like the interpreter (`^` is `pow` in `fun2[5]`), and without
timers, took:

| RHS | Interpreter (µs/call) | Straight C (µs/call) | s |
|---|---|---|---|
| 2 cells | 0.50 (with ~0.1 timer) | 0.067 | about 6.5 |
| 100 cells, chain | 19.8 | 3.63 | 5.5 |
| 1000 cells, chain | 204 | 31.7 | 6.4 |
| Kuramoto, 100 | 880 | 135 | 6.5 |
| Kuramoto, 1000 | 88 670 | 18 905 | 4.7 |

This straight C is a lower bound on the cost of a generated RHS, which will
also copy variables in and out of XPPAUT's arrays. Expect s between 4 and 6.

### Largest speed-up of the whole run

With s = 5.5 and the f of the table, Amdahl's law gives:

| Model | Bound (RHS free) | With s = 5.5 |
|---|---|---|
| fhn2 | 1.33 | 1.3 |
| ns100, ns1000 | 1.5 to 1.7 | 1.4 to 1.5 |
| st100, st1000, sg1000 | 1.05 to 1.08 | 1.04 to 1.06 |
| sg100 | 1.85 | 1.6 |
| ku100, ku1000 | 23, 60 | 4.4 to 5.2 |

For runs long enough that parsing and writing are small (take f_loop), a
non-stiff network gains about 4 to 5 times. The desktop app keeps the points in memory instead of writing a file, so
its share is nearer f_loop than f. That was not measured, and neither were
AUTO runs or the graphics.

### What would help instead

- **Stiff networks: a banded Jacobian.** XPPAUT's CVODE has a banded solver
  (`bandup` and `bandlo`, `cv_bandflag` in `engine/xppaut/numerics.c`). A
  test with 1000 stiff cells written with the variables interleaved
  (`v0, w0, v1, w1, ...`, 2000 equations written out one by one) and
  `@ bandup=2,bandlo=2` took 0.74 s in all, of which 0.013 s were in the
  integrator beyond the RHS. The same file without the band options took
  1.73 s with 0.41 s in the integrator (the first two columns of the last row
  are the same, to the digits written). The same system with the arrays ordered
  `v0..v999, w0..w999` took 9.5 s, 8.2 s of it in the integrator. So the
  order of the variables changes the cost by a factor of 20 even for the
  dense solver, and the band option removes the rest. Neither is documented
  for network models; a section in the manual on writing a network so that
  its Jacobian is banded would help more students than a compiler.
- **Output:** for runs like ns100, 58% was the output file. Writing the
  numbers is not a compiler's business, but is cheap to improve.
- **Interpreter:** not measured here. The RHS of a 100-cell chain costs
  about 100 ns per equation; some of that may be removable inside
  `eval_rpn` without changing a number. A profile of `eval_rpn` is the first
  piece of work below, because it costs less than a compiler.

**Conclusion of point 1.** The compiled RHS is worth building for explicit
methods and `sum` networks of 100 variables and more (4 to 5 times), and
not for stiff models (at most 1.08 times). For small models (fhn2) it gives
at most 1.3 times.

## 2. The rule about numbers

The interpreter stays the default. `golden.py check` keeps reporting
`{"nooutput": 10, "ok": 187}` with no change in any of its checksums. The
compiled path is opt-in: with the option off nothing changes, not a byte of
output, and the code of the interpreter is not touched.

### What "checked against XPPAUT's" means

**Criterion.** For every example model that runs under the golden check,
the compiled path, run the way `golden.py` runs the interpreter, writes
output whose rows are equal to the interpreter's: the same SHA-256 over the
rows' numbers (`digest()` in `tests/golden.py`), with no tolerance. Because
a chaotic or long run can turn a one-bit difference into a different
answer, a second, finer test is made without integration: at the stored
states of the golden run and at 100 random points near them, `xppe_rhs`
(`engine/api/xppe.h`) with the compiled path and with the interpreter must
return the same 64-bit patterns for every component, for every model, and
likewise the auxiliaries and the fixed variables.

A model that fails the second test on some platform is not given a
tolerance silently. It is listed in the note and in the docs as "not
bit-identical on <platform>", with the largest difference seen, and the
compiled path refuses to run it on that platform (it falls back to the
interpreter, see point 4). A relative tolerance on trajectories is not a
criterion: for a stiff or chaotic model it measures the model, not the code.

### Same order of operations

`eval_rpn` (`engine/xppaut/parserslow2.c`) is a stack machine: each
postfix symbol does one operation on doubles, in the order of the parsed
list. The generated code must be one statement per symbol, in that order,
with a named temporary for each result: `t3 = t1 * t2;` and never
`a*b+c*d` written in one expression. Rules that follow from reading
`eval_rpn`:

- Addition, subtraction and multiplication are the plain operations
  (`FUN2TYPE`, entries 0, 1 and 2). **Division is not plain**: if the
  divisor is exactly zero it is replaced by `DOUB_EPS` (entry 3). The
  generated code must do the same.
- Numbers are not re-parsed: `NUMSYM` carries the 64 bits of the double, so
  the generated code must write literals that are exact, with
  `%a` (hexadecimal floats) or 17 significant digits.
- `^` is `pow`, `mod` is the engine's `pmod`, `max` and `min` are the
  engine's functions, the comparisons return 1.0 or 0.0, and `if(...)then(...)else(...)` evaluates only
  the chosen branch (`MYIF`, `MYTHEN`): a branch the interpreter skips must
  not run, because it could have a side effect (`ran()`, `delay`).
- `sum(lo,hi)of(...)` adds in increasing order from 0.0: one sequential
  accumulator, not a vectorised or reassociated sum.

### Floating-point contraction

The engine is built with `-ffp-contract=off` (`engine/CMakeLists.txt`). On
arm64 Apple Clang otherwise fuses `a*b - c` into one fused multiply-add and
the numbers move; this was found when the macOS build was first compared with
unmodified XPPAUT (`docs/development/macos.md`). The generated code needs
the same, and more:

- `-ffp-contract=off` on every compiler, written in the generator's command
  line and again as `#pragma STDC FP_CONTRACT OFF` at the top of the file
  (GCC ignores that pragma, Clang honours it, so both are there).
- No `-ffast-math`, `-Ofast`, `-funsafe-math-optimizations`,
  `-fassociative-math`, `-freciprocal-math`; `-fno-math-errno` is not needed.
- No x87 on 32-bit x86: only SSE2 and arm64 are supported
  (`FLT_EVAL_METHOD` 0), which are the three platforms of the release.
- `-fno-builtin`, so the compiler does not fold `sin(0.5)` at compile time
  with its own correctly rounded library (MPFR in GCC) where the run-time
  library may differ in the last bit.

If the compiler offers no switch for contraction (some embedded ones simply
do not fuse, TinyCC among them), the test above is what decides.

### The math library

The interpreter calls the library through the tables `fun1[]` and `fun2[]`
(`one_arg()` and `two_args()` in `parserslow2.c`): `sin`, `cos`, `tan`,
`exp`, `log`, `log10`, `pow`, `atan2`, `erf`, `lgamma`, `poidev`, the Bessel
functions and so on. Libraries differ in the last bit: glibc, the Windows
UCRT and Apple's libm do not give the same `sin` or `pow` for every input.
**So the generated code does not link its own libm.** It receives the engine's
table as a pointer and calls through it. Then every library call is the
engine's own function, in the same process, on every platform, and the
library question disappears for the compiled path. Only operations that are exactly specified by
IEEE 754 (`+ − × ÷`, `sqrt`, `fabs`, `floor`) may be written inline. Calling through a pointer costs a few
nanoseconds per call, small beside `sin`.

### Is bit-identical achievable?

Yes on all three platforms, by the rules above, because every operation is
either an IEEE basic operation, which is the same everywhere without
contraction, or a call of the engine's own function. What is not within the
rule: the compiler on the student's computer is not ours, so a compiler
that contracts despite the switch, or evaluates in a wider format, is
caught by the test and not by argument. Therefore **the first run of a
generated library checks itself**: the engine compares the compiled RHS with
the interpreter at the initial state and at a few perturbed states before
the first step (cost: a few evaluations); on any difference it prints a
message and uses the interpreter. The tolerance where it is not achievable
is zero, and the consequence is the fallback, not a looser test.

Not re-measured here: contraction on Linux and arm64 macOS. The finding about
Apple Clang is from the earlier macOS work; this machine is x86-64 Windows.

## 3. How the code gets compiled on a student's computer

Most students have no compiler: Windows has none, macOS has one only after
Xcode's command-line tools are installed, and Linux laptops often have none
until someone installs one. `docs/vision.md` says "no X server, no compiler,
no terminal". The options:

| | Embedded compiler (TinyCC's `libtcc`) | The system's compiler, if present | Pre-compiled pieces |
|---|---|---|---|
| Needs from the student | Nothing | A compiler on `PATH` | Nothing |
| Licence | LGPL-2.1 (see below) | Not ours: not shipped | Ours |
| Size | A few hundred KB per platform (to be measured) | 0 | One library per variant |
| Code quality | No optimiser; fast to compile | Full optimiser (`-O2`) | Full optimiser, ahead of time |
| arm64 macOS | **Not confirmed** (see below) | Apple Clang, yes | Yes, built in CI |
| Handles any model | Yes | Yes | No: only fixed model families |

**Embedded compiler.** TinyCC is a small C compiler with a library interface
(`libtcc`) that compiles a string to memory. It compiles in milliseconds.
Its code is unoptimised, so the gain over the interpreter is smaller than
s above: not measured here, since no TinyCC is installed on this machine,
and it must be measured before this option is chosen. Licence: LGPL-2.1.
That is compatible with XPP Lab's GPL-2.0-or-later: LGPL-2.1 section 3
allows applying the GPL to a copy of a library, and the project already ships LGPL code
(libiconv, in `THIRD-PARTY-NOTICES.md`) with its licence text in `licenses/`.
Upstream was also working on relicensing under MIT; check `COPYING` and
`RELICENSING` in the revision chosen. The open risk is **arm64 macOS**: a web
search found a 2020 report of a Mach-O port tested on macOS 10.14, and a 2022 report that
on an M1 TinyCC produced only x86-64 code, usable under Rosetta; it did not find a current
statement of Apple Silicon support. It must be tested on an arm64 Mac first. Executing generated machine code in
memory on Apple Silicon also needs the hardened-runtime entitlement for JIT
(`com.apple.security.cs.allow-jit`) and `MAP_JIT` memory, while the signing plan in
`docs/development/macos.md` has only `disable-library-validation` so far.
**It needs a fallback for platforms it does not support.**

**The system's compiler.** The generator writes a C file and runs `cc`,
`clang` or `gcc` (or `cl` on Windows) to make a shared library, which the
engine loads with `dlopen` (the engine already does this for `export`:
`my_fun()` in `engine/xppaut/extra.c`, with `engine/platform/compat/dlfcn_win.c`
on Windows). No size and no licence cost, and the best code. It only helps
people who have a compiler, which is a researcher or a developer, not the
student the project is for. On macOS the first call of `cc` opens the
installer for Xcode's tools, which must not happen behind the user's back:
check with `xcode-select -p` first.

**Pre-compiled pieces.** Compile ahead of time what the user cannot: for
example the library's evaluator (a faster interpreter, point 1) and
specialised kernels for common network forms (a chain, a mean-field `sum`).
These cannot cover an arbitrary model, but nothing needs to be loaded or
generated, and they are tested in CI like any other code. A faster
interpreter is the same idea taken to the end, with no code generation.

**Recommendation.** Use the system's compiler first, because it needs no
decision about licences, size or Apple's entitlements and tests the idea.
Add the embedded compiler only if (a) the prototype shows the bit-identical
criterion holds with TinyCC on Windows, Linux, and arm64 and x86-64 macOS, (b)
its speed-up is at least 3 times on ku100, and (c) the macOS entitlements
are settled. Because all of the following must hold, the decision is made
at the end of piece 5 below, with the measurements.

**When no compiler is available.** The run uses the interpreter, as if the
option were off, and says why in one line: "Compiled engine not used: no C
compiler found; running the interpreter (results are the same)." In the
app it is shown once in the log and on the Run page; in Python it is a
`warnings.warn` (not an exception), and in the batch tool a line on standard error. The same applies when
the compiler fails, when the self-check of point 2 fails, and when the
model is refused (point 4). The result of a run never depends on whether
compiling worked: that is the property that keeps the numbers safe.

## 4. What is compiled, and from what

**Source of truth: XPPAUT's parser.** The generator reads the postfix
arrays that the engine has already built, `my_ode[i]` (equations,
fixed variables and auxiliaries) and the other `int *` command arrays, and
turns each symbol into a C statement. It does not read `.ode` text, so a
parsing question has one answer. What the symbols are is defined in
`engine/xppaut/parserslow.h` and `parser.h`: `NUMSYM`, `ENDEXP`, and the
codes `COM(type, index)` with `MAXTYPE = 20000000`: `CONTYPE`
(parameters and constants), `VARTYPE` (variables), `FUN1TYPE`, `FUN2TYPE`,
`UFUNTYPE`, `USTACKTYPE`, `TABTYPE`, `VECTYPE`, `NETTYPE`, `KERTYPE`,
`SCONTYPE` and `SVARTYPE`.

What the generated code has to handle, from reading `engine/xppaut/`:

| Feature | Where in the engine | What generated code does |
|---|---|---|
| Order of evaluation | `my_rhs()` in `my_rhs.c`: variables set, fixed variables `NODE..NODE+FIX_VAR`, `eval_all_nets()`, `do_daes()`, `do_in_out()`, then the ODEs | Reproduce this order exactly. A first version may compile only the fixed variables and the ODE loop, and call the engine's own routines for the rest. |
| Fixed variables and auxiliaries | Fixed variables are in `my_ode` after the ODEs and are stored in `variables[i+1]` by `my_rhs`, `set_fix_rhs`, `fix_only`; auxiliaries are evaluated by `extra()` for the output | Generate three entry points: the RHS, `fix_only`, `extra`. They share the same statements. |
| Parameters | `CONTYPE` reads `constants[in]` at run time | Read `constants[k]` through a pointer, never fold it. A parameter changed between runs (the `set` commands, `many`, `range`, the API's `values`) needs no recompiling. |
| Variables and time | `variables[]`, with `variables[0]` = t | The same array, passed in. |
| `sum(lo,hi)of(...)`, the index `i'` | `SUMSYM`, `ENDSUM`; the index is `constants[SumIndex]`, set before every term | A loop with one accumulator from 0.0, and the index variable written to `constants[SumIndex]` because user functions and `shift` read it. |
| `shift(x0, n)` and `ishift` | `do_shift()`: `(variable index) + n`, with a bounds test (`> MAXODE` gives 0.0, negative gives 0.0) | The same test, on the same `variables[]`. |
| Arrays `[j=0..N]` | Expanded by the loader into one equation per element | Nothing special: the generator sees N equations. A 1000-cell model is 1000 statements per equation form, so the generated file is long (compile time is the cost of the system compiler for 2000 equations: not measured, to measure in piece 2). |
| Tables | `TABTYPE`, `lookup()` in `tabular.c`, with linear, cubic or x-y interpolation and an "undefined" case that returns 0.0 | Call the engine's `lookup`. Do not inline it. |
| User functions | `UFUNTYPE`: arguments are pushed on `ustack`, `ENDFUN` pops them, the body is `ufun[in]` evaluated recursively | Inline each call as a block, or call a generated function; arguments in named temporaries. Recursion in `ufun` is not generated in the first version. |
| Delays | `ENDDELAY`, `ENDDELSHFT`, `do_delay()`, `get_delay()` in `delay_handle.c`; `DelayFlag`, `del_stab_flag`, `delay_stab_eval()` | Call the engine's functions. The interpolation history stays in the engine. First version: refuse models with delays that use `del_stab_flag` (the stability branch). |
| Markov variables | `markov.c`: `update_markov()` evaluates the fixed variables and then `new_state()` with `ndrand48()` after each step; the transition rates are their own postfix arrays | Not in the RHS. Compile the rate expressions at most; the random draw order must not change. First version: refuse. |
| `global` flags | `flags.c`: `compile_flags()` makes `comcond` and `comrhs` arrays; events change variables and parameters, and `one_flag_step()` finds the crossing | The conditions and events are evaluated between integrator steps, not inside the RHS. They stay interpreted in the first version, and the compiled RHS must see the parameters they change, which it does because it reads `constants[]`. |
| Networks, `vector_value`, `network_value` | `eval_all_nets()`, `simplenet.c` | Call the engine's functions. |
| `export` and DLLs | `do_in_out()`, `my_fun()` in `extra.c` | Keep calling it in the same place. |
| Random numbers | `ran`, `normal` and `poidev` use the engine's `ndrand48()` state | Call the engine's functions in the same order, so the stream is the same. |
| `SETVAR`/`ENDSET` (`set(...)`) | `ENDSET` writes into `variables[]` from inside an expression | Refused in the first version (a side effect in an expression). |
| Mouse and animation constants (`mouse_x`, ...) | Ordinary entries of `constants[]` | Read like parameters. |

**First version: what is refused or falls back to the interpreter.** The
whole model falls back (with the message of point 3), not one equation,
when it has: a Markov variable, a `global` flag that is not only a
parameter or variable assignment, `set(...)`, a recursive user function,
delays with the stability flag, a Volterra kernel (`KERTYPE`), or a model
with more than one data structure the generator does not know (new symbol
types found by a check that fails when `parser.h` gains a code the
generator has no case for). Models that use `export` keep it. It also falls back for the
integration methods in which the RHS is not the main cost (CVODE, Gear, the
stiff solvers): the option is accepted, there is nothing to gain, and a
message says so. The one-step methods and `dp5`, `dp83`, `rb23`, discrete,
Euler, `modified Euler` and Runge-Kutta are compiled.

Which numbers are used by AUTO: `xppe_auto.c` calls the same `my_rhs`, so a
compiled RHS speeds up the Jacobian by finite differences too. Not measured here; to test in piece 6.

## 5. What users see

- **Python:** `m.compile()` returns `True` or `False` and the reason;
  `xs.load(path, compiled=True)` does it at load. `m.compiled` tells
  whether the compiled path is in use. Off by default.
- **App:** a check box *Compiled engine* in the numerics window, and the
  command `compile on|off` (a reference page like the others, `docs/reference/`).
  The state is shown in the status line: *interpreter* or *compiled*.
- **Batch tool:** `xppaut-headless --compiled model.ode -silent`. Without the option nothing changes.
- **The echo line** (`app/src/echo.c`): `m.compile()` is written when the
  action succeeds, so the saved script reproduces the run; `tests/replay.py`
  gets a case that fails if the line is missing.
- **The cached file:** the generated C source and the library are kept in
  the user's cache directory (`%LOCALAPPDATA%\xpplab\compiled` on Windows,
  `~/Library/Caches/xpplab` on macOS, `$XDG_CACHE_HOME/xpplab` on Linux),
  named by the SHA-256 of: the postfix arrays, the symbol tables, the
  compiler's name and version, the generator's version and the API
  version. It is rebuilt when any of these changes, which is when the model's equations change, and
  not when parameters, initial conditions or numerical options change. Files older than 30 days and
  beyond 100 MB are removed on start. Paths are UTF-8 on Windows, like
  every file-opening function of the engine.
- **Security.** Compiling a model file runs a compiler on text from the
  file and loads the result into the process: loading it is running code.
  The generator only emits statements from the fixed set of symbols above
  and writes numbers, never strings of the model, into the C file (names
  appear in comments with every character outside `[A-Za-z0-9_]`
  replaced). `export` already loads a library chosen by a model file; the
  compiled path adds no such choice. The library is written only into the cache
  directory with the owner's permissions, and its name is the hash, so that
  another model cannot replace it; the engine checks that the file's hash
  matches before it loads it. The option is off by default and is never turned on by a model file. On
  macOS it needs the entitlements discussed in point 3. `SECURITY.md` gets a paragraph.

## 6. How it will be tested, and the pieces

**Tests.**

- Every golden example through the compiled path: `golden.py check
  --compiled` runs all 197 models with the option on and demands the same
  statuses and digests as the reference (`{"nooutput": 10, "ok": 187}`,
  with the compiled count shown beside it: models that fall back are listed,
  and the list is part of the reference so that a model that stops being compiled fails the check).
- The finer test of point 2: `xppe_rhs`, the fixed variables and the
  auxiliaries equal bit for bit between the two paths at the stored points
  of each golden run and at 100 random points, for every model that is
  compiled.
- Every integration method that the first version compiles, on one model
  each (the same for AUTO's Jacobian).
- A parameter changed between runs (no recompilation, new answer equal to
  the interpreter's), and the same model run twice in a row.
- The fallbacks: no compiler (`PATH` emptied), a compiler that fails, a model
  that is refused, a library with the wrong hash; each must give the
  interpreter's numbers and the message.
- A model in a folder with a non-ASCII name.
- CI on Windows, Linux and macOS (arm64), where the compiled golden check
  is a separate step; a platform that cannot compile (no compiler on the
  runner) must be tested with the compiler present, which the runners have.
  The check is the one that finds the contraction problem of arm64.

**The pieces**, one pull request each, in this order. Stop at the first
that fails its test.

1. **Profile and speed up the interpreter.** Profile `eval_rpn`
   and `my_rhs` on ns100 and ku100; fix what costs without changing a bit
   (the goldens prove it). Possibly the cheapest 1.3 to 2 times. No design change.
2. **Document and test banded CVODE for networks.** A manual section,
   an example (the interleaved chain) and a test of `bandup`, `bandlo`
   against the dense solver. This is the answer for stiff models.
3. **Prototype of the generator (batch tool only).** From `my_ode`
   to a C file; build it with the system compiler; load it; call it from
   `my_rhs` only when the option is on. Models with fixed variables, `sum`,
   shifts and tables; everything else refused. The golden compiled check
   on the platforms where the system compiler exists, and the second test of point 2.
   **Decision point:** ku100 at least 3 times faster and 0 differing bits
   everywhere; otherwise stop and do not build the rest.
4. **Fallbacks and the cache.** The message, the cache with its hash and clean-up,
   the self-check, and the tests of failures.
5. **Python and the app.** `m.compile()`, the numerics check box, the
   status line, `compile` command with its reference page, the echo line and the
   replay case. **Decision point:** the embedded compiler, using the criteria of point 3.
6. **User functions, delays and `global` flags**, then AUTO's Jacobian, each
   with its own golden list.
7. **The embedded compiler,** if piece 5 said yes: the library, its licence
   notice, its size in the download, the macOS entitlements and a Mac test.

If the numbers of piece 3 do not show the speed-up, or bits differ in a way
that the rules above do not explain, the recommendation becomes: do not
build it, and keep pieces 1 and 2.
