/*
 * The animator for the engine API (see xppe.h). XPPAUT's own parser reads
 * the .ani file and its own code evaluates every command at a row of the
 * run, exactly as its Viewer does before drawing; here the results are
 * handed to the caller as a list of shapes instead of being drawn with X.
 */
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "xppe.h"
#include "xppe_internal.h"

#include <math.h>
#include <stdlib.h>

#include "xpplim.h"
#include "browse.h"
#include "aniparse.h"
#include "nullcline.h"

/* aniparse.c's command types and flags */
#define LINE 0
#define RLINE 1
#define CIRC 2
#define FCIRC 3
#define RECT 4
#define FRECT 5
#define TEXT 6
#define VTEXT 7
#define SETTEXT 8
#define ELLIP 9
#define FELLIP 10
#define COMET 11
#define AXNULL 13
#define AYNULL 14
#define MAX_NULL 10000 /* nullcline.c's segment limit */
#define TRANSIENT 20

extern ANI_COM my_ani[];
extern int n_anicom;
extern double ani_xlo, ani_xhi, ani_ylo, ani_yhi, ani_lastx, ani_lasty;
extern int NODE, NMarkov;
extern BROWSER my_browser;
void set_fix_rhs(double t, double *y);
int ani_new_file(char *filename);
void init_ani_stuff(void);
void free_ani(void);
void set_ani_perm(void);
void eval_ani_com(int j);
void eval_ani_color(int j);
void fix_only(void);
int get_val(char *name, double *value);
int set_val(char *name, double value);
double evaluate(int *equat);
double get_ivar(int i);
extern ANI_GRAB ani_grab[];
extern int n_ani_grab, who_was_grabbed;
extern int null_ix, null_iy, WHICH_CRV, num_x_n, num_y_n, n_nstore;
extern float *X_n, *Y_n, *NTop, *NBot;
extern double last_ic[MAXODE];
extern int FIX_VAR;

static xppe_ani_item items[MAX_ANI_LINES];
static int nitems, loaded_ani;
/* The segments of the nullcline items of the last frame, 4 numbers each, in
   the animation's coordinates; an item's segments follow those before it. */
static double *segs;
static int nsegs, segcap;
static int seg_start[MAX_ANI_LINES], seg_count[MAX_ANI_LINES];

int xppe_ani_load(const char *path) {
  char name[1024];
  int rc, ok = -1;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  snprintf(name, sizeof name, "%s", path);
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  /* As when XPPAUT opens its Viewer: a fresh start, commands transient
     until the file says "permanent", the window 0..1 until "dimension".
     (XPPAUT's own start-up only sets these when its windows open.) */
  if (n_anicom > 0) free_ani();
  init_ani_stuff();
  ok = ani_new_file(name);
  if (ok == 0) set_ani_perm();
  XPPE_GUARD_END;
  xppe_end_call();
  loaded_ani = ok == 0;
  nitems = 0;
  if (!loaded_ani) {
    xppe_set_error("could not read the animation file; see the log", NULL);
    return XPPE_ERR_LOAD;
  }
  return n_anicom;
}

int xppe_ani_window(double *xlo, double *ylo, double *xhi, double *yhi) {
  if (xlo) *xlo = ani_xlo;
  if (ylo) *ylo = ani_ylo;
  if (xhi) *xhi = ani_xhi;
  if (yhi) *yhi = ani_yhi;
  return loaded_ani ? XPPE_OK : XPPE_ERR_STATE;
}

static void colour(int j, xppe_ani_item *it) {
  int c = my_ani[j].col[0];
  if (c <= 0) {
    it->color = -c; /* XPPAUT's colour index: 0 black, 20-29 named */
    it->level = -1;
  } else {
    it->color = -1;
    it->level = my_ani[j].zcol; /* 0..1 along the colour scale */
  }
}

/* ani_flip1() and render_ani() without the drawing. With `current`, as after
   a grab move, the variables keep the values they have: ani_frame(0). */
static int frame_at(int row, int current) {
  double y[MAXODE], t;
  int rc, i, j, size = 0, serif = 0, text_colour = 0;
  float **ss;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (!loaded_ani) {
    xppe_set_error("load an animation file first", NULL);
    return XPPE_ERR_STATE;
  }
  ss = my_browser.data;
  if (!current && (my_browser.maxrow < 1 || !ss)) {
    xppe_set_error("integrate the model first", NULL);
    return XPPE_ERR_STATE;
  }
  if (row < 0) row = 0;
  if (!current && row >= my_browser.maxrow) row = my_browser.maxrow - 1;
  nitems = 0;
  nsegs = 0;
  XPPE_GUARD_BEGIN((void)0);
  if (!current) {
    if (row == 0) set_ani_perm();
    t = (double)ss[0][row];
    for (i = 0; i < NODE + NMarkov; i++) y[i] = (double)ss[i + 1][row];
    set_fix_rhs(t, y);
  }
  for (j = 0; j < n_anicom; j++) {
    int type = my_ani[j].type;
    xppe_ani_item *it = &items[nitems];
    if (type == SETTEXT) {
      size = my_ani[j].tsize;
      serif = my_ani[j].tfont;
      text_colour = my_ani[j].tcolor;
      continue;
    }
    if (type == AXNULL || type == AYNULL) {
      /* draw_ani_null(): the segments of the nullcline, with the box
         (x1,y1)-(x2,y2) of the phase plane mapped onto 0..1. */
      double xl, xh, yl, yh;
      float *v, dx, dy;
      int n, k;
      memset(it, 0, sizeof *it);
      eval_ani_color(j);
      colour(j, it);
      if (my_ani[j].flag == TRANSIENT) eval_ani_com(j);
      xl = my_ani[j].zx1;
      yl = my_ani[j].zy1;
      xh = my_ani[j].zx2;
      yh = my_ani[j].zy2;
      dx = (float)(xh - xl);
      dy = (float)(yh - yl);
      if (dx == 0.0 || dy == 0.0) continue;
      if (get_nullcline_floats(&v, &n, (int)my_ani[j].zval, type - AXNULL))
        continue;
      if (nsegs + n > segcap) {
        double *g;
        segcap = 2 * (nsegs + n);
        g = realloc(segs, sizeof(double) * 4 * (size_t)segcap);
        if (!g) continue;
        segs = g;
      }
      for (k = 0; k < n; k++) {
        double *o = segs + 4 * (size_t)(nsegs + k);
        o[0] = (float)((v[4 * k] - xl) / dx);
        o[1] = (float)((v[4 * k + 1] - yl) / dy);
        o[2] = (float)((v[4 * k + 2] - xl) / dx);
        o[3] = (float)((v[4 * k + 3] - yl) / dy);
      }
      it->command = j;
      it->shape = XPPE_ANI_NULLCLINE;
      it->thickness = 1;
      it->trail = n;
      seg_start[nitems] = nsegs;
      seg_count[nitems] = n;
      nsegs += n;
      nitems++;
      continue;
    }
    if (type > COMET) continue; /* grab points are not shapes */
    memset(it, 0, sizeof *it);
    if (type == LINE || type == RLINE || type == RECT || type == FRECT ||
        type == CIRC || type == FCIRC || type == ELLIP || type == FELLIP ||
        type == COMET) {
      eval_ani_color(j);
      colour(j, it); /* text has no colour of its own: see SETTEXT */
    }
    if (my_ani[j].flag == TRANSIENT) eval_ani_com(j);
    it->command = j;
    it->thickness = my_ani[j].zthick;
    it->x1 = my_ani[j].zx1;
    it->y1 = my_ani[j].zy1;
    switch (type) {
    case LINE:
      it->shape = XPPE_ANI_LINE;
      it->x2 = my_ani[j].zx2;
      it->y2 = my_ani[j].zy2;
      ani_lastx = it->x2;
      ani_lasty = it->y2;
      break;
    case RLINE: /* from the end of the last line */
      it->shape = XPPE_ANI_LINE;
      it->x1 = ani_lastx;
      it->y1 = ani_lasty;
      it->x2 = ani_lastx + my_ani[j].zx1;
      it->y2 = ani_lasty + my_ani[j].zy1;
      ani_lastx = it->x2;
      ani_lasty = it->y2;
      break;
    case RECT:
    case FRECT:
      it->shape = XPPE_ANI_RECT;
      it->filled = type == FRECT;
      it->x2 = my_ani[j].zx2;
      it->y2 = my_ani[j].zy2;
      break;
    case CIRC:
    case FCIRC:
      it->shape = XPPE_ANI_ELLIPSE;
      it->filled = type == FCIRC;
      it->x2 = it->y2 = my_ani[j].zrad;
      break;
    case ELLIP:
    case FELLIP:
      it->shape = XPPE_ANI_ELLIPSE;
      it->filled = type == FELLIP;
      it->x2 = my_ani[j].zx2; /* radii */
      it->y2 = my_ani[j].zy2;
      break;
    case COMET:
      it->shape = XPPE_ANI_COMET;
      it->trail = my_ani[j].c.n;
      break;
    case TEXT:
    case VTEXT:
      it->shape = XPPE_ANI_TEXT;
      it->color = text_colour;
      it->level = -1;
      it->font_size = size;
      it->serif = serif;
      if (type == TEXT)
        snprintf(it->text, sizeof it->text, "%s", (char *)my_ani[j].y2);
      else
        snprintf(it->text, sizeof it->text, "%s%g", (char *)my_ani[j].y2,
                 my_ani[j].zval);
      break;
    default:
      continue;
    }
    nitems++;
  }
  XPPE_GUARD_END;
  return nitems;
}

int xppe_ani_frame(int row) { return frame_at(row, 0); }

int xppe_ani_frame_current(void) { return frame_at(0, 1); }

int xppe_ani_get(int i, xppe_ani_item *out) {
  if (i < 0 || i >= nitems) {
    xppe_set_error("no such animation item", NULL);
    return XPPE_ERR_RANGE;
  }
  *out = items[i];
  return XPPE_OK;
}

int xppe_ani_segments(int i, double *out, int max) {
  int n;
  if (i < 0 || i >= nitems || items[i].shape != XPPE_ANI_NULLCLINE) {
    xppe_set_error("that animation item is not a nullcline", NULL);
    return XPPE_ERR_RANGE;
  }
  n = seg_count[i];
  if (out) {
    if (n > max) n = max;
    memcpy(out, segs + 4 * (size_t)seg_start[i], sizeof(double) * 4 * (size_t)n);
  }
  return n;
}

/* Nullcline > New (and, with a parameter, > Range) for the plane of ODE
   variables ix and iy over the box of the file's first xnull or ynull
   command: the curves that xnull and ynull draw. */
int xppe_ani_nullclines(int ix, int iy, int grid, const char *param, int steps,
                        double plo, double phi) {
  double box[4], zold = 0;
  float *xs, *ys, *top, *bot, *old_top = NTop, *old_bot = NBot;
  char name[64] = "";
  int rc, j, k, nx = 0, ny = 0, found = 0, frozen = 0;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (!loaded_ani) {
    xppe_set_error("load an animation file first", NULL);
    return XPPE_ERR_STATE;
  }
  if (ix < 0 || iy < 0 || ix >= NODE || iy >= NODE || ix == iy || grid < 2) {
    xppe_set_error("bad nullcline request", NULL);
    return XPPE_ERR_RANGE;
  }
  if (param && *param && steps > 0) {
    snprintf(name, sizeof name, "%s", param);
    if (!get_val(name, &zold) || !(phi > plo)) {
      xppe_set_error("bad parameter or range for the nullclines", NULL);
      return XPPE_ERR_RANGE;
    }
  }
  for (j = 0; j < n_anicom; j++)
    if (my_ani[j].type == AXNULL || my_ani[j].type == AYNULL) {
      found = 1;
      break;
    }
  if (!found) {
    xppe_set_error("the animation has no xnull or ynull command", NULL);
    return XPPE_ERR_STATE;
  }
  xs = malloc(sizeof(float) * 4 * MAX_NULL);
  ys = malloc(sizeof(float) * 4 * MAX_NULL);
  top = malloc(sizeof(float) * (size_t)(grid + 1));
  bot = malloc(sizeof(float) * (size_t)(grid + 1));
  if (!X_n) X_n = malloc(sizeof(float) * 4 * MAX_NULL);
  if (!Y_n) Y_n = malloc(sizeof(float) * 4 * MAX_NULL);
  if (!xs || !ys || !top || !bot || !X_n || !Y_n) {
    free(xs);
    free(ys);
    free(top);
    free(bot);
    xppe_set_error("out of memory", NULL);
    return XPPE_ERR_RANGE;
  }
  xppe_begin_call();
  XPPE_GUARD_BEGIN((NTop = old_top, NBot = old_bot,
                    *name ? (void)set_val(name, zold) : (void)0));
  box[0] = evaluate(my_ani[j].x1);
  box[1] = evaluate(my_ani[j].y1);
  box[2] = evaluate(my_ani[j].x2);
  box[3] = evaluate(my_ani[j].y2);
  for (k = NODE; k < NODE + NMarkov; k++) set_ivar(k + 1 + FIX_VAR, last_ic[k]);
  NTop = top;
  NBot = bot;
  null_ix = ix + 1; /* XPPAUT numbers variables from 1; 0 is time */
  null_iy = iy + 1;
  if (n_nstore == 0) start_ncline();
  if (*name) {
    clear_froz_cline();
    for (k = 0; k <= steps; k++) {
      set_val(name, plo + (phi - plo) * (double)k / (double)steps);
      WHICH_CRV = null_ix;
      new_nullcline(grid, (float)box[0], (float)box[1], (float)box[2],
                    (float)box[3], xs, &nx);
      WHICH_CRV = null_iy;
      new_nullcline(grid, (float)box[0], (float)box[1], (float)box[2],
                    (float)box[3], ys, &ny);
      add_froz_cline(xs, nx, null_ix, ys, ny, null_iy);
      frozen++;
    }
    set_val(name, zold);
  }
  WHICH_CRV = null_ix;
  new_nullcline(grid, (float)box[0], (float)box[1], (float)box[2],
                (float)box[3], X_n, &num_x_n);
  WHICH_CRV = null_iy;
  new_nullcline(grid, (float)box[0], (float)box[1], (float)box[2],
                (float)box[3], Y_n, &num_y_n);
  XPPE_GUARD_END;
  NTop = old_top;
  NBot = old_bot;
  free(xs);
  free(ys);
  free(top);
  free(bot);
  xppe_end_call();
  return frozen;
}

/* ---- Grab points ------------------------------------------------------------
   ani_buttonx() and update_ani_motion_stuff() of aniparse.c without the
   window: the same routines run in the same order. */
static struct {
  double x, y; /* the last mouse position */
} mouse;

int xppe_ani_grab_count(void) { return loaded_ani ? n_ani_grab : 0; }

int xppe_ani_grab_point(int i, double *x, double *y, double *tol) {
  int rc;
  double u = 0, v = 0;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (!loaded_ani || i < 0 || i >= n_ani_grab) {
    xppe_set_error("no such grab point", NULL);
    return XPPE_ERR_RANGE;
  }
  XPPE_GUARD_BEGIN((void)0);
  u = evaluate(ani_grab[i].x);
  v = evaluate(ani_grab[i].y);
  XPPE_GUARD_END;
  if (x) *x = u;
  if (y) *y = v;
  if (tol) *tol = ani_grab[i].tol;
  return XPPE_OK;
}

int xppe_ani_grab_begin(double x, double y, int *index) {
  int rc, i;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (!loaded_ani) {
    xppe_set_error("load an animation file first", NULL);
    return XPPE_ERR_STATE;
  }
  XPPE_GUARD_BEGIN((void)0);
  /* draw_grab_points(): where the points are at the frame shown */
  for (i = 0; i < n_ani_grab; i++) {
    ani_grab[i].zx = evaluate(ani_grab[i].x);
    ani_grab[i].zy = evaluate(ani_grab[i].y);
  }
  who_was_grabbed = search_for_grab(x, y);
  XPPE_GUARD_END;
  mouse.x = x;
  mouse.y = y;
  if (index) *index = who_was_grabbed;
  return XPPE_OK;
}

int xppe_ani_grab_move(double x, double y, double dt) {
  int rc;
  double vx, vy;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (who_was_grabbed < 0 || !loaded_ani) return 0;
  if (dt == 0.0) dt = 10000000000.0;
  vx = (x - mouse.x) / dt;
  vy = (y - mouse.y) / dt;
  mouse.x = x;
  mouse.y = y;
  XPPE_GUARD_BEGIN((void)0);
  set_val("mouse_x", x);
  set_val("mouse_y", y);
  set_val("mouse_vx", vx);
  set_val("mouse_vy", vy);
  do_grab_tasks(1);
  fix_only();
  XPPE_GUARD_END;
  return 1;
}

int xppe_ani_grab_end(int *index, int *runnow) {
  int rc, i, who = who_was_grabbed;
  if (index) *index = -1;
  if (runnow) *runnow = 0;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (who < 0 || !loaded_ani) return XPPE_OK;
  XPPE_GUARD_BEGIN((void)0);
  do_grab_tasks(2);
  /* set_to_init_data() without its last call, redraw_ics(), which draws the
     edit boxes of XPPAUT's window */
  for (i = 0; i < NODE; i++) last_ic[i] = get_ivar(i + 1);
  for (i = NODE + FIX_VAR; i < NODE + FIX_VAR + NMarkov; i++)
    last_ic[i - FIX_VAR] = get_ivar(i + 1);
  XPPE_GUARD_END;
  if (index) *index = who;
  if (runnow) *runnow = run_now_grab();
  who_was_grabbed = -1;
  return XPPE_OK;
}
