/*
 * Phase-plane tools for the engine API (see xppe.h): the vector field and
 * XPPAUT's own nullcline algorithm, with the plot window passed in instead
 * of read from XPPAUT's graphics state.
 */
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
