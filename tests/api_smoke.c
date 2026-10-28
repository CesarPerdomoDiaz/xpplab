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

/* Linearised at the equilibrium x (eigenvalues re, im from Sing pts), the
   Jacobian's eigenvalues agree with XPPAUT's, whose Jacobian is coarser;
   the input parameter keeps its value. */
static void check_linearize(int n, const double *x, const double *re,
                            const double *im) {
  static double A[64 * 64], Ae[64 * 64], B[64], Be[64], lre[64], lim[64];
  const char *in[1];
  double before = 0, after = 0;
  int i, j, rc;
  if (xppe_count(XPPE_PARAMETERS) < 1) return;
  in[0] = xppe_name(XPPE_PARAMETERS, 0);
  xppe_get(in[0], &before);
  rc = xppe_linearize(x, 1, in, 0.0, A, B, Ae, Be);
  CHECK(rc == XPPE_OK, "linearize: %d %s", rc, xppe_error());
  xppe_get(in[0], &after);
  CHECK(before == after, "linearize changed %s: %.17g -> %.17g", in[0], before, after);
  rc = xppe_eigenvalues(n, A, lre, lim);
  CHECK(rc == XPPE_OK, "eigenvalues: %d %s", rc, xppe_error());
  for (i = 0; i < n; i++) {
    double best = 1e300;
    for (j = 0; j < n; j++) {
      double d = hypot(lre[j] - re[i], lim[j] - im[i]);
      if (d < best) best = d;
    }
    CHECK(best < 1e-3 * (1 + hypot(re[i], im[i])),
          "eigenvalue %g%+gi of the equilibrium is %g from A's", re[i], im[i], best);
  }
  CHECK(xppe_linearize(x, 1, (const char *const[]){"no_such_par"}, 0.0, A, B,
                       NULL, NULL) == XPPE_ERR_NAME, "unknown input refused");
  printf("linearized at the equilibrium with input %s: d/d%s =", in[0], in[0]);
  for (i = 0; i < n; i++) printf(" %.6g", B[i]);
  printf("\n");
}

/* Known answers: x'' + 0.5 x' + 4 x = u, and a pair the input cannot reach. */
static void check_control_known(void) {
  const double A[4] = {0, 1, -4, -0.5}, B[2] = {0, 1};
  const double A2[4] = {-1, 0, 0, -2}, B2[2] = {1, 0};
  double re[2], im[2], C[4], sv[2];
  int rank;
  CHECK(xppe_eigenvalues(2, A, re, im) == XPPE_OK, "eigenvalues");
  CHECK(fabs(re[0] + 0.25) < 1e-9 && fabs(re[1] + 0.25) < 1e-9 &&
            fabs(fabs(im[0]) - sqrt(3.9375)) < 1e-9 && im[0] == -im[1],
        "eigenvalues %g%+gi %g%+gi", re[0], im[0], re[1], im[1]);
  rank = xppe_controllability(2, 1, A, B, 0.0, C, sv);
  CHECK(rank == 2, "controllable rank %d", rank);
  CHECK(C[0] == 0 && C[1] == 1 && C[2] == 1 && C[3] == -0.5,
        "controllability matrix %g %g; %g %g", C[0], C[1], C[2], C[3]);
  CHECK(fabs(sv[0] * sv[1] - 1) < 1e-12 && sv[0] >= sv[1],
        "singular values %g %g (their product is |det C| = 1)", sv[0], sv[1]);
  rank = xppe_controllability(2, 1, A2, B2, 0.0, C, sv);
  CHECK(rank == 1 && sv[1] == 0, "uncontrollable rank %d, sv %g %g", rank,
        sv[0], sv[1]);
  CHECK(xppe_controllability(2, 0, A, NULL, 0.0, NULL, NULL) == 0, "no inputs");
  printf("control known answers checked\n");
}

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
  /* xppe_columns_at() recomputes the first stored row from its state. */
  {
    int ncol = xppe_count(XPPE_COLUMNS), c;
    double *row = malloc(sizeof(double) * (size_t)ncol), *first = row;
    n = xppe_count(XPPE_VARIABLES);
    for (i = 0; i < n && i < 64; i++)
      for (c = 0; c < ncol; c++)
        if (strcmp(xppe_name(XPPE_COLUMNS, c), xppe_name(XPPE_VARIABLES, i)) == 0) {
          xppe_column(c, col, rows);
          x[i] = col[0];
        }
    xppe_column(-1, col, rows);
    CHECK(xppe_columns_at(col[0], x, row, ncol) == ncol, "columns_at");
    for (c = 0; c < ncol; c++) {
      xppe_column(c, col, rows);
      CHECK(fabs(first[c] - col[0]) <= 1e-6 * (1 + fabs(col[0])),
            "columns_at %s: %g vs %g", xppe_name(XPPE_COLUMNS, c), first[c], col[0]);
    }
    free(row);
    CHECK(xppe_halt_flag() == 0, "no flag stopped the run");
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
    if (rc == XPPE_OK) check_linearize(n, x, re, im);
  }
  check_control_known();

  CHECK(xppe_load(argv[1], 0, NULL) == XPPE_ERR_STATE, "second load refused");
  free(col);
  printf(failures ? "%d FAILURES\n" : "api_smoke OK\n", failures);
  return failures ? 1 : 0;
}
