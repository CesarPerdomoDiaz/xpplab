/* Figure export: PNG from the screen, SVG drawn from the data. */
#include "export.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8 /* file names from SDL's dialogs are UTF-8 */
#include "stb/stb_image_write.h"

FigureState g_figures[FIG_COUNT];

const char *figure_name(FigureKind kind) {
  static const char *names[] = {"time-series", "phase-plane", "bifurcation",
                                "two-parameter"};
  return kind >= 0 && kind < FIG_COUNT ? names[kind] : "figure";
}

/* ---- PNG ------------------------------------------------------------------ */

static char png_path[1024];
static FigureKind png_kind;
static bool png_pending;

void export_png(FigureKind kind, const char *path) {
  snprintf(png_path, sizeof png_path, "%s", path);
  png_kind = kind;
  png_pending = true;
}

void export_process_pending(App *a, void *renderer, float scale) {
  FigureState *f;
  SDL_Rect r;
  SDL_Surface *s, *rgb;
  if (!png_pending) return;
  png_pending = false;
  f = &g_figures[png_kind];
  if (!f->valid) {
    app_log(a, "error: show the %s first, then export it.", figure_name(png_kind));
    return;
  }
  r.x = (int)(f->x0 * scale);
  r.y = (int)(f->y0 * scale);
  r.w = (int)((f->x1 - f->x0) * scale);
  r.h = (int)((f->y1 - f->y0) * scale);
  s = SDL_RenderReadPixels((SDL_Renderer *)renderer, &r);
  if (!s) {
    app_log(a, "error: cannot read the screen: %s", SDL_GetError());
    return;
  }
  rgb = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGB24);
  SDL_DestroySurface(s);
  if (!rgb) return;
  if (stbi_write_png(png_path, rgb->w, rgb->h, 3, rgb->pixels, rgb->pitch))
    app_log(a, "Saved %s (%d x %d pixels).", png_path, rgb->w, rgb->h);
  else
    app_log(a, "error: cannot write %s", png_path);
  SDL_DestroySurface(rgb);
}

bool export_surface_png(void *surface, const char *path) {
  SDL_Surface *rgb = SDL_ConvertSurface((SDL_Surface *)surface,
                                        SDL_PIXELFORMAT_RGB24);
  bool ok;
  if (!rgb) return false;
  ok = stbi_write_png(path, rgb->w, rgb->h, 3, rgb->pixels, rgb->pitch) != 0;
  SDL_DestroySurface(rgb);
  return ok;
}

/* ---- SVG ------------------------------------------------------------------ */

enum { W = 800, H = 560, ML = 84, MR = 28, MT = 28, MB = 64 };

typedef struct {
  FILE *f;
  double xmin, xmax, ymin, ymax;
  int legend_n;
  char legend[16][96];
  char legend_color[16][16];
  bool legend_dashed[16];
} Svg;

static const char *palette[] = {"#1f77b4", "#d62728", "#2ca02c", "#9467bd",
                                "#ff7f0e", "#8c564b", "#e377c2", "#7f7f7f",
                                "#bcbd22", "#17becf"}; /* as ui_plots.c */

static double px(const Svg *s, double x) {
  return ML + (x - s->xmin) / (s->xmax - s->xmin) * (W - ML - MR);
}
static double py(const Svg *s, double y) {
  return H - MB - (y - s->ymin) / (s->ymax - s->ymin) * (H - MT - MB);
}

static void esc(FILE *f, const char *t) {
  for (; *t; t++) {
    if (*t == '<') fputs("&lt;", f);
    else if (*t == '>') fputs("&gt;", f);
    else if (*t == '&') fputs("&amp;", f);
    else fputc(*t, f);
  }
}

/* Round numbers for ticks: 1, 2 or 5 times a power of ten. */
static double nice_step(double range) {
  double raw = range / 5, p = pow(10, floor(log10(raw))), m = raw / p;
  return (m < 1.5 ? 1 : m < 3.5 ? 2 : m < 7.5 ? 5 : 10) * p;
}

static void axes(Svg *s, const char *xlabel, const char *ylabel) {
  FILE *f = s->f;
  double step, v;
  fprintf(f, "<rect x='%d' y='%d' width='%d' height='%d' fill='white' "
             "stroke='#333' stroke-width='1'/>\n",
          ML, MT, W - ML - MR, H - MT - MB);
  step = nice_step(s->xmax - s->xmin);
  for (v = ceil(s->xmin / step) * step; v <= s->xmax + step * 1e-9; v += step) {
    double x = px(s, v);
    fprintf(f, "<line x1='%.2f' y1='%d' x2='%.2f' y2='%d' stroke='#ddd'/>\n", x,
            MT, x, H - MB);
    fprintf(f, "<line x1='%.2f' y1='%d' x2='%.2f' y2='%d' stroke='#333'/>\n", x,
            H - MB, x, H - MB + 5);
    fprintf(f, "<text x='%.2f' y='%d' text-anchor='middle'>%g</text>\n", x,
            H - MB + 22, fabs(v) < step * 1e-9 ? 0.0 : v);
  }
  step = nice_step(s->ymax - s->ymin);
  for (v = ceil(s->ymin / step) * step; v <= s->ymax + step * 1e-9; v += step) {
    double y = py(s, v);
    fprintf(f, "<line x1='%d' y1='%.2f' x2='%d' y2='%.2f' stroke='#ddd'/>\n", ML,
            y, W - MR, y);
    fprintf(f, "<line x1='%d' y1='%.2f' x2='%d' y2='%.2f' stroke='#333'/>\n",
            ML - 5, y, ML, y);
    fprintf(f, "<text x='%d' y='%.2f' text-anchor='end' dy='0.35em'>%g</text>\n",
            ML - 9, y, fabs(v) < step * 1e-9 ? 0.0 : v);
  }
  fprintf(f, "<text x='%d' y='%d' text-anchor='middle' font-size='17'>", (ML + W - MR) / 2,
          H - 16);
  esc(f, xlabel);
  fprintf(f, "</text>\n<text transform='translate(22,%d) rotate(-90)' "
             "text-anchor='middle' font-size='17'>", (MT + H - MB) / 2);
  esc(f, ylabel);
  fputs("</text>\n", f);
}

static bool svg_begin(Svg *s, const char *path, const FigureState *fig) {
  memset(s, 0, sizeof *s);
  s->f = fopen(path, "w");
  if (!s->f) return false;
  s->xmin = fig->xmin;
  s->xmax = fig->xmax;
  s->ymin = fig->ymin;
  s->ymax = fig->ymax;
  if (!(s->xmax > s->xmin)) s->xmax = s->xmin + 1;
  if (!(s->ymax > s->ymin)) s->ymax = s->ymin + 1;
  fprintf(s->f,
          "<?xml version='1.0' encoding='UTF-8'?>\n"
          "<svg xmlns='http://www.w3.org/2000/svg' width='%d' height='%d' "
          "viewBox='0 0 %d %d' font-family='Helvetica, Arial, sans-serif' "
          "font-size='14' fill='#222'>\n"
          "<!-- Made with XPP Studio -->\n"
          "<rect width='100%%' height='100%%' fill='white'/>\n"
          "<defs><clipPath id='plot'><rect x='%d' y='%d' width='%d' height='%d'/>"
          "</clipPath></defs>\n",
          W, H, W, H, ML, MT, W - ML - MR, H - MT - MB);
  return true;
}

static void legend_add(Svg *s, const char *label, const char *color, bool dashed) {
  int i;
  for (i = 0; i < s->legend_n; i++)
    if (strcmp(s->legend[i], label) == 0) return;
  if (s->legend_n == 16) return;
  snprintf(s->legend[s->legend_n], 96, "%s", label);
  snprintf(s->legend_color[s->legend_n], 16, "%s", color);
  s->legend_dashed[s->legend_n++] = dashed;
}

static void svg_end(Svg *s) {
  int i;
  if (s->legend_n) {
    int w = 0, x, y = MT + 10;
    for (i = 0; i < s->legend_n; i++) {
      int l = (int)strlen(s->legend[i]) * 8 + 40;
      if (l > w) w = l;
    }
    x = W - MR - w - 10;
    fprintf(s->f, "<rect x='%d' y='%d' width='%d' height='%d' fill='white' "
                  "fill-opacity='0.85' stroke='#bbb'/>\n",
            x, y, w, s->legend_n * 20 + 8);
    for (i = 0; i < s->legend_n; i++) {
      int ly = y + 16 + i * 20;
      fprintf(s->f,
              "<line x1='%d' y1='%d' x2='%d' y2='%d' stroke='%s' stroke-width='2.2'%s/>\n",
              x + 8, ly - 4, x + 30, ly - 4, s->legend_color[i],
              s->legend_dashed[i] ? " stroke-dasharray='6 4'" : "");
      fprintf(s->f, "<text x='%d' y='%d'>", x + 36, ly);
      esc(s->f, s->legend[i]);
      fputs("</text>\n", s->f);
    }
  }
  fputs("</svg>\n", s->f);
  fclose(s->f);
}

/* A polyline, broken where values are not finite. */
static void polyline(Svg *s, const double *x, const double *y, int n, int stride,
                     const char *color, double width, bool dashed, double opacity) {
  int i, open = 0;
  for (i = 0; i < n; i++) {
    double a = x[(size_t)i * stride], b = y[(size_t)i * stride];
    if (!isfinite(a) || !isfinite(b)) {
      if (open) fputs("'/>\n", s->f);
      open = 0;
      continue;
    }
    if (!open) {
      fprintf(s->f,
              "<polyline clip-path='url(#plot)' fill='none' stroke='%s' "
              "stroke-width='%.1f' stroke-linejoin='round'%s%s points='",
              color, width, dashed ? " stroke-dasharray='6 4'" : "",
              opacity < 1 ? " stroke-opacity='0.35'" : "");
      open = 1;
    }
    fprintf(s->f, "%.2f,%.2f ", px(s, a), py(s, b));
  }
  if (open) fputs("'/>\n", s->f);
}

static void segments(Svg *s, const float *seg, int n, const char *color) {
  int i;
  if (n <= 0) return;
  fprintf(s->f, "<path clip-path='url(#plot)' fill='none' stroke='%s' "
                "stroke-width='2' d='", color);
  for (i = 0; i < n; i++)
    fprintf(s->f, "M%.2f %.2fL%.2f %.2f", px(s, seg[4 * i]), py(s, seg[4 * i + 1]),
            px(s, seg[4 * i + 2]), py(s, seg[4 * i + 3]));
  fputs("'/>\n", s->f);
}

static int col_of(const Trajectory *t, const char *name) {
  int j;
  for (j = 0; j < t->ncols; j++)
    if (SDL_strcasecmp(t->names[j], name) == 0) return j;
  return -1;
}

static void svg_time(App *a, Svg *s) {
  int k, j;
  for (k = 0; k < a->nfrozen; k++)
    for (j = 1; j < a->frozen[k].ncols && j < a->ncols; j++)
      if (a->plot_col[j])
        polyline(s, a->frozen[k].cols[0], a->frozen[k].cols[j], a->frozen[k].nrows,
                 1, palette[(j - 1) % 10], 1.2, false, 0.35);
  if (a->have_traj)
    for (j = 1; j < a->traj.ncols; j++)
      if (a->plot_col[j]) {
        polyline(s, a->traj.cols[0], a->traj.cols[j], a->traj.nrows, 1,
                 palette[(j - 1) % 10], 2.0, false, 1);
        legend_add(s, a->traj.names[j], palette[(j - 1) % 10], false);
      }
}

static void svg_phase(App *a, Svg *s) {
  PhasePlane *p = &a->pp;
  const char *xn = a->vars[p->xvar], *yn = a->vars[p->yvar];
  char label[128];
  int i, k;
  if (p->nullclines) {
    segments(s, p->ncx, p->ncx_n, "#d62728");
    segments(s, p->ncy, p->ncy_n, "#2ca02c");
    snprintf(label, sizeof label, "%s nullcline", xn);
    legend_add(s, label, "#d62728", false);
    snprintf(label, sizeof label, "%s nullcline", yn);
    legend_add(s, label, "#2ca02c", false);
  }
  for (k = 0; k < a->nfrozen; k++) {
    int jx = col_of(&a->frozen[k], xn), jy = col_of(&a->frozen[k], yn);
    if (jx > 0 && jy > 0)
      polyline(s, a->frozen[k].cols[jx], a->frozen[k].cols[jy], a->frozen[k].nrows,
               1, palette[0], 1.2, false, 0.35);
  }
  if (a->have_traj) {
    int jx = col_of(&a->traj, xn), jy = col_of(&a->traj, yn);
    if (jx > 0 && jy > 0) {
      polyline(s, a->traj.cols[jx], a->traj.cols[jy], a->traj.nrows, 1, palette[0],
               2.0, false, 1);
      legend_add(s, "trajectory", palette[0], false);
    }
  }
  for (i = 0; i < a->neq; i++) {
    Equilibrium *e = &a->eq[i];
    for (k = 0; k < e->nmanifolds; k++) {
      Trajectory *t = &e->manifold[k];
      int jx = col_of(t, xn), jy = col_of(t, yn);
      const char *c = e->manifold_type[k] > 0 ? "#ff7f0e" : "#9467bd";
      if (jx > 0 && jy > 0) {
        polyline(s, t->cols[jx], t->cols[jy], t->nrows, 1, c, 1.6, false, 1);
        legend_add(s, e->manifold_type[k] > 0 ? "unstable manifold" : "stable manifold",
                   c, false);
      }
    }
  }
  for (i = 0; i < a->neq; i++) {
    double x = a->eq[i].x[p->xvar], y = a->eq[i].x[p->yvar];
    if (x < s->xmin || x > s->xmax || y < s->ymin || y > s->ymax) continue;
    fprintf(s->f, "<circle cx='%.2f' cy='%.2f' r='5.5' fill='%s' stroke='black' "
                  "stroke-width='1.5'/>\n",
            px(s, x), py(s, y), a->eq[i].unstable ? "white" : "black");
  }
}

static double auto_y(App *a, int i, int which) {
  if (a->au.yvar < 0) return a->au.info[i].norm;
  return (which ? a->au.umin : a->au.umax)[(size_t)i * a->nvars + a->au.yvar];
}

static void svg_auto(App *a, Svg *s, bool two) {
  AutoUI *u = &a->au;
  int i = 0, n = u->count;
  double *xs = malloc(sizeof(double) * (size_t)(n + 1));
  double *ys = malloc(sizeof(double) * (size_t)(n + 1));
  double *ys2 = malloc(sizeof(double) * (size_t)(n + 1));
  if (!xs || !ys || !ys2) goto done;
  while (i < n) {
    const xppe_auto_point_info *p0 = &u->info[i];
    int j = i, m = 0;
    bool per = p0->periodic, st = p0->stable;
    const char *c;
    if ((p0->two_parameter != 0) != two) {
      i++;
      continue;
    }
    if (i > 0 && u->info[i - 1].branch == p0->branch &&
        (u->info[i - 1].two_parameter != 0) == two) {
      xs[m] = u->info[i - 1].par1;
      ys[m] = two ? u->info[i - 1].par2 : auto_y(a, i - 1, 0);
      ys2[m] = two ? 0 : auto_y(a, i - 1, 1);
      m++;
    }
    while (j < n && u->info[j].branch == p0->branch &&
           (u->info[j].stable != 0) == st && (u->info[j].two_parameter != 0) == two &&
           (j == i || abs(u->info[j].point) == abs(u->info[j - 1].point) + 1)) {
      xs[m] = u->info[j].par1;
      ys[m] = two ? u->info[j].par2 : auto_y(a, j, 0);
      ys2[m] = two ? 0 : auto_y(a, j, 1);
      m++;
      j++;
    }
    c = two ? "#9467bd" : per ? (st ? "#2ca02c" : "#1f77b4") : (st ? "#d62728" : "#333333");
    polyline(s, xs, ys, m, 1, c, st || two ? 2.2 : 1.4, !st && !two, 1);
    if (per && !two && u->yvar >= 0) polyline(s, xs, ys2, m, 1, c, st ? 2.2 : 1.4, !st, 1);
    if (!two)
      legend_add(s,
                 per ? (st ? "stable periodic orbits" : "unstable periodic orbits")
                     : (st ? "stable steady states" : "unstable steady states"),
                 c, !st);
    i = j > i ? j : i + 1;
  }
  for (i = 0; i < n; i++) {
    const xppe_auto_point_info *p = &u->info[i];
    double x, y;
    if (!p->label || !p->type_name[0] || !strcmp(p->type_name, "EP") ||
        (p->two_parameter != 0) != two)
      continue;
    x = p->par1;
    y = two ? p->par2 : auto_y(a, i, 0);
    if (x < s->xmin || x > s->xmax || y < s->ymin || y > s->ymax) continue;
    fprintf(s->f, "<circle cx='%.2f' cy='%.2f' r='3.5' fill='#ff7f0e'/>\n"
                  "<text x='%.2f' y='%.2f' fill='#c45f00' font-size='12'>%s</text>\n",
            px(s, x), py(s, y), px(s, x) + 6, py(s, y) - 6, p->type_name);
  }
done:
  free(xs);
  free(ys);
  free(ys2);
}

bool export_svg(App *a, FigureKind kind, const char *path) {
  Svg s;
  const FigureState *fig = &g_figures[kind];
  char ylabel[128];
  const char *xlabel = "t";
  if (!fig->valid) {
    app_log(a, "error: show the %s first, then export it.", figure_name(kind));
    return false;
  }
  if (!svg_begin(&s, path, fig)) {
    app_log(a, "error: cannot write %s", path);
    return false;
  }
  ylabel[0] = '\0';
  if (kind == FIG_PHASE) {
    xlabel = a->vars[a->pp.xvar];
    snprintf(ylabel, sizeof ylabel, "%s", a->vars[a->pp.yvar]);
  } else if (kind == FIG_AUTO || kind == FIG_AUTO2) {
    xlabel = a->au.main_par >= 0 ? a->pars[a->au.main_par] : "parameter";
    if (kind == FIG_AUTO2)
      snprintf(ylabel, sizeof ylabel, "%s",
               a->au.second_par >= 0 ? a->pars[a->au.second_par] : "");
    else if (a->au.yvar < 0)
      snprintf(ylabel, sizeof ylabel, "norm");
    else
      snprintf(ylabel, sizeof ylabel, "%s (max / min)", a->vars[a->au.yvar]);
  }
  axes(&s, xlabel, ylabel);
  if (kind == FIG_TIME) svg_time(a, &s);
  if (kind == FIG_PHASE) svg_phase(a, &s);
  if (kind == FIG_AUTO) svg_auto(a, &s, false);
  if (kind == FIG_AUTO2) svg_auto(a, &s, true);
  svg_end(&s);
  app_log(a, "Saved %s (vector figure).", path);
  return true;
}
