# Design: a model builder that writes the .ode file

Status: proposal. Nothing described here exists yet. The roadmap entry is
*A model builder that writes the `.ode` file* under M11.


## Conventions

Fixed here once; every section below and every piece of code uses them.

- **The text is the model.** The `.ode` text in the editor
  (`App.editor` in `app/src/ui_editor.c`) is the only copy. The builder
  holds no model of its own: each frame in which the text changed, it reads
  the text again, and every action of the builder is a *patch* of that text
  (replace some lines, insert a line, delete a line). Nothing is generated
  from scratch, so what the builder does not touch stays as the user wrote
  it, byte for byte: comments, blank lines, order, spacing, `\r\n` line ends,
  and statements the builder cannot represent.
- **Items.** The reader splits the text into *items*, in file order. An item
  is a run of one or more lines: a statement, a group of comment lines, or
  blank lines. An item has a kind, a first line and a line count. The kinds
  are in the table below; the reader needs no more of the language than
  that table, and it is not a second parser of the numbers: whether the text
  is a valid model is decided by XPPAUT's own parser (`xppe_load`), as for
  every other file. A statement the reader does not recognise is an item of
  kind `other` (see "Files the builder cannot represent").
- **Names.** A name is what XPPAUT accepts: letters, digits and `_`,
  starting with a letter, compared without regard to case, and shorter than
  `MAXVNAM` (33, `engine/xppaut/form_ode.h`). A variable, a parameter, a
  fixed quantity, an auxiliary and a function that the builder *adds* must have
  names distinct from every name in the text. Names already in the file are
  read as given, duplicates included: `lecar.ode` has a fixed quantity `ica`
  and an auxiliary `aux ica=ica`, and the builder keeps both, lists both, and
  refuses only to add a third. The builder shows a name as typed and writes it as typed, but it looks names
  up without regard to case. It refuses a name that is a function or a
  reserved word of `docs/manual/odefile.md` (`sin`, `t`, `pi`, `done`, ...).
  The limits are XPPAUT's: `MAXODE` = 5000 equations, `MAXPAR` = 400
  parameters, `MAXEXPLEN` = 1024 characters in one expression
  (`xpplim.h`, `newpars.h`); the builder shows them and does not repeat them
  in code that the engine can answer with an error.
- **Canonical form of what the builder writes.** One statement a line, one
  space on each side of `=` in equations and none in `par`, `init`, `number`
  and `@` lists: `x' = a*x - b*x*y`, `par a=1, b=0.5`, `init x=2, y=1`,
  `aux total = x + y`, `@ total=50, dt=0.02`. A differential equation is
  written with the prime (`x' = ...`), never `dx/dt`. When the builder edits
  a line that was written another way, it rewrites that line (and only that
  line) in the canonical form.
- **Numbers.** The builder writes a number with `app_format_double`
  (`app/src/echo.c`): the shortest text that reads back as the same double,
  as Python writes floats. A number the user typed and did not change is not
  rewritten. A value that is an expression (`par a=2*pi`) is kept as text,
  never evaluated.
- **Order of a new item.** Declarations of one kind stay together. A new
  equation goes after the last equation (or the first line that is not a
  comment, if there is none), `par` and `init` entries join the last line of
  their kind or start a new one after the last equation, `aux` lines go
  after the last `aux`, `@` options join an existing `@` line that has the
  option, else the last `@` line, else a new one before `done`. Everything
  goes before `done`. If the text has no `done`, the builder adds one only
  when the text has no other lines after the item, as `insert_snippet`
  does.
- **Units.** XPPAUT has none, and the builder adds none. The panel can show
  a free-text note for a name, kept as a trailing comment (`par a=1  # per
  second`); it does not use it anywhere.
- **Errors.** The text is checked after each patch with `xppe_load`, as
  Apply does (`app_apply_editor`). A patch that gives a model that does not
  load is shown with XPPAUT's own message and line, and the text is kept: the
  builder does not guess the repair.


## What users get

A new user meets the `.ode` syntax before understanding the model. The
builder is a panel beside the editor that shows the model as a form and
changes the text when the form changes. The text stays visible and
editable, so the form teaches the language: a user adds a variable and sees
`x' = ...`, `init x=0` and `par` lines appear.

The builder does not replace the editor, does not hide statements it does
not know, and does not make a model that XPPAUT cannot read. It does not
run models (the existing windows do), and it makes no numbers of its own.

### The panel

A window *Builder* (`ui_builder`, in `app/src/ui_builder.c`; opened from
`View > Builder` and by the command `builder`), docked next to the editor.
Its sections follow the order of the table below:

1. **Variables.** A table with one row per differential equation, map or
   algebraic equation: name, the equation's right-hand side, the initial
   value. A button adds a row (a new name `x1`, `x2`, ... that is not taken),
   a button on a row deletes it, and the name, the equation and the initial
   value are text fields. The kind of equation (differential `x' =`, map
   `x(t+1) =`, algebraic `0 =`) is a menu on the row.
2. **Parameters.** Name and value; a row for each name of the `par` lines.
3. **Fixed quantities and auxiliaries.** Name and expression, in two tables
   (`s = ...` and `aux name = ...`).
4. **Functions and constants.** `f(u,v) = ...` and `number n=...` lines.
5. **Options.** The options of `docs/manual/odefile.md`, grouped as there:
   the integration (`total`, `dt`, `meth` as a menu, ...), the phase plane
   (`xp`, `yp`, the window), AUTO. A field shows the model's own value, or
   the default dimmed when the text has no such option, and writing in it
   adds the option to an `@` line.
6. **Parameter sets.** Each `set name {a=2, total=50}` shown as a row; a
   button copies the current parameter values into a new set.
7. **Not shown in the form.** The items of kind `other` listed in file
   order, each with its line number, read-only, with a button that puts the
   cursor on its line in the editor.

A field that is empty or not valid is marked in red as soon as the line it
belongs to fails; the message of XPPAUT (with the line number, as the
editor shows it) appears under the table. A row's tooltip names the line it
comes from. The panel never opens a dialog: a user who types in the text
sees the form follow at once, and a user who types in the form sees the
text follow.

### The data model

| Kind | Statement | Fields |
|---|---|---|
| `ode` | `x' = f`, `dx/dt = f` | name, right-hand side, kind (`ode`) |
| `map` | `x(t+1) = f` | name, right-hand side |
| `algebraic` | `0 = f` | right-hand side (with a `solv` variable) |
| `fixed` | `s = f` | name, right-hand side |
| `aux` | `aux name = f` | name, expression |
| `function` | `f(u,v) = g` | name, argument names, body |
| `par` | `par a=1, b=2` | entries: name, value text |
| `init` | `init x=0.1` or `x(0)=0.1` | entries: name, value text |
| `number` | `number pi2=6.28` | entries: name, value text |
| `derived` | `!d = 2*a` | name, expression |
| `option` | `@ total=50, dt=0.02` | entries: name, value text |
| `set` | `set name {a=2, total=50}` | name, entries |
| `comment` | `# text` | text, first comment lines are the description |
| `blank` | an empty line | |
| `done` | `done` | |
| `other` | everything else | the lines, verbatim |

The kinds that carry entries (`par`, `init`, `number`, `option`, `set`) are
one item per line: an edit changes the entries it names and writes the line
again in canonical form, and an entry the builder does not know (an `@`
option it has no field for) is kept as text in its place. A variable (the
rows of section 1) is the pair of an `ode`, `map` or `algebraic` item and
the `init` entry of the same name, if there is one; a variable with no
`init` entry has the value 0, as in XPPAUT, and the panel shows it dimmed.

### Round trip with the editor text

1. The reader (`xppe_model_parse`) takes the text and returns the items. It
   is a function of the text alone, needs no loaded model and does not change
   the engine's state.
2. Each action of the form is a patch (`xppe_model_edit`): it takes the
   text, the action and its arguments, and returns the new text. The panel
   writes the returned text into `App.editor` and sets `editor_dirty`, as
   `insert_snippet` does. The reader runs again on the next frame.
3. **Reading the patch's own output gives the item that was written.** This
   is the property the tests check (below): `parse(edit(text, op)) ==
   parse(text) + op`, and for an action that changes nothing the text is
   returned identical, byte for byte.
4. **Apply is the user's.** The builder never saves and never reloads the
   model; `Apply` (`app_apply_editor`) does, as it does for text typed in
   the editor. The form's *Apply* button is the same function.
5. **New file.** *New* (`app_new_model`) now offers a blank form (an empty
   model with `done`) and the FitzHugh-Nagumo template as today. The blank
   form is the text `done\n`.

### Spellings the reader accepts

XPPAUT reads more spellings than `odefile.md` lists, and the shipped examples
use them, so the reader accepts them and keeps each as it was written:

- **Declaration words** without regard to case: `par`, `param`, `params` and
  `parameter` for parameters (and `init`/`initial`-style words only as
  `odefile.md` gives them). Entries are separated by commas or by spaces.
- **Option names** are matched by XPPAUT by their first characters
  (`msc` in `engine/xppaut/load_eqn.c`: the name in the text only has to begin
  with the option's name), so `xplot=t` is `xp=t`, and the names are read
  without regard to case (`TOTAL=100`). The panel recognises an option when
  its name equals one of `odefile.md`, without regard to case; any other
  spelling (`xplot`) is an entry the builder does not know: it is kept as
  text, shown in section 7, and not turned into a field.
- **Entries separated by spaces** (`@ dsmin=1e-5,dsmax=.02,parmin=-.2
  parmax=.5`) are read as separate entries.

The rule for all of them: **a line the reader cannot split cleanly into
entries is not guessed at; it is one item of kind `other`.** A `set` line
whose body the reader cannot split (`lecar.ode` has `set pp
{xp=v,yp=w,xlo=-.6.xhi=1.2,...}`, where a comma is missing) is `other`, shown
read-only, and loaded or refused by XPPAUT as it is.

### Files the builder cannot represent

The builder opens every file XPPAUT opens. What it does not understand it
leaves alone:

- Statements the reader has no kind for: `table`, `wiener`, `markov` (and its
  transition lines), `global`, `bdry`, `#include`, interactive `"` lines,
  and statements of XPPAUT that `odefile.md` does not list. They are items
  of kind `other`, listed read-only in section 7 of the panel and kept
  verbatim by every patch.
- Arrays (`x[1..10]' = ...`). The reader sees one item of kind `other`:
  the row it stands for is not one variable. The panel shows it as a
  single read-only row `x[1..10]`, and the names it defines come from the
  engine (`xppe_name`), not from the reader.
- Statements that continue on the next line (a `\` at the end of the line) and
  lines that hold more than one statement: one item of kind `other`.
- Interactive text. A line that begins with `"` is tutorial text. A run of
  such lines, and a quoted string whose closing `"` is on a later line, is
  one `other` item that runs to the last line it touches, so a continuation
  line is never read as a statement. The tail of `lecar.ode` (a title line
  and about ten `"` lines, with `{total=100,iapp=.1}` inside them) is one
  item, and the `{...}` inside it is not read as an entry.
- Text with an error: the lines before the error are read as usual; the
  line of the error and every line after it that the reader cannot place are
  `other`. The form stays usable and the error message stays shown.
- A name used by an `other` item (a name defined by `markov`, `table`, or an
  array) is reserved, so the builder does not make a variable of the same
  name; it cannot rename or delete a name that an `other` item uses, and it
  says which line uses it.

The rule behind all of these is that the builder is allowed to make the
file worse only in the lines that the user's action names. Opening a file in
the builder and closing it again does not change one byte of it.

### Echo as Python

The echo (`app/src/echo.c`) writes each action of the app as a line of a
Python script, so that the script run with the package writes the same
numbers (`tests/replay.py`). The builder follows it: every action calls
`echo_begin()`, changes the text, and writes its line with `echo_line()`
after the patch loaded or after the text was changed, before `echo_end()`.

The package gets a class `xpplab.ModelBuilder`, which holds the text and
applies the same patches by the same C functions (`xppe_model_edit`), so the
text is the same in the app and in Python, bit for bit:

```python
import xpplab as xs
b = xs.ModelBuilder()                      # the blank form: "done\n"
b.ode("v", "v - v^3/3 - w + I", init=-1)   # add a differential equation
b.ode("w", "eps*(v + a - b*w)", init=1)
b.par(I=0.5, a=0.7, b=0.8, eps=0.08)       # one par line
b.option(total=200, dt=0.05, xp="v", yp="w")
m = b.model()                              # xs.Model.from_text(b.text)
```

One method for each action of the panel: `ode`, `map`, `algebraic`, `fixed`,
`aux`, `function`, `par`, `init`, `number`, `option`, `set`, plus `remove`,
`rename` and `edit` for existing items, and `from_file(path)` to start from
an `.ode` file. `b.text` is the text, and `b.items` the items the reader sees.
All of them are thin wrappers of the C functions and do not compute.

The echo of the app writes a method of `b` for each action, and the
scripts begin the builder with its starting text:

- a new blank form: `b = xs.ModelBuilder()`;
- a file that is open in the editor: `b = xs.ModelBuilder.from_file(PATH)`
  (the path as `echo_str` writes it), or, if the editor's text was changed
  by typing since it was loaded, `b = xs.ModelBuilder(TEXT)` with the text
  as a string, because the script must not read a file that the app has since
  changed;
- *Apply* with the builder: `m = b.model()` and, as now, `sol = m.run()` when
  the run succeeds.

Typing in the editor is not an action the echo can write as a method, so the
text typed is written in whole at the next *Apply*, as the string above.
That is the only place the echo has a long line.

## Where each part goes

AGENTS.md requires that numbers the app and Python share are computed in
the C API. The text a patch gives is such a number: both give it by the same
functions.

### C API (`engine/api/xppe_model.c`, declared in `xppe.h` and `xpp_instance.h`)

New file, because the functions need no engine state (they work on text
and do not load a model). They follow `xppe_result_*` for the lists they
return:

- `xppe_model_parse(const char *text)`: reads the text; returns the number
  of items (the items stay until the next call). A negative value is an
  `XPPE_ERR_*` code.
- `xppe_model_item(int i, int *kind, int *first_line, int *nlines)` and
  `xppe_model_field(int i, int k)` (a string; `k` 0 is the name, 1 the
  expression, 2 the value text, ... as the table of kinds lists), and
  `xppe_model_entries(int i)`, the number of entries of a `par`-like item.
- `xppe_model_edit(const char *text, const char *action, int nargs, const
  char *const *args)`: applies one action (`add`, `set`, `remove`,
  `rename`, `move`; the arguments are the kind, the name and the values as
  text) and returns the new text through `xppe_model_text()`; it leaves the
  text alone and returns an error when the action is refused (a name taken,
  a name used by an `other` item, an unknown kind).
- `xppe_model_text(void)`: the text of the last `edit`, as bytes with the
  line ends of the input.

Each addition bumps `XPPE_API_VERSION` and `API_VERSION` in
`python/src/xpplab/_engine.py` (the first piece takes 17 to 18; later
pieces bump it only when they add functions). The functions open no file,
so no UTF-8 path handling is added. Text is passed as UTF-8 bytes, kept
unchanged. The reader takes `\n` and `\r\n` as line ends and keeps the one
it found on each line; a lone `\r` is not a line end (it stays inside the line;
piece 1 checks this by loading such a file with `xppe_load`). A line the
builder inserts takes the line end of the line before it, or `\n` if the text
has none; a file that mixes the two keeps each line's own. Piece 1 also
checks a file with `\r\n` line ends against `xppe_load`.

### Python (`python/src/xpplab/builder.py`)

`ModelBuilder` as above, exported from `xpplab/__init__.py`, with a reference
page in `docs/python/` (and an entry in `python/tests/api_pending.txt` until
the page exists, if the API check asks for one). `Model.from_text` already
exists (`python/src/xpplab/model.py`) and gives the model. The MCP server
gets a tool `edit_model` only if the maintainer wants it, in a later
piece: `check_model` and `describe_model` already read and check files.

### App (`app/src/ui_builder.c`, `app_builder.c`)

- `ui_builder.c` draws the panel and calls the patch functions through a
  small layer in `app_builder.c`, which owns the echo lines and the call to
  `xppe_model_parse`, so that the commands and the buttons reach the same
  code (`commands.c` has the commands, as for the other windows).
- Commands for scripts, `tests/app_check.py` and the replay: `builder ode
  NAME RHS [INIT]`, `builder par NAME VALUE`, `builder init NAME VALUE`,
  `builder aux NAME EXPR`, `builder option NAME VALUE`, `builder map NAME RHS [INIT]`, `builder algebraic RHS`, `builder fixed NAME
  EXPR`, `builder function NAME(ARGS) BODY`, `builder number NAME VALUE`,
  `builder set NAME ENTRIES`, `builder edit NAME FIELD VALUE`, `builder move
  NAME POSITION`, `builder remove NAME`, `builder rename OLD NEW`. Every action
  of the panel and of `ModelBuilder` has a command; none is left for later. A command's text after the name is the
  expression (it contains spaces), so these commands take the rest of the
  line.
- Each action is wrapped in `echo_begin()`/`echo_end()` and writes its line
  only when it succeeded (the patch was accepted), as AGENTS.md asks.

### Documentation

A section *The builder* in `docs/manual/` (a new page `builder.md`, listed in
`mkdocs.yml` and in the app's help, `app/src/docs.c`, since every page of
`docs/manual` is also the app's Help), a short paragraph in
`docs/manual/odefile.md` pointing to it, and a first-model tutorial with the
form. The ROADMAP line links to this note and is ticked when the last piece
lands. Every claim the page makes about a model (the FitzHugh-Nagumo example)
is a test.

## How it will be tested

Goldens must not change: `golden.py check` still reports
`{"nooutput": 10, "ok": 187}`. The work adds functions and a window and
touches no file in `engine/xppaut/`.

### The generated file parses and gives the same numbers

The tests compare a file the builder wrote with a hand-written one, for
the same model:

1. **Text identity.** For every file in `engine/xppaut/ode/` and
   `examples/classics/` (about 200 files): `edit` with an action that
   changes nothing (set a value to the text it has) returns the input, byte
   for byte, and `parse` followed by writing every item's lines in order
   reproduces the file. This is the guarantee for the files the builder
   cannot represent. The list includes `lecar.ode`, whose tail has the
   duplicate name `ica`, the `parmin=-.2 parmax=.5` option line, the
   `xplot=` spelling, the `set pp` line with the missing comma, and the
   multi-line tutorial text; the test also checks that these are the items
   `parse` returns for them (the aux and the fixed quantity both listed, the
   `set pp` line and the tutorial each one `other`), and that
   `ModelBuilder.from_file` then `edit` of a parameter changes only that line.
2. **Generated equals hand-written.** For a list of models in
   `python/tests/test_builder.py`, each built with `ModelBuilder` calls only,
   and a hand-written `.ode` for it in `tests/models/`: both load, `check_model`
   reports no error and no warning, the variables, parameters, auxiliary
   columns and options agree, and `m.run()` gives the same table bit for bit
   (the same text gives the same numbers; where the builder's text differs
   from the hand-written text only in spacing or in `dx/dt` against `x'`, the
   engine reads the same model). Use `numpy.array_equal`, not a tolerance.
3. **Round trip of items.** For each action the test builds the text, reads
   it with `parse`, and compares the items with what was added (kind, name,
   expression text, value text). Adding then removing leaves the text as it
   was.
4. **Refusals.** A name that is taken, differs only in case from one that is
   taken, is a function name (`sin`), is longer than the limit, or is used by
   an `other` item gives an error and leaves the text unchanged (adding `ica` to
   `lecar.ode` is refused although the file already has it twice); an
   expression with an unbalanced parenthesis is accepted by the builder (it
   only writes text) and the error comes from the engine with the right line
   number.
5. **Non-ASCII text.** A model with a name in a comment and a description
   in UTF-8, and a path with a non-ASCII folder, round trips unchanged
   (`python/tests/test_unicode_paths.py` is the pattern).

### Models with a known answer

The expected numbers are derived here, so that the tests do not take them
from the code they test. Unless a model says otherwise it runs with
`meth=rungekutta` and `dt=0.01`. **Which routine limits the accuracy:** the
comparison with a closed form is limited by the fourth-order Runge-Kutta
step (the local error is about (λh)^5/120 for a rate λ, the global error
about that times the number of steps); the comparison of a generated file
with the hand-written one, and every map run, is **exact** (`array_equal`),
because the engine reads the same model. The tolerances below are bounds from
that estimate; piece 2 measures the error once and writes the measured value
in the test beside the bound, and the bound stays.

1. **Exponential decay.** `x' = -a*x`, `par a=0.5`, `init x=2`, `@ total=4`.
   The solution is x(t) = 2 e^(−0.5 t), so x(4) = 2 e^(−2) = 0.2706705664.
   With λh = 0.005 the local error is below 3e-14 and the 400 steps give
   below 1e-11, so the test uses an absolute tolerance of 1e-9.
   It tests the smallest builder model: one `ode`, one `par`, one `init`, one
   `option`.
2. **Logistic growth with an auxiliary.** `x' = r*x*(1 - x/k)`,
   `par r=1, k=10`, `init x=1`, `aux half = x/2`, `@ total=5`. The solution
   is x(t) = k / (1 + (k/x₀ − 1) e^(−r t)), so x(5) = 10 / (1 + 9 e^(−5)) =
   9.428... (the test computes it from the formula, with `math.exp`, and does
   not store the digits), and the column `HALF` equals `X/2` in every row,
   exactly (the auxiliary is computed from the stored X). The closed-form check
   uses an absolute tolerance of 1e-8 (the rate r = 1 is twice that of model 1,
   so the error is a few times larger; the bound has room). It tests `aux` and a second parameter.
3. **A map.** `x(t+1) = r*x*(1-x)`, `par r=2`, `init x=0.5`, `@ meth=discrete,
   total=10`. The point x = 1/2 is a fixed point for r = 2 (r/2 · (1/2) = 1/2),
   so every row of X is 0.5, exactly. With `init x=0.25` and r = 2, x₁ = 0.375
   and the sequence tends to 0.5; the test checks the first three terms
   (0.25, 0.375, 0.46875). It tests the kind menu (`map`) and `meth`.
4. **The FitzHugh-Nagumo template** of `app_new_model` against the same text
   built by calls: the two texts have the same items, and the runs are equal
   bit for bit. The expected equilibrium is the solution of
   `v − v³/3 − w + I = 0` and `v + a − b w = 0`, which the test solves with a
   root finder to 1e-13 and compares with `m.equilibria()`; the accuracy is
   that of XPPAUT's Newton iteration, so the tolerance is 10 times `NEWT_TOL`,
   read with `m.get`, not a number typed in the test.
5. **An example with an `other` item.** `lecar.ode` with a parameter edited
   through the builder: the run equals that of `xs.load` of the same file with
   the same parameter set through `m.run(iapp=...)` (bit for bit), and every
   line except the one edited is unchanged in the text.

### API, Python and app

- `tests/api_check.py` (or a C test next to `tests/api_smoke.c`) calls the
  `xppe_model_*` functions on the cases above, including an empty text, a
  text with no `done`, a text ending without a line end, `\r\n` text, a line
  of 100 000 characters, and the refusal of a text that is not UTF-8 (it is
  kept as bytes, not refused: the test checks that it is returned unchanged).
  The sanitizer and valgrind jobs run it as before.
- `python/tests/test_builder.py` runs the five groups above.
- `tests/app_check.py` runs the `builder` commands, then reads `editor` text
  back and `xppe_load`s it.
- `tests/replay.py` gets one case per action (the case table has a list of
  line patterns per case, as `missing_lines` uses). A case checks that the
  saved script contains **that action's own line**, so a case where the
  action is skipped fails:

  | Action | Pattern the script must have, after the previous one |
  |---|---|
  | start from a blank form | `^b = xs\.ModelBuilder\(\)` |
  | add an equation | `^b\.ode\("w",` |
  | add parameters | `^b\.par\(` |
  | set an initial value | `^b\.init\(` |
  | add an auxiliary | `^b\.aux\("` |
  | set an option | `^b\.option\(` |
  | add a map | `^b\.map\("` |
  | add an algebraic equation | `^b\.algebraic\(` |
  | add a fixed quantity | `^b\.fixed\("` |
  | add a function | `^b\.function\("` |
  | add a constant | `^b\.number\(` |
  | add a parameter set | `^b\.set\("` |
  | change a field of an item | `^b\.edit\("` |
  | move an item | `^b\.move\("` |
  | rename | `^b\.rename\("` |
  | remove | `^b\.remove\("` |
  | apply | `^m = b\.model\(\)`, then `^sol = m\.run\(\)` |

  Each case exports the run as CSV (`export {out}/run.csv`), and the script
  must write the same CSV bit for bit. A second case edits the text by typing
  (a command that sets `editor` text) and checks that the script starts with
  `b = xs.ModelBuilder(` followed by the text of what was typed, and not with
  a read of the file.
- A failed action (a refused name) writes no line: the case checks that the
  script has no `b.ode("x",` after it.

## The pieces, one pull request each

1. **C API: the reader and the patch** (`engine/api/xppe_model.c`,
   `xppe.h`, `xpp_instance.h`, `python/src/xpplab/_engine.py`): the kinds
   and conventions above, with the C tests, the API version bump to 18 and
   the text-identity test over every example. Needs no other piece.
2. **Python: `xpplab.ModelBuilder`** (`python/src/xpplab/builder.py`,
   `__init__.py`, the reference page): tests 2 to 5 of the first group and
   the known-answer models 1 to 3. Needs 1.
3. **App: the panel, read-only** (`app/src/ui_builder.c`, `ui_menu.c`,
   `app.h`): variables, parameters, auxiliaries and options as tables that
   follow the text, and the *Not shown in the form* list. No edit yet. Needs 1.
4. **App: editing, the commands and the echo** (`app_builder.c`,
   `commands.c`, `echo.c`, `tests/app_check.py`, `tests/replay.py`): every
   action of the form, with its command, its echo line and its replay case.
   Needs 2 and 3.
5. **App: a blank form and the first-model tutorial** (`app_new_model`, the
   *New* menu). Needs 4.
6. **Documentation** (`docs/manual/builder.md`, `odefile.md`, `mkdocs.yml`,
   the Help index): ticks the roadmap line. Comes last.
7. **Later, optional:** renaming across the whole model, array rows
   (`x[1..10]`), and an MCP tool that edits a model.

Pieces 1 and 2 are the library; 3 to 5 the app. Pieces 2 and 3 do not
depend on each other.

## What this note does not settle

- The reader is a new piece of code that recognises statements. It does not
  evaluate, and the engine's parser stays the judge, but it is a second place
  where the syntax is written; piece 1 tests it against every example file
  for that reason. If the maintainer prefers that the engine's own parser
  report the statements (as items with line numbers), that is a change in
  `engine/xppaut/` and needs a marked upstream edit; this note does not
  propose it.
- Whether `Insert` in the editor (`snippets_menu`) stays beside the builder or
  is replaced by it.
