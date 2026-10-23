/*
 * Phase-plane tools for the engine API (see xppe.h): the vector field and
 * XPPAUT's own nullcline algorithm, with the plot window passed in instead
 * of read from XPPAUT's graphics state.
 */
#include <math.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "xppe.h"
#include "xppe_internal.h"

#include "xpplim.h"

#define MAX_NULL 10000 /* as in nullcline.c */

extern int NODE, NMarkov, FIX_VAR;
extern double last_ic[MAXODE];
extern int (*rhs)();
extern int null_ix, null_iy, WHICH_CRV;
extern float *NTop, *NBot;

void set_ivar(int i, double value);
void new_nullcline(int course, double xlo, double ylo, double xhi,
                   double yhi, float *stor, int *npts);

int xppe_rhs(double t, const double *x, double *dxdt) {
  double y[MAXODE];
  int rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  memcpy(y, x, sizeof(double) * (size_t)NODE);
  XPPE_GUARD_BEGIN((void)0);
  rhs(t, y, dxdt, NODE);
  XPPE_GUARD_END;
  return XPPE_OK;
}

/* ---- Invariant manifolds of saddles ------------------------------------- */

/* After xppe_equilibrium() finds a saddle with a one-dimensional stable or
   unstable direction, do_sing_info() (gear.c) leaves starting points just
   off the equilibrium along each eigenvector in ShootIC, with ShootType +1
   (unstable) or -1 (stable). */
extern double ShootIC[8][MAXODE];
extern int ShootIndex, ShootType[8];
extern int STORFLAG, MyStart, storind, RANGE_FLAG, DelayErr, POIEXT;
extern double DELTA_T, T0, MyTime;
void usual_integrate_stuff(double *x);
void reset_browser(void);

int xppe_manifold_count(void) { return xppe_loaded() ? ShootIndex : 0; }

/* Integrate manifold branch k (forward in time for an unstable branch,
   backward for a stable one), as save_batch_shoot() in gear.c does. The
   result replaces the current run: read it with xppe_rows/xppe_column. */
int xppe_manifold(int k, int *type) {
  double x[MAXODE], olddt = DELTA_T;
  int rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (k < 0 || k >= ShootIndex) {
    xppe_set_error("no such manifold branch; find a saddle first", NULL);
    return XPPE_ERR_RANGE;
  }
  memcpy(x, ShootIC[k], sizeof(double) * (size_t)NODE);
  if (type) *type = ShootType[k];
  xppe_begin_call();
  XPPE_GUARD_BEGIN((DELTA_T = olddt));
  MyStart = 1;
  RANGE_FLAG = 0;
  DelayErr = 0;
  MyTime = T0;
  STORFLAG = 1;
  POIEXT = 0;
  storind = 0;
  reset_browser();
  DELTA_T = ShootType[k] > 0 ? fabs(DELTA_T) : -fabs(DELTA_T);
  usual_integrate_stuff(x);
  XPPE_GUARD_END;
  DELTA_T = olddt;
  xppe_end_call();
  return XPPE_OK;
}

/* Mirrors new_clines_com(0) in nullcline.c for one of the two curves. */
int xppe_nullcline(int which, int ix, int iy, double xlo, double xhi,
                   double ylo, double yhi, int grid, float *segments,
                   int max_segments) {
  float *store, *top, *bot, *old_top = NTop, *old_bot = NBot;
  int i, n = 0, rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (ix < 0 || iy < 0 || ix >= NODE || iy >= NODE || ix == iy || grid < 2 ||
      (which != 0 && which != 1)) {
    xppe_set_error("bad nullcline request", NULL);
    return XPPE_ERR_RANGE;
  }
  store = malloc(sizeof(float) * 4 * MAX_NULL);
  top = malloc(sizeof(float) * (size_t)(grid + 1));
  bot = malloc(sizeof(float) * (size_t)(grid + 1));
  if (!store || !top || !bot) {
    free(store);
    free(top);
    free(bot);
    xppe_set_error("out of memory", NULL);
    return XPPE_ERR_RANGE;
  }
  xppe_begin_call();
  XPPE_GUARD_BEGIN((NTop = old_top, NBot = old_bot));
  for (i = NODE; i < NODE + NMarkov; i++) set_ivar(i + 1 + FIX_VAR, last_ic[i]);
  NTop = top;
  NBot = bot;
  null_ix = ix + 1; /* XPPAUT numbers variables from 1; 0 is time */
  null_iy = iy + 1;
  WHICH_CRV = which == 0 ? null_ix : null_iy;
  new_nullcline(grid, (float)xlo, (float)ylo, (float)xhi, (float)yhi, store,
                &n);
  XPPE_GUARD_END;
  NTop = old_top;
  NBot = old_bot;
  if (n > max_segments) n = max_segments;
  if (segments) memcpy(segments, store, sizeof(float) * 4 * (size_t)n);
  free(store);
  free(top);
  free(bot);
  xppe_end_call();
  return n;
}
