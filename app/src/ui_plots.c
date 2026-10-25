/* Time series and phase-plane plots. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "export.h"
#include "ui.h"

static const ImVec4_c palette[] = {
    {0.12f, 0.47f, 0.71f, 1}, {0.84f, 0.15f, 0.16f, 1}, {0.17f, 0.63f, 0.17f, 1},
    {0.58f, 0.40f, 0.74f, 1}, {1.00f, 0.50f, 0.05f, 1}, {0.55f, 0.34f, 0.29f, 1},
    {0.89f, 0.47f, 0.76f, 1}, {0.50f, 0.50f, 0.50f, 1}, {0.74f, 0.74f, 0.13f, 1},
    {0.09f, 0.75f, 0.81f, 1}};
#define NPAL ((int)(sizeof palette / sizeof palette[0]))

static ImVec4_c faded(ImVec4_c c) {
  c.w = 0.35f;
  return c;
}

/* ---- Time series ---------------------------------------------------------- */

void ui_time_plot(App *a) {
  int j, k;
  if (!igBegin(WIN_TIME, &a->show_time, 0)) {
    igEnd();
    return;
  }
  if (!a->model_ok) {
    igTextDisabled("Open a model to see its solution here.");
    igEnd();
    return;
  }
  if (Button("Variables...")) igOpenPopup_Str("cols", 0);
  Tooltip("Choose which variables to plot.");
  if (igBeginPopup("cols", 0)) {
    for (j = 1; j < a->ncols; j++) igCheckbox(a->cols[j], &a->plot_col[j]);
    igEndPopup();
  }
  igSameLine(0, -1);
  if (Button("Fit")) ImPlot_SetNextAxesToFit();
  Tooltip("Fit the axes to the data (W F). Double-click the plot does the "
          "same.");
  igSameLine(0, -1);
  ui_export_button(a, FIG_TIME);
  igSameLine(0, -1);
  igTextDisabled("Drag to pan, scroll to zoom, right-click for options.");

  if (ImPlot_BeginPlot("##time", V2(-1, -1), ImPlotFlags_NoTitle)) {
    ImPlot_SetupAxes("t", NULL, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    ImPlot_SetupLegend(ImPlotLocation_NorthEast, 0);
    for (k = 0; k < a->nfrozen; k++) {
      Trajectory *t = &a->frozen[k];
      for (j = 1; j < t->ncols && j < a->ncols; j++) {
        char id[96];
        if (!a->plot_col[j]) continue;
        snprintf(id, sizeof id, "##f%d_%d", k, j);
        ImPlot_PlotLine_doublePtrdoublePtr(id, t->cols[0], t->cols[j], t->nrows,
                                           Spec(faded(palette[(j - 1) % NPAL]),
                                                1.0f));
      }
    }
    if (a->have_traj)
      for (j = 1; j < a->traj.ncols; j++) {
        if (!a->plot_col[j]) continue;
        ImPlot_PlotLine_doublePtrdoublePtr(a->traj.names[j], a->traj.cols[0],
                                           a->traj.cols[j], a->traj.nrows,
                                           Spec(palette[(j - 1) % NPAL], 2.0f));
      }
    ui_figure_limits(FIG_TIME);
    ImPlot_EndPlot();
    ui_figure_rect(FIG_TIME);
  }
  igEnd();
}

/* ---- Array plot ------------------------------------------------------------ */

/* XPPAUT's array plot: many variables (say, the cells of a chain) against
   time, as colours. */
void ui_array_plot(App *a) {
  enum { MAXT = 800 };
  static double *grid;
  static size_t grid_cap;
  int first, last, rows, cols, i, j, step;
  double lo = 1e300, hi = -1e300;
  if (!igBegin(WIN_ARRAY, &a->show_array, 0)) {
    igEnd();
    return;
  }
  if (!a->have_traj || a->ncols < 3) {
    igTextDisabled("Run a model with several variables to see them as an array.");
    igEnd();
    return;
  }
  if (a->array_first < 1 || a->array_first >= a->ncols) a->array_first = 1;
  if (a->array_last < a->array_first || a->array_last >= a->ncols)
    a->array_last = a->nvars < a->ncols ? a->nvars : a->ncols - 1;
  igSetNextItemWidth(110);
  Combo("from", &a->array_first, a->cols, a->ncols);
  igSameLine(0, -1);
  igSetNextItemWidth(110);
  Combo("to", &a->array_last, a->cols, a->ncols);
  if (a->array_first < 1) a->array_first = 1;
  if (a->array_last < a->array_first) a->array_last = a->array_first;
  first = a->array_first;
  last = a->array_last;
  rows = last - first + 1;
  step = a->traj.nrows > MAXT ? (a->traj.nrows + MAXT - 1) / MAXT : 1;
  cols = (a->traj.nrows + step - 1) / step;
  if ((size_t)rows * (size_t)cols > grid_cap) {
    free(grid);
    grid_cap = (size_t)rows * (size_t)cols;
    grid = malloc(sizeof(double) * grid_cap);
  }
  if (!grid || cols < 1) {
    igEnd();
    return;
  }
  /* Row 0 is drawn at the top: put the last variable there. */
  for (i = 0; i < rows; i++)
    for (j = 0; j < cols; j++) {
      double v = a->traj.cols[last - i][j * step];
      grid[(size_t)i * cols + j] = v;
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
  if (!(hi > lo)) hi = lo + 1;
  if (ImPlot_BeginPlot("##array", V2(-90, -1), ImPlotFlags_NoTitle | ImPlotFlags_NoLegend)) {
    ImPlotPoint_c bmin = {a->traj.cols[0][0], first - 0.5};
    ImPlotPoint_c bmax = {a->traj.cols[0][a->traj.nrows - 1], last + 0.5};
    ImPlot_SetupAxes("t", "variable", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
    ImPlot_PushColormap_PlotColormap(ImPlotColormap_Viridis);
    ImPlot_PlotHeatmap_doublePtr("##heat", grid, rows, cols, lo, hi, NULL, bmin, bmax,
                                 Spec(V4(0, 0, 0, -1), 1.0f));
    ImPlot_PopColormap(1);
    ImPlot_EndPlot();
  }
  igSameLine(0, -1);
  ImPlot_ColormapScale("##scale", lo, hi, V2(80, -1), "%g", 0, ImPlotColormap_Viridis);
  igEnd();
}

/* ---- Phase plane ---------------------------------------------------------- */

static void draw_dirfield(App *a, ImPlotRect_c lim) {
  enum { N = 21 };
  static double xs[N * N * 2], ys[N * N * 2];
  double *x = malloc(sizeof(double) * (size_t)a->nvars);
  double *f = malloc(sizeof(double) * (size_t)a->nvars);
  double dx = (lim.X.Max - lim.X.Min) / (N - 1);
  double dy = (lim.Y.Max - lim.Y.Min) / (N - 1);
  double len = 0.4 * (dx < dy ? dx : dy);
  int i, j, n = 0;
  if (!x || !f) {
    free(x);
    free(f);
    return;
  }
  for (i = 0; i < N; i++)
    for (j = 0; j < N; j++) {
      double px = lim.X.Min + i * dx, py = lim.Y.Min + j * dy, u, v, m;
      memcpy(x, a->ic, sizeof(double) * (size_t)a->nvars);
      x[a->pp.xvar] = px;
      x[a->pp.yvar] = py;
      if (a->eng->api.xppe_rhs(0.0, x, f) != XPPE_OK) continue;
      if (a->meth == 0) { /* maps: show the displacement */
        f[a->pp.xvar] -= px;
        f[a->pp.yvar] -= py;
      }
      u = f[a->pp.xvar] / dx;
      v = f[a->pp.yvar] / dy;
      m = sqrt(u * u + v * v);
      if (!(m > 0) || !isfinite(m)) continue;
      /* An arrow of fixed screen length, centred on the grid point. */
      xs[n] = px - 0.5 * len * (u / m) * (dx / (dx < dy ? dx : dy));
      ys[n] = py - 0.5 * len * (v / m) * (dy / (dx < dy ? dx : dy));
      xs[n + 1] = px + 0.5 * len * (u / m) * (dx / (dx < dy ? dx : dy));
      ys[n + 1] = py + 0.5 * len * (v / m) * (dy / (dx < dy ? dx : dy));
      n += 2;
    }
  {
    ImPlotSpec_c s = Spec(a->dark ? V4(0.7f, 0.7f, 0.75f, 0.8f)
                                  : V4(0.45f, 0.45f, 0.5f, 0.8f),
                          1.0f);
    s.Flags = ImPlotLineFlags_Segments;
    ImPlot_PlotLine_doublePtrdoublePtr("##dirfield", xs, ys, n, s);
    /* A dot at the head of each arrow shows the direction. */
    {
      ImPlotSpec_c h = SpecMarker(s.LineColor, ImPlotMarker_Circle, 1.8f);
      h.Offset = 0;
      h.Stride = (int)(2 * sizeof(double));
      h.LineWeight = 0;
      ImPlot_PlotScatter_doublePtrdoublePtr("##heads", xs + 1, ys + 1, n / 2, h);
    }
  }
  free(x);
  free(f);
}

static void draw_nullclines(App *a) {
  PhasePlane *p = &a->pp;
  ImPlotSpec_c s;
  char label[96];
  if (p->ncx_n > 0) {
    s = Spec(COL_XNULL, 2.0f);
    s.Flags = ImPlotLineFlags_Segments;
    s.Stride = (int)(2 * sizeof(float));
    snprintf(label, sizeof label, "%s nullcline", a->vars[p->xvar]);
    ImPlot_PlotLine_FloatPtrFloatPtr(label, p->ncx, p->ncx + 1, 2 * p->ncx_n, s);
  }
  if (p->ncy_n > 0) {
    s = Spec(COL_YNULL, 2.0f);
    s.Flags = ImPlotLineFlags_Segments;
    s.Stride = (int)(2 * sizeof(float));
    snprintf(label, sizeof label, "%s nullcline", a->vars[p->yvar]);
    ImPlot_PlotLine_FloatPtrFloatPtr(label, p->ncy, p->ncy + 1, 2 * p->ncy_n, s);
  }
}

static void draw_manifolds(App *a, Equilibrium *e) {
  int k, j, jx, jy;
  for (k = 0; k < e->nmanifolds; k++) {
    Trajectory *t = &e->manifold[k];
    jx = jy = -1;
    for (j = 1; j < t->ncols; j++) {
      if (SDL_strcasecmp(t->names[j], a->vars[a->pp.xvar]) == 0) jx = j;
      if (SDL_strcasecmp(t->names[j], a->vars[a->pp.yvar]) == 0) jy = j;
    }
    if (jx < 0 || jy < 0) continue;
    ImPlot_PlotLine_doublePtrdoublePtr(
        e->manifold_type[k] > 0 ? "unstable manifold" : "stable manifold",
        t->cols[jx], t->cols[jy], t->nrows,
        Spec(e->manifold_type[k] > 0 ? HEX(0xff7f0e) : HEX(0x9467bd), 1.6f));
  }
}

static void draw_equilibria(App *a) {
  int i;
  for (i = 0; i < a->neq; i++) {
    Equilibrium *e = &a->eq[i];
    double x = e->x[a->pp.xvar], y = e->x[a->pp.yvar];
    draw_manifolds(a, e);
    bool stable = e->unstable == 0;
    ImPlotSpec_c s = SpecMarker(stable ? HEX(0x000000) : HEX(0xffffff),
                                ImPlotMarker_Circle, 5.5f);
    s.MarkerLineColor = HEX(0x000000);
    s.LineWeight = 1.5f;
    ImPlot_PlotScatter_doublePtrdoublePtr("##eq", &x, &y, 1, s);
  }
}

static int column_of_var(App *a, int v) {
  int j;
  for (j = 1; j < a->traj.ncols; j++)
    if (SDL_strcasecmp(a->traj.names[j], a->vars[v]) == 0) return j;
  return -1;
}

void ui_phase_plot(App *a) {
  PhasePlane *p = &a->pp;
  int k;
  if (!igBegin(WIN_PHASE, &a->show_phase, 0)) {
    igEnd();
    return;
  }
  if (!a->model_ok || a->nvars < 2) {
    igTextDisabled(a->model_ok ? "The phase plane needs at least two variables."
                               : "Open a model to see its phase plane here.");
    igEnd();
    return;
  }
  igSetNextItemWidth(90);
  if (Combo("x", &p->xvar, a->vars, a->nvars)) p->nc_dirty = true;
  igSameLine(0, -1);
  igSetNextItemWidth(90);
  if (Combo("y", &p->yvar, a->vars, a->nvars)) p->nc_dirty = true;
  igSameLine(0, 14);
  if (igCheckbox("Nullclines", &p->nullclines)) {
    p->nc_dirty = true;
    a->echo.nc_written = false;
  }
  Tooltip("Red: where dx/dt = 0. Green: where dy/dt = 0. Equilibria are where "
          "they cross (N N).");
  igSameLine(0, -1);
  igCheckbox("Flow", &p->dirfield);
  Tooltip("The direction field (D D).");
  igSameLine(0, -1);
  igCheckbox("Click to start", &p->click_to_start);
  Tooltip("Click anywhere in the plot to start a trajectory there, like "
          "XPPAUT's Initial conds > Mouse.");
  if (p->click_to_start) {
    igSameLine(0, -1);
    igCheckbox("Keep each", &p->click_keep);
    Tooltip("Every click adds a trajectory and the earlier ones stay, so a few "
            "clicks sketch the phase portrait (XPPAUT's Initial conds > mIce). "
            "Erase (E) clears them; up to 16 are kept.");
  }
  igSameLine(0, -1);
  if (Button("All equilibria")) app_command(a, "eq all");
  Tooltip("Look for every equilibrium in this window from 100 random "
          "starting points, and mark each with its stability (XPPAUT's Sing "
          "pts > Monte Carlo; eq all N for more points).");
  igSameLine(0, -1);
  if (Button("Fit")) ImPlot_SetNextAxesToFit();
  igSameLine(0, -1);
  ui_export_button(a, FIG_PHASE);

  if (ImPlot_BeginPlot("##phase", V2(-1, -1), ImPlotFlags_NoTitle)) {
    ImPlotRect_c lim;
    int jx = -1, jy = -1;
    ImPlot_SetupAxes(a->vars[p->xvar], a->vars[p->yvar], 0, 0);
    if (p->set_window) {
      ImPlot_SetupAxesLimits(p->win_xlo, p->win_xhi, p->win_ylo, p->win_yhi,
                             ImPlotCond_Always);
      p->set_window = false;
    }
    if (a->have_traj) {
      jx = column_of_var(a, p->xvar);
      jy = column_of_var(a, p->yvar);
    }
    ImPlot_SetupFinish();
    lim = ImPlot_GetPlotLimits(ImAxis_X1, ImAxis_Y1);

    if (p->nullclines) {
      if (p->nc_dirty || lim.X.Min != p->xlo || lim.X.Max != p->xhi ||
          lim.Y.Min != p->ylo || lim.Y.Max != p->yhi)
        if (!igIsMouseDragging(ImGuiMouseButton_Left, -1))
          app_nullclines(a, lim.X.Min, lim.X.Max, lim.Y.Min, lim.Y.Max);
      draw_nullclines(a);
    }
    if (p->dirfield) draw_dirfield(a, lim);
    for (k = 0; k < a->nfrozen; k++) {
      Trajectory *t = &a->frozen[k];
      int fx = -1, fy = -1, j;
      char id[32];
      for (j = 1; j < t->ncols; j++) {
        if (SDL_strcasecmp(t->names[j], a->vars[p->xvar]) == 0) fx = j;
        if (SDL_strcasecmp(t->names[j], a->vars[p->yvar]) == 0) fy = j;
      }
      if (fx < 0 || fy < 0) continue;
      snprintf(id, sizeof id, "##fr%d", k);
      ImPlot_PlotLine_doublePtrdoublePtr(id, t->cols[fx], t->cols[fy], t->nrows,
                                         Spec(faded(palette[0]), 1.0f));
    }
    if (jx > 0 && jy > 0) {
      ImPlot_PlotLine_doublePtrdoublePtr("trajectory", a->traj.cols[jx],
                                         a->traj.cols[jy], a->traj.nrows,
                                         Spec(palette[0], 2.0f));
      {
        double x0 = a->traj.cols[jx][0], y0 = a->traj.cols[jy][0];
        ImPlot_PlotScatter_doublePtrdoublePtr(
            "##start", &x0, &y0, 1, SpecMarker(palette[0], ImPlotMarker_Square, 3));
      }
    }
    if (p->show_equilibria) draw_equilibria(a);

    /* A click without dragging starts a trajectory there. */
    if (p->click_to_start && ImPlot_IsPlotHovered() &&
        igIsMouseReleased_Nil(ImGuiMouseButton_Left)) {
      ImVec2_c drag = igGetMouseDragDelta(ImGuiMouseButton_Left, 0);
      if (fabsf(drag.x) < 3 && fabsf(drag.y) < 3) {
        ImPlotPoint_c m = ImPlot_GetPlotMousePos(ImAxis_X1, ImAxis_Y1);
        app_click_start(a, m.x, m.y);
      }
    }
    ui_figure_limits(FIG_PHASE);
    ImPlot_EndPlot();
    ui_figure_rect(FIG_PHASE);
  }
  igEnd();
}
