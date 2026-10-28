/*
 * Equilibria in bulk for the engine API (see xppe.h): XPPAUT's Sing pts >
 * Range, which follows an equilibrium while a parameter changes, and Sing
 * pts > Monte Carlo, which looks for every equilibrium in a box. Both use
 * do_sing_info() (gear.c), the Newton's method and eigenvalue code of
 * Sing pts > Go without its dialogs and drawing, as xppe_equilibrium() does.
 */
#include <math.h>
#include <stdio.h>
#include <setjmp.h>
#include <string.h>

#include "xppe.h"
#include "xppe_internal.h"

#include "xpplim.h"

#define PARAM_BOX 1 /* as in auto_nox.c */

extern int NODE, METHOD, EVEC_ITER;
extern double NEWT_ERR, EVEC_ERR, BOUND;
extern char uvar_names[MAXODE][12];
void do_sing_info(double *x, double eps, double err, double big, int maxit,
                  int n, double *er, double *em, int *ierr);
int find_user_name(int type, char *name);
int set_val(char *name, double value);
int get_val(char *name, double *value);
void evaluate_derived(void);
void redo_all_fun_tables(void);
void do_monte_carlo_search(int append, int stuffbrowse, int ishoot);

/* As declared in integrate.c. */
#define MAXFP 400
typedef struct {
  int n, flag;
  double *x[MAXFP];
  double *er[MAXFP];
  double *em[MAXFP];
  double *x1[MAXFP], *x2[MAXFP], *x3[MAXFP], *x4[MAXFP];
  int t1, t2, t3, t4;
} FIXPTLIST;
typedef struct {
  int n;
  double tol;
  double xlo[MAXODE], xhi[MAXODE];
} FIXPTGUESS;
extern FIXPTLIST fixptlist;
extern FIXPTGUESS fixptguess;

/* Stability as Sing pts > Go counts it (do_sing() in gear.c): for maps, the
   eigenvalue's modulus against 1. */
static void count_stability(const double *re, const double *im, int n,
                            int *unstable, int *stable) {
  int i;
  *unstable = *stable = 0;
  for (i = 0; i < n; i++) {
    double real = re[i];
    if (METHOD == 0) real = re[i] * re[i] + im[i] * im[i] - 1.0;
    if (real > 0.0) (*unstable)++;
    if (real < 0.0) (*stable)++;
  }
}

/* do_eq_range() (integrate.c) with the values given rather than equal
   steps, the results in the result table rather than the data browser, and
   the parameter restored at the end. */
int xppe_equilibrium_sweep(const char *parameter, const double *values, int n,
                           const double *guess) {
  static const char *names[MAXODE + 3];
  char name[256];
  double x[MAXODE], re[MAXODE], im[MAXODE], old = 0;
  int rc, i, j, ierr, unstable, stable;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (n < 1 || !values || !guess) {
    xppe_set_error("give at least one value and a starting guess", NULL);
    return XPPE_ERR_RANGE;
  }
  if (strlen(parameter) >= sizeof name) {
    xppe_set_error("parameter name too long", NULL);
    return XPPE_ERR_RANGE;
  }
  strcpy(name, parameter);
  if (find_user_name(PARAM_BOX, name) < 0) {
    xppe_set_error("no parameter named %s", parameter);
    return XPPE_ERR_NAME;
  }
  names[0] = parameter;
  for (j = 0; j < NODE; j++) names[j + 1] = uvar_names[j];
  names[NODE + 1] = "unstable";
  names[NODE + 2] = "stable";
  if ((rc = xppe_result_start(n, NODE + 3, names)) != XPPE_OK) return rc;
  get_val(name, &old);
  for (j = 0; j < NODE; j++) x[j] = guess[j];
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  for (i = 0; i < n; i++) {
    set_val(name, values[i]);
    evaluate_derived();
    redo_all_fun_tables();
    ierr = 0;
    /* Each search starts where the last one ended, or after a failure
       from the last equilibrium found, as in do_eq_range(). */
    do_sing_info(x, NEWT_ERR, EVEC_ERR, BOUND, EVEC_ITER, NODE, re, im, &ierr);
    count_stability(re, im, NODE, &unstable, &stable);
    xppe_result_put(0, i, values[i]);
    for (j = 0; j < NODE; j++) xppe_result_put(j + 1, i, ierr ? NAN : x[j]);
    xppe_result_put(NODE + 1, i, ierr ? NAN : (double)unstable);
    xppe_result_put(NODE + 2, i, ierr ? NAN : (double)stable);
  }
  set_val(name, old);
  evaluate_derived();
  redo_all_fun_tables();
  XPPE_GUARD_END;
  xppe_end_call();
  return XPPE_OK;
}


/* The eigenvalues along a branch of equilibria (Sing pts > Range with the
   eigenvalues kept), matched from step to step. */
int xppe_parameter_locus(const char *parameter, double from, double to, int steps,
                         const double *guess) {
  static const char *names[3 * MAXODE + 1];
  static char labels[2 * MAXODE][16];
  char name[256];
  static double pre[MAXODE], pim[MAXODE]; /* the last good step, off the stack */
  double x[MAXODE], re[MAXODE], im[MAXODE], old = 0;
  int rc, i, j, ierr, have_prev = 0;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (steps < 1 || !guess || !isfinite(from) || !isfinite(to)) {
    xppe_set_error("give finite ends, at least one step and a starting guess", NULL);
    return XPPE_ERR_RANGE;
  }
  if (strlen(parameter) >= sizeof name) {
    xppe_set_error("parameter name too long", NULL);
    return XPPE_ERR_RANGE;
  }
  strcpy(name, parameter);
  if (find_user_name(PARAM_BOX, name) < 0) {
    xppe_set_error("no parameter named %s", parameter);
    return XPPE_ERR_NAME;
  }
  names[0] = parameter;
  for (j = 0; j < NODE; j++) {
    names[j + 1] = uvar_names[j];
    snprintf(labels[2 * j], sizeof labels[0], "re%d", j + 1);
    snprintf(labels[2 * j + 1], sizeof labels[0], "im%d", j + 1);
    names[NODE + 1 + 2 * j] = labels[2 * j];
    names[NODE + 2 + 2 * j] = labels[2 * j + 1];
  }
  if ((rc = xppe_result_start(steps, 1 + 3 * NODE, names)) != XPPE_OK) return rc;
  get_val(name, &old);
  for (j = 0; j < NODE; j++) x[j] = guess[j];
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  for (i = 0; i < steps; i++) {
    double v = xppe_locus_value(from, to, steps, i);
    set_val(name, v);
    evaluate_derived();
    redo_all_fun_tables();
    ierr = 0;
    /* each search starts where the last one ended, as in do_eq_range() */
    do_sing_info(x, NEWT_ERR, EVEC_ERR, BOUND, EVEC_ITER, NODE, re, im, &ierr);
    if (!ierr) {
      if (have_prev) xppe_match_eigenvalues(NODE, pre, pim, re, im);
      memcpy(pre, re, sizeof(double) * (size_t)NODE);
      memcpy(pim, im, sizeof(double) * (size_t)NODE);
      have_prev = 1;
    }
    xppe_result_put(0, i, v);
    for (j = 0; j < NODE; j++) {
      xppe_result_put(j + 1, i, ierr ? NAN : x[j]);
      xppe_result_put(NODE + 1 + 2 * j, i, ierr ? NAN : re[j]);
      xppe_result_put(NODE + 2 + 2 * j, i, ierr ? NAN : im[j]);
    }
  }
  set_val(name, old);
  evaluate_derived();
  redo_all_fun_tables();
  XPPE_GUARD_END;
  xppe_end_call();
  return XPPE_OK;
}
/* monte_carlo() (integrate.c) with its dialog's answers as arguments. */
int xppe_find_equilibria(int guesses, double tolerance, const double *lo,
                         const double *hi) {
  int rc, j;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (guesses < 1 || tolerance < 0 || !lo || !hi) {
    xppe_set_error("give at least one guess, a tolerance of 0 or more, and "
                   "the box", NULL);
    return XPPE_ERR_RANGE;
  }
  fixptguess.n = guesses;
  fixptguess.tol = tolerance;
  for (j = 0; j < NODE; j++) {
    fixptguess.xlo[j] = lo[j];
    fixptguess.xhi[j] = hi[j];
  }
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  evaluate_derived();
  do_monte_carlo_search(0, 0, 0);
  XPPE_GUARD_END;
  xppe_end_call();
  /* XPPAUT counts past MAXFP but keeps only that many. */
  return fixptlist.n < MAXFP ? fixptlist.n : MAXFP;
}

int xppe_found_equilibrium(int i, double *x, double *re, double *im) {
  int rc, j;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (!fixptlist.flag || i < 0 || i >= fixptlist.n || i >= MAXFP) {
    xppe_set_error("no such equilibrium (see xppe_find_equilibria)", NULL);
    return XPPE_ERR_RANGE;
  }
  for (j = 0; j < NODE; j++) {
    x[j] = fixptlist.x[i][j];
    re[j] = fixptlist.er[i][j];
    im[j] = fixptlist.em[i][j];
  }
  return XPPE_OK;
}
