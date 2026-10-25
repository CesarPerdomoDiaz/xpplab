/* The bifurcation (AUTO) window. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "export.h"
#include "ui.h"

static const char *describe(const xppe_auto_point_info *p) {
  const char *t = p->type_name;
  if (!strcmp(t, "HB")) return "Hopf bifurcation";
  if (!strcmp(t, "LP")) return p->periodic ? "fold of limit cycles" : "fold (saddle-node)";
  if (!strcmp(t, "BP")) return "branch point";
  if (!strcmp(t, "PD")) return "period doubling";
  if (!strcmp(t, "TR")) return "torus bifurcation";
  if (!strcmp(t, "EP")) return "end point";
  if (!strcmp(t, "UZ")) return "user point";
  if (!strcmp(t, "MX")) return "no convergence";
  return "";
}

static double yvalue(App *a, int i, int which) {
  const xppe_auto_point_info *p = &a->au.info[i];
  int v = a->au.yvar;
  if (v < 0) return p->norm;
  return which == 0 ? a->au.umax[(size_t)i * a->nvars + v]
                    : a->au.umin[(size_t)i * a->nvars + v];
}

/* ImPlot has no dashed lines: draw the dashes in screen space, so they
   keep the same length at any zoom. */
static void plot_dashed(const double *xs, const double *ys, int n, ImVec4_c color,
                        float weight) {
  const float on = 6.0f, off = 4.0f;
  ImDrawList *dl;
  ImU32 col = igColorConvertFloat4ToU32(color);
  float phase = 0; /* distance into the current on+off cycle */
  int i;
  if (n < 2) return;
  dl = ImPlot_GetPlotDrawList();
  ImPlot_PushPlotClipRect(0);
  for (i = 1; i < n; i++) {
    ImVec2_c p = ImPlot_PlotToPixels_double(xs[i - 1], ys[i - 1], ImAxis_X1, ImAxis_Y1);
    ImVec2_c q = ImPlot_PlotToPixels_double(xs[i], ys[i], ImAxis_X1, ImAxis_Y1);
    float dx = q.x - p.x, dy = q.y - p.y, len = sqrtf(dx * dx + dy * dy), s = 0;
    if (!(len > 0) || len > 1e5f) continue;
    while (s < len) {
      float left = phase < on ? on - phase : on + off - phase;
      float step = left < len - s ? left : len - s;
      if (phase < on)
        ImDrawList_AddLine(dl, V2(p.x + dx * s / len, p.y + dy * s / len),
                           V2(p.x + dx * (s + step) / len, p.y + dy * (s + step) / len),
                           col, weight, 0);
      s += step;
      phase += step;
      if (phase >= on + off) phase = 0;
    }
  }
  ImPlot_PopPlotClipRect();
}

/* Draw runs of consecutive points with the same branch and stability. */
static void draw_branches(App *a, bool two_param) {
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
    if ((p0->two_parameter != 0) != two_param) {
      i++;
      continue;
    }
    /* Start with the previous point of the same branch, so runs join up. */
    if (i > 0 && u->info[i - 1].branch == p0->branch &&
        (u->info[i - 1].two_parameter != 0) == two_param) {
      xs[m] = u->info[i - 1].par1;
      ys[m] = two_param ? u->info[i - 1].par2 : yvalue(a, i - 1, 0);
      ys2[m] = two_param ? 0 : yvalue(a, i - 1, 1);
      m++;
    }
    while (j < n && u->info[j].branch == p0->branch &&
           (u->info[j].stable != 0) == st &&
           (u->info[j].two_parameter != 0) == two_param &&
           (j == i || abs(u->info[j].point) == abs(u->info[j - 1].point) + 1)) {
      xs[m] = u->info[j].par1;
      ys[m] = two_param ? u->info[j].par2 : yvalue(a, j, 0);
      ys2[m] = two_param ? 0 : yvalue(a, j, 1);
      m++;
      j++;
    }
    {
      ImVec4_c c = two_param ? HEX(0x9467bd)
                             : per ? (st ? COL_STABLE_PER : COL_UNSTABLE_PER)
                                   : (st ? COL_STABLE_EQ : COL_UNSTABLE_EQ);
      bool both = per && !two_param && a->au.yvar >= 0;
      if (st || two_param) {
        ImPlotSpec_c s = Spec(c, 2.2f);
        ImPlot_PlotLine_doublePtrdoublePtr("##br", xs, ys, m, s);
        if (both) ImPlot_PlotLine_doublePtrdoublePtr("##br2", xs, ys2, m, s);
      } else {
        /* Unstable: dashed. An invisible line keeps the axes fitting. */
        ImPlotSpec_c s = Spec(V4(0, 0, 0, 0), 0.0f);
        ImPlot_PlotLine_doublePtrdoublePtr("##bru", xs, ys, m, s);
        plot_dashed(xs, ys, m, c, 1.4f);
        if (both) {
          ImPlot_PlotLine_doublePtrdoublePtr("##bru2", xs, ys2, m, s);
          plot_dashed(xs, ys2, m, c, 1.4f);
        }
      }
    }
    i = j > i ? j : i + 1;
  }
done:
  free(xs);
  free(ys);
  free(ys2);
}

static void draw_special(App *a, bool two_param) {
  enum { MAXL = 256 };
  AutoUI *u = &a->au;
  ImVec2_c drawn[MAXL];
  char drawn_type[MAXL][4];
  int ndrawn = 0, i;
  for (i = 0; i < u->count; i++) {
    const xppe_auto_point_info *p = &u->info[i];
    double x, y;
    if (!p->label || (p->two_parameter != 0) != two_param) continue;
    if (!p->type_name[0] && i != u->selected) continue;
    x = p->par1;
    y = two_param ? p->par2 : yvalue(a, i, 0);
    {
      ImPlotSpec_c s = SpecMarker(i == u->selected ? HEX(0xffd600) : COL_SPECIAL,
                                  ImPlotMarker_Circle,
                                  i == u->selected ? 6.0f : 4.0f);
      ImPlot_PlotScatter_doublePtrdoublePtr("##sp", &x, &y, 1, s);
    }
    if (p->type_name[0] && strcmp(p->type_name, "EP") != 0) {
      /* AUTO often passes the same point several times (closed loops of
         orbits): label it once. */
      ImVec2_c px = ImPlot_PlotToPixels_double(x, y, ImAxis_X1, ImAxis_Y1);
      int k;
      bool crowded = false;
      for (k = 0; k < ndrawn; k++)
        if (fabsf(drawn[k].x - px.x) < 18 && fabsf(drawn[k].y - px.y) < 14 &&
            strcmp(drawn_type[k], p->type_name) == 0)
          crowded = true;
      if (!crowded) {
        ImPlot_PlotText(p->type_name, x, y, V2(12, -10), Spec(COL_SPECIAL, 1.0f));
        if (ndrawn < MAXL) {
          drawn[ndrawn] = px;
          snprintf(drawn_type[ndrawn], 4, "%s", p->type_name);
          ndrawn++;
        }
      }
    }
  }
}

/* Select the labelled point nearest to the mouse (in screen terms). */
static void pick_point(App *a, bool two_param) {
  AutoUI *u = &a->au;
  ImPlotPoint_c m = ImPlot_GetPlotMousePos(ImAxis_X1, ImAxis_Y1);
  ImPlotRect_c lim = ImPlot_GetPlotLimits(ImAxis_X1, ImAxis_Y1);
  double sx = lim.X.Max - lim.X.Min, sy = lim.Y.Max - lim.Y.Min, best = 1e300;
  int i, pick = -1;
  for (i = 0; i < u->count; i++) {
    const xppe_auto_point_info *p = &u->info[i];
    double dx, dy, d;
    if (!p->label || (p->two_parameter != 0) != two_param) continue;
    dx = (p->par1 - m.x) / sx;
    dy = ((two_param ? p->par2 : yvalue(a, i, 0)) - m.y) / sy;
    d = dx * dx + dy * dy;
    if (d < best) {
      best = d;
      pick = i;
    }
  }
  if (pick >= 0 && best < 0.0025) u->selected = pick;
}

/* Is point i inside the window AUTO was asked to cover? AUTO checks Par
   min/max and Norm max only after a step, so a step that jumps far (6x6.ode
   jumps from a = 0.26 to a = -1.3e6) leaves one stray end point outside. */
static bool in_window(App *a, int i, bool two_param) {
  const AutoUI *u = &a->au;
  const xppe_auto_point_info *p = &u->info[i];
  double margin = 0.1 * (u->parmax - u->parmin);
  if (u->parmax > u->parmin &&
      (p->par1 < u->parmin - margin || p->par1 > u->parmax + margin))
    return false;
  return two_param || !(u->normmax > 0) || p->norm <= u->normmax;
}

/* The finite extent of the diagram's points along x and y: those inside
   AUTO's window, or all of them when none is. */
static void diagram_extent(App *a, bool two_param, double *xlo, double *xhi,
                           double *ylo, double *yhi) {
  AutoUI *u = &a->au;
  int i, k, pass;
  for (pass = 0; pass < 2; pass++) {
    *xlo = *ylo = HUGE_VAL;
    *xhi = *yhi = -HUGE_VAL;
    for (i = 0; i < u->count; i++) {
      const xppe_auto_point_info *p = &u->info[i];
      double y[2];
      if ((p->two_parameter != 0) != two_param || !isfinite(p->par1)) continue;
      if (pass == 0 && !in_window(a, i, two_param)) continue;
      y[0] = two_param ? p->par2 : yvalue(a, i, 0);
      y[1] = two_param ? p->par2 : yvalue(a, i, 1);
      *xlo = fmin(*xlo, p->par1);
      *xhi = fmax(*xhi, p->par1);
      for (k = 0; k < 2; k++)
        if (isfinite(y[k])) {
          *ylo = fmin(*ylo, y[k]);
          *yhi = fmax(*yhi, y[k]);
        }
    }
    if (*xlo <= *xhi) return;
  }
}

/* Fit an axis to [lo, hi] with a margin, but never to a span of rounding
   noise: points that all share one value (a branch that stopped at once)
   would make the axis zoom into nothing, label every tick alike and fly
   off at a pan or a zoom. Such an axis shows a window around the value. */
static void fit_axis(ImAxis axis, double lo, double hi) {
  double mid = 0.5 * (lo + hi), d;
  if (!(lo <= hi)) return;   /* nothing to fit: ImPlot's own default */
  if (hi - lo <= 1e-6 * fmax(1.0, fabs(mid)))
    d = fmax(0.05 * fabs(mid), 1e-3);
  else
    d = 0.55 * (hi - lo);
  ImPlot_SetupAxisLimits(axis, mid - d, mid + d, ImPlotCond_Always);
}

static void diagram_plot(App *a, bool two_param) {
  char ylabel[96];
  AutoUI *u = &a->au;
  const char *xname = u->main_par >= 0 ? a->pars[u->main_par] : "parameter";
  if (two_param)
    snprintf(ylabel, sizeof ylabel, "%s",
             u->second_par >= 0 ? a->pars[u->second_par] : "parameter 2");
  else if (u->yvar < 0)
    snprintf(ylabel, sizeof ylabel, "norm");
  else
    snprintf(ylabel, sizeof ylabel, "%s (max / min)", a->vars[u->yvar]);
  if (ImPlot_BeginPlot(two_param ? "##auto2" : "##auto", V2(-1, -1),
                       ImPlotFlags_NoTitle | ImPlotFlags_NoLegend)) {
    /* The axes follow the diagram's points inside AUTO's window, as
       autofit did, without a stray end point or rounding noise taking
       over the scale. */
    double xlo, xhi, ylo, yhi;
    diagram_extent(a, two_param, &xlo, &xhi, &ylo, &yhi);
    if (!two_param && a->sw.n > 0 && a->sw.result_mode == SWEEP_MAXMIN && !a->sw.over_ic &&
        a->sw.par == u->main_par && u->yvar == a->sw.var) {
      int k;
      for (k = 0; k < a->sw.n; k++) {
        if (isfinite(a->sw.x[k])) {
          xlo = fmin(xlo, a->sw.x[k]);
          xhi = fmax(xhi, a->sw.x[k]);
        }
        if (isfinite(a->sw.ymax[k])) yhi = fmax(yhi, a->sw.ymax[k]);
        if (isfinite(a->sw.ymin[k])) ylo = fmin(ylo, a->sw.ymin[k]);
      }
    }
    ImPlot_SetupAxes(xname, ylabel, ImPlotAxisFlags_None, ImPlotAxisFlags_None);
    fit_axis(ImAxis_X1, xlo, xhi);
    fit_axis(ImAxis_Y1, ylo, yhi);
    /* A brute-force sweep of the same parameter and variable, for
       comparison: simulations should sit on the stable branches. */
    if (!two_param && a->sw.n > 0 && a->sw.result_mode == SWEEP_MAXMIN &&
        !a->sw.over_ic && a->sw.par == u->main_par && u->yvar == a->sw.var) {
      ImPlotSpec_c s = SpecMarker(V4(0.5f, 0.5f, 0.5f, 0.7f), ImPlotMarker_Circle, 2.2f);
      ImPlot_PlotScatter_doublePtrdoublePtr("##swmax", a->sw.x, a->sw.ymax, a->sw.n, s);
      ImPlot_PlotScatter_doublePtrdoublePtr("##swmin", a->sw.x, a->sw.ymin, a->sw.n, s);
    }
    /* Equilibria followed over the same parameter (Range > Equilibria):
       each one a dot in AUTO's colours, on AUTO's branches where they
       agree. */
    if (!two_param && a->sw.n > 0 && a->sw.result_mode == SWEEP_EQUILIBRIA &&
        a->sw.par == u->main_par && u->yvar >= 0 && u->yvar < a->nvars &&
        a->sw.eqt.ncols > a->nvars + 1) {
      const Trajectory *t = &a->sw.eqt;
      int k;
      for (k = 0; k < t->nrows; k++) {
        double px = t->cols[0][k], py = t->cols[u->yvar + 1][k],
               un = t->cols[a->nvars + 1][k];
        if (isnan(un)) continue;
        ImPlot_PlotScatter_doublePtrdoublePtr(
            un > 0 ? "##equ" : "##eqs", &px, &py, 1,
            SpecMarker(un > 0 ? COL_UNSTABLE_EQ : COL_STABLE_EQ, ImPlotMarker_Circle, 2.6f));
      }
    }
    draw_branches(a, two_param);
    draw_special(a, two_param);
    if (ImPlot_IsPlotHovered() && igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
      pick_point(a, two_param);
    ui_figure_limits(two_param ? FIG_AUTO2 : FIG_AUTO);
    ImPlot_EndPlot();
    ui_figure_rect(two_param ? FIG_AUTO2 : FIG_AUTO);
  }
}

static void controls(App *a) {
  AutoUI *u = &a->au;
  int nmax = u->nmax, npr = u->npr, ntst = u->ntst;
  igSeparatorText("What to vary");
  igSetNextItemWidth(-90);
  Combo("Parameter", &u->main_par, a->pars, a->npars);
  Tooltip("The continuation parameter: the horizontal axis of the diagram.");
  igSetNextItemWidth(-90);
  Combo("2nd param.", &u->second_par, a->pars, a->npars);
  Tooltip("The second parameter, used for two-parameter curves (folds, Hopf "
          "points...).");
  igSetNextItemWidth(-90);
  {
    int choice = u->yvar + 1, i;
    const char *prev = u->yvar < 0 ? "norm" : a->vars[u->yvar];
    if (igBeginCombo("Plot", prev, 0)) {
      if (igSelectable_Bool("norm", choice == 0, 0, V2(0, 0))) u->yvar = -1;
      for (i = 0; i < a->nvars; i++)
        if (igSelectable_Bool(a->vars[i], u->yvar == i, 0, V2(0, 0))) u->yvar = i;
      igEndCombo();
    }
  }
  Tooltip("The vertical axis: a variable (its max and min for orbits) or "
          "the norm of the solution.");

  if (igCollapsingHeader_TreeNodeFlags("Numerics", ImGuiTreeNodeFlags_DefaultOpen)) {
    igSetNextItemWidth(-90);
    igInputDouble("Step DS", &u->ds, 0, 0, "%.4g", 0);
    Tooltip("The first step. Its sign sets the direction: make it negative "
            "to go towards smaller parameter values.");
    igSetNextItemWidth(-90);
    igInputDouble("DS min", &u->dsmin, 0, 0, "%.4g", 0);
    igSetNextItemWidth(-90);
    igInputDouble("DS max", &u->dsmax, 0, 0, "%.4g", 0);
    Tooltip("Largest step. Smaller values give smoother, slower curves.");
    igSetNextItemWidth(-90);
    igInputDouble("Par min", &u->parmin, 0, 0, "%.4g", 0);
    igSetNextItemWidth(-90);
    igInputDouble("Par max", &u->parmax, 0, 0, "%.4g", 0);
    Tooltip("AUTO stops when the parameter leaves [Par min, Par max].");
    igSetNextItemWidth(-90);
    if (igInputInt("Max steps", &nmax, 10, 100, 0)) u->nmax = nmax < 1 ? 1 : nmax;
    Tooltip("NMAX: the most steps along one branch.");
    igSetNextItemWidth(-90);
    if (igInputInt("Label every", &npr, 10, 100, 0)) u->npr = npr < 1 ? 1 : npr;
    Tooltip("NPR: also save a labelled point every N steps, so you can restart "
            "from there.");
    igSetNextItemWidth(-90);
    if (igInputInt("Mesh NTST", &ntst, 1, 5, 0)) u->ntst = ntst < 2 ? 2 : ntst;
    Tooltip("Mesh intervals for periodic orbits. Increase it for orbits with "
            "sharp features.");
  }

  igSpacing();
  igPushStyleColor_Vec4(ImGuiCol_Button, HEX(0x1565c0));
  igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, HEX(0x1976d2));
  igPushStyleColor_Vec4(ImGuiCol_Text, HEX(0xffffff));
  if (ButtonW("Follow steady states", -1)) app_auto_equilibria(a);
  igPopStyleColor(3);
  Tooltip("Find the equilibrium near the current initial values, then follow "
          "it as the parameter changes, detecting folds, Hopf and branch "
          "points.");
  if (ButtonW("Clear diagram", -1)) app_auto_reset(a);
}

static void points_table(App *a) {
  AutoUI *u = &a->au;
  int i, shown = 0;
  ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                       ImGuiTableFlags_BordersInnerV;
  igSeparatorText("Special points");
  if (u->count == 0) {
    TextWrapped("No diagram yet. Press 'Follow steady states'.");
    return;
  }
  if (!igBeginTable("pts", 4, tf, V2(0, igGetTextLineHeightWithSpacing() * 9), 0))
    return;
  igTableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 28, 0);
  igTableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 34, 0);
  igTableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0, 0);
  igTableSetupColumn("Stab.", ImGuiTableColumnFlags_WidthFixed, 40, 0);
  igTableSetupScrollFreeze(0, 1);
  igTableHeadersRow();
  for (i = 0; i < u->count; i++) {
    const xppe_auto_point_info *p = &u->info[i];
    char lab[16];
    if (!p->label || !p->type_name[0]) continue;
    shown++;
    igTableNextRow(0, 0);
    igTableSetColumnIndex(0);
    snprintf(lab, sizeof lab, "%d##p%d", p->label, i);
    if (igSelectable_Bool(lab, u->selected == i,
                          ImGuiSelectableFlags_SpanAllColumns, V2(0, 0)))
      u->selected = i;
    if (igIsItemHovered(ImGuiHoveredFlags_DelayShort)) {
      igSetTooltip("%s %s at %s = %.8g%s", describe(p),
                   p->periodic ? "(periodic orbit)" : "(steady state)",
                   a->eng->api.xppe_auto_parameter_name(p->par1_slot), p->par1,
                   p->periodic ? "" : "");
    }
    igTableSetColumnIndex(1);
    igTextUnformatted(p->type_name, NULL);
    igTableSetColumnIndex(2);
    igText("%.6g", p->par1);
    igTableSetColumnIndex(3);
    igTextColored(p->stable ? HEX(0x2e7d32) : HEX(0xc62828), "%s",
                  p->stable ? "yes" : "no");
  }
  igEndTable();
  if (!shown) igTextDisabled("No special points on this diagram.");
}

static void selected_actions(App *a) {
  AutoUI *u = &a->au;
  const char *labels[8], *keys[8];
  int i, n;
  const xppe_auto_point_info *p;
  if (u->selected < 0 || u->selected >= u->count) return;
  p = &u->info[u->selected];
  igSeparatorText("Continue from the selected point");
  igText("%s #%d", p->type_name[0] ? p->type_name : "point", p->label);
  igSameLine(0, -1);
  igTextDisabled("%s", describe(p));
  igText("%s = %.8g", a->eng->api.xppe_auto_parameter_name(p->par1_slot), p->par1);
  if (p->periodic) igText("period = %.6g", p->period);
  n = app_auto_actions(a, u->selected, labels, keys, 8);
  for (i = 0; i < n; i++) {
    igPushID_Int(i);
    if (ButtonW(labels[i], -1)) {
      int sel = u->selected;
      app_auto_continue(a, sel, keys[i]);
    }
    igPopID();
  }
}

void ui_auto(App *a) {
  if (!igBegin(WIN_AUTO, &a->show_auto, 0)) {
    igEnd();
    return;
  }
  if (!a->model_ok) {
    igTextDisabled("Open a model to compute its bifurcation diagram.");
    igEnd();
    return;
  }
  if (igBeginChild_Str("autoside", V2(270, 0), ImGuiChildFlags_ResizeX, 0)) {
    controls(a);
    points_table(a);
    selected_actions(a);
  }
  igEndChild();
  igSameLine(0, -1);
  if (igBeginChild_Str("autoplot", V2(0, 0), 0, 0)) {
    if (igBeginTabBar("autotabs", 0)) {
      if (igBeginTabItem("One parameter", NULL, 0)) {
        ui_export_button(a, FIG_AUTO);
        diagram_plot(a, false);
        igEndTabItem();
      }
      if (igBeginTabItem("Two parameters", NULL, 0)) {
        ui_export_button(a, FIG_AUTO2);
        diagram_plot(a, true);
        igEndTabItem();
      }
      igEndTabBar();
    }
  }
  igEndChild();
  igEnd();
}
