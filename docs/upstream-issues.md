# Upstream XPPAUT issues found while building XPP Studio

Bugs found in the XPPAUT 8.0 sources. Fixes made in `engine/xppaut/` are
marked in the code with an `XPP Studio fix:` comment. Everything here is
worth reporting to Bard Ermentrout.

## Fixed

| File | Problem | Fix | Effect on results |
|---|---|---|---|
| `integrate.c` `init_ar_ic()` | `ar_ic[i].var[i]=0` writes past the 20-byte `var` field for `i >= 20` (found with `-fsanitize=bounds-strict`) | `var[0]` | None: it only wrote zeros into zero-initialised memory |
| `gogoauto.c` `go_go_auto()` | `findlb()` steps backwards through `fort.3` with relative `fseek()`. That works only when lines end in one byte. On Windows, text mode (CRLF) lands on the wrong record, so **restarting AUTO from any labelled point read garbage** (a singular matrix and exit, or a crash) | Open `fort.3` in binary mode | Windows: continuation from labelled points works. POSIX: none |
| `auto_nox.c` `close_auto()` | `fort.8` is never closed before it is renamed to `model.s`. POSIX renames the open file and leaves its tail unflushed; Windows cannot rename it at all, so the `.s` restart file was missing | Close `fort.8` first | `.s` files are complete on every platform |
| `auto_nox.c` `renamef()` | On Windows `rename()` fails when the target exists | Remove the target first (Windows only) | Repeated AUTO runs update `.b`/`.d`/`.s` |
| `extra.c` `add_export_list()` | `malloc(strlen(s))` then `strcpy()`: one byte too few for the terminating zero, so **every model with an `export` statement corrupts the heap**. On Linux, glibc aborts (`buffer overflow detected` in fortified builds, `free(): invalid pointer` or a crash later); Windows does not notice (found with AddressSanitizer) | `malloc(strlen(s)+1)` | Models with `export` (the `efm` family, `koho`, `minode`, `tstdll`) run on Linux. Output unchanged on Windows |
| `simplenet.c` `evaluate_network()` | The delayed networks `delmmult` and `delsparse` pass `get_delay()` an index into `variables[]`, where `t` is entry 0, but `get_delay()` expects the 0-based ODE index (the parser's `delay()` passes `variable-1`). So **each term used the delayed value of the next variable, `u[j+1]`, instead of `u[j]`**, and the last one read past the delay buffer (found with AddressSanitizer on `tstdel.ode`) | Subtract 1 | Changes results for models that couple through `delmmult` or `delsparse`: they now compute what XPPAUT's documentation says. The examples set the coupling to 0, so their output is unchanged |
| `form_ode.c` `read_a_line()` | When a model file ends without `done`, the last `fgets()` reads nothing and the line buffer is used as it was: uninitialised, or in practice often still holding the previous line, **which is then parsed a second time**. When it came out empty, the code read `s[-1]`, the byte before the buffer, and could overwrite it (found with AddressSanitizer and Valgrind on `hedge.ode` and `osc.ode`) | Treat end of file as an empty line; check the line is not empty before looking at its last character | All example models give the same output. Broken models can report a different error: `hedge.ode` (`x'=` with nothing on the right) used to fail with a spurious "duplicate" from re-reading `y'=eps*x`, and now reports the empty right-hand side |
| `load_eqn.c` `set_option()` | Option names are matched by prefix and the shorter name is tested first, so `HISTLO2`, `HISTHI2`, `HISTBINS2`, `HISTCOL2` and `SPECCOL2` set `HISTLO`, `HISTHI`, `HISTBINS`, `HISTCOL` and `SPECCOL` instead: the second column of a 2-D histogram or cross spectrum could never be chosen in batch mode (found while checking XPP Studio's spectra against batch post-processing) | Test that the longer name does not match | Models that use these options get the histogram or spectrum they ask for; no example uses them |
| `aniparse.c` `add_ani_rline()` | A named colour is stored as `+index` where every other animation command stores `-index`, and a positive value is taken for a compiled colour formula. So `rline ...;$RED` evaluated a bogus formula; only `$BLACK` (index 0) worked (found while writing the animator) | Store `-index` | `rline` draws in the colour asked for |
| `form_ode.c` `find_ker()` | The exponent of a singular Volterra kernel, `int[mu]{...}`, is copied into a buffer without a terminating zero and read with `atof()`, which runs on into uninitialised stack. **Now and then mu picks up stray digits** (found with Valgrind; see `mlvolt.ode` below) | Terminate the buffer | Models with `int[mu]` give the same result on every run, the one they gave most of the time |
| `form_ode.c` `do_new_parser()` | After a line with no statements, the Markov clean-up tests `v`, which was never set (found with Valgrind) | Zero `v` first | None |
| `autlib1.c` `fnhbae()` | The eigenvalue array has `ndim` entries, `eig()` fills the first `ndm`, and `send_eigen()` reads all `ndim`, so XPPAUT's copy of the eigenvalues ends in uninitialised values (found with Valgrind) | `calloc` the array | None: XPPAUT uses only the first `NODE` eigenvalues, for stability |
| `form_ode.c` `compile_em()` | The name taken from an `init` statement goes into `char name[10]`, one byte short for a name of 10 characters, which XPPAUT allows. The terminating zero lands in the next buffer, `tmp`, and `convert(name,tmp)` then copies the name onto itself without end. **Optimised builds overrun the stack and crash; unoptimised ones carry on but silently drop the initial values after that name on the line** (found with ModelDB model 112079, `init ... inhibitedA=1.0, D=0.1, ...`) | Make `name` and `tmp` as long as a line | Models with 10-character names in `init` run, with every initial value set. Nothing else changes |
| `cv2.c` `end_cv()` | With `meth=cvode`, `integrate()` stops at once when `dt` is below `dtmin` and calls `end_cv()`, which frees CVODE's state. On a first run nothing was allocated yet, so **`N_VFree(NULL)` crashes**; `end_cv()` also left its pointers set, so a second call would free them twice (found with ModelDB model 136097, `dt=0.00001`, `dtmin=0.0001`) | Free only what is allocated, and clear the pointers | These models stop with "Integration not completed", as XPPAUT intends, instead of crashing. Nothing else changes |
| `extra.c` `add_export_list()` | When the library named by an `export` statement cannot be loaded, `my_fun()` leaves the output array alone, and `do_in_out()` copies it into the exported variables at every step. The array comes from `malloc()`, so **those variables take whatever the heap held**: usually zero, but not always (found when `efmx.ode`, `getfrefm.ode`, `koho.ode`, `tstdll.ode` and others came out different from run to run in a checkout without their `.so` files) | `calloc` the array | Exported outputs of a missing library are always zero, the value they had in the recorded outputs |
| `volterra2.c` `volterra()`, `init_sums()`, `volt_step()` | `allocate_volterra()` allocates the history `Memory[]` only when the model has integrals (`NKernel > 0`), but the Volterra method reads and writes it for every model. So **choosing `meth=volterra` for a model without integrals crashes XPPAUT** on the first step, on every platform (found with XPP Studio's parity check, `tests/parity.py`, on `lecar.ode`; unmodified XPPAUT crashes the same way in batch mode) | Without integrals, keep no history | Such models run: the method is then the implicit trapezoidal rule (on `lecar.ode` within 5e-4 of RK4 at dt = 0.05, and 4 times closer at dt = 0.025). Models with integrals are unchanged |

### How `mlvolt.ode` came to give different results from run to run

In unmodified XPPAUT, a few runs in a hundred of `ode/mlvolt.ode` give a
trajectory that is wrong from the first output step. The cause is the
`find_ker()` bug above: the kernel `int[.5]{...}` has a singular exponent
μ = 0.5, read with `atof()` from a buffer that was never terminated. When
the stack byte after the `5` happened to be a digit, μ became `.57...`
instead. Filling the stack with zeros or a fixed pattern always gave the
usual result, which is why that probe found nothing. Valgrind named the
line. With the fix, 150 runs in a row are identical; without it, 4 of 150
differed, each one differently.

## Behaviour that changes only when embedded

These guards are active only under the library API (`xpps_embedded`).
Standalone builds behave exactly like XPPAUT.

- Dialog primitives (`pop_up_list`, `two_choice`, `respond_box`,
  `do_string_box`, `new_string`, `get_dialog`, `get_mouse_xy`, `rubber`)
  take their answers from a queue instead of waiting for input. Under the
  null X server they would otherwise spin forever.
- `exit()` returns to the API call instead of ending the process. In
  XPPAUT itself, an AUTO failure such as the singular matrix in `ge()`
  closes the whole program.
- The `@ logfile=...` option is ignored: the log goes to the API
  (`xppe_log()`, the app's Command Window) instead of a file.

## Open

### Any backslash continues a line, even in a comment

The documentation says lines "can be continued with the standard backslash
character". `read_a_line()` in `form_ode.c` treats a backslash *anywhere*
in a line as a continuation: it cuts the line there and appends the next
one. A comment that contains one, such as `# ghat \equiv 0`, therefore
swallows the line after it. In ModelDB model 185334 that line is an
equation, so XPPAUT finds 8 equations for 9 boundary conditions and stops.
XPP Studio keeps this behaviour, since changing the continuation rule could
change how other files parse; see [ModelDB models](development/modeldb.md).

### The Volterra history has room for 50 variables

`parserslow2.c` defines `double *Memory[MAXKER]` (50 entries), while
`volterra2.c` declares it `extern double *Memory[MAXODE]` and fills one
entry for every variable, fixed variable and Markov variable. A model with
integrals and more than 50 of those together would write past the array.
No example comes near that; it was found while fixing the crash above and
has not been seen to happen.

### Compiler warnings that may be real bugs

- `auto_nox.c:2393`: argument 2 of `do_auto` is given a pointer where an
  integer is expected.
- `auto_nox.c:1231`: a self-comparison that is always false.
- `auto_nox.c:1116`, `1187`: `x`, `y1` and `y2` may be used uninitialised.
- `auto_x11.c:793`: `dx` and `dy` are used uninitialised (GUI code).
- `simplenet.c:301`, `comline.c:481`, `gogoauto.c:525`: functions declared
  to return a value can fall off the end without returning one.
