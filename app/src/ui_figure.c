/* The Figure window: a figure for a paper, made from the runs, the
   diagram and formulas, with its page, fonts, labels (mathtext), limits,
   line styles and legend, previewed exactly as it is written to PDF, SVG,
   EPS or PNG (app_figure.c, fig.h). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "ui.h"

/* ---- Export dialog ------------------------------------------------------------- */

static SDL_Mutex *lock;
static char chosen[1024];
static bool chosen_ready;

static void SDLCALL saved(void *user, const char *const *files, int filter) {
  (void)user;
  (void)filter;
  SDL_LockMutex(lock);
  if (files && files[0]) {
    snprintf(chosen, sizeof chosen, "%s", files[0]);
    chosen_ready = true;
  }
  SDL_UnlockMutex(lock);
}

static const char *ext_of[] = {".pdf", ".svg", ".eps", ".png"};
static const SDL_DialogFileFilter filters[4][1] = {
    {{"PDF (*.pdf)", "pdf"}}, {{"SVG (*.svg)", "svg"}}, {{"EPS (*.eps)", "eps"}},
    {{"PNG image (*.png)", "png"}}};
static int chosen_kind;

static void ask_path(App *a, int kind) {
  if (!lock) lock = SDL_CreateMutex();
  chosen_kind = kind;
  SDL_ShowSaveFileDialog(saved, NULL, (SDL_Window *)a->window, filters[kind], 1,
                         a->fg.path[0] ? a->fg.path : NULL);
}

static void poll_path(App *a) {
  char path[1100];
  bool ready;
  if (!lock) return;
  SDL_LockMutex(lock);
  ready = chosen_ready;
  chosen_ready = false;
  snprintf(path, sizeof path, "%s", chosen);
  SDL_UnlockMutex(lock);
  if (ready) {
    size_t n = strlen(path), e = strlen(ext_of[chosen_kind]);
    if (n < e || SDL_strcasecmp(path + n - e, ext_of[chosen_kind]) != 0)
      snprintf(path + n, sizeof path - n, "%s", ext_of[chosen_kind]);
    app_fig_export(a, path);
  }
}

/* ---- Preview ---------------------------------------------------------------------- */

static FigDL preview;

static ImTextureRef_c preview_texture(App *a, int w, int h) {
  FigUI *g = &a->fg;
  ImTextureRef_c ref;
  memset(&ref, 0, sizeof ref);
  if (w < 16 || h < 16) return ref;
  if (g->tex && (g->preview_w != w || g->preview_h != h)) {
    SDL_DestroyTexture((SDL_Texture *)g->tex);
    g->tex = NULL;
  }
  if (!g->tex && a->renderer) {
    g->tex = SDL_CreateTexture((SDL_Renderer *)a->renderer, SDL_PIXELFORMAT_RGBA32,
                               SDL_TEXTUREACCESS_STREAMING, w, h);
    g->preview_w = w;
    g->preview_h = h;
    g->dirty = true;
  }
  if (g->tex && g->dirty) {
    unsigned char *px;
    fig_build(&g->fig, &preview);
    px = fig_raster(&preview, w, h);
    if (px) {
      SDL_UpdateTexture((SDL_Texture *)g->tex, NULL, px, w * 4);
      free(px);
    }
    g->dirty = false;
  }
  ref._TexID = (ImTextureID)(intptr_t)g->tex;
  return ref;
}

/* ---- Small helpers ------------------------------------------------------------------ */

static bool text_field(App *a, const char *label, char *buf, size_t len) {
  char msg[160];
  bool changed = igInputText(label, buf, len, 0, NULL, NULL);
  (void)a;
  if (buf[0] && !fig_text_check(buf, msg, sizeof msg))
    igTextColored(V4(0.85f, 0.2f, 0.2f, 1), "  %s", msg);
  return changed;
}

static const char *line_items[] = {"none", "solid", "dashed", "dotted", "dash-dot"};
static const char *marker_items[] = {"none", "circle", "square", "triangle", "diamond",
                                     "x", "plus", "star", "point"};
static const char *legend_items[] = {"none", "best place", "upper right", "upper left",
                                     "lower left", "lower right", "outside, right"};

static bool limits_row(const char *axis, bool *autom, double *lo, double *hi, bool *lg,
                       double *step, double data[2]) {
  bool c = false;
  char id[32];
  igPushID_Str(axis);
  snprintf(id, sizeof id, "%s auto", axis);
  if (igCheckbox(id, autom)) {
    if (!*autom) {
      *lo = data[0];
      *hi = data[1];
    }
    c = true;
  }
  Tooltip("Limits from the data, widened by 5%. Untick to type them, or drag "
          "a rectangle on the preview to zoom.");
  igSameLine(0, -1);
  c |= igCheckbox("log", lg);
  igBeginDisabled(*autom);
  igSetNextItemWidth(90);
  c |= igInputDouble("##lo", lo, 0, 0, "%.6g", 0);
  igSameLine(0, -1);
  igSetNextItemWidth(90);
  c |= igInputDouble("##hi", hi, 0, 0, "%.6g", 0);
  igEndDisabled();
  igSameLine(0, -1);
  igSetNextItemWidth(70);
  c |= igInputDouble("step", step, 0, 0, "%.4g", 0);
  Tooltip("Tick spacing; 0 chooses it.");
  if (*step < 0) *step = 0;
  igPopID();
  return c;
}

/* ---- The form to add a series ----------------------------------------------------------- */

enum { SRC_RUN, SRC_MANY, SRC_DIAGRAM, SRC_RANGE, SRC_FORMULA, SRC_CURVE, SRC_EQ, SRC_COUNT };
static const char *src_items[] = {"Columns of the run", "Many runs / shooting",
                                  "Bifurcation diagram", "Range (max/min)",
                                  "Formula y = f(x)", "Model function of a variable",
                                  "Equilibria (markers)"};

static struct {
  int src, xcol, ycol, event; /* event: 0 every run, 1 one series per flag */
  FigDiagram d;
  int dq;                      /* index into the quantity list */
  int dxq;                     /* x: 0 parameter, else quantity index + 1 */
  bool split;                  /* stable solid / unstable dashed, orbits max and min */
  char expr[256];
  double lo, hi;
  int n;
  int cy, cx;                  /* curve: y item, x variable */
  bool init;
} add;

/* The diagram's quantities: variables, aux, period, norm, eigenvalues,
   multipliers. */
static int quantity_list(App *a, char (*out)[APP_NAME_LEN], int max) {
  int n = 0, j, k;
  for (j = 1; j < a->ncols && n < max; j++) snprintf(out[n++], APP_NAME_LEN, "%s", a->cols[j]);
  if (n < max) snprintf(out[n++], APP_NAME_LEN, "period");
  if (n < max) snprintf(out[n++], APP_NAME_LEN, "norm");
  for (k = 1; k <= a->nvars && k <= 8; k++) {
    if (n < max) snprintf(out[n++], APP_NAME_LEN, "re%d", k);
    if (n < max) snprintf(out[n++], APP_NAME_LEN, "im%d", k);
  }
  for (k = 1; k <= a->nvars && k <= 8; k++) {
    if (n < max) snprintf(out[n++], APP_NAME_LEN, "mu%d_abs", k);
    if (n < max) snprintf(out[n++], APP_NAME_LEN, "mu%d_re", k);
    if (n < max) snprintf(out[n++], APP_NAME_LEN, "mu%d_im", k);
  }
  return n;
}

static void add_form(App *a) {
  FigUI *g = &a->fg;
  int panel = g->panel;
  if (!add.init) {
    add.xcol = 0;
    add.ycol = a->ncols > 1 ? 1 : 0;
    add.d.stable = add.d.periodic = -1;
    add.d.any_branch = true;
    add.split = true;
    snprintf(add.expr, sizeof add.expr, "1/sqrt(x)");
    add.lo = 0.1;
    add.hi = 10;
    add.n = 400;
    add.init = true;
  }
  igSetNextItemWidth(-1);
  igCombo_Str_arr("##src", &add.src, src_items, SRC_COUNT, 10);
  switch (add.src) {
  case SRC_RUN:
  case SRC_MANY:
    igSetNextItemWidth(120);
    Combo("x##run", &add.xcol, a->cols, a->ncols);
    igSameLine(0, -1);
    igSetNextItemWidth(120);
    Combo("y##run", &add.ycol, a->cols, a->ncols);
    if (add.src == SRC_MANY) {
      igRadioButton_IntPtr("one series", &add.event, 0);
      igSameLine(0, -1);
      igRadioButton_IntPtr("a series per ending", &add.event, 1);
      Tooltip("A colour for each way a run ended: each global flag that "
              "stopped runs, and the runs that went to the end.");
      igBeginDisabled(a->many.n == 0);
    } else
      igBeginDisabled(!a->have_traj);
    if (Button("Add")) {
      if (add.src == SRC_RUN) app_fig_add_run(a, panel, add.xcol, add.ycol);
      else if (add.event == 0) app_fig_add_runs(a, panel, add.xcol, add.ycol, -1);
      else {
        int e, i;
        bool seen[64] = {0};
        for (i = 0; i < a->many.n; i++)
          if (a->many.event[i] >= 0 && a->many.event[i] < 64) seen[a->many.event[i]] = true;
        for (e = 0; e < 64; e++)
          if (seen[e]) app_fig_add_runs(a, panel, add.xcol, add.ycol, e);
      }
    }
    igEndDisabled();
    if (add.src == SRC_MANY && a->many.n == 0)
      igTextDisabled("Make runs first: Workspace > Many runs.");
    break;
  case SRC_DIAGRAM: {
    static char q[96][APP_NAME_LEN];
    int nq = quantity_list(a, q, 96);
    static const char *tri[] = {"either", "no", "yes"};
    int st = add.d.stable + 1, pe = add.d.periodic + 1;
    if (add.dq >= nq) add.dq = 0;
    igSetNextItemWidth(130);
    Combo("y##dq", &add.dq, q, nq);
    Tooltip("A variable or an aux column (min/max over orbits), the period, the "
            "norm, eigenvalues re1, im1, ... of steady states (sorted by real "
            "part), or Floquet multipliers mu1_abs, ... of orbits (by size).");
    igSameLine(0, -1);
    {
      static char xq[97][APP_NAME_LEN];
      int k;
      snprintf(xq[0], APP_NAME_LEN, "parameter");
      for (k = 0; k < nq; k++) snprintf(xq[k + 1], APP_NAME_LEN, "%s", q[k]);
      igSetNextItemWidth(110);
      Combo("x##dx", &add.dxq, xq, nq + 1);
    }
    igCheckbox("XPPAUT's styles", &add.split);
    Tooltip("Add a series for each kind: stable steady states red and solid, "
            "unstable black and dashed, stable orbits green, unstable blue, "
            "orbits with their max and min.");
    if (!add.split) {
      igSetNextItemWidth(70);
      igCombo_Str_arr("stable", &st, tri, 3, 3);
      igSameLine(0, -1);
      igSetNextItemWidth(70);
      igCombo_Str_arr("orbits", &pe, tri, 3, 3);
      igSameLine(0, -1);
      igRadioButton_IntPtr("max", &add.d.which, 0);
      igSameLine(0, -1);
      igRadioButton_IntPtr("min", &add.d.which, 1);
      add.d.stable = st - 1;
      add.d.periodic = pe - 1;
    }
    {
      bool all = add.d.any_branch;
      if (igCheckbox("every branch", &all)) add.d.any_branch = all;
      if (!all) {
        igSameLine(0, -1);
        igSetNextItemWidth(80);
        igInputInt("branch", &add.d.branch, 1, 1, 0);
      }
    }
    igSetNextItemWidth(70);
    igInputInt("from label", &add.d.lab1, 0, 0, 0);
    igSameLine(0, -1);
    igSetNextItemWidth(70);
    igInputInt("to label", &add.d.lab2, 0, 0, 0);
    Tooltip("Only the points between two labelled points, such as the upper "
            "branch from its end point to the fold. 0: everywhere.");
    igBeginDisabled(a->au.count == 0);
    if (Button("Add")) {
      FigDiagram d = add.d;
      snprintf(d.y, sizeof d.y, "%s", q[add.dq]);
      if (add.dxq > 0) snprintf(d.x, sizeof d.x, "%s", q[add.dxq - 1]);
      else d.x[0] = '\0';
      if (!add.split) {
        app_fig_add_diagram(a, panel, &d);
      } else {
        int per, s2, w, i;
        /* a column has a max and a min over each orbit */
        bool periodic_y = false;
        for (i = 1; i < a->ncols; i++)
          if (!SDL_strcasecmp(a->cols[i], d.y)) periodic_y = true;
        for (per = 0; per <= 1; per++)
          for (s2 = 1; s2 >= 0; s2--)
            for (w = 0; w <= (per && periodic_y ? 1 : 0); w++) {
              bool have = false;
              for (i = 0; i < a->au.count; i++)
                if (!a->au.info[i].two_parameter && a->au.info[i].periodic == per &&
                    a->au.info[i].stable == s2)
                  have = true;
              if (!have) continue;
              d.stable = s2;
              d.periodic = per;
              d.which = w;
              if (app_fig_add_diagram(a, panel, &d) && w) {
                FigPanel *p = &g->fig.p[panel];
                p->s[p->nseries - 1].label[0] = '\0';
              }
            }
      }
    }
    igEndDisabled();
    if (a->au.count == 0) igTextDisabled("Compute a diagram first (Bifurcation window).");
    break;
  }
  case SRC_RANGE:
    igBeginDisabled(a->sw.n <= 0);
    if (Button("Add max")) app_fig_add_sweep(a, panel, false);
    igSameLine(0, -1);
    if (Button("Add min")) app_fig_add_sweep(a, panel, true);
    igEndDisabled();
    igTextDisabled("The last Range that recorded a variable's max and min.");
    break;
  case SRC_FORMULA:
    igSetNextItemWidth(-1);
    igInputText("##expr", add.expr, sizeof add.expr, 0, NULL, NULL);
    Tooltip("A formula in x: + - * / ^, sqrt, exp, log, sin, cos, tan, atan, "
            "abs, pi, ... For example 1.109724/sqrt(x).");
    igSetNextItemWidth(90);
    igInputDouble("x from", &add.lo, 0, 0, "%.6g", 0);
    igSameLine(0, -1);
    igSetNextItemWidth(90);
    igInputDouble("to", &add.hi, 0, 0, "%.6g", 0);
    if (Button("Add")) app_fig_add_function(a, panel, add.expr, add.lo, add.hi, add.n);
    break;
  case SRC_CURVE: {
    static char ys[128][APP_NAME_LEN];
    int ny = 0, j;
    for (j = 0; j < a->nvars && ny < 128; j++) snprintf(ys[ny++], APP_NAME_LEN, "%s'", a->vars[j]);
    for (j = 1; j < a->ncols && ny < 128; j++) snprintf(ys[ny++], APP_NAME_LEN, "%s", a->cols[j]);
    if (add.cy >= ny) add.cy = 0;
    igSetNextItemWidth(110);
    Combo("y##cy", &add.cy, ys, ny);
    Tooltip("A right-hand side (V' is dV/dt) or an output column, as one "
            "variable sweeps a range and the others keep their initial values.");
    igSameLine(0, -1);
    igSetNextItemWidth(110);
    Combo("x##cx", &add.cx, a->vars, a->nvars);
    igSetNextItemWidth(90);
    igInputDouble("from##c", &add.lo, 0, 0, "%.6g", 0);
    igSameLine(0, -1);
    igSetNextItemWidth(90);
    igInputDouble("to##c", &add.hi, 0, 0, "%.6g", 0);
    if (Button("Add")) app_fig_add_curve(a, panel, ys[add.cy], add.cx, add.lo, add.hi, add.n);
    break;
  }
  case SRC_EQ:
    igSetNextItemWidth(110);
    Combo("x##ex", &a->pp.xvar, a->vars, a->nvars);
    igSameLine(0, -1);
    igSetNextItemWidth(110);
    Combo("y##ey", &a->pp.yvar, a->vars, a->nvars);
    igBeginDisabled(a->neq == 0);
    if (Button("Add")) app_fig_add_equilibria(a, panel, a->pp.xvar, a->pp.yvar);
    igEndDisabled();
    if (a->neq == 0) igTextDisabled("Find equilibria in the phase plane first.");
    break;
  }
}

/* ---- The window ---------------------------------------------------------------------- */

static void series_editor(App *a, FigPanel *p, int k) {
  FigSeries *s = &p->s[k];
  FigUI *g = &a->fg;
  bool c = false;
  float old[3];
  int line = s->line, mark = s->marker;
  memcpy(old, s->color, sizeof old);
  igPushID_Int(k);
  c |= text_field(a, "label", s->label, sizeof s->label);
  {
    float col[3];
    memcpy(col, s->color, sizeof col);
    if (igColorEdit3("colour", col, ImGuiColorEditFlags_NoInputs)) {
      memcpy(s->color, col, sizeof col);
      /* markers follow the line's colour unless given their own */
      if (!memcmp(old, s->mcolor, sizeof old)) memcpy(s->mcolor, s->color, sizeof old);
      c = true;
    }
  }
  igSameLine(0, -1);
  igSetNextItemWidth(90);
  if (igCombo_Str_arr("line", &line, line_items, FIG_LINE_COUNT, 6)) {
    s->line = line;
    c = true;
  }
  igSameLine(0, -1);
  igSetNextItemWidth(60);
  c |= igDragFloat("width", &s->width, 0.05f, 0.1f, 10, "%.2f", 0);
  igSetNextItemWidth(90);
  if (igCombo_Str_arr("marker", &mark, marker_items, FIG_MARK_COUNT, 9)) {
    s->marker = mark;
    c = true;
  }
  if (s->marker) {
    igSameLine(0, -1);
    igSetNextItemWidth(60);
    c |= igDragFloat("size", &s->msize, 0.1f, 1, 30, "%.1f", 0);
    igSameLine(0, -1);
    c |= igCheckbox("filled", &s->mfill);
    igSameLine(0, -1);
    igSetNextItemWidth(60);
    c |= igInputInt("every", &s->every, 0, 0, 0);
    if (s->every < 1) s->every = 1;
  }
  igSetNextItemWidth(90);
  c |= igInputDouble("x scale", &s->sx, 0, 0, "%.6g", 0);
  igSameLine(0, -1);
  igSetNextItemWidth(90);
  c |= igInputDouble("y scale", &s->sy, 0, 0, "%.6g", 0);
  igSameLine(0, -1);
  if (Button("y rad->deg")) {
    s->sy = 180 / 3.14159265358979323846;
    c = true;
  }
  Tooltip("Show radians as degrees: multiply y by 180/pi. Any factor works, "
          "such as 1/sqrt(g L) to make a speed dimensionless.");
  igPopID();
  if (c) g->dirty = true;
}

void ui_figure(App *a) {
  FigUI *g = &a->fg;
  Figure *f = &g->fig;
  FigPanel *p;
  ImVec2_c avail;
  float left;
  bool c = false;
  poll_path(a);
  if (!igBegin(WIN_FIGURE, &a->show_figure, 0)) {
    igEnd();
    return;
  }
  if (g->panel >= f->npanels) g->panel = 0;
  p = &f->p[g->panel];
  avail = igGetContentRegionAvail();
  left = 360 * igGetStyle()->FontScaleDpi;
  if (left < 340) left = 340;
  if (left > avail.x * 0.55f) left = avail.x * 0.55f;

  igBeginChild_Str("##figform", V2(left, 0), 0, 0);
  igSeparatorText("Page");
  {
    static const char *sizes[] = {"journal column (86 x 65 mm)", "1.5 columns (120 x 80 mm)",
                                  "two columns (178 x 100 mm)", "slide (254 x 143 mm)",
                                  "as typed"};
    static const float wh[4][2] = {{86, 65}, {120, 80}, {178, 100}, {254, 143}};
    int k = 4, r = f->rows, cc = f->cols, font = f->font, i;
    for (i = 0; i < 4; i++)
      if (f->width_mm == wh[i][0] && f->height_mm == wh[i][1]) k = i;
    igSetNextItemWidth(-1);
    if (igCombo_Str_arr("##size", &k, sizes, 5, 5) && k < 4) {
      f->width_mm = wh[k][0];
      f->height_mm = wh[k][1];
      c = true;
    }
    igSetNextItemWidth(80);
    c |= igInputFloat("width mm", &f->width_mm, 0, 0, "%.1f", 0);
    igSameLine(0, -1);
    igSetNextItemWidth(80);
    c |= igInputFloat("height mm", &f->height_mm, 0, 0, "%.1f", 0);
    if (f->width_mm < 20) f->width_mm = 20;
    if (f->height_mm < 20) f->height_mm = 20;
    igSetNextItemWidth(80);
    if (igCombo_Str_arr("font", &font, (const char *[]){"sans", "serif"}, 2, 2)) {
      f->font = font;
      c = true;
    }
    Tooltip("DejaVu Sans, or STIX (close to Times, like LaTeX papers); the "
            "fonts matplotlib uses, so the Python echo draws the same.");
    igSameLine(0, -1);
    igSetNextItemWidth(70);
    c |= igInputFloat("size pt", &f->font_pt, 0, 0, "%.1f", 0);
    if (f->font_pt < 4) f->font_pt = 4;
    igSetNextItemWidth(60);
    igInputInt("rows", &r, 1, 1, 0);
    igSameLine(0, -1);
    igSetNextItemWidth(60);
    igInputInt("columns", &cc, 1, 1, 0);
    if ((r != f->rows || cc != f->cols) && r >= 1 && cc >= 1) app_fig_grid(a, r, cc);
    igSameLine(0, -1);
    if (Button("Inset")) app_fig_inset(a, g->panel < f->rows * f->cols ? g->panel : 0);
    Tooltip("A small panel inside the selected one, for a close-up.");
    if (Button("New figure")) app_fig_new(a, f->width_mm, f->height_mm, 1, 1);
    Tooltip("Start again with an empty figure of this size.");
  }

  igSeparatorText("Panel");
  {
    static char names[FIG_MAX_PANELS][APP_NAME_LEN];
    int i, ng = f->rows * f->cols;
    for (i = 0; i < f->npanels; i++) {
      if (i < ng)
        snprintf(names[i], APP_NAME_LEN, "panel %d (row %d, column %d)", i + 1, i / f->cols + 1,
                 i % f->cols + 1);
      else
        snprintf(names[i], APP_NAME_LEN, "inset %d in panel %d", i - ng + 1, f->p[i].parent + 1);
    }
    igSetNextItemWidth(-1);
    Combo("##panel", &g->panel, names, f->npanels);
    p = &f->p[g->panel];
    c |= text_field(a, "title", p->title, sizeof p->title);
    c |= text_field(a, "x label", p->xlabel, sizeof p->xlabel);
    Tooltip("Mathtext between dollar signs: $V_W$ [m/s], $\\Gamma$ [deg], "
            "$\\tilde{V}_{W,H}$, $T/(mg)$.");
    c |= text_field(a, "y label", p->ylabel, sizeof p->ylabel);
    {
      double lim[4], dx[2], dy[2];
      fig_limits(p, lim);
      dx[0] = lim[0];
      dx[1] = lim[1];
      dy[0] = lim[2];
      dy[1] = lim[3];
      c |= limits_row("x", &p->autox, &p->xmin, &p->xmax, &p->logx, &p->xstep, dx);
      c |= limits_row("y", &p->autoy, &p->ymin, &p->ymax, &p->logy, &p->ystep, dy);
    }
    c |= igCheckbox("grid", &p->grid);
    igSameLine(0, -1);
    c |= igCheckbox("equal scales", &p->equal);
    igSameLine(0, -1);
    igSetNextItemWidth(110);
    c |= igCombo_Str_arr("legend", &p->legend, legend_items, FIG_LEGEND_COUNT, 7);
    if (p->inset) {
      igSetNextItemWidth(-60);
      c |= igSliderFloat("left", &p->ix, 0, 1, "%.2f", 0);
      igSetNextItemWidth(-60);
      c |= igSliderFloat("bottom", &p->iy, 0, 1, "%.2f", 0);
      igSetNextItemWidth(-60);
      c |= igSliderFloat("width", &p->iw, 0.05f, 1, "%.2f", 0);
      igSetNextItemWidth(-60);
      c |= igSliderFloat("height", &p->ih, 0.05f, 1, "%.2f", 0);
    }
  }

  igSeparatorText("Series");
  {
    int k, remove = -1;
    if (p->nseries == 0) igTextDisabled("None yet: add one below.");
    for (k = 0; k < p->nseries; k++) {
      char item[300];
      ImVec4_c col = V4(p->s[k].color[0], p->s[k].color[1], p->s[k].color[2], 1);
      igPushID_Int(k);
      igColorButton("##c", col, ImGuiColorEditFlags_NoTooltip, V2(12, 12));
      igSameLine(0, -1);
      snprintf(item, sizeof item, "%s", p->s[k].label[0] ? p->s[k].label : "(no label)");
      if (igSelectable_Bool(item, g->series == k, 0, V2(-30, 0))) g->series = k;
      igSameLine(0, -1);
      if (igSmallButton("x")) remove = k;
      igPopID();
    }
    if (remove >= 0) app_fig_remove(a, g->panel, remove);
    if (g->series >= 0 && g->series < p->nseries) {
      igSpacing();
      series_editor(a, p, g->series);
    }
  }

  igSeparatorText("Add to this panel");
  add_form(a);

  igSeparatorText("Export");
  igSetNextItemWidth(80);
  igInputInt("PNG dpi", &f->dpi, 50, 100, 0);
  if (f->dpi < 50) f->dpi = 50;
  if (f->dpi > 2400) f->dpi = 2400;
  if (Button("PDF...")) ask_path(a, 0);
  igSameLine(0, -1);
  if (Button("SVG...")) ask_path(a, 1);
  igSameLine(0, -1);
  if (Button("EPS...")) ask_path(a, 2);
  igSameLine(0, -1);
  if (Button("PNG...")) ask_path(a, 3);
  Tooltip("Vector figures keep every line sharp at any size; the text is "
          "drawn as outlines, so no fonts are needed to open them.");
  if (g->error[0]) igTextColored(V4(0.85f, 0.2f, 0.2f, 1), "%s", g->error);
  else if (g->path[0]) igTextDisabled("Saved %s", g->path);
  igEndChild();

  if (c) g->dirty = true;
  /* settings go to the session once nothing is being edited */
  if (!igIsAnyItemActive()) app_fig_sync_echo(a);

  igSameLine(0, -1);
  igBeginChild_Str("##figpreview", V2(0, 0), 0, 0);
  {
    ImVec2_c room = igGetContentRegionAvail(), pos;
    ImVec2_c fb = igGetIO_Nil()->DisplayFramebufferScale;
    float aspect = f->width_mm / f->height_mm, w = room.x, h = room.x / aspect;
    static bool dragging;
    static ImVec2_c start;
    ImTextureRef_c tex;
    if (h > room.y - 24) {
      h = room.y - 24;
      w = h * aspect;
    }
    if (w < 40 || h < 30) {
      igEndChild();
      igEnd();
      return;
    }
    pos = igGetCursorScreenPos();
    tex = preview_texture(a, (int)(w * (fb.x > 0 ? fb.x : 1)), (int)(h * (fb.y > 0 ? fb.y : 1)));
    if (tex._TexID) igImage(tex, V2(w, h), V2(0, 0), V2(1, 1));
    else igDummy(V2(w, h));
    /* the mouse: which panel, where; drag a rectangle to zoom */
    {
      ImVec2_c m = igGetMousePos();
      float sc = w / preview.width, px = (m.x - pos.x) / sc, py = (m.y - pos.y) / sc;
      int i, over = -1;
      for (i = f->npanels - 1; i >= 0; i--) {
        const FigAxes *ax = &preview.axes[i];
        if (ax->valid && px >= ax->x0 && px <= ax->x1 && py >= ax->y0 && py <= ax->y1) {
          over = i;
          break;
        }
      }
      if (igIsItemHovered(0) && over >= 0) {
        double x, y;
        fig_from_page(&preview.axes[over], px, py, &x, &y);
        igText("panel %d: x = %.6g, y = %.6g. Drag to zoom, double-click for all.", over + 1, x, y);
        if (igIsMouseClicked_Bool(ImGuiMouseButton_Left, false)) {
          g->panel = over;
          dragging = true;
          start = m;
        }
        if (igIsMouseDoubleClicked_Nil(ImGuiMouseButton_Left)) {
          f->p[over].autox = f->p[over].autoy = true;
          g->dirty = true;
          dragging = false;
        }
      } else {
        igTextDisabled("%g x %g mm. Click a panel to select it; drag to zoom.", f->width_mm,
                       f->height_mm);
      }
      if (dragging) {
        ImDrawList *dl = igGetWindowDrawList();
        ImDrawList_AddRect(dl, start, m, igGetColorU32_Col(ImGuiCol_PlotLinesHovered, 1), 0, 0, 1.5f);
        if (igIsMouseReleased_Nil(ImGuiMouseButton_Left)) {
          dragging = false;
          if (fabsf(m.x - start.x) > 6 && fabsf(m.y - start.y) > 6 && g->panel < f->npanels &&
              preview.axes[g->panel].valid) {
            double x0, y0, x1, y1;
            FigPanel *q = &f->p[g->panel];
            fig_from_page(&preview.axes[g->panel], (start.x - pos.x) / sc, (start.y - pos.y) / sc,
                          &x0, &y0);
            fig_from_page(&preview.axes[g->panel], px, py, &x1, &y1);
            q->autox = q->autoy = false;
            q->xmin = x0 < x1 ? x0 : x1;
            q->xmax = x0 < x1 ? x1 : x0;
            q->ymin = y0 < y1 ? y0 : y1;
            q->ymax = y0 < y1 ? y1 : y0;
            g->dirty = true;
          }
        }
      }
    }
  }
  igEndChild();
  igEnd();
}
