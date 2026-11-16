# Design: importing SBML and NeuroML models

Status: proposal. Nothing in this note is built. The roadmap entry is
*SBML and NeuroML import* under M11.

Systems biology shares models as SBML, computational neuroscience as
NeuroML. Importing them gives the models people already have to XPP Lab's
analysis (phase plane, AUTO, control design) without retyping them. The
result of an import is an ordinary `.ode` file that opens in the editor,
as `docs/vision.md` asks: the researcher reads it, edits it and keeps it.

This note was written after trying the riskiest assumption, that a generated
`.ode` file can carry real models. The trial (a throwaway converter, not
committed) is reported in [What the trial found](#what-the-trial-found), and
the limits it hit are in the sections that deal with them.


## Conventions

Fixed here once; the converter, its tests and the app use them.

- **The state variable of a species is its amount** (substance), as SBML's
  reactions define it: the kinetic law of a reaction is an amount per time,
  and `d(amount)/dt` is the sum of stoichiometry times rate. A species
  written in the model's math as a concentration (the SBML default,
  `hasOnlySubstanceUnits="false"`) stands for `amount / compartment size`
  wherever it appears. This keeps a compartment that changes size correct
  without extra terms. When the size is 1 the two coincide. For every
  species whose concentration differs from its amount the file adds an
  `aux` with the concentration, so that plots and tables can show either.
- **Time and units are not converted.** The generated model runs in the
  time unit of the source. SBML units are not checked, only carried over as
  comments. NeuroML units are converted, once, to the fixed system below.
- **NeuroML units** are mV for voltage, ms for time, µF/cm² for specific
  capacitance, mS/cm² for conductance density, µA/cm² for current density,
  and nA for the current of a pulse generator, which is divided by the
  cell's area to give µA/cm² (a worked example is under
  [NeuroML](#neuroml-what-is-realistic)).
- **Test tolerance.** Where a reference set gives one, a result agrees when
  `|sim − ref| ≤ absolute + relative · |ref|` at every reference time and
  for every listed variable. The SBML test suite gives `absolute` and
  `relative` per case in its `*-settings.txt` (for case 00001: 1e-7 and
  1e-4). The accuracy of the XPPAUT side is then limited by the integrator
  and `TOLER`, not by the converter: the tests run with CVODE at
  `TOLER=1e-10`, `ATOL=1e-12` (or fixed-step Runge-Kutta with a small `DT`
  when the model has events; see the events section).
- **Imported ids and names.** The id in the source is the *id*; the name in
  the `.ode` file is the *name*. They are the same unless the id cannot be
  used, and then the file says so in a comment (see Names).


## What users get

1. **A model from a file.** `xs.import_model("model.xml")` (SBML) or
   `xs.import_model("cell.nml")` (NeuroML) writes `model.ode` next to the
   source (or to `out=`), loads it, and returns the `Model`. Simulation,
   phase plane, AUTO, sweeps, the control-design tools and the fit all
   work on it as on any model.
2. **Refusals that say why.** A model with an element that XPP Lab cannot
   represent is refused with the element, its id and the reason, and no
   file is written. A model is never imported with part of its meaning
   silently dropped.
3. **A file to read.** The generated `.ode` is plain XPPAUT text with a
   comment header that names the source, the converter's version and the
   renamed ids, one comment per species and parameter with the source id.
   In the app it opens in the editor.


## What is imported and what is refused

### SBML

**Levels and versions.** All of them are read: Level 1 version 2, Level 2
versions 1 to 5 and Level 3 versions 1 and 2. Level 3 version 2 is the
target; the converter reads the differences in the source (Level 1 and 2
kinetic laws and initial values) as the SBML specification describes them.
The trial read the Level 3 file of each test case where there was one
(version 2, else version 1), so the older levels are *not yet tested*; the
conformance harness (piece 3) runs every level of every case.

**Imported**, with the `.ode` form each takes:

| SBML | `.ode` |
|---|---|
| compartment, constant | `par` |
| compartment with a rate rule | state variable |
| species (not boundary, not constant) | state variable (the amount); `d/dt` = sum of stoichiometry × rate |
| species with `boundaryCondition` or `constant` and no rule | `par` (the amount) |
| parameter, constant | `par` |
| parameter with a rate rule | state variable |
| assignment rule (any symbol) | a fixed variable `name=expression`, in dependency order |
| rate rule | the right-hand side of the state variable's equation (for a species in concentration, times the compartment size) |
| reaction | a fixed variable `r_id` with the kinetic law, and its stoichiometry terms in the species' equations |
| local parameter of a kinetic law | the number, written into the law |
| function definition | substituted into each use (see the end of the mapping section) |
| initial assignment, initial amount, initial concentration | the number of the `init` and `par` lines; see the next paragraph |
| event without delay or priority | `global` (see Events) |
| `time`, `avogadro` | `t`, and the number |
| `piecewise`, `and`, `or`, `not`, comparisons | `if(c)then(a)else(b)` and the operators `& | < >` (see the end of the mapping section) |

*Initial values are numbers.* XPPAUT's `init x=a*2` and `par` lines do not
evaluate expressions: the trial wrote `init x=a*2` with `par a=2` and the
run started from 0, without a message. An initial assignment that depends on
other symbols therefore cannot stay a formula. The converter needs its
value. It gets it from the engine and not from a second evaluator (XPP Lab
never reimplements XPPAUT's numerics): it writes a scratch model whose `aux`
lines are the initial assignments, in dependency order, loads it with
`xppe_load`, runs one step, and reads the first row. The numbers go into the
file, with a comment that says which formula they came from. The cost is
that such a value does not follow a parameter the user changes later; the
comment says so.

**Refused, with a message:**

| SBML | reason given |
|---|---|
| algebraic rule | XPPAUT has no implicit algebraic constraint among the SBML kinds (`0=` lines exist for DAEs, but a general SBML algebraic rule needs index analysis) |
| event with a delay or a priority, or a non-persistent trigger | `global` fires at the step where the trigger changes sign, with no queue |
| `delay` (the `csymbol`), `rateOf` | no delay differential equations from SBML in the first version |
| reaction with `fast="true"` | no quasi-steady-state reduction |
| stoichiometry given by an id or by `stoichiometryMath` | species references with a variable stoichiometry; later |
| event that assigns a symbol that is neither a state variable nor a constant parameter | `global` assigns only to those |
| event whose assignments read what an earlier assignment of the same event wrote | `global` assigns in order, not at once (see Events) |
| trigger that is not a comparison, `and`, `or` or `not` of comparisons, or is an equality or `xor` | no sign change to detect |
| trigger true at t = 0 with `initialValue="false"` | SBML fires at 0; `global` does not |
| rate rule on a species in a compartment that also has a rate rule | later |
| `rational` numbers, `quotient`, `rem`, `xor`, `lambda` outside a function definition | no operator for them (see the end of the mapping section) |
| any package (`comp`, `fbc`, `qual`, `arrays`, `layout`, `multi`, ...) that changes the model's meaning | the package is named. `layout`, `render` and annotations only add drawing and are ignored |
| more than 5000 state variables | XPPAUT's `MAXODE` (below) |
| missing MathML, a symbol with no initial value, a circular rule | the model is not valid SBML |

*Packages* that add things the converter ignores would be wrong to ignore:
`comp` adds submodels, `fbc` a flux-balance problem (which is not a time
course at all), `qual` a logic model. All three are refused by name. The
trial counted them in the SBML test suite: 116 `comp` cases and 34 `fbc`
cases (which are not time courses).

### NeuroML: what is realistic

NeuroML 2 is the XML layer; the behaviour of its components is defined in
LEMS, a second language in which each *ComponentType* has parameters, state
variables, derived variables and `OnCondition` events. The question is how
much of this to take on.

**Do not write a LEMS interpreter.** jLEMS is one, and a general
interpretation (nested component types, structure, event ports between
components) is a project of its own. The core component types that matter
for single neurons are a short, fixed list defined in
`NeuroML2CoreTypes/Cells.xml` and `Channels.xml` of the NeuroML2 repository.
In `Cells.xml` the types that are neurons (and not drawing or
morphology parts) are `iafCell`, `iafTauCell`, `iafRefCell`,
`iafTauRefCell`, `izhikevichCell`, `izhikevich2007Cell`, `adExIaFCell`,
`fitzHughNagumoCell`, `hindmarshRose1984Cell` and `pinskyRinzelCA3Cell`, and
the conductance-based `cell` with `channelDensity` over `ionChannelHH`
channels. The converter implements *these by name*, each as a template that
mirrors its LEMS definition, and refuses any `ComponentType` the file
defines itself with the message `custom LEMS component type <name>`.

**Imported (point cells):**

- the abstract cells above: their equations, and for those with a reset
  (`OnCondition v > thresh`) a `global` line (see Events);
- a `cell` with a single segment (a point neuron) with `channelDensity` of
  `ionChannelHH` channels whose gates are `gateHHrates`, `gateHHtauInf` or
  `gateHHratesInf` with the rate types `HHExpRate`, `HHSigmoidRate`,
  `HHExpLinearRate`; `specificCapacitance`, `initMembPotential`,
  `spikeThresh`;
- the inputs `pulseGenerator` and `pulseGeneratorDL` (a `heav` window);
- a `network` of one population of size 1 with an `explicitInput`, which is
  what the NeuroML2 single-cell examples have.

**Refused:** multi-compartment cells (more than one segment), cable
equations, ion concentration pools, `ionChannelKS` (kinetic scheme)
channels, synapses and projections, and networks of more than one cell.
The message names the element: `network: 2 populations, 1 projection`.
Networks of point cells are the natural *next* step, as XPPAUT arrays (a
population of N identical cells with its connection matrix), but they are a
design of their own (what the `[0..N-1]` array syntax and `shift` can
express, how projections' weights and delays map) and they stay out of the
first version.

*A hand check.* `NML2_SingleCompHHCell.nml` (NeuroML2 `examples/`) is a
Hodgkin-Huxley cell. Converted by hand, with the unit system above, its
model is the classic HH equations with conductances 0.3, 120 and
36 mS/cm², reversal potentials −54.3, 50 and −77 mV and `Cm` = 1 µF/cm².
The soma is one segment of diameter 17.841242 µm with equal proximal and
distal points, a sphere of area π d² = 1000.00009 µm² (the file's comment
says 1000). The pulse is 0.08 nA for 100 ms from 100 ms, so
0.08 nA / 1000 µm² = 8 · 10⁻⁶ A/cm² = 8 µA/cm². Run in XPPAUT (fourth-order
Runge-Kutta, `DT = 0.01`) it gives 7 spikes at 102.1, 118.3 and 134.3 ms
(the first three), and an independent fourth-order Runge-Kutta of the same
equations in NumPy gives 7 spikes at 102.18, 118.38 and 134.37 ms: the
engine and the equations agree; the *conversion* was not checked against
a NeuroML simulator (jNeuroML and a Java runtime were not used). What
the check showed about the mapping:

- `HHExpLinearRate`, `r = rate · x / (1 − exp(−x))` with
  `x = (v − midpoint)/scale`, is 0/0 at `x = 0`. XPPAUT gives NaN there. The
  template writes `if(abs(x)<1e-9)then(1)else(x/(1-exp(-x)))`, as the
  hand conversion did.
- The pulse generator's attribute `delay` is a **reserved name** in XPPAUT
  (see Names). A parameter called `delay` is refused by the parser with
  `delay is a duplicate name`.

**SBML first, NeuroML later.** Recommended. Reasons:

1. SBML is a closed set of elements with a full conformance suite (1823
   semantic cases with reference results); NeuroML has no equivalent set
   of reference trajectories that can be fetched and compared in a script,
   so there is nothing to state a pass criterion against.
2. SBML's reaction networks are the case where retyping hurts most
   (dozens to hundreds of species), and the trial already converts 45 % of
   the suite with a throwaway.
3. The NeuroML work is a *library of templates plus unit handling*, which
   can reuse the SBML piece's name, line-splitting and `global` machinery.
   It is cheaper and safer built second.


## Mapping onto what XPPAUT can read

Each limit below was checked in the parser, then tried with a small
`.ode` file through `xppaut-headless -silent`. "Trial" means that run.

### Line length

XPPAUT reads each model line with `fgets(bob, MAXEXPLEN, fptr)`
(`engine/xppaut/form_ode.c`, lines 406 and 444; `read_a_line` at 2641,
fgets at 2654), `MAXEXPLEN` being 1024 (`engine/xppaut/newpars.h:43`), so a
line holds at most 1023 characters. A longer line is not refused: `fgets`
returns the first 1023 characters, and the rest is read as the next line.
In the trial a 1104-character line crashed the run (access violation) and,
in the BioModels trial, cut expressions gave `Illegal syntax` and
`illegal expression: 1.` (a truncated number). *The converter must
never write a line over the limit.* Backslash continuation exists
(`read_a_line` joins lines that end in a backslash) but worked in the
trial only for an ODE line and not for a fixed variable or `aux` line, so
the converter does not use it.

The converter follows `xppe_closed_loop` (`engine/api/xppe_design.c`,
`LAW_LINE` 900, `TERMS_PER_PART` 8, `NAMES_PER_SUM` 48, lines 797–801):

- a sum of many terms (the equation of a species used in many reactions)
  is split into partial sums of at most 8 terms, each a fixed variable
  `dxdt_k_1`, ...; sums of those follow if there are more than 48, and the
  equation is the last sum. The fixed variables stand before the equation,
  because XPPAUT evaluates fixed variables in the order they are written
  (the comment in `xppe_closed_loop`);
- a single expression over 900 characters (a long kinetic law) is split at
  its operands: the largest sub-expressions are bound to fixed variables
  `q_1`, `q_2`, ... defined earlier, until each line fits;
- a `par` line holds a few parameters at most, and the converter writes one
  per line.

In the BioModels trial 5 of 40 models would have written a line longer than
1000 characters with no splitting (the longest was 1866).

### Names: length and characters

A name is at most **10 characters**: `MXLEN` is 10
(`engine/xppaut/parser.h:52`) and `NAMLEN` is 10 (`newpars.h:41`);
`add_constant` truncates to `MXLEN` (`parserslow2.c`, `len>MXLEN`). In the
trial a parameter of 10 characters (`_k123456`) was accepted, one of 11 was
refused (`Illegal syntax`). Underscores are allowed, also as the first
character. SBML ids are letters, digits and underscore, so characters are
not a problem; length is: `compartment` (11 characters), the id the SBML
test suite itself uses, is too long.

### Upper-casing

`parse_a_string` upper-cases the whole line: `strupr(s1)`
(`form_ode.c:2270`). XPPAUT names are case-insensitive, and the trial
showed `par A=1, a=2` gives `a is a duplicate name`. SBML ids are case
sensitive, so `S` and `s` are two species. They must not collide.

### Reserved names

The symbol table (`engine/xppaut/parserslow2.c`, from line 99) holds the
operators and these names, which cannot be used as parameters or variables
(trial: `delay`, `sin`, `shift`, `ran`, `normal`, `sum`, `flr`, `mod`,
`max`, `t` and `pi` were refused as `... is a duplicate name`):

    START END ATAN2 MAX MIN SIN COS TAN ASIN ACOS ATAN SINH TANH COSH ABS
    EXP LN LOG LOG10 SQRT HEAV SIGN FLR MOD DELAY RAN IF THEN ELSE NOT
    NORMAL BESSELJ BESSELY ERF ERFC SUM OF SHIFT DEL_SHFT HOM_BCS ISHIFT
    POISSON SET ARG1 ... ARG20 BESSELI LGAMMA BESSELIS

plus the constants `PI` (`parserslow2.c:275`), `I'` (277, the index of
`sum(...)of(...)`), `mouse_x`, `mouse_y`, and `t`. `I'` matters: the derivative
of a state variable `i` is also named `I'`, and a model with a species `I`
failed in the BioModels trial with `I' is a duplicate name`. So `i` is
reserved too. SBML names that collide are common (`I`, `S`, `E`, `delay`,
`max`, `time`): in the BioModels trial two of 40 models failed on `I`.
(`e` and `ceil` are not reserved; `ceil` is not a function either, see
Limits.)

**The rule.** The converter keeps an id as its name when it has at most 10
characters, matches `[A-Za-z_][A-Za-z0-9_]*`, is not reserved, and its
upper-case form is not taken. Otherwise the name is the first 7 characters
(non-word characters turned to `_`) plus `_` and the smallest number `n`
for which the upper-case name is free: `compartment` → `compart_1`, `a` (when
`A` exists) → `a_1`, `delay` → `delay_1`, `i` → `i_1`. Ids are processed
in document order, so the result is the same on every run. Every renamed id
gets a comment (`# delay -> delay_1`) and appears in the mapping the Python
call returns (`Model.import_info.names`, `{id: name}`) and the app prints.

### Counts

| Limit | Value | Source | Behaviour |
|---|---|---|---|
| state variables | 5000 | `xpplim.h:4` `MAXODE` | the trial ran 4990 |
| parameters (`par`) | 400 minus the built-in constants = 393 (measured) | `xpplim.h:8` `MAXPAR`; `parserslow2.c:330` | past it: `too many constants !!`, and the line's parameters after it are not defined; the trial ran 390, failed at 395 |
| fixed variables | no limit found; 2000 ran | | symbols in all: `MAX_SYMBS` 10000 (`xpplim.h`) |
| `aux` | no limit found; 1000 ran | | |
| user functions (`f(x)=`) | 50 | `xpplim.h:11` `MAXUFUN` | not used: function definitions are substituted |
| `global` lines | 2000 | `flags.c:107` `MAXFLAG` | refused beyond |
| assignments per `global` | 20 | `flags.c:85` `MAX_EVENTS` | refused beyond |
| a `global` assignment list | 255 characters | `flags.c:120` `char temp[256]`, not bounds-checked | refused beyond 200 |

A model with more than 393 constants has its extra constants written as
numbers inside the formulas, with a warning that lists them, since refusing
a large model for a count that has a way round would be a worse outcome. A
model with more than 5000 states is refused.

A model with **no differential equation** (every species is boundary or
constant) is refused by XPPAUT (`form_ode.c:456`: `Must have at least one
equation`). The converter adds `dummy'=0` and `init dummy=0` and says so.

### Other places where the `.ode` language differs

Found in the trial and checked by running them; each has its handling in
the converter, tested in piece 1 (below):

- **Syntax.** `2^-1` and `2*-3` are `Illegal syntax`, and `a--b` too;
  `2^3^2` is 64 (left to right). The converter writes every operation
  fully parenthesised and every negative number in parentheses, so it
  never depends on precedence. `-2^2` is −4 as usual.
- **Missing functions.** XPPAUT has no `ceil` (use `-flr(-x)`), no
  `asinh`, `acosh`, `atanh`, `sec`, `csc`, `cot` (write them with the
  others), no `%`, no `&&`; `factorial(x)` is `exp(lgamma(x+1))`.
  Comparisons give 1 or 0, `&`, `|`, `not(...)` work on them, and
  `if(c)then(a)else(b)` accepts any non-zero number as true.
- **Function definitions are substituted** into each use, so they have
  no limit of 50 and no limit of argument names; the cost is that the
  text grows with each use, which is why lines are split as above.
- **The run stops at `BOUND`.** XPPAUT's `BOUND` defaults to 100
  (`load_eqn.c:599`); when a variable's absolute value passes it,
  integration stops (`integrate.c:2216`). A concentration of 280 or an
  amount of 10²³ (Avogadro) stops the run after the first step, with no
  error. The converter writes `@ bound=1e30` in the header, or larger if an
  initial value is larger.
- **Output precision.** In the trial the batch output file carried about
  eight digits (1e-05 was written 9.9999997e-06). The tests read the
  values through the Python package; the suite's tolerance (1e-4) is far
  above that either way.
- **Constants.** `pi` is defined; `e` is not (use `exp(1)`). Avogadro's
  number is written as `6.02214179e23`, the value of SBML Level 3
  Version 1: of the 23 suite cases that use it, 9 pass with it and 7 with
  the 2019 SI value 6.02214076e23.

### Events and `global`

An SBML event `trigger → assignments` maps to XPPAUT's
`global sign condition {name=expression; ...}`
(`docs/manual/odefile.md`, line 44; parsed at `form_ode.c:680`,
`add_global` at `flags.c:115`). The condition is a number: XPPAUT sees an
event when it changes sign between two steps, with `sign` = 1 for
negative to positive (`flags.c`, from line 307: `f0<0 && f1>0`).

| SBML trigger | `global` condition (positive when true) |
|---|---|
| `gt(a, b)`, `geq(a, b)` | `a-b` |
| `lt(a, b)`, `leq(a, b)` | `b-a` |
| `and(c1, c2)` | `min(m1, m2)` |
| `or(c1, c2)` | `max(m1, m2)` |
| `not(c)` | `-(m)` |

What `global` **cannot express**, from the source and from the trial:

1. **A delay, a priority, a non-persistent trigger.** There is no event
   queue. The 220 suite cases with a delay or a priority are the largest
   single block of refusals.
2. **A trigger that is true at the start and has `initialValue="false"`.**
   SBML fires at t = 0; `global` fires on a sign change between steps, so
   it cannot. Refused (the converter evaluates the trigger at 0 with the
   engine, as for initial values).
3. **Equality and `xor`.** No sign change. Refused.
4. **Simultaneous assignment.** SBML evaluates every assignment of an
   event with the values before the event. `one_flag_step` assigns in
   sequence (`flags.c:364–366`: `SETVAR` inside the evaluation loop): the
   trial `{y=z;z=y}` from y=1, z=2 gave y=2, z=2, not a swap. The converter
   orders the assignments so that none reads a symbol written earlier in the
   same event, and refuses the event when that is impossible (10 suite
   cases).
5. **A crossing that lands exactly on zero.** The test is strict
   (`f0<0 && f1>0`), so a value that is exactly 0 at a step boundary and
   positive after is missed: the trial `x'=1`, `global 1 x-0.55 {...}`
   missed the event with `DT=0.01` (the sum of steps hits 0.55 exactly) and
   fired with 0.555. SBML's `gt` is also strict, so the effect is a
   floating-point coincidence that SBML models with round numbers meet
   often (`time > 5` with `DT=0.1`).
6. **Two crossings inside one step.** The condition is evaluated at the end
   of each step, so a trigger that goes true and false inside one step is
   missed.
7. **Event time.** The time of the event is a linear interpolation within
   the step (`tstar = f0/(f0-f1)`). With CVODE and an output step of 0.1
   the trial gave 0.9332 where the exact value is 0.9095 (SBML case
   00026), a 2.6 % error; fourth-order Runge-Kutta with `DT = 0.001`
   gave 0.90952779 against the reference 0.909528. So a model with events
   is run with a fixed step method and a small `DT`: the importer writes
   `@ meth=rk4, dt=<span/steps/200>, nout=200` for these and says so in the
   header, and the user can change it. A model with events is not a
   stiff-solver model in the first version.
8. **Assigned symbols.** Only state variables and `par` parameters can be
   assigned (`flags.c`, `compile_flags`: other names give `Error in
   compiling a flag`). A fixed variable made by an assignment rule cannot be
   an event target (SBML forbids it too), and a species or compartment that
   is a `par` can, because `set_val` changes it.

For NeuroML the reset of an abstract cell is the same: Izhikevich's
`OnCondition v > thresh` with `v = c; u = u + d` is `global 1 v-thresh
{v=c; u=u+d}`; `u=u+d` reads `u` itself, not a symbol written before, so it
is allowed.


## Where the converter lives

Three places were compared.

**A. Pure Python, standard library only.** `xml.etree.ElementTree` for the
XML, a recursive converter for MathML, in `python/src/xpplab/importers/`.
The throwaway used exactly this. *Costs:* the app needs the console's
Python for File > Import (it has `pyconsole.c`, an optional Python that
`python fetch` installs on request: `docs/manual/commands.md`). Without
Python the app says so and shows the command. We own SBML's semantics (the
trial shows how many corners there are, 53 wrong results at the third
iteration). XML bombs: `ElementTree` expands entities; the converter
refuses any document with a `<!DOCTYPE` or `<!ENTITY`, and limits file
size.

**B. An optional dependency, `python-libsbml`.** LGPL, wheels of 5.5 to
8.1 MB for Windows, macOS and Linux on Python 3.10 to 3.15 (PyPI:
`python-libsbml` 5.21.2, including a WebAssembly build), which would
parse every level, validate, expand function definitions and initial
assignments, and convert Level 1/2 to 3 (`SBMLDocument.setLevelAndVersion`,
`SBMLLevelVersionConverter`). It makes the semantics safer. *Costs:* a
compiled dependency that the app's console Python would also have to
install, a licence to track (LGPL as a dependency is compatible with
GPL-2.0-or-later, but must be mentioned), and a second code path for
people who do not install it. It does not help with the XPPAUT side,
which is where the trial's problems were (names, lines, events, bounds).

**C. C in `engine/api/`.** One implementation, no Python, the same in the
app, the package, the MCP server and (later) the browser build; it follows
the project's rule for numbers the app and Python share. *Costs:* an XML
parser (vendored: expat is MIT, or a small one written for the SBML subset),
a MathML reader, name and line handling, the event logic, and the NeuroML
templates, all in C that reads untrusted input, under the sanitizer and
valgrind jobs. It is several times the work of A, and the converter will
change often while coverage grows (the trial's 327 gaps).

**Recommendation: A, with the app calling it through the console's
Python; decide on C after piece 3.** An import is a one-way translation
whose output is text, so it does not have to match bit for bit between
the app and Python, and the numbers it needs from the engine (initial
values) come from the engine. The converter will change quickly while the
gaps close, and Python is where that is cheap to do and to test. B is a
fallback if the trial's wrong results turn out to come from the corners of
SBML we have not read (the older levels): it can be added behind the same
function (`xs.import_model(..., parser="libsbml")`) without changing
anything else. C becomes worth its cost if users of the app without Python
matter: the measure is how often the message "import needs Python" is
the end of the road, and a port to C would be tested against the Python
output, file for file. Until then the app shows, when no Python is found:
`Import needs Python: type  python fetch`.

### Existing tools

- **SBFC (Systems Biology Format Converter)**, Java, has an SBML-to-XPP
  converter (SourceForge `sbfc`, `sbml2xpp`, last dated 2014-11-24).
  Its licence is stated differently by sbml.org (LGPL) and by the
  SourceForge project page (GPL 2.0): unresolved. *Ignore:* Java, old, and it
  would be a second runtime; its output was not examined. We did not read
  its code, so as not to have to argue about derivation.
- **jNeuroML / `org.neuroml.export`**, an XPP exporter for LEMS
  (`org/neuroml/export/xpp/XppWriter.java`, LGPL-3.0; jLEMS, MIT). Its
  own support table gives XPP the level LOW for abstract cells,
  single-compartment cells and HH channels, and NONE for populations,
  projections, inputs and kinetic-scheme channels; the file it writes
  says it is "a work in progress and only works for a limited subset". It
  substitutes the keyword `compartment` with `compart`, the same problem
  met here. *Learn from it* (what it maps and what it gives up); do not
  copy its code. The project is GPL-2.0-or-later, and LGPL-3.0 code can
  only join it under GPL-3.0, which would take the choice of version away
  from users. Running jNeuroML to compare results would be fine, as a tool
  in a test environment and not a dependency.
- **`sbmlutils`** (MIT) has `xpp2sbml`, the other direction. It is a
  useful list of what XPPAUT files contain; not used.
- **COPASI** reads and writes SBML but does not write XPPAUT files (the
  search found no such export).

None is used in the build.


## What users see

**Python**

    import xpplab as xs
    m = xs.import_model("BIOMD0000000012.xml")        # writes BIOMD0000000012.ode
    m = xs.import_model("cell.nml", out="cell.ode")
    m.path                                             # the .ode file
    m.import_info.names                                # {"delay": "delay_1", ...}
    m.import_info.warnings                             # constants written as numbers, ...

`import_model` chooses the reader from the root element (`sbml`, `neuroml`);
`xs.ImportRefused` (a `ValueError`) carries the element id and the reason
for a refusal. The return is the same `Model` as `xs.load`, so everything
else is unchanged. The MCP server gets an `import_model` tool with the same
arguments, which returns the `.ode` text, the mapping, the warnings and the
refusal message.

**App.** File > Import SBML or NeuroML... opens a file chooser, and the
command is `import FILE [OUT]` (listed in the command table with the others
and in `docs/manual/commands.md`; a `?` button opens its reference page).
It writes the `.ode`, opens it in the editor with the model loaded, and
prints in the command window the number of species, parameters and
reactions, the renamed ids and the warnings. A refusal prints the message
and opens nothing. The editor shows the file as text, editable: after an
import the user edits the file as any other model.

**Echo.** The session script records the import and the loaded file, in
the same way as `open`:

    # imported from BIOMD0000000012.xml: xs.import_model("BIOMD0000000012.xml")
    m = xs.load("BIOMD0000000012.ode")

The `.ode` is what the session used and what the user may have edited, so
the script loads it (the line the echo writes for `open`, `echo.c:338`);
the comment line records where it came from. An action added to the app needs its
line in the echo; `tests/replay.py` checks for it (piece 4).

**What the file says.** A header and one comment per rename:

    # imported from BIOMD0000000012.xml by xpplab 0.x (SBML level 2 version 3)
    # model: Elowitz2000 - Repressilator
    # compartment -> compart_1
    # reaction Reaction1 -> r_React_1
    # a time-course option block follows; events need a fixed step (see ...)

The mapping is in the comments, in `Import.names`, and printed by the
command. Nothing is written beside the `.ode`.


## How it will be tested

### Models with a closed-form answer (the normal test suite)

Small SBML models, and one NeuroML cell, written in the test. Their answers
are derived here and checked at `abs 1e-9 + rel 1e-6` (CVODE,
`TOLER=1e-10`) unless stated.

1. **Decay in a compartment of size 2.** One compartment of size 2, species
   S with initial concentration 3, reaction S → nothing with the law
   `k · S · compartment`, k = 0.5. The amount is n = 6 e^(−t/2) and the
   concentration 3 e^(−t/2). At t = 2: amount 2 · 3/e = 2.2072766,
   concentration 3/e = 1.1036383. (Checks amount versus concentration, the
   rename `compartment` → `compart_1`, and the equation form.)
2. **A reversible pair with the ids that collide.** Species `A` and `a`
   (amounts 1 and 0), parameters `delay` = 2 (forward) and `i` = 1
   (backward), reactions A → a with `delay · A` and a → A with `i · a`, and
   an assignment rule `total = A + a`. With kf = 2, kr = 1, A_eq = 1/3 and
   A(t) = 1/3 + (2/3) e^(−3t). At t = 1: 0.3665247, and `total` = 1 at all
   times. The names must come out as `A`, `a_1`, `delay_1`, `i_1`, and the
   header must list them. (Checks upper-casing, reserved names, rules.)
3. **A saw-tooth from an event.** One parameter x, initial value 1, rate
   rule dx/dt = −x, one event with trigger `x < 0.5` and assignment
   `x = 1`. Then x(t) = e^(−t) up to t₁ = ln 2 = 0.6931, then
   x(t) = e^(−(t − t₁)) up to 2 ln 2, so x(1) = e^(−(1 − ln 2)) = 0.7357589
   and x(2) = e^(−(2 − 2 ln 2)) = 0.5413411. Run with `rk4`, `DT = 0.001`:
   tolerance 1e-5 (the throwaway gave 0.54134125 at t = 2, an error of
   1.2e-7). (Checks the trigger margin, `global`, and the fixed step.)
4. **Many reactions on one species.** 60 reactions A → B with rate
   `k_j · A`, so that the equation of A is over 1024 characters if not
   split; the answer is A(t) = A₀ e^(−Σk_j t). Checks that no line passes
   900 characters and the result.
5. **Refusals.** A model with an algebraic rule, one with an event delay, a
   `comp` package model and one with 5001 species each fail with a message
   that names the element (a test of the exception's fields, not of its
   wording).
6. **Hand-checkable NeuroML.** An `izhikevichCell` with a constant current:
   the spike times for given a, b, c, d and I compared, to 1e-6 relative,
   with a run of a hand-written Izhikevich `.ode` file with the same
   numbers and options; and the HH example above, 7 spikes in 300 ms,
   each within 0.2 ms of a fourth-order Runge-Kutta of the same equations
   written in the test.

Each test runs with the two methods the converter writes (`cvode`, and
`rk4` for models with events), and each import twice in a row in one
session. (A probe of `dopri5` with an event ran to a wrong time and
state; it was not diagnosed, and the converter does not write it.)

### Public sets with reference results

**SBML test suite** (`sbmlteam/sbml-test-suite`, branch `release`, licence
LGPL 2.1 or later: compatible, and not committed because of its size: its
`cases/semantic` is 69 MB). A script `tools/fetch_sbml_suite.py`, in the
manner of `tools/fetch_modeldb.py`, fetches `cases/semantic` by a
sparse, partial `git clone` of a pinned commit (the trial used
`cf38585fac5de8e0e90112febb62851ee2181816`), records the commit and a
SHA-256 of every file it reads in `tests/sbml/manifest.json`, and checks
later runs against it. `tests/sbml_check.py` converts each case's model in
each level it exists in, runs it, and compares with `*-results.csv`
at the tolerances of `*-settings.txt` (the criterion in Conventions). Each
case has one of four outcomes: **pass**, **refused** (with the reason),
**wrong** (ran, disagrees) and **failed** (XPPAUT or the converter broke).
The check fails on any *wrong* or *failed*, and on a refusal that is not in
`tests/sbml/expected_refusals.json` (so a new refusal is a decision, and
a case that starts to pass has its entry removed). Cases of the suite that
are not time courses (the 34 `fbc` ones have no tolerances) count as
"not applicable".

**BioModels**, curated models, are the wider check, for what the suite
does not hold: real models with long names, many reactions and unusual
laws. `tools/fetch_biomodels.py` fetches a fixed list of identifiers from
`https://www.ebi.ac.uk/biomodels/model/download/<id>?filename=<id>_url.xml`
(the URL the trial used) and records SHA-256s. The models are not
committed: the two the trial fetched carry a CC0 statement, but the licence
of each model is its own, as for ModelDB. BioModels ships no reference
trajectories in the file, so the reference is an independent SBML
simulator run once in a separate environment: libroadrunner (the trial
could install it from PyPI but not import it on this Windows machine:
`DLL load failed`, so no BioModels result was compared yet), COPASI or
the model's own SED-ML where it has one. Pass: relative agreement 1e-4 plus
1e-9 absolute on every species at 100 times over the model's own
time scale. It runs by hand, as `modeldb.yml` does, not on every push.

### Expected coverage (trial, one run)

A throwaway converter of about 700 lines of Python (not committed) was
run on all 1823 semantic cases at the suite's pinned commit, converting
the highest-level file of each case (Level 3 version 2 where it exists), CVODE at `TOLER=1e-10`
(`rk4` with `DT = span/steps/200` when the model has events), and the
tolerances of each case:

| outcome | cases | share of the 1789 time courses |
|---|---|---|
| pass | 815 | 45.6 % |
| refused | 916 | 51.2 % |
| wrong (ran, disagrees) | 53 | 3.0 % |
| failed (XPPAUT or the throwaway broke) | 5 | 0.3 % |
| not time courses (`fbc`) | 34 | |

The refusals split into 589 that the first version refuses *by design*
(event delay or priority 220, algebraic rules 125, `comp` 116, `delay` 82,
fast reactions 34, missing MathML 12) and 327 that are gaps of the
throwaway (stoichiometry by id or math 169, a compartment with no size 27,
rational numbers 19, a species in a growing compartment 15, `arccos` and
other names 11, event assignments that depend on each other 10, `xor` 10,
and a tail). So if every gap closed and nothing else changed, the most the
first version can pass is (1789 − 589) / 1789 = 67 %; the target to state
in the piece-3 pull request is **at least 60 % of the time-course cases
passing, no case wrong or failed that is not listed as a known deviation**,
and the number reported afterwards. Of the 53 wrong results, 23 are among the 105 event cases that ran (the
timing described above is one cause, not the only one), 12 among the 158 that
have an initial assignment (some already wrong at t = 0) and 5 among the 14
that use Avogadro's number. The groups overlap. They were not diagnosed
one by one; piece 3 is to understand them, not accept them.

The Level 1 and 2 files were not used where a Level 3 file existed, so these
numbers say nothing about those levels yet.

### What the trial found

Things that went wrong, in the order met, and where the note answers them:

- Species and compartment ids over 10 characters (`compartment` itself)
  broke at the first case (names).
- `S1'=+1.0*...` and `a--b`: XPPAUT has no unary plus and no double sign
  (syntax).
- A model with no ODE at all (all species boundary) is refused by XPPAUT
  (counts).
- Runs stopped at the first step when a value passed `BOUND` = 100:
  280 for an MKK in a MAPK cascade, 10²³ for an Avogadro model (the run).
- `init x=a*2` silently starts at 0 (initial values).
- Events: strict crossing, linear event times, sequential assignments
  (events).
- On BioModels: a species named `I`, and lines over 1000 characters
  (names; line length). Seven of the 40 curated models the trial took
  (`BIOMD0000000001` to `40`) failed in XPPAUT: two on `I`, at least three on
  long lines and two with `Illegal syntax` at a column that was not
  diagnosed; 28 ran (to the end of the time span, with no reference to
  compare against) and 5 were refused (3 delay, 1 event delay, 1 stoichiometry
  by id). `BIOMD0000000012` (the repressilator) ran, oscillating, with
  proteins peaking near 2000; `BIOMD0000000010` (a MAPK cascade)
  ran. None of these was compared with a reference yet.

### Golden tests

The import is new code: no file under `engine/xppaut/` changes, and
`python tests/golden.py check` must still report
`{"nooutput": 10, "ok": 187}`. The one place the engine may need an edit is
the unchecked `char temp[256]` in `add_global` (`flags.c:120`); that would
be a marked `XPP Lab fix:` and a row in `docs/upstream-issues.md`, and is
not required by this design because the converter keeps its lists short.


## The pieces, one pull request each

1. **SBML core, in Python.** `xpplab.importers.sbml`, `mathml`, `names`:
   compartments, species, parameters, reactions and kinetic laws, rate and
   assignment rules, function definitions, names (renames and comments),
   line splitting, the `bound` and `dummy` handling, refusals; the
   `import_model` function; closed-form tests 1, 2, 4 and 5 above and unit
   tests of the name rule and of the line splitting. No engine change.
2. **Initial assignments and events.** The scratch-model evaluation of
   initial values, `global` for events with its refusals and the ordering
   of assignments, the fixed-step choice; closed-form test 3; needs piece 1.
3. **The conformance harness.** `tools/fetch_sbml_suite.py`,
   `tests/sbml/manifest.json`, `tests/sbml_check.py` and the list of expected
   refusals; the first reported coverage and the fixes it asks for in
   pieces 1 and 2 (stoichiometry by id, rational numbers, names of
   operators). Needs 1 and 2.
4. **App.** File > Import, the `import` command, the echo comment and `load`
   lines, the message when no Python is found, the `?` reference page, and
   the replay and `tests/app_check.py` cases; the echo case must check for
   the import's own line in the saved script. Needs 1; the other pieces
   improve it.
5. **MCP and documentation.** The `import_model` tool, the manual page, the
   Python reference and a notebook that imports a BioModels model; the
   `skills/xpplab/SKILL.md` section; the BioModels fetch script and its
   manual check. Needs 1 to 3.
6. **NeuroML point cells.** The unit table (from `NeuroMLCoreDimensions.xml`),
   the templates for the abstract cells and HH channels, the pulse
   generator, refusals for networks and multi-compartment cells, tests 6.
   Reuses the names, the line splitting and `global` of pieces 1 and 2, so
   it follows them.
7. **Later, optional.** SBML event delays by an explicit pending-event
   state, stoichiometry by id, `delay` for models whose delays are
   constant (XPPAUT's `delay(x,tau)`), networks of identical NeuroML point
   cells as arrays, and the move to C if the Python dependency turns out to
   matter.

Pieces 1 to 3 are in order. Piece 4 can start after 1 (it only calls
`import_model`). Piece 6 can start after 2. Piece 7's items are each their
own pull request.
