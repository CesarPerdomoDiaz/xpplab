/* Figures: the page laid out as paths (fig.h).

   The look follows matplotlib's defaults, so that a figure redrawn by
   xpplab.figure in Python matches: axes lines and ticks 0.8 pt, ticks 3.5
   pt long pointing out, data limits widened by 5%, tab10 colours, dash
   patterns scaled with the line width. */
#include "fig.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MM 2.8346457f /* points per millimetre */

static const unsigned tab10[] = {0x1f77b4, 0xff7f0e, 0x2ca02c, 0xd62728, 0x9467bd,
                                 0x8c564b, 0xe377c2, 0x7f7f7f, 0xbcbd22, 0x17becf};

void fig_color(int k, float rgb[3]) {
  unsigned c = tab10[((k % 10) + 10) % 10];
  rgb[0] = ((c >> 16) & 255) / 255.0f;
  rgb[1] = ((c >> 8) & 255) / 255.0f;
  rgb[2] = (c & 255) / 255.0f;
}

void fig_panel_init(FigPanel *p) {
  memset(p, 0, sizeof *p);
  p->autox = p->autoy = true;
  p->xmin = p->ymin = 0;
  p->xmax = p->ymax = 1;
  p->legend = FIG_LEGEND_BEST;
  p->ix = 0.55f;
  p->iy = 0.55f;
  p->iw = p->ih = 0.4f;
}

void fig_init(Figure *f) {
  memset(f, 0, sizeof *f);
  f->width_mm = 86;
  f->height_mm = 65;
  f->rows = f->cols = 1;
  f->font = FIG_FONT_SANS;
  f->font_pt = 9;
  f->npanels = 1;
  f->dpi = 300;
  fig_panel_init(&f->p[0]);
}

void fig_series_free(FigSeries *s) {
  free(s->x);
  free(s->y);
  free(s->py);
  s->x = s->y = NULL;
  s->py = NULL;
  s->n = 0;
}

void fig_free(Figure *f) {
  int i, k;
  for (i = 0; i < FIG_MAX_PANELS; i++)
    for (k = 0; k < f->p[i].nseries; k++) fig_series_free(&f->p[i].s[k]);
  f->npanels = 0;
}

FigSeries *fig_add(FigPanel *p, const double *x, const double *y, int n) {
  FigSeries *s;
  if (p->nseries >= FIG_MAX_SERIES || n < 0) return NULL;
  s = &p->s[p->nseries];
  memset(s, 0, sizeof *s);
  s->x = malloc(sizeof(double) * (size_t)(n > 0 ? n : 1));
  s->y = malloc(sizeof(double) * (size_t)(n > 0 ? n : 1));
  if (!s->x || !s->y) {
    fig_series_free(s);
    return NULL;
  }
  if (n > 0) {
    memcpy(s->x, x, sizeof(double) * (size_t)n);
    memcpy(s->y, y, sizeof(double) * (size_t)n);
  }
  s->n = n;
  s->sx = s->sy = 1;
  fig_color(p->nseries, s->color);
  memcpy(s->mcolor, s->color, sizeof s->color);
  s->line = FIG_LINE_SOLID;
  s->width = 1.5f;
  s->msize = 6;
  s->mfill = true;
  s->every = 1;
  p->nseries++;
  return s;
}

void fig_remove(FigPanel *p, int k) {
  if (k < 0 || k >= p->nseries) return;
  fig_series_free(&p->s[k]);
  memmove(&p->s[k], &p->s[k + 1], sizeof(FigSeries) * (size_t)(p->nseries - k - 1));
  p->nseries--;
}

void fig_set_grid(Figure *f, int rows, int cols) {
  int old = f->rows * f->cols, nw, i;
  if (rows < 1) rows = 1;
  if (cols < 1) cols = 1;
  if (rows * cols > FIG_MAX_PANELS) return;
  nw = rows * cols;
  if (nw > old) {
    /* new grid panels go before the insets */
    int ninsets = f->npanels - old;
    if (f->npanels + (nw - old) > FIG_MAX_PANELS) return;
    memmove(&f->p[nw], &f->p[old], sizeof(FigPanel) * (size_t)ninsets);
    for (i = old; i < nw; i++) fig_panel_init(&f->p[i]);
    for (i = nw; i < nw + ninsets; i++)
      if (f->p[i].parent >= old) f->p[i].parent += nw - old;
    f->npanels += nw - old;
  } else if (nw < old) {
    int k;
    for (i = nw; i < old; i++)
      for (k = 0; k < f->p[i].nseries; k++) fig_series_free(&f->p[i].s[k]);
    memmove(&f->p[nw], &f->p[old], sizeof(FigPanel) * (size_t)(f->npanels - old));
    f->npanels -= old - nw;
    for (i = nw; i < f->npanels; i++) {
      f->p[i].parent -= old - nw;
      if (f->p[i].parent < 0 || f->p[i].parent >= nw) f->p[i].parent = 0;
    }
  }
  f->rows = rows;
  f->cols = cols;
}

/* ---- The display list ---------------------------------------------------------- */

int fig_dl_op(FigDL *dl, int op, const float rgb[3]) {
  FigOp *o;
  if (dl->nop == dl->capop) {
    int cap = dl->capop ? 2 * dl->capop : 64;
    FigOp *t = realloc(dl->op, sizeof(FigOp) * (size_t)cap);
    if (!t) return dl->nop - 1;
    dl->op = t;
    dl->capop = cap;
  }
  o = &dl->op[dl->nop];
  memset(o, 0, sizeof *o);
  o->op = op;
  if (rgb) memcpy(o->rgb, rgb, sizeof o->rgb);
  o->first = dl->ncmd;
  return dl->nop++;
}

void fig_dl_cmd(FigDL *dl, int type, const float *p) {
  FigCmd *c;
  int k = type == FIG_MOVE || type == FIG_LINE ? 2 : type == FIG_QUAD ? 4 : type == FIG_CUBIC ? 6 : 0;
  if (dl->ncmd == dl->capcmd) {
    int cap = dl->capcmd ? 2 * dl->capcmd : 1024;
    FigCmd *t = realloc(dl->cmd, sizeof(FigCmd) * (size_t)cap);
    if (!t) return;
    dl->cmd = t;
    dl->capcmd = cap;
  }
  c = &dl->cmd[dl->ncmd++];
  memset(c, 0, sizeof *c);
  c->type = type;
  if (k) memcpy(c->p, p, sizeof(float) * (size_t)k);
}

static void close_op(FigDL *dl, int op) {
  if (op >= 0 && op < dl->nop) dl->op[op].count = dl->ncmd - dl->op[op].first;
}

void fig_dl_free(FigDL *dl) {
  free(dl->op);
  free(dl->cmd);
  memset(dl, 0, sizeof *dl);
}

static void move(FigDL *dl, float x, float y) {
  float p[2] = {x, y};
  fig_dl_cmd(dl, FIG_MOVE, p);
}

static void line(FigDL *dl, float x, float y) {
  float p[2] = {x, y};
  fig_dl_cmd(dl, FIG_LINE, p);
}

static void closep(FigDL *dl) { fig_dl_cmd(dl, FIG_CLOSE, NULL); }

static void rect_path(FigDL *dl, float x0, float y0, float x1, float y1) {
  move(dl, x0, y0);
  line(dl, x1, y0);
  line(dl, x1, y1);
  line(dl, x0, y1);
  closep(dl);
}

static void fill_rect(FigDL *dl, float x0, float y0, float x1, float y1, const float rgb[3]) {
  int op = fig_dl_op(dl, FIG_OP_FILL, rgb);
  rect_path(dl, x0, y0, x1, y1);
  close_op(dl, op);
}

static int stroke_op(FigDL *dl, const float rgb[3], float width, const float *dash) {
  int op = fig_dl_op(dl, FIG_OP_STROKE, rgb);
  dl->op[op].width = width;
  if (dash) memcpy(dl->op[op].dash, dash, sizeof(float) * 4);
  return op;
}

static void segment(FigDL *dl, float x0, float y0, float x1, float y1, const float rgb[3],
                    float width) {
  int op = stroke_op(dl, rgb, width, NULL);
  move(dl, x0, y0);
  line(dl, x1, y1);
  close_op(dl, op);
}

/* ---- Limits and ticks ------------------------------------------------------------ */

static double tr(double v, bool lg) { return lg ? (v > 0 ? log10(v) : NAN) : v; }

static void data_range(const FigPanel *p, int axis, double *lo, double *hi) {
  int k, i;
  bool lg = axis ? p->logy : p->logx;
  *lo = DBL_MAX;
  *hi = -DBL_MAX;
  for (k = 0; k < p->nseries; k++) {
    const FigSeries *s = &p->s[k];
    const double *v = axis ? s->y : s->x;
    double sc = axis ? s->sy : s->sx;
    for (i = 0; i < s->n; i++) {
      double t = tr(v[i] * sc, lg);
      if (!isfinite(t)) continue;
      if (t < *lo) *lo = t;
      if (t > *hi) *hi = t;
    }
  }
  if (*lo > *hi) {
    *lo = 0;
    *hi = 1;
  }
}

void fig_limits(const FigPanel *p, double lim[4]) {
  int axis;
  for (axis = 0; axis < 2; axis++) {
    bool au = axis ? p->autoy : p->autox, lg = axis ? p->logy : p->logx;
    double lo, hi;
    if (au) {
      double m;
      data_range(p, axis, &lo, &hi);
      if (hi - lo <= 0) {
        double d = lo != 0 ? fabs(lo) * 0.05 : 0.05;
        lo -= d;
        hi += d;
      } else {
        m = 0.05 * (hi - lo);
        lo -= m;
        hi += m;
      }
      if (lg) {
        lo = pow(10, lo);
        hi = pow(10, hi);
      }
    } else {
      lo = axis ? p->ymin : p->xmin;
      hi = axis ? p->ymax : p->xmax;
      if (lg && lo <= 0) lo = hi > 0 ? hi * 1e-3 : 1e-3;
      if (lg && hi <= lo) hi = lo * 10;
      if (!(hi > lo)) hi = lo + 1;
    }
    lim[2 * axis] = lo;
    lim[2 * axis + 1] = hi;
  }
}

/* A tick spacing of 1, 2, 2.5 or 5 times a power of ten giving about n
   intervals. */
static double nice_step(double range, int n) {
  double raw = range / (n > 0 ? n : 1), e = pow(10, floor(log10(raw))), m = raw / e;
  if (m <= 1) m = 1;
  else if (m <= 2) m = 2;
  else if (m <= 2.5) m = 2.5;
  else if (m <= 5) m = 5;
  else m = 10;
  return m * e;
}

typedef struct {
  int n;
  double v[64];       /* positions, in data units (log axes: decades) */
  char label[64][48];
  char offset[48];    /* "$\times10^{5}$" when the labels are scaled */
} Ticks;

static void format_tick(double v, double step, double scale, char *out, size_t len) {
  int dec = 0;
  double s = step / scale;
  char buf[48];
  while (dec < 10 && fabs(s * pow(10, dec) - floor(s * pow(10, dec) + 0.5)) > 1e-6 * s * pow(10, dec))
    dec++;
  v /= scale;
  if (fabs(v) < 1e-9 * s) v = 0;
  snprintf(buf, sizeof buf, "%.*f", dec, v);
  if (buf[0] == '-') /* a true minus sign, as matplotlib draws it */
    snprintf(out, len, "\xe2\x88\x92%s", buf + 1);
  else
    snprintf(out, len, "%s", buf);
}

static void make_ticks(double lo, double hi, bool lg, double step, float length_pt,
                       float font_pt, Ticks *t) {
  double a = lo < hi ? lo : hi, b = lo < hi ? hi : lo, v, scale = 1, big;
  int n = (int)(length_pt / (font_pt * 3.2f)), e;
  memset(t, 0, sizeof *t);
  if (n < 2) n = 2;
  if (n > 9) n = 9;
  if (lg) {
    double la = log10(a), lb = log10(b);
    int d0 = (int)ceil(la - 1e-9), d1 = (int)floor(lb + 1e-9), every = 1;
    while ((d1 - d0) / every > 8) every++;
    for (e = d0; e <= d1 && t->n < 64; e += every) {
      t->v[t->n] = e;
      snprintf(t->label[t->n], sizeof t->label[0], "$10^{%d}$", e);
      t->n++;
    }
    return;
  }
  if (!(step > 0) || (b - a) / step > 60) step = nice_step(b - a, n);
  big = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
  if (big >= 1e5 || (big > 0 && big < 1e-3)) {
    e = (int)floor(log10(big));
    scale = pow(10, e);
    snprintf(t->offset, sizeof t->offset, "$\\times10^{%d}$", e);
  }
  for (v = ceil(a / step - 1e-9) * step; v <= b + 1e-9 * step && t->n < 64; v += step) {
    double vv = fabs(v) < 1e-12 * step ? 0 : v;
    t->v[t->n] = vv;
    format_tick(vv, step, scale, t->label[t->n], sizeof t->label[0]);
    t->n++;
  }
}

/* ---- Mapping ------------------------------------------------------------------ */

void fig_to_page(const FigAxes *a, double x, double y, float *px, float *py) {
  double x0 = a->logx ? log10(a->xmin) : a->xmin, x1 = a->logx ? log10(a->xmax) : a->xmax;
  double y0 = a->logy ? log10(a->ymin) : a->ymin, y1 = a->logy ? log10(a->ymax) : a->ymax;
  double u = a->logx ? (x > 0 ? log10(x) : NAN) : x;
  double v = a->logy ? (y > 0 ? log10(y) : NAN) : y;
  *px = (float)(a->x0 + (u - x0) / (x1 - x0) * (a->x1 - a->x0));
  *py = (float)(a->y1 - (v - y0) / (y1 - y0) * (a->y1 - a->y0));
}

void fig_from_page(const FigAxes *a, float px, float py, double *x, double *y) {
  double x0 = a->logx ? log10(a->xmin) : a->xmin, x1 = a->logx ? log10(a->xmax) : a->xmax;
  double y0 = a->logy ? log10(a->ymin) : a->ymin, y1 = a->logy ? log10(a->ymax) : a->ymax;
  double u = x0 + (px - a->x0) / (a->x1 - a->x0) * (x1 - x0);
  double v = y0 + (a->y1 - py) / (a->y1 - a->y0) * (y1 - y0);
  *x = a->logx ? pow(10, u) : u;
  *y = a->logy ? pow(10, v) : v;
}

/* ---- Series ------------------------------------------------------------------- */

static void dash_of(int style, float width, float dash[4]) {
  float w = width < 1 ? 1 : width;
  memset(dash, 0, sizeof(float) * 4);
  if (style == FIG_LINE_DASHED) {
    dash[0] = 3.7f * w;
    dash[1] = 1.6f * w;
    dash[2] = 3.7f * w;
    dash[3] = 1.6f * w;
  } else if (style == FIG_LINE_DOTTED) {
    dash[0] = 1.0f * w;
    dash[1] = 1.65f * w;
    dash[2] = 1.0f * w;
    dash[3] = 1.65f * w;
  } else if (style == FIG_LINE_DASHDOT) {
    dash[0] = 6.4f * w;
    dash[1] = 1.6f * w;
    dash[2] = 1.0f * w;
    dash[3] = 1.6f * w;
  }
}

static void marker_path(FigDL *dl, int m, float x, float y, float size) {
  float r = size / 2, k;
  int i;
  switch (m) {
  case FIG_MARK_CIRCLE:
  case FIG_MARK_POINT:
    if (m == FIG_MARK_POINT) r = size / 4;
    /* four cubic arcs */
    k = 0.5522847f * r;
    move(dl, x + r, y);
    {
      float c1[6] = {x + r, y + k, x + k, y + r, x, y + r};
      float c2[6] = {x - k, y + r, x - r, y + k, x - r, y};
      float c3[6] = {x - r, y - k, x - k, y - r, x, y - r};
      float c4[6] = {x + k, y - r, x + r, y - k, x + r, y};
      fig_dl_cmd(dl, FIG_CUBIC, c1);
      fig_dl_cmd(dl, FIG_CUBIC, c2);
      fig_dl_cmd(dl, FIG_CUBIC, c3);
      fig_dl_cmd(dl, FIG_CUBIC, c4);
    }
    closep(dl);
    break;
  case FIG_MARK_SQUARE:
    r *= 0.85f;
    rect_path(dl, x - r, y - r, x + r, y + r);
    break;
  case FIG_MARK_TRIANGLE:
    move(dl, x, y - r);
    line(dl, x + r * 0.866f, y + r * 0.5f);
    line(dl, x - r * 0.866f, y + r * 0.5f);
    closep(dl);
    break;
  case FIG_MARK_DIAMOND:
    move(dl, x, y - r);
    line(dl, x + r * 0.7f, y);
    line(dl, x, y + r);
    line(dl, x - r * 0.7f, y);
    closep(dl);
    break;
  case FIG_MARK_STAR:
    for (i = 0; i < 10; i++) {
      float a = (float)(-1.5707963 + i * 3.14159265 / 5), rr = i % 2 ? r * 0.4f : r;
      if (i == 0) move(dl, x + rr * cosf(a), y + rr * sinf(a));
      else line(dl, x + rr * cosf(a), y + rr * sinf(a));
    }
    closep(dl);
    break;
  case FIG_MARK_X:
    r *= 0.75f;
    move(dl, x - r, y - r);
    line(dl, x + r, y + r);
    move(dl, x - r, y + r);
    line(dl, x + r, y - r);
    break;
  case FIG_MARK_PLUS:
    move(dl, x - r, y);
    line(dl, x + r, y);
    move(dl, x, y - r);
    line(dl, x, y + r);
    break;
  }
}

static void draw_marker(FigDL *dl, const FigSeries *s, float x, float y, float size) {
  int op;
  float edge = 1.0f;
  bool lines_only = s->marker == FIG_MARK_X || s->marker == FIG_MARK_PLUS;
  if (s->mfill && !lines_only) {
    op = fig_dl_op(dl, FIG_OP_FILL, s->color);
    marker_path(dl, s->marker, x, y, size);
    close_op(dl, op);
  }
  op = stroke_op(dl, lines_only ? s->color : s->mcolor, lines_only ? 1.5f * edge : edge, NULL);
  marker_path(dl, s->marker, x, y, size);
  close_op(dl, op);
}

static void draw_series(FigDL *dl, const FigAxes *a, const FigSeries *s) {
  int i, op, run = 0;
  float px, py, lastx = 0, lasty = 0;
  if (s->line != FIG_LINE_NONE && s->width > 0 && s->n > 1) {
    float dash[4];
    dash_of(s->line, s->width, dash);
    op = stroke_op(dl, s->color, s->width, dash);
    for (i = 0; i < s->n; i++) {
      double x = s->x[i] * s->sx, y = s->y[i] * s->sy;
      fig_to_page(a, x, y, &px, &py);
      if (!isfinite(px) || !isfinite(py) || fabsf(px) > 1e6f || fabsf(py) > 1e6f) {
        run = 0;
        continue;
      }
      /* skip points closer than a hundredth of a point to the last one */
      if (run && fabsf(px - lastx) < 0.01f && fabsf(py - lasty) < 0.01f && i < s->n - 1)
        continue;
      if (run == 0) move(dl, px, py);
      else line(dl, px, py);
      lastx = px;
      lasty = py;
      run++;
    }
    close_op(dl, op);
  }
  if (s->marker != FIG_MARK_NONE && s->msize > 0) {
    int every = s->every > 0 ? s->every : 1;
    for (i = 0; i < s->n; i += every) {
      fig_to_page(a, s->x[i] * s->sx, s->y[i] * s->sy, &px, &py);
      if (!isfinite(px) || !isfinite(py)) continue;
      if (px < a->x0 - s->msize || px > a->x1 + s->msize || py < a->y0 - s->msize ||
          py > a->y1 + s->msize)
        continue;
      draw_marker(dl, s, px, py, s->msize);
    }
  }
}

/* ---- Text sizes ------------------------------------------------------------------- */

typedef struct {
  float label, tick, title, legend;
} Sizes;

static Sizes sizes_of(const Figure *f, const FigPanel *p) {
  Sizes z;
  float base = f->font_pt > 2 ? f->font_pt : 9;
  if (p->inset) base *= 0.8f;
  z.label = base;
  z.tick = base * 0.85f;
  z.title = base * 1.1f;
  z.legend = base * 0.85f;
  return z;
}

static float text_width(const Figure *f, const char *s, float pt, float *asc, float *desc) {
  return fig_text(NULL, s, f->font, pt, 0, 0, 0, NULL, asc, desc);
}

#define TICK_LEN 3.5f
#define TICK_PAD 3.5f
#define LABEL_PAD 4.0f
#define TITLE_PAD 6.0f
#define AXIS_W 0.8f

/* The room a panel needs around its axes (left, right, top, bottom). */
static void margins(const Figure *f, const FigPanel *p, float axw, float axh, float m[4]) {
  Sizes z = sizes_of(f, p);
  double lim[4];
  Ticks tx, ty;
  float wmax = 0, a, d, hx = 0;
  int i;
  fig_limits(p, lim);
  make_ticks(lim[0], lim[1], p->logx, p->xstep, axw, z.tick, &tx);
  make_ticks(lim[2], lim[3], p->logy, p->ystep, axh, z.tick, &ty);
  for (i = 0; i < ty.n; i++) {
    float w = text_width(f, ty.label[i], z.tick, NULL, NULL);
    if (w > wmax) wmax = w;
  }
  for (i = 0; i < tx.n; i++) {
    text_width(f, tx.label[i], z.tick, &a, &d);
    if (a + d > hx) hx = a + d;
  }
  if (hx == 0) hx = z.tick * 0.75f;
  m[0] = TICK_LEN + TICK_PAD + wmax + 2;
  if (p->ylabel[0]) {
    text_width(f, p->ylabel, z.label, &a, &d);
    m[0] += LABEL_PAD + a + d;
  }
  m[3] = TICK_LEN + TICK_PAD + hx + 2;
  if (p->xlabel[0]) {
    text_width(f, p->xlabel, z.label, &a, &d);
    m[3] += LABEL_PAD + a + d;
  }
  m[2] = z.tick * 0.6f + 2;
  if (ty.offset[0] || p->title[0]) {
    float t = 0;
    if (p->title[0]) {
      text_width(f, p->title, z.title, &a, &d);
      t = TITLE_PAD + a + d;
    }
    if (ty.offset[0]) {
      text_width(f, ty.offset, z.tick, &a, &d);
      if (a + d + 4 > t) t = a + d + 4;
    }
    m[2] += t;
  }
  m[1] = tx.n ? text_width(f, tx.label[tx.n - 1], z.tick, NULL, NULL) / 2 + 3 : 6;
  if (tx.offset[0]) m[3] += 0; /* drawn at the right end, inside the bottom margin */
  if (p->legend == FIG_LEGEND_OUTSIDE) {
    float lw = 0;
    for (i = 0; i < p->nseries; i++)
      if (p->s[i].label[0]) {
        float w = text_width(f, p->s[i].label, z.legend, NULL, NULL);
        if (w > lw) lw = w;
      }
    if (lw > 0) m[1] += lw + 2.8f * z.legend + 6;
  }
}

/* ---- Legend ------------------------------------------------------------------- */

static void legend_sample(FigDL *dl, const FigSeries *s, float x, float y, float len) {
  if (s->line != FIG_LINE_NONE && s->width > 0) {
    float dash[4];
    int op;
    dash_of(s->line, s->width, dash);
    op = stroke_op(dl, s->color, s->width, dash);
    move(dl, x, y);
    line(dl, x + len, y);
    close_op(dl, op);
  }
  if (s->marker != FIG_MARK_NONE) draw_marker(dl, s, x + len / 2, y, s->msize);
}

static void draw_legend(const Figure *f, const FigPanel *p, const FigAxes *a, FigDL *dl) {
  Sizes z = sizes_of(f, p);
  float pad = 0.4f * z.legend, len = 2.0f * z.legend, gap = 0.8f * z.legend;
  float row = 1.3f * z.legend, w = 0, h, x, y, as, ds;
  int i, n = 0, where = p->legend;
  static const float white[3] = {1, 1, 1}, edge[3] = {0.8f, 0.8f, 0.8f}, black[3] = {0, 0, 0};
  for (i = 0; i < p->nseries; i++)
    if (p->s[i].label[0]) {
      float tw = text_width(f, p->s[i].label, z.legend, NULL, NULL);
      if (tw > w) w = tw;
      n++;
    }
  if (!n || where == FIG_LEGEND_NONE) return;
  w += 2 * pad + len + gap;
  h = 2 * pad + n * row;
  if (where == FIG_LEGEND_BEST) {
    /* the corner that covers the fewest points */
    int best = FIG_LEGEND_UPPER_RIGHT, c, k, j, least = -1;
    for (c = FIG_LEGEND_UPPER_RIGHT; c <= FIG_LEGEND_LOWER_RIGHT; c++) {
      float bx = c == FIG_LEGEND_UPPER_LEFT || c == FIG_LEGEND_LOWER_LEFT ? a->x0 + pad : a->x1 - pad - w;
      float by = c <= FIG_LEGEND_UPPER_LEFT ? a->y0 + pad : a->y1 - pad - h;
      int count = 0;
      for (k = 0; k < p->nseries; k++) {
        const FigSeries *s = &p->s[k];
        int step = s->n > 400 ? s->n / 400 : 1;
        for (j = 0; j < s->n; j += step) {
          float px, py;
          fig_to_page(a, s->x[j] * s->sx, s->y[j] * s->sy, &px, &py);
          if (px >= bx && px <= bx + w && py >= by && py <= by + h) count++;
        }
      }
      if (least < 0 || count < least) {
        least = count;
        best = c;
      }
    }
    where = best;
  }
  if (where == FIG_LEGEND_OUTSIDE) {
    x = a->x1 + 6;
    y = a->y0;
  } else {
    x = where == FIG_LEGEND_UPPER_LEFT || where == FIG_LEGEND_LOWER_LEFT ? a->x0 + pad : a->x1 - pad - w;
    y = where == FIG_LEGEND_UPPER_RIGHT || where == FIG_LEGEND_UPPER_LEFT ? a->y0 + pad : a->y1 - pad - h;
  }
  fill_rect(dl, x, y, x + w, y + h, white);
  {
    int op = stroke_op(dl, edge, AXIS_W, NULL);
    rect_path(dl, x, y, x + w, y + h);
    close_op(dl, op);
  }
  n = 0;
  for (i = 0; i < p->nseries; i++) {
    float cy;
    if (!p->s[i].label[0]) continue;
    cy = y + pad + (n + 0.5f) * row;
    legend_sample(dl, &p->s[i], x + pad, cy, len);
    text_width(f, p->s[i].label, z.legend, &as, &ds);
    fig_text(dl, p->s[i].label, f->font, z.legend, x + pad + len + gap,
             cy + 0.35f * z.legend, 0, black, NULL, NULL);
    n++;
  }
}

/* ---- One panel ------------------------------------------------------------------ */

static void draw_panel(const Figure *f, const FigPanel *p, FigAxes *a, FigDL *dl) {
  static const float black[3] = {0, 0, 0}, white[3] = {1, 1, 1}, grid[3] = {0.69f, 0.69f, 0.69f};
  Sizes z = sizes_of(f, p);
  float axw = a->x1 - a->x0, axh = a->y1 - a->y0, px, py, as, ds, w;
  Ticks tx, ty;
  int i, op;
  if (p->inset) fill_rect(dl, a->x0, a->y0, a->x1, a->y1, white);
  make_ticks(a->xmin, a->xmax, a->logx, p->xstep, axw, z.tick, &tx);
  make_ticks(a->ymin, a->ymax, a->logy, p->ystep, axh, z.tick, &ty);
  if (p->grid) {
    op = stroke_op(dl, grid, 0.6f, NULL);
    for (i = 0; i < tx.n; i++) {
      fig_to_page(a, a->logx ? pow(10, tx.v[i]) : tx.v[i], a->ymin, &px, &py);
      move(dl, px, a->y0);
      line(dl, px, a->y1);
    }
    for (i = 0; i < ty.n; i++) {
      fig_to_page(a, a->xmin, a->logy ? pow(10, ty.v[i]) : ty.v[i], &px, &py);
      move(dl, a->x0, py);
      line(dl, a->x1, py);
    }
    close_op(dl, op);
  }
  /* the curves, cut at the axes */
  op = fig_dl_op(dl, FIG_OP_CLIP, NULL);
  rect_path(dl, a->x0, a->y0, a->x1, a->y1);
  close_op(dl, op);
  for (i = 0; i < p->nseries; i++) draw_series(dl, a, &p->s[i]);
  fig_dl_op(dl, FIG_OP_UNCLIP, NULL);
  /* frame and ticks */
  op = stroke_op(dl, black, AXIS_W, NULL);
  rect_path(dl, a->x0, a->y0, a->x1, a->y1);
  close_op(dl, op);
  op = stroke_op(dl, black, AXIS_W, NULL);
  for (i = 0; i < tx.n; i++) {
    fig_to_page(a, a->logx ? pow(10, tx.v[i]) : tx.v[i], a->ymin, &px, &py);
    move(dl, px, a->y1);
    line(dl, px, a->y1 + TICK_LEN);
  }
  for (i = 0; i < ty.n; i++) {
    fig_to_page(a, a->xmin, a->logy ? pow(10, ty.v[i]) : ty.v[i], &px, &py);
    move(dl, a->x0, py);
    line(dl, a->x0 - TICK_LEN, py);
  }
  close_op(dl, op);
  for (i = 0; i < tx.n; i++) {
    fig_to_page(a, a->logx ? pow(10, tx.v[i]) : tx.v[i], a->ymin, &px, &py);
    w = text_width(f, tx.label[i], z.tick, &as, &ds);
    fig_text(dl, tx.label[i], f->font, z.tick, px - w / 2,
             a->y1 + TICK_LEN + TICK_PAD + z.tick * 0.75f, 0, black, NULL, NULL);
  }
  for (i = 0; i < ty.n; i++) {
    fig_to_page(a, a->xmin, a->logy ? pow(10, ty.v[i]) : ty.v[i], &px, &py);
    w = text_width(f, ty.label[i], z.tick, &as, &ds);
    fig_text(dl, ty.label[i], f->font, z.tick, a->x0 - TICK_LEN - TICK_PAD - w,
             py + z.tick * 0.36f, 0, black, NULL, NULL);
  }
  if (tx.offset[0]) {
    w = text_width(f, tx.offset, z.tick, &as, &ds);
    fig_text(dl, tx.offset, f->font, z.tick, a->x1 - w,
             a->y1 + TICK_LEN + TICK_PAD + 2 * z.tick * 0.95f, 0, black, NULL, NULL);
  }
  if (ty.offset[0])
    fig_text(dl, ty.offset, f->font, z.tick, a->x0, a->y0 - 4, 0, black, NULL, NULL);
  /* labels and title */
  if (p->xlabel[0]) {
    float hx = 0;
    for (i = 0; i < tx.n; i++) {
      text_width(f, tx.label[i], z.tick, &as, &ds);
      if (as + ds > hx) hx = as + ds;
    }
    if (hx == 0) hx = z.tick * 0.75f;
    w = text_width(f, p->xlabel, z.label, &as, &ds);
    fig_text(dl, p->xlabel, f->font, z.label, (a->x0 + a->x1) / 2 - w / 2,
             a->y1 + TICK_LEN + TICK_PAD + hx + LABEL_PAD + as, 0, black, NULL, NULL);
  }
  if (p->ylabel[0]) {
    float wmax = 0;
    for (i = 0; i < ty.n; i++) {
      float tw = text_width(f, ty.label[i], z.tick, NULL, NULL);
      if (tw > wmax) wmax = tw;
    }
    w = text_width(f, p->ylabel, z.label, &as, &ds);
    fig_text(dl, p->ylabel, f->font, z.label,
             a->x0 - TICK_LEN - TICK_PAD - wmax - LABEL_PAD - ds,
             (a->y0 + a->y1) / 2 + w / 2, 90, black, NULL, NULL);
  }
  if (p->title[0]) {
    w = text_width(f, p->title, z.title, &as, &ds);
    fig_text(dl, p->title, f->font, z.title, (a->x0 + a->x1) / 2 - w / 2,
             a->y0 - TITLE_PAD - ds, 0, black, NULL, NULL);
  }
  draw_legend(f, p, a, dl);
}

static void set_axes(const FigPanel *p, FigAxes *a, float x0, float y0, float x1, float y1) {
  double lim[4];
  fig_limits(p, lim);
  a->x0 = x0;
  a->y0 = y0;
  a->x1 = x1;
  a->y1 = y1;
  a->xmin = lim[0];
  a->xmax = lim[1];
  a->ymin = lim[2];
  a->ymax = lim[3];
  a->logx = p->logx;
  a->logy = p->logy;
  a->valid = x1 > x0 + 4 && y1 > y0 + 4;
  if (p->equal && a->valid && !p->logx && !p->logy) {
    /* shrink one side so that a unit is as long on both axes */
    double sx = (x1 - x0) / (lim[1] - lim[0]), sy = (y1 - y0) / (lim[3] - lim[2]);
    if (sx > sy) {
      float w = (float)((lim[1] - lim[0]) * sy), c = (x0 + x1) / 2;
      a->x0 = c - w / 2;
      a->x1 = c + w / 2;
    } else {
      float h = (float)((lim[3] - lim[2]) * sx), c = (y0 + y1) / 2;
      a->y0 = c - h / 2;
      a->y1 = c + h / 2;
    }
  }
}

void fig_build(const Figure *f, FigDL *dl) {
  static const float white[3] = {1, 1, 1};
  float W = f->width_mm * MM, H = f->height_mm * MM;
  int rows = f->rows > 0 ? f->rows : 1, cols = f->cols > 0 ? f->cols : 1;
  int ngrid = rows * cols, i, r, c, pass;
  float cw = W / cols, ch = H / rows;
  float left[FIG_MAX_PANELS], right[FIG_MAX_PANELS], top[FIG_MAX_PANELS], bottom[FIG_MAX_PANELS];
  dl->nop = dl->ncmd = 0;
  memset(dl->axes, 0, sizeof dl->axes);
  dl->width = W;
  dl->height = H;
  fill_rect(dl, 0, 0, W, H, white);
  if (ngrid > f->npanels) ngrid = f->npanels;
  /* Margins, shared by a column (left, right) and a row (top, bottom), so
     the axes line up; twice, since the ticks depend on the axes' size. */
  for (i = 0; i < FIG_MAX_PANELS; i++) left[i] = right[i] = top[i] = bottom[i] = 0;
  for (pass = 0; pass < 2; pass++) {
    float nl[FIG_MAX_PANELS] = {0}, nr[FIG_MAX_PANELS] = {0}, nt[FIG_MAX_PANELS] = {0},
          nb[FIG_MAX_PANELS] = {0};
    for (i = 0; i < ngrid; i++) {
      float m[4], axw, axh;
      r = i / cols;
      c = i % cols;
      axw = cw - (pass ? left[c] + right[c] : 50);
      axh = ch - (pass ? top[r] + bottom[r] : 40);
      margins(f, &f->p[i], axw > 20 ? axw : 20, axh > 20 ? axh : 20, m);
      if (m[0] > nl[c]) nl[c] = m[0];
      if (m[1] > nr[c]) nr[c] = m[1];
      if (m[2] > nt[r]) nt[r] = m[2];
      if (m[3] > nb[r]) nb[r] = m[3];
    }
    memcpy(left, nl, sizeof left);
    memcpy(right, nr, sizeof right);
    memcpy(top, nt, sizeof top);
    memcpy(bottom, nb, sizeof bottom);
  }
  for (i = 0; i < ngrid; i++) {
    r = i / cols;
    c = i % cols;
    set_axes(&f->p[i], &dl->axes[i], c * cw + left[c], r * ch + top[r],
             (c + 1) * cw - right[c], (r + 1) * ch - bottom[r]);
    if (dl->axes[i].valid) draw_panel(f, &f->p[i], &dl->axes[i], dl);
  }
  for (i = ngrid; i < f->npanels; i++) {
    const FigPanel *p = &f->p[i];
    const FigAxes *pa;
    if (!p->inset || p->parent < 0 || p->parent >= ngrid) continue;
    pa = &dl->axes[p->parent];
    if (!pa->valid) continue;
    {
      float aw = pa->x1 - pa->x0, ah = pa->y1 - pa->y0;
      float x0 = pa->x0 + p->ix * aw, x1 = x0 + p->iw * aw;
      float y1 = pa->y1 - p->iy * ah, y0 = y1 - p->ih * ah;
      set_axes(p, &dl->axes[i], x0, y0, x1, y1);
      if (dl->axes[i].valid) draw_panel(f, p, &dl->axes[i], dl);
    }
  }
}
