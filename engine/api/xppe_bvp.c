/*
 * Boundary-value problems for the engine API (see xppe.h): XPPAUT's
 * Bndryval > No show, by its own shooting method (bvshoot in pp_shoot.c).
 */
#include <setjmp.h>
#include <string.h>

#include "xppe.h"
#include "xppe_internal.h"

#include "xpplim.h"
#include "pp_shoot.h"

extern int NODE, NMarkov, NKernel, BVP_N, BVP_FLAG, BVP_MAXIT;
extern double BVP_TOL, BVP_EPS, TRANS;
extern int FFT, HIST, DelayFlag, STORFLAG, POIMAP, RANGE_FLAG, INFLAG;
extern int storind;
void get_ic(int it, double *x);
void data_back(void);
void refresh_browser(int length);

#define GOODSHOT 1 /* as in pp_shoot.c */
#define NOCHANGE 2

int xppe_bvp_count(void) { return xppe_loaded() ? BVP_N : 0; }

/* find_bvp_com(1) without its dialogs, returning the shooting result. */
int xppe_bvp(int max_iterations, double tolerance, double epsilon) {
  double ystart[MAXODE], yend[MAXODE], oldtrans = TRANS;
  int rc, iret = 0, usable = 0;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (BVP_N <= 0) {
    xppe_set_error("the model has no boundary conditions (bndry "
                   "statements)", NULL);
    return XPPE_ERR_STATE;
  }
  if (NMarkov > 0 || NKernel > 0) {
    xppe_set_error("Can't do BVP with integral or markov eqns", NULL);
    return XPPE_ERR_STATE;
  }
  if (max_iterations > 0) BVP_MAXIT = max_iterations;
  if (tolerance > 0) BVP_TOL = tolerance;
  if (epsilon > 0) BVP_EPS = epsilon;
  xppe_begin_call();
  XPPE_GUARD_BEGIN((TRANS = oldtrans));
  data_back();
  compile_bvp();
  usable = !(FFT || HIST || DelayFlag || BVP_FLAG == 0);
  if (usable) {
    STORFLAG = 0;
    RANGE_FLAG = 1;
    POIMAP = 0;
    TRANS = 0.0;
    get_ic(1, ystart);
    bvshoot(ystart, yend, BVP_TOL, BVP_EPS, BVP_MAXIT, &iret, NODE, 0, 0, 0,
            0, 0.0);
    bad_shoot(iret);
    if (iret == GOODSHOT || iret == NOCHANGE) {
      get_ic(0, ystart); /* the solution's initial conditions */
      last_shot(1);      /* and the solution, as the current run */
      INFLAG = 1;
      refresh_browser(storind);
    }
  }
  TRANS = oldtrans;
  XPPE_GUARD_END;
  xppe_end_call();
  if (!usable) {
    xppe_set_error("the boundary conditions could not be compiled, or the "
                   "model has delays", NULL);
    return XPPE_ERR_STATE;
  }
  if (iret != GOODSHOT && iret != NOCHANGE) {
    xppe_set_error("the shooting method did not converge; see the log", NULL);
    return XPPE_ERR_NUMERIC;
  }
  return XPPE_OK;
}
