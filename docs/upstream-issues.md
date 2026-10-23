# Upstream XPPAUT issues found while building XPP Studio

Bugs found in the XPPAUT 8.0 sources. Fixes made in `engine/xppaut/` are
marked in the code with an `XPP Studio fix:` comment. Everything here is
worth reporting to Bard Ermentrout.

## Fixed

| File | Problem | Fix | Effect on results |
|---|---|---|---|
| `integrate.c` `init_ar_ic()` | `ar_ic[i].var[i]=0` writes past the 20-byte `var` field for `i >= 20` (found with `-fsanitize=bounds-strict`) | `var[0]` | None: it only wrote zeros into zero-initialised memory |

## Open

### `ode/mlvolt.ode` gives different results from run to run

About 1 run in 20 produces a different trajectory. The difference appears
at the very first output step and is large (v1 = -0.00018 in most runs,
-0.02 or -0.2 in the others), so it is not round-off. Ruled out so far:

- **Stack:** forcing automatic variables to zero or to a pattern
  (`-ftrivial-auto-var-init`) still gives the usual result.
- **Heap:** filling every `malloc` block with a fixed byte (0, 85, 127,
  170, 255) still gives the usual result, and occasional outliers still
  appear.
- **Volterra history buffer:** zeroing `Memory[]` (`calloc`) does not remove
  the outliers.
- **Fixed-size arrays:** `-fsanitize=bounds-strict` finds no out-of-bounds
  index in the Volterra code path.

Next step: run it under Valgrind on Linux CI, which can see heap overreads
and uses of uninitialised values that these probes miss.

### Compiler warnings that may be real bugs

- `auto_nox.c:2393`: argument 2 of `do_auto` is given a pointer where an
  integer is expected.
- `auto_nox.c:1231`: a self-comparison that is always false.
- `auto_nox.c:1116`, `1187`: `x`, `y1` and `y2` may be used uninitialised.
- `auto_x11.c:793`: `dx` and `dy` are used uninitialised (GUI code).
- `simplenet.c:301`, `comline.c:481`, `gogoauto.c:525`: functions declared
  to return a value can fall off the end without returning one.
