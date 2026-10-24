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

#include "xpplim.h"
#include "browse.h"
#include "aniparse.h"

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

static xppe_ani_item items[MAX_ANI_LINES];
static int nitems, loaded_ani;

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

/* ani_flip1() and render_ani() without the drawing. */
int xppe_ani_frame(int row) {
  double y[MAXODE], t;
  int rc, i, j, size = 0, serif = 0, text_colour = 0;
  float **ss;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (!loaded_ani) {
    xppe_set_error("load an animation file first", NULL);
    return XPPE_ERR_STATE;
  }
  ss = my_browser.data;
  if (my_browser.maxrow < 1 || !ss) {
    xppe_set_error("integrate the model first", NULL);
    return XPPE_ERR_STATE;
  }
  if (row < 0) row = 0;
  if (row >= my_browser.maxrow) row = my_browser.maxrow - 1;
  nitems = 0;
  XPPE_GUARD_BEGIN((void)0);
  if (row == 0) set_ani_perm();
  t = (double)ss[0][row];
  for (i = 0; i < NODE + NMarkov; i++) y[i] = (double)ss[i + 1][row];
  set_fix_rhs(t, y);
  for (j = 0; j < n_anicom; j++) {
    int type = my_ani[j].type;
    xppe_ani_item *it = &items[nitems];
    if (type == SETTEXT) {
      size = my_ani[j].tsize;
      serif = my_ani[j].tfont;
      text_colour = my_ani[j].tcolor;
      continue;
    }
    if (type > COMET) continue; /* nullclines and grab points: not drawn */
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

int xppe_ani_get(int i, xppe_ani_item *out) {
  if (i < 0 || i >= nitems) {
    xppe_set_error("no such animation item", NULL);
    return XPPE_ERR_RANGE;
  }
  *out = items[i];
  return XPPE_OK;
}
