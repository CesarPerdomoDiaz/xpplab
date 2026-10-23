/*
 * Smoke test for the engine API: load a model, inspect it, integrate,
 * change a parameter, find an equilibrium, and recover from a bad file.
 * Usage: api_smoke <model.ode> [bad-model.ode]
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xppe.h"

static int failures;

#define CHECK(cond, ...)                         \
  do {                                           \
    if (!(cond)) {                               \
      failures++;                                \
      printf("FAIL %s:%d: ", __FILE__, __LINE__); \
      printf(__VA_ARGS__);                       \
      printf("\n");                              \
    }                                            \
  } while (0)

int main(int argc, char **argv) {
  int i, n, rows, rc;
  double v, *col;
  double x[64], re[64], im[64];

  if (argc < 2) {
    fprintf(stderr, "usage: api_smoke model.ode\n");
    return 2;
  }
  CHECK(xppe_api_version() == XPPE_API_VERSION, "api version");

  rc = xppe_load(argv[1], 0, NULL);
  CHECK(rc == XPPE_OK, "load: %d %s\n%s", rc, xppe_error(), xppe_log());
  if (rc != XPPE_OK) return 1;

  n = xppe_count(XPPE_VARIABLES);
  printf("variables (%d):", n);
  for (i = 0; i < n; i++) printf(" %s", xppe_name(XPPE_VARIABLES, i));
  printf("\nparameters (%d):", xppe_count(XPPE_PARAMETERS));
  for (i = 0; i < xppe_count(XPPE_PARAMETERS); i++) {
    xppe_get(xppe_name(XPPE_PARAMETERS, i), &v);
    printf(" %s=%g", xppe_name(XPPE_PARAMETERS, i), v);
  }
  printf("\ncolumns (%d):", xppe_count(XPPE_COLUMNS));
  for (i = 0; i < xppe_count(XPPE_COLUMNS); i++)
    printf(" %s", xppe_name(XPPE_COLUMNS, i));
  printf("\nsets (%d):", xppe_count(XPPE_SETS));
  for (i = 0; i < xppe_count(XPPE_SETS); i++)
    printf(" %s", xppe_name(XPPE_SETS, i));
  printf("\n");

  CHECK(xppe_get("total", &v) == XPPE_OK, "get total");
  printf("total=%g", v);
  CHECK(xppe_get("dt", &v) == XPPE_OK, "get dt");
  printf(" dt=%g\n", v);
  CHECK(xppe_get("no_such_thing", &v) == XPPE_ERR_NAME, "unknown name");

  rc = xppe_run();
  rows = xppe_rows();
  CHECK(rc == XPPE_OK && rows > 0, "run: rc=%d rows=%d", rc, rows);
  col = malloc(sizeof(double) * (size_t)rows);
  for (i = 0; i < xppe_count(XPPE_COLUMNS) && i < 4; i++) {
    xppe_column(i, col, rows);
    printf("%s[0]=%.8g %s[end]=%.8g\n", xppe_name(XPPE_COLUMNS, i), col[0],
           xppe_name(XPPE_COLUMNS, i), col[rows - 1]);
  }
  /* Write the run like XPPAUT's output.dat so the test driver can diff it. */
  {
    FILE *f = fopen("api_run1.dat", "w");
    int ncol = xppe_count(XPPE_COLUMNS), r, c;
    double **cols = malloc(sizeof(double *) * (size_t)ncol);
    for (c = 0; c < ncol; c++) {
      cols[c] = malloc(sizeof(double) * (size_t)rows);
      xppe_column(c, cols[c], rows);
    }
    for (r = 0; r < rows; r++) {
      for (c = 0; c < ncol; c++) fprintf(f, "%.8g ", cols[c][r]);
      fprintf(f, "\n");
    }
    fclose(f);
    for (c = 0; c < ncol; c++) free(cols[c]);
    free(cols);
  }

  /* A second run with the same settings must give the same numbers. */
  {
    double first_end, second_end;
    int c = xppe_count(XPPE_COLUMNS) > 1 ? 1 : 0;
    xppe_column(c, col, rows);
    first_end = col[rows - 1];
    xppe_run();
    CHECK(xppe_rows() == rows, "rerun rows");
    xppe_column(c, col, rows);
    second_end = col[rows - 1];
    CHECK(first_end == second_end, "rerun differs: %.17g vs %.17g", first_end,
          second_end);
  }

  if (xppe_count(XPPE_PARAMETERS) > 0) {
    const char *p = xppe_name(XPPE_PARAMETERS, 0);
    double before, after;
    xppe_get(p, &before);
    CHECK(xppe_set_number(p, before + 0.01) == XPPE_OK, "set %s", p);
    xppe_get(p, &after);
    CHECK(fabs(after - (before + 0.01)) < 1e-12, "set %s: %g -> %g", p,
          before, after);
    xppe_set_number(p, before);
  }

  n = xppe_count(XPPE_VARIABLES);
  if (n <= 64) {
    for (i = 0; i < n; i++) xppe_get(xppe_name(XPPE_VARIABLES, i), &x[i]);
    rc = xppe_equilibrium(x, re, im);
    printf("equilibrium rc=%d:", rc);
    for (i = 0; i < n; i++)
      printf(" %s=%.6g (eig %.4g%+.4gi)", xppe_name(XPPE_VARIABLES, i), x[i],
             re[i], im[i]);
    printf("\n");
  }

  CHECK(xppe_load(argv[1], 0, NULL) == XPPE_ERR_STATE, "second load refused");
  free(col);
  printf(failures ? "%d FAILURES\n" : "api_smoke OK\n", failures);
  return failures ? 1 : 0;
}
