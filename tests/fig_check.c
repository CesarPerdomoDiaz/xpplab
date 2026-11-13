/* The figure engine (app/src/fig.h) without the app: lay out a figure with
   two panels, an inset, mathtext, markers, dashes and a legend, write it as
   PNG, PDF, SVG and EPS, and check the files and the text parser.

   fig_check [dir]: the files go to dir (default: the current folder). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fig.h"

static int failures;

#define CHECK(cond, ...)                         \
  do {                                           \
    if (!(cond)) {                               \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
      fprintf(stderr, __VA_ARGS__);              \
      fprintf(stderr, "\n");                     \
      failures++;                                \
    }                                            \
  } while (0)

static long file_size(const char *path, char *head, size_t nhead, char *tail, size_t ntail) {
  FILE *f = fopen(path, "rb");
  long n;
  if (!f) return -1;
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  if (head) {
    fseek(f, 0, SEEK_SET);
    memset(head, 0, nhead);
    if (fread(head, 1, nhead - 1, f) == 0) head[0] = '\0';
  }
  if (tail && n > (long)ntail) {
    fseek(f, n - (long)ntail + 1, SEEK_SET);
    memset(tail, 0, ntail);
    if (fread(tail, 1, ntail - 1, f) == 0) tail[0] = '\0';
  }
  fclose(f);
  return n;
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : ".";
  char path[1024], err[256], head[64], tail[64], msg[160];
  Figure f;
  FigSeries *s;
  double x[200], y[200], y2[200];
  int i;
  FigDL dl;

  CHECK(fig_fonts_ok(), "fonts");
  CHECK(fig_text_check("$\\tilde{V}_{W,H}$ [m/s]", msg, sizeof msg), "mathtext: %s", msg);
  CHECK(fig_text_check("$\\frac{T}{mg}$ and $\\sqrt{g/L}$", msg, sizeof msg), "frac: %s", msg);
  CHECK(!fig_text_check("$\\nosuchthing$", msg, sizeof msg), "unknown command accepted");
  CHECK(strstr(msg, "nosuchthing") != NULL, "message: %s", msg);
  CHECK(!fig_text_check("$x^{2$", msg, sizeof msg), "open brace accepted");
  {
    float a1, d1, a2, d2;
    float w1 = fig_text(NULL, "$x$", FIG_FONT_SANS, 10, 0, 0, 0, NULL, &a1, &d1);
    float w2 = fig_text(NULL, "$x^2$", FIG_FONT_SANS, 10, 0, 0, 0, NULL, &a2, &d2);
    CHECK(w2 > w1 && a2 > a1, "a superscript widens and raises: %g %g %g %g", w1, w2, a1, a2);
    w2 = fig_text(NULL, "$x_2$", FIG_FONT_SERIF, 10, 0, 0, 0, NULL, &a2, &d2);
    CHECK(d2 > 0, "a subscript goes below the baseline: %g", d2);
  }

  for (i = 0; i < 200; i++) {
    x[i] = i * 0.05;
    y[i] = sin(x[i]);
    y2[i] = exp(-0.3 * x[i]) * cos(2 * x[i]);
  }
  fig_init(&f);
  f.width_mm = 120;
  f.height_mm = 60;
  fig_set_grid(&f, 1, 2);
  CHECK(f.npanels == 2, "two panels: %d", f.npanels);
  snprintf(f.p[0].title, FIG_TEXT, "Equilibria");
  snprintf(f.p[0].xlabel, FIG_TEXT, "Wind velocity $V_W$ [m/s]");
  snprintf(f.p[0].ylabel, FIG_TEXT, "Elevation $\\Gamma$ [deg]");
  s = fig_add(&f.p[0], x, y, 200);
  snprintf(s->label, FIG_TEXT, "stable");
  s->sy = 180 / 3.141592653589793;
  s = fig_add(&f.p[0], x, y2, 200);
  snprintf(s->label, FIG_TEXT, "$\\lambda_+ > 0$");
  s->line = FIG_LINE_DASHED;
  s->sy = 180 / 3.141592653589793;
  s = fig_add(&f.p[0], x, y2, 200);
  s->line = FIG_LINE_NONE;
  s->marker = FIG_MARK_CIRCLE;
  s->every = 20;
  s->mfill = false;
  snprintf(s->label, FIG_TEXT, "samples");
  s->sy = 180 / 3.141592653589793;
  f.p[0].grid = true;
  snprintf(f.p[1].xlabel, FIG_TEXT, "$\\Lambda$");
  snprintf(f.p[1].ylabel, FIG_TEXT, "$\\tilde{V}_{W,H}$");
  {
    double lx[100], ly[100];
    for (i = 0; i < 100; i++) {
      lx[i] = 10 + i * 100.0;
      ly[i] = 1.109724 / sqrt(lx[i]);
    }
    s = fig_add(&f.p[1], lx, ly, 100);
    snprintf(s->label, FIG_TEXT, "$1.109724/\\sqrt{\\Lambda}$");
    f.p[1].logx = true;
  }
  /* an inset in panel 0 */
  f.npanels = 3;
  fig_panel_init(&f.p[2]);
  f.p[2].inset = true;
  f.p[2].parent = 0;
  f.p[2].autox = false;
  f.p[2].xmin = 2;
  f.p[2].xmax = 4;
  s = fig_add(&f.p[2], x, y, 200);
  s->color[0] = 0.8f;
  f.p[2].legend = FIG_LEGEND_NONE;

  memset(&dl, 0, sizeof dl);
  fig_build(&f, &dl);
  CHECK(dl.nop > 50 && dl.ncmd > 1000, "paths: %d ops, %d commands", dl.nop, dl.ncmd);
  CHECK(dl.axes[0].valid && dl.axes[1].valid && dl.axes[2].valid, "axes laid out");
  CHECK(dl.axes[0].y0 == dl.axes[1].y0 && dl.axes[0].y1 == dl.axes[1].y1,
        "a row's axes line up: %g %g / %g %g", dl.axes[0].y0, dl.axes[1].y0, dl.axes[0].y1,
        dl.axes[1].y1);
  {
    double xx, yy;
    float px, py;
    fig_to_page(&dl.axes[0], 3.0, 10.0, &px, &py);
    fig_from_page(&dl.axes[0], px, py, &xx, &yy);
    CHECK(fabs(xx - 3) < 1e-4 && fabs(yy - 10) < 1e-3, "page mapping %g %g", xx, yy);
  }
  fig_dl_free(&dl);

  snprintf(path, sizeof path, "%s/fig_check.pdf", dir);
  CHECK(fig_write(&f, path, err, sizeof err), "pdf: %s", err);
  CHECK(file_size(path, head, sizeof head, tail, sizeof tail) > 1000, "pdf size");
  CHECK(!strncmp(head, "%PDF-1.4", 8), "pdf header");
  CHECK(strstr(tail, "%%EOF") != NULL, "pdf end");
  snprintf(path, sizeof path, "%s/fig_check.svg", dir);
  CHECK(fig_write(&f, path, err, sizeof err), "svg: %s", err);
  CHECK(file_size(path, head, sizeof head, tail, sizeof tail) > 1000, "svg size");
  CHECK(strstr(tail, "</svg>") != NULL, "svg end");
  snprintf(path, sizeof path, "%s/fig_check.eps", dir);
  CHECK(fig_write(&f, path, err, sizeof err), "eps: %s", err);
  CHECK(file_size(path, head, sizeof head, NULL, 0) > 1000, "eps size");
  CHECK(!strncmp(head, "%!PS-Adobe-3.0 EPSF-3.0", 23), "eps header");
  f.dpi = 300;
  snprintf(path, sizeof path, "%s/fig_check.png", dir);
  CHECK(fig_write(&f, path, err, sizeof err), "png: %s", err);
  CHECK(file_size(path, head, sizeof head, NULL, 0) > 10000, "png size");
  {
    /* 120 mm at 300 dpi is 1417 pixels: IHDR holds the width */
    unsigned w = ((unsigned char)head[16] << 24) | ((unsigned char)head[17] << 16) |
                 ((unsigned char)head[18] << 8) | (unsigned char)head[19];
    CHECK(w == 1417, "png width %u", w);
    CHECK(!memcmp(head + 37, "pHYs", 4), "png resolution chunk");
  }
  snprintf(path, sizeof path, "%s/fig_check.doc", dir);
  CHECK(!fig_write(&f, path, err, sizeof err), "unknown extension accepted");
  {
    /* Annotations in data coordinates: a red text, an arrow and a marker.
       They add paths in their colour, the arrow's head lies where its tip
       is on the page, they show in the raster and in every file, and
       removing them gives back the figure without. */
    Figure g;
    FigDL d0, d1, d2;
    FigNote n;
    double gx[2] = {0, 10}, gy[2] = {0, 10};
    float tx, ty, px, py;
    int k, tip = 0, red0 = 0, red1 = 0, w = 0, h = 0;
    long plain[4], marked[4];
    unsigned char *img0, *img1;
    static const char *ext[4] = {"pdf", "svg", "eps", "png"};
    fig_init(&g);
    g.p[0].autox = g.p[0].autoy = false;
    g.p[0].xmax = g.p[0].ymax = 10;
    fig_add(&g.p[0], gx, gy, 2);
    for (k = 0; k < 4; k++) {
      snprintf(path, sizeof path, "%s/fig_note_plain.%s", dir, ext[k]);
      CHECK(fig_write(&g, path, err, sizeof err), "plain %s: %s", ext[k], err);
      plain[k] = file_size(path, NULL, 0, NULL, 0);
    }
    memset(&d0, 0, sizeof d0);
    fig_build(&g, &d0);
    memset(&n, 0, sizeof n);
    n.rgb[0] = 0.84f;
    n.rgb[1] = 0.15f;
    n.rgb[2] = 0.16f;
    n.kind = FIG_NOTE_TEXT;
    n.x = 1;
    n.y = 8;
    n.pt = 14;
    snprintf(n.text, FIG_TEXT, "$\\Gamma$ peak");
    CHECK(fig_note_add(&g.p[0], &n), "text added");
    n.kind = FIG_NOTE_ARROW;
    n.x = 2;
    n.y = 2;
    n.x2 = 7;
    n.y2 = 6;
    CHECK(fig_note_add(&g.p[0], &n), "arrow added");
    n.kind = FIG_NOTE_MARKER;
    n.x = 5;
    n.y = 9;
    n.marker = FIG_MARK_SQUARE;
    CHECK(fig_note_add(&g.p[0], &n), "marker added");
    CHECK(g.p[0].nnote == 3, "three annotations: %d", g.p[0].nnote);
    memset(&d1, 0, sizeof d1);
    fig_build(&g, &d1);
    CHECK(d1.nop > d0.nop, "annotations add paths: %d %d", d1.nop, d0.nop);
    fig_to_page(&d1.axes[0], 7, 6, &tx, &ty);
    for (k = 0; k < d1.nop; k++) {
      const FigOp *o = &d1.op[k];
      const FigCmd *c = &d1.cmd[o->first];
      if (o->op == FIG_OP_FILL && o->rgb[0] == n.rgb[0] && o->rgb[1] == n.rgb[1] &&
          c->type == FIG_MOVE && fabsf(c->p[0] - tx) < 0.01f && fabsf(c->p[1] - ty) < 0.01f)
        tip++;
    }
    CHECK(tip == 1, "the arrow's head has its tip at the data point: %d", tip);
    w = (int)(d1.width * 4);
    h = (int)(d1.height * 4);
    img0 = fig_raster(&d0, w, h);
    img1 = fig_raster(&d1, w, h);
    CHECK(img0 && img1, "rasters");
    for (k = 0; img0 && img1 && k < w * h; k++) {
      if (img0[4 * k] > 180 && img0[4 * k + 1] < 90 && img0[4 * k + 2] < 90) red0++;
      if (img1[4 * k] > 180 && img1[4 * k + 1] < 90 && img1[4 * k + 2] < 90) red1++;
    }
    CHECK(red0 == 0 && red1 > 150, "red pixels without and with: %d %d", red0, red1);
    if (img1) {
      /* the shaft passes the middle of the arrow, (4.5, 4) in data */
      int found = 0, dx, dy;
      fig_to_page(&d1.axes[0], 4.5, 4, &px, &py);
      for (dy = -2; dy <= 2; dy++)
        for (dx = -2; dx <= 2; dx++) {
          int ix = (int)(px * 4) + dx, iy = (int)(py * 4) + dy;
          const unsigned char *q = img1 + 4 * (iy * w + ix);
          if (q[0] > 180 && q[1] < 90 && q[2] < 90) found = 1;
        }
      CHECK(found, "the arrow's shaft is red at its middle");
    }
    free(img0);
    free(img1);
    for (k = 0; k < 4; k++) {
      snprintf(path, sizeof path, "%s/fig_note_marked.%s", dir, ext[k]);
      CHECK(fig_write(&g, path, err, sizeof err), "marked %s: %s", ext[k], err);
      marked[k] = file_size(path, NULL, 0, NULL, 0);
      CHECK(marked[k] > plain[k], "%s grows with annotations: %ld %ld", ext[k], plain[k],
            marked[k]);
    }
    fig_note_remove(&g.p[0], 1);
    CHECK(g.p[0].nnote == 2 && g.p[0].note[1].kind == FIG_NOTE_MARKER, "one removed");
    fig_note_remove(&g.p[0], -1);
    CHECK(g.p[0].nnote == 0, "all removed");
    memset(&d2, 0, sizeof d2);
    fig_build(&g, &d2);
    CHECK(d2.nop == d0.nop && d2.ncmd == d0.ncmd, "the figure is as before: %d %d", d2.nop,
          d0.nop);
    fig_dl_free(&d0);
    fig_dl_free(&d1);
    fig_dl_free(&d2);
    fig_free(&g);
  }
  fig_free(&f);
  if (failures) fprintf(stderr, "%d failures\n", failures);
  else printf("fig_check: all good\n");
  return failures ? 1 : 0;
}
