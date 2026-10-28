/* Workspace: parameters, initial values, numerics and parameter sets. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "pyconsole.h"
#include "ui.h"

static char filter[64];

static bool matches(const char *name) {
  return !filter[0] || SDL_strcasestr(name, filter) != NULL;
}

static void parameters_tab(App *a) {
  int i;
  ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                       ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
  igSetNextItemWidth(-1);
  igInputTextWithHint("##filter", "Filter parameters...", filter, sizeof filter,
                      0, NULL, NULL);
  if (!igBeginTable("params", 3, tf, V2(0, 0), 0)) return;
  igTableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 80, 0);
  igTableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 76, 0);
  igTableSetupColumn("Slider", ImGuiTableColumnFlags_WidthStretch, 0, 0);
  igTableHeadersRow();
  for (i = 0; i < a->npars; i++) {
    ParamUI *p = &a->par[i];
    double v = p->value;
    if (!matches(a->pars[i])) continue;
    igPushID_Int(i);
    igTableNextRow(0, 0);
    igTableSetColumnIndex(0);
    igAlignTextToFramePadding();
    igTextUnformatted(a->pars[i], NULL);
    igTableSetColumnIndex(1);
    igSetNextItemWidth(-1);
    if (igInputDouble("##v", &v, 0, 0, "%.6g",
                      ImGuiInputTextFlags_EnterReturnsTrue)) {
      app_set_param(a, i, v);
      if (v < p->lo) p->lo = v;
      if (v > p->hi) p->hi = v;
    }
    igTableSetColumnIndex(2);
    igSetNextItemWidth(-28);
    if (igSliderScalar("##s", ImGuiDataType_Double, &v, &p->lo, &p->hi, "%.4g",
                       0))
      app_set_param(a, i, v);
    igSameLine(0, 2);
    if (igSmallButton("..")) igOpenPopup_Str("range", 0);
    Tooltip("Change the slider range.");
    if (igBeginPopup("range", 0)) {
      igText("Slider range for %s", a->pars[i]);
      igSetNextItemWidth(120);
      igInputDouble("from", &p->lo, 0, 0, "%.6g", 0);
      igSetNextItemWidth(120);
      igInputDouble("to", &p->hi, 0, 0, "%.6g", 0);
      if (Button("Around the value")) {
        double w = fabs(p->value) > 0 ? fabs(p->value) : 1;
        p->lo = p->value - w;
        p->hi = p->value + w;
      }
      igEndPopup();
    }
    igPopID();
  }
  igEndTable();
}

static void initial_tab(App *a) {
  int i;
  ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                       ImGuiTableFlags_ScrollY;
  static char target[128], formula[512];
  bool go;
  if (Button("From end of last run")) app_command(a, "ic last");
  Tooltip("Start the next run where the last one ended (XPPAUT: I L).");
  igSameLine(0, -1);
  if (Button("From equilibrium") && a->neq > 0) {
    for (i = 0; i < a->nvars; i++) app_set_ic(a, i, a->eq[a->neq - 1].x[i]);
  }
  Tooltip("Use the last equilibrium found as the initial value.");
  if (Button("From end, time goes on")) app_command(a, "ic shift");
  Tooltip("Start where the last run ended, and let the time go on from there: "
          "the start time T0 becomes the time the run ended (XPPAUT's Initial "
          "conds > Shift, I S). It matters for models that depend on t, such "
          "as a periodic forcing.");
  if (Button("From file...")) ui_dialog_ics(a, false);
  Tooltip("Read the initial values from a text file: one number per variable, "
          "in the order of the table (XPPAUT's Initial conds > File, I F).");
  igSameLine(0, -1);
  if (Button("Save to file...")) ui_dialog_ics(a, true);
  Tooltip("Write these initial values to a text file, one per line, exactly.");
  igSeparatorText("From a formula");
  igSetNextItemWidth(90);
  igInputTextWithHint("##target", "V, u[1..9]", target, sizeof target, 0, NULL, NULL);
  igSameLine(0, 4);
  igTextUnformatted("=", NULL);
  igSameLine(0, 4);
  igSetNextItemWidth(-50);
  go = igInputTextWithHint("##formula", "vk + 0.1", formula, sizeof formula,
                           ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL);
  igSameLine(0, -1);
  go = Button("Set") || go;
  if (go && target[0] && formula[0]) {
    char cmd[700];
    snprintf(cmd, sizeof cmd, "ic %s = %s", target, formula);
    app_command(a, cmd);
  }
  Tooltip("XPPAUT's Initial conds > formUla: a variable, or numbered variables "
          "such as u[1..50], take the value of a formula in the parameters and "
          "the model's functions. In a range, [j] is the index: u[1..50] = "
          "exp(-([j]-25)^2/20) is a bump around u25.");
  if (!igBeginTable("ics", 2, tf, V2(0, 0), 0)) return;
  igTableSetupColumn("Variable", ImGuiTableColumnFlags_WidthFixed, 90, 0);
  igTableSetupColumn("Initial value", ImGuiTableColumnFlags_WidthStretch, 0, 0);
  igTableHeadersRow();
  for (i = 0; i < a->nvars; i++) {
    double v = a->ic[i];
    igPushID_Int(i);
    igTableNextRow(0, 0);
    igTableSetColumnIndex(0);
    igAlignTextToFramePadding();
    igTextUnformatted(a->vars[i], NULL);
    igTableSetColumnIndex(1);
    igSetNextItemWidth(-1);
    if (igInputDouble("##ic", &v, 0, 0, "%.8g",
                      ImGuiInputTextFlags_EnterReturnsTrue))
      app_set_ic(a, i, v);
    igPopID();
  }
  igEndTable();
}

static void option_double(App *a, const char *label, const char *name,
                          double value, const char *tip) {
  double v = value;
  igSetNextItemWidth(110);
  if (igInputDouble(label, &v, 0, 0, "%.6g",
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
    char s[64];
    snprintf(s, sizeof s, "%.17g", v);
    app_set_option(a, name, s);
  }
  if (tip) HelpMarker(tip);
}

static void numerics_tab(App *a) {
  int nout = a->nout;
  igSeparatorText("Time");
  option_double(a, "Total", "TOTAL", a->total, "How long to integrate (TOTAL).");
  option_double(a, "Start time", "T0", a->t0, "The initial time (T0).");
  option_double(a, "Transient", "TRANS", a->trans,
                "Discard output before this time, e.g. to skip transients "
                "(TRANS).");
  option_double(a, "Step dt", "DT", a->dt,
                "The time step. Fixed-step methods use it directly; adaptive "
                "methods use it as the output interval (DT).");
  igSetNextItemWidth(110);
  if (igInputInt("Output every", &nout, 1, 10,
                 ImGuiInputTextFlags_EnterReturnsTrue)) {
    char s[32];
    snprintf(s, sizeof s, "%d", nout < 1 ? 1 : nout);
    app_set_option(a, "NOUT", s);
  }
  HelpMarker("Store one point every N steps (NOUT).");
  igSeparatorText("Method");
  igSetNextItemWidth(160);
  if (igBeginCombo("Integrator", app_method_name(a->meth), 0)) {
    int i;
    for (i = 0; i < 15; i++)
      if (igSelectable_Bool(app_method_name(i), i == a->meth, 0, V2(0, 0)))
        app_set_option(a, "METH", app_method_name(i));
    igEndCombo();
  }
  HelpMarker("rungekutta: fixed-step RK4, a good default.\n"
             "5dp / 83dp: adaptive Dormand-Prince, accurate.\n"
             "stiff / cvode / 2rb: for stiff systems.\n"
             "discrete: the equations are a map x(n+1) = f(x(n)).\n"
             "See Help > Numerical methods.");
  option_double(a, "Rel. tolerance", "TOL", a->tol,
                "Relative error tolerance of adaptive methods (TOL).");
  option_double(a, "Abs. tolerance", "ATOL", a->atol,
                "Absolute error tolerance of adaptive methods (ATOL).");
  option_double(a, "Bound", "BOUND", a->bound,
                "Stop if any variable grows beyond this (BOUND).");

  igSeparatorText("Poincare map");
  {
    static const char *modes[] = {"Off", "Section", "Max / min", "Period"};
    static const char *keys[] = {"0", "section", "max", "period"};
    int i, m = a->poimap >= 0 && a->poimap < 4 ? a->poimap : 0;
    igSetNextItemWidth(160);
    if (igBeginCombo("Mode", modes[m], 0)) {
      for (i = 0; i < 4; i++)
        if (igSelectable_Bool(modes[i], i == m, 0, V2(0, 0)))
          app_set_option(a, "POIMAP", keys[i]);
      igEndCombo();
    }
    HelpMarker("Section: store only the points where a variable crosses a "
               "plane.\nMax / min: store the extrema of a variable.\nPeriod: "
               "store the times between crossings.");
    if (m != 0) {
      const char *vname = a->poivar <= 0 ? "t" : a->poivar - 1 < a->nvars
                                                     ? a->vars[a->poivar - 1]
                                                     : "?";
      igSetNextItemWidth(160);
      if (igBeginCombo("Variable", vname, 0)) {
        for (i = 0; i < a->nvars; i++)
          if (igSelectable_Bool(a->vars[i], a->poivar == i + 1, 0, V2(0, 0)))
            app_set_option(a, "POIVAR", a->vars[i]);
        igEndCombo();
      }
      option_double(a, "Plane", "POIPLN", a->poipln,
                    "Record a point each time the variable crosses this "
                    "value (POIPLN).");
      {
        static const char *dirs[] = {"Both ways", "Increasing", "Decreasing"};
        static const char *dkeys[] = {"0", "1", "-1"};
        int d = a->poisgn == 1 ? 1 : a->poisgn == -1 ? 2 : 0;
        igSetNextItemWidth(160);
        if (igBeginCombo("Direction", dirs[d], 0)) {
          for (i = 0; i < 3; i++)
            if (igSelectable_Bool(dirs[i], i == d, 0, V2(0, 0)))
              app_set_option(a, "POISGN", dkeys[i]);
          igEndCombo();
        }
      }
      {
        bool stop = a->poistop != 0;
        if (igCheckbox("Stop at first crossing", &stop))
          app_set_option(a, "POISTOP", stop ? "1" : "0");
      }
    }
  }
}

static void sets_tab(App *a) {
  int i;
  if (a->nsets == 0) {
    TextWrapped("This model defines no parameter sets. A line like\n"
                "  set fast {eps=0.5, total=50}\n"
                "in the .ode file adds one.");
    return;
  }
  TextWrapped("Named parameter sets from the model file:");
  for (i = 0; i < a->nsets; i++) {
    igPushID_Int(i);
    if (ButtonW(a->sets[i], -1)) app_apply_set(a, a->sets[i]);
    igPopID();
  }
}

/* Blue below zero, white at zero, red above: ImPlot's RdBu the other way
   round, so that chaotic runs (a positive Liapunov exponent) show red. */
static ImPlotColormap blue_red(void) {
#define RGB(r, g, b) (0xFF000000u | (ImU32)(b) << 16 | (ImU32)(g) << 8 | (ImU32)(r))
  static const ImU32 cols[] = {
      RGB(5, 48, 97),     RGB(33, 102, 172),  RGB(67, 147, 195), RGB(146, 197, 222),
      RGB(209, 229, 240), RGB(247, 247, 247), RGB(253, 219, 199), RGB(244, 165, 130),
      RGB(214, 96, 77),   RGB(178, 24, 43),   RGB(103, 0, 31)};
#undef RGB
  ImPlotColormap c = ImPlot_GetColormapIndex("BlueRed");
  return c >= 0 ? c : ImPlot_AddColormap_U32Ptr("BlueRed", cols, 11, false);
}

/* A grid of measures as a map: the first value across, the second up, a
   colour for the measure (max - min, max or min of the variable, or the
   Liapunov exponent). */
static void grid_map(App *a) {
  SweepUI *s = &a->sw;
  const bool lyap = s->result_mode == SWEEP_LYAPUNOV;
  int n1 = s->n1, n2 = s->n2, i, j;
  double *z, lo = INFINITY, hi = -INFINITY, dx, dy;
  float h;
  ImPlotPoint_c b0, b1;
  if (!lyap) {
    static const char *shows[] = {"max - min", "max", "min"};
    igSetNextItemWidth(130);
    igCombo_Str_arr("Show", &s->show, shows, 3, -1);
  }
  z = malloc(sizeof(double) * (size_t)(n1 * n2));
  if (!z) return;
  /* Row 0 of a heatmap is drawn at the top: the second value's last row. */
  for (j = 0; j < n2; j++)
    for (i = 0; i < n1; i++) {
      int k = j * n1 + i;
      double v = lyap || s->show == 1 ? s->ymax[k] : s->show == 2 ? s->ymin[k]
                                                                : s->ymax[k] - s->ymin[k];
      z[(n2 - 1 - j) * n1 + i] = v;
      if (isfinite(v)) {
        lo = fmin(lo, v);
        hi = fmax(hi, v);
      }
    }
  if (lyap && isfinite(lo)) { /* zero in the middle, white */
    hi = fmax(fabs(lo), fabs(hi));
    lo = -hi;
  }
  if (!(hi > lo)) {
    lo = isfinite(lo) ? lo - 0.5 : 0;
    hi = lo + 1;
  }
  dx = n1 > 1 ? (s->x[n1 - 1] - s->x[0]) / (n1 - 1) : 1;
  dy = n2 > 1 ? (s->x2[(n2 - 1) * n1] - s->x2[0]) / (n2 - 1) : 1;
  b0.x = s->x[0] - dx / 2;
  b0.y = s->x2[0] - dy / 2;
  b1.x = s->x[n1 - 1] + dx / 2;
  b1.y = s->x2[(n2 - 1) * n1] + dy / 2;
  /* At least a readable square, the tab scrolling if the panel is short. */
  h = igGetContentRegionAvail().y;
  if (h < 300) h = 300;
  ImPlot_PushColormap_PlotColormap(lyap ? blue_red() : ImPlotColormap_Viridis);
  if (ImPlot_BeginPlot("##grid", V2(-70, h), ImPlotFlags_NoTitle | ImPlotFlags_NoLegend)) {
    static PlotView view;
    const char *xl = a->echo.sweep_name;
    const char *yl = a->echo.sweep_name2[0] ? a->echo.sweep_name2 : "";
    ImPlotAxisFlags fit = ui_view_follow(a, &view, xl, yl) ? ImPlotAxisFlags_AutoFit : 0;
    ImPlot_SetupAxes(xl, yl, fit, fit);
    ImPlot_PlotHeatmap_doublePtr("##map", z, n2, n1, lo, hi, NULL, b0, b1,
                                 Spec(V4(0, 0, 0, -1), 1.0f));
    ui_view_end(&view, true);
    ImPlot_EndPlot();
  }
  igSameLine(0, -1);
  ImPlot_ColormapScale(lyap ? "Liapunov" : s->show == 1 ? "max" : s->show == 2 ? "min"
                                                                             : "max - min",
                       lo, hi, V2(60, h), "%.3g", 0, -1);
  ImPlot_PopColormap(1);
  free(z);
}

static void sweep_tab(App *a) {
  SweepUI *s = &a->sw;
  char label[APP_NAME_LEN + 16];
  int i;
  TextWrapped("Run the model once for each value of a parameter or initial "
              "value, as XPPAUT's Initial conds > Range does.");
  snprintf(label, sizeof label, "%s%s",
           s->over_ic ? (s->par < a->nvars ? a->vars[s->par] : "")
                      : (s->par < a->npars ? a->pars[s->par] : ""),
           s->over_ic ? " (initial)" : "");
  igSetNextItemWidth(140);
  if (igBeginCombo("Range over", label, 0)) {
    for (i = 0; i < a->npars; i++)
      if (igSelectable_Bool(a->pars[i], !s->over_ic && s->par == i, 0, V2(0, 0))) {
        s->over_ic = false;
        s->par = i;
        s->from = a->par[i].lo;
        s->to = a->par[i].hi;
      }
    igSeparator();
    for (i = 0; i < a->nvars; i++) {
      snprintf(label, sizeof label, "%s (initial)", a->vars[i]);
      if (igSelectable_Bool(label, s->over_ic && s->par == i, 0, V2(0, 0))) {
        s->over_ic = true;
        s->par = i;
        s->from = a->ic[i] - 1;
        s->to = a->ic[i] + 1;
      }
    }
    igEndCombo();
  }
  igSetNextItemWidth(110);
  igInputDouble("From", &s->from, 0, 0, "%.6g", 0);
  igSetNextItemWidth(110);
  igInputDouble("To", &s->to, 0, 0, "%.6g", 0);
  igSetNextItemWidth(110);
  igInputInt("Steps", &s->steps, 1, 10, 0);
  if (s->steps < 2) s->steps = 2;
  if (s->steps > 1000) s->steps = 1000;
  igCheckbox("Vary a second value too", &s->two);
  Tooltip("XPPAUT's 2par range: both values on a grid, every pair run once, "
          "or both together along a line.");
  if (s->two) {
    char label2[APP_NAME_LEN + 16];
    snprintf(label2, sizeof label2, "%s%s",
             s->over_ic2 ? (s->par2 < a->nvars ? a->vars[s->par2] : "")
                         : (s->par2 < a->npars ? a->pars[s->par2] : ""),
             s->over_ic2 ? " (initial)" : "");
    igSetNextItemWidth(140);
    if (igBeginCombo("And over", label2, 0)) {
      for (i = 0; i < a->npars; i++)
        if (igSelectable_Bool(a->pars[i], !s->over_ic2 && s->par2 == i, 0, V2(0, 0))) {
          s->over_ic2 = false;
          s->par2 = i;
          s->from2 = a->par[i].lo;
          s->to2 = a->par[i].hi;
        }
      igSeparator();
      for (i = 0; i < a->nvars; i++) {
        snprintf(label2, sizeof label2, "%s (initial)", a->vars[i]);
        if (igSelectable_Bool(label2, s->over_ic2 && s->par2 == i, 0, V2(0, 0))) {
          s->over_ic2 = true;
          s->par2 = i;
          s->from2 = a->ic[i] - 1;
          s->to2 = a->ic[i] + 1;
        }
      }
      igEndCombo();
    }
    igSetNextItemWidth(110);
    igInputDouble("From##2", &s->from2, 0, 0, "%.6g", 0);
    igSetNextItemWidth(110);
    igInputDouble("To##2", &s->to2, 0, 0, "%.6g", 0);
    if (igRadioButton_Bool("On a grid", s->grid)) s->grid = true;
    igSameLine(0, -1);
    if (igRadioButton_Bool("Together, along a line", !s->grid)) s->grid = false;
    if (s->grid) {
      if (s->steps2 < 2) s->steps2 = 10;
      igSetNextItemWidth(110);
      igInputInt("Steps##2", &s->steps2, 1, 10, 0);
      if (s->steps2 < 2) s->steps2 = 2;
      if (s->steps2 > 400) s->steps2 = 400;
      igTextDisabled("%d runs", s->steps * s->steps2);
    }
    if (s->mode == SWEEP_EQUILIBRIA)
      igTextColored(HEX(0xc96f00), "Equilibria follow the first value only.");
  }
  igCheckbox("Start each run where the last ended", &s->continue_runs);
  Tooltip("XPPAUT's \"Use old ICs\" = No: follow an attractor as the value "
          "changes, instead of starting every run from the initial values.");
  if (igRadioButton_Bool("Keep each trajectory", s->mode == SWEEP_OVERLAY))
    s->mode = SWEEP_OVERLAY;
  Tooltip("Overlay the runs on the plots (at most 16).");
  if (igRadioButton_Bool("Max / min of a variable", s->mode == SWEEP_MAXMIN))
    s->mode = SWEEP_MAXMIN;
  Tooltip("Record the largest and smallest value after a transient: a "
          "brute-force bifurcation diagram. Compare it with AUTO's.");
  if (igRadioButton_Bool("Liapunov exponent", s->mode == SWEEP_LYAPUNOV))
    s->mode = SWEEP_LYAPUNOV;
  Tooltip("The largest Liapunov exponent of each run: above zero means chaos "
          "(XPPAUT's Liapunov with a range).");
  if (igRadioButton_Bool("Equilibria (no runs)", s->mode == SWEEP_EQUILIBRIA))
    s->mode = SWEEP_EQUILIBRIA;
  Tooltip("Follow the equilibrium as the parameter changes, each search "
          "starting from the last one found (XPPAUT's Sing pts > Range): "
          "steady states and their stability without AUTO. Red is stable, "
          "black unstable.");
  if (s->mode == SWEEP_EQUILIBRIA && s->over_ic)
    igTextColored(HEX(0xe53935), "Choose a parameter: equilibria do not depend "
                                 "on initial values.");
  if (s->mode == SWEEP_EQUILIBRIA) {
    igSetNextItemWidth(110);
    Combo("Variable", &s->var, a->vars, a->nvars);
  }
  if (s->mode == SWEEP_MAXMIN) {
    int pct = (int)(s->transient * 100 + 0.5);
    igSetNextItemWidth(110);
    Combo("Variable", &s->var, a->vars, a->nvars);
    igSetNextItemWidth(110);
    if (igSliderInt("Skip first", &pct, 0, 95, "%d%%", 0)) s->transient = pct / 100.0;
    HelpMarker("The part of each run to ignore, so that transients do not "
               "count.");
  }
  if (ButtonW("Run range", -1)) app_sweep(a);
  if (s->mode != SWEEP_EQUILIBRIA &&
      ButtonW(a->ks.film.on ? "Filming..." : "Film it in the Kinescope", -1) &&
      !a->ks.film.on)
    app_command(a, "kinescope film");
  Tooltip("XPPAUT's Range with Movie: run each value in turn, each run drawn "
          "alone, and capture the Kinescope's figure (the Phase Plane unless "
          "you choose another there) after each. Keep that figure in view.");
  if (s->result_kind == 2 && s->n == s->n1 * s->n2 && s->result_mode != SWEEP_OVERLAY) {
    grid_map(a);
    return;
  }
  if (s->result_mode != SWEEP_OVERLAY && s->n > 0 &&
      ImPlot_BeginPlot("##sweep", V2(-1, -1), ImPlotFlags_NoTitle | ImPlotFlags_NoLegend)) {
    static PlotView view;
    const char *xl = s->over_ic ? a->vars[s->par] : a->pars[s->par];
    ImPlotAxisFlags fit;
    if (s->result_mode == SWEEP_EQUILIBRIA && s->eqt.ncols > a->nvars + 1) {
      /* Recolour by stability; the variable shown can change afterwards. */
      int col = s->var >= 0 && s->var < a->nvars ? s->var + 1 : 1, k, ns = 0, nu = 0;
      double *sx = malloc(sizeof(double) * (size_t)(2 * s->n + 2)), *sy, *ux, *uy;
      if (sx) {
        sy = sx + s->n + 1;
        ux = malloc(sizeof(double) * (size_t)(2 * s->n + 2));
        uy = ux ? ux + s->n + 1 : NULL;
        for (k = 0; ux && k < s->n; k++) {
          double u = s->eqt.cols[a->nvars + 1][k];
          if (isnan(u)) continue;
          if (u > 0) {
            ux[nu] = s->eqt.cols[0][k];
            uy[nu++] = s->eqt.cols[col][k];
          } else {
            sx[ns] = s->eqt.cols[0][k];
            sy[ns++] = s->eqt.cols[col][k];
          }
        }
        fit = ui_view_follow(a, &view, s->eqt.names[0], a->vars[col - 1])
                  ? ImPlotAxisFlags_AutoFit : 0;
        ImPlot_SetupAxes(s->eqt.names[0], a->vars[col - 1], fit, fit);
        ImPlot_PlotScatter_doublePtrdoublePtr("stable", sx, sy, ns,
                                              SpecMarker(COL_STABLE_EQ, ImPlotMarker_Circle, 3.0f));
        if (uy)
          ImPlot_PlotScatter_doublePtrdoublePtr("unstable", ux, uy, nu,
                                                SpecMarker(COL_UNSTABLE_EQ, ImPlotMarker_Circle, 3.0f));
        free(ux);
        free(sx);
      }
    } else if (s->result_mode == SWEEP_LYAPUNOV) {
      double zero = 0;
      ImPlotSpec_c line = Spec(V4(0.5f, 0.5f, 0.5f, 0.6f), 1.0f);
      line.Flags = ImPlotInfLinesFlags_Horizontal;
      fit = ui_view_follow(a, &view, xl, "Liapunov exponent") ? ImPlotAxisFlags_AutoFit : 0;
      ImPlot_SetupAxes(xl, "Liapunov exponent", fit, fit);
      ImPlot_PlotInfLines_doublePtr("##zero", &zero, 1, line);
      ImPlot_PlotLine_doublePtrdoublePtr("lyap", s->x, s->ymax, s->n,
                                         SpecMarker(HEX(0xd62728), ImPlotMarker_Circle, 2.5f));
    } else {
      fit = ui_view_follow(a, &view, xl, a->vars[s->var]) ? ImPlotAxisFlags_AutoFit : 0;
      ImPlot_SetupAxes(xl, a->vars[s->var], fit, fit);
      ImPlot_PlotScatter_doublePtrdoublePtr("max", s->x, s->ymax, s->n,
                                            SpecMarker(HEX(0x1f77b4), ImPlotMarker_Circle, 2.5f));
      ImPlot_PlotScatter_doublePtrdoublePtr("min", s->x, s->ymin, s->n,
                                            SpecMarker(HEX(0xd62728), ImPlotMarker_Circle, 2.5f));
    }
    ui_view_end(&view, true);
    ImPlot_EndPlot();
  }
}

/* The console's variables: what the Python in the command window holds. */
static void python_tab(App *a) {
  const PyVar *v;
  int n, i;
  ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                       ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
  if (!pyconsole_running()) {
    TextWrapped("Python is not running. Click >> in the command window (it becomes "
                "py>), or type py, and type Python there: m.run() runs the model "
                "here, and the variables you make are listed in this tab.");
    if (Button("Start Python")) app_command(a, "py");
    return;
  }
  v = pyconsole_vars(&n);
  if (!v) pyconsole_request_vars(a); /* asked again after each line runs */
  igTextDisabled("%s", pyconsole_python());
  if (!igBeginTable("pyvars", 3, tf, V2(0, 0), 0)) return;
  igTableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 80, 0);
  igTableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90, 0);
  igTableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0, 0);
  igTableHeadersRow();
  for (i = 0; v && i < n; i++) {
    igTableNextRow(0, 0);
    igTableSetColumnIndex(0);
    igTextUnformatted(v[i].name, NULL);
    igTableSetColumnIndex(1);
    igTextDisabled("%s", v[i].type);
    igTableSetColumnIndex(2);
    igTextUnformatted(v[i].summary, NULL);
    if (igIsItemHovered(0)) igSetTooltip("%s", v[i].summary);
  }
  igEndTable();
}

/* Many runs: initial values drawn in a box, or shooting from an
   equilibrium along its eigenvectors; each run keeps the global flag that
   stopped it (app_figure.c). */
static void many_tab(App *a) {
  ManyUI *m = &a->mu;
  RunSet *r = &a->many;
  int i;
  static const char *dirs[] = {"unstable (forward in time)", "stable (backward)", "both"};
  if (!m->use) return;
  TextWrapped("Run from many initial values at once and see how each run "
              "ends. A model's global flags with {arret=1} stop runs (the "
              "ground, a slack tether, ...); each ending gets its colour.");
  igSeparatorText("Random initial values");
  igSetNextItemWidth(90);
  igInputInt("runs", &m->n, 10, 100, 0);
  if (m->n < 1) m->n = 1;
  if (m->n > 5000) m->n = 5000;
  igSameLine(0, -1);
  igSetNextItemWidth(90);
  igInputInt("seed", &m->seed, 1, 10, 0);
  Tooltip("The same seed gives the same initial values, here and in Python "
          "(Model.sample_initial).");
  for (i = 0; i < a->nvars; i++) {
    igPushID_Int(i);
    igCheckbox(a->vars[i], &m->use[i]);
    Tooltip("Draw this variable uniformly between the two values; unticked, "
            "it keeps its initial value.");
    igSameLine(110, -1);
    igBeginDisabled(!m->use[i]);
    igSetNextItemWidth(80);
    igInputDouble("##lo", &m->lo[i], 0, 0, "%.6g", 0);
    igSameLine(0, -1);
    igSetNextItemWidth(80);
    igInputDouble("##hi", &m->hi[i], 0, 0, "%.6g", 0);
    igEndDisabled();
    igPopID();
  }
  {
    static char cols[257][APP_NAME_LEN];
    int nc = 1, k = m->req_col + 1;
    snprintf(cols[0], APP_NAME_LEN, "(no condition)");
    for (i = 1; i < a->ncols && nc < 257; i++) snprintf(cols[nc++], APP_NAME_LEN, "%s", a->cols[i]);
    igSetNextItemWidth(110);
    if (Combo("keep where", &k, cols, nc)) m->req_col = k - 1 > 0 ? k : -1;
    Tooltip("Keep only the draws where this column (a variable or an aux "
            "quantity, such as a tension) lies between the two values.");
    if (m->req_col > 0) {
      igSetNextItemWidth(80);
      igInputDouble("from##req", &m->req_lo, 0, 0, "%.6g", 0);
      igSameLine(0, -1);
      igSetNextItemWidth(80);
      igInputDouble("to##req", &m->req_hi, 0, 0, "%.6g", 0);
      Tooltip("inf is allowed.");
    }
  }
  if (Button("Run them")) app_many_runs(a);
  igSeparatorText("Shoot from an equilibrium");
  TextWrapped("From the equilibrium Newton's method finds near the initial "
              "values, along each real eigenvector, both ways: the start of "
              "its invariant manifolds, a node's too (XPPAUT's sHoot).");
  igSetNextItemWidth(90);
  igInputDouble("step off", &m->eps, 0, 0, "%.3g", 0);
  igSameLine(0, -1);
  igSetNextItemWidth(90);
  igInputDouble("time", &m->shoot_total, 0, 0, "%.6g", 0);
  Tooltip("The time of each run; 0 keeps the model's total time.");
  igSetNextItemWidth(-1);
  igCombo_Str_arr("##dirs", &m->dirs, dirs, 3, 3);
  if (Button("Shoot")) app_shoot(a);
  if (r->n > 0) {
    igSeparator();
    igText("%s.", r->what);
    igCheckbox("Show on the plots", &r->show);
    igSameLine(0, -1);
    if (Button("Clear")) app_many_free(a);
  }
}

void ui_workspace(App *a) {
  if (!igBegin(WIN_WORKSPACE, &a->show_workspace, 0)) {
    igEnd();
    return;
  }
  if (!a->model_ok) {
    igTextDisabled("No model loaded.");
    igEnd();
    return;
  }
  if (igBeginTabBar("ws", 0)) {
    if (igBeginTabItem("Parameters", NULL, 0)) {
      parameters_tab(a);
      igEndTabItem();
    }
    if (igBeginTabItem("Initial values", NULL,
                       a->reveal_ics ? ImGuiTabItemFlags_SetSelected : 0)) {
      initial_tab(a);
      igEndTabItem();
    }
    if (igBeginTabItem("Numerics", NULL, 0)) {
      numerics_tab(a);
      igEndTabItem();
    }
    if (igBeginTabItem("Sets", NULL, 0)) {
      sets_tab(a);
      igEndTabItem();
    }
    if (igBeginTabItem("Range", NULL, a->sw.reveal ? ImGuiTabItemFlags_SetSelected : 0)) {
      sweep_tab(a);
      igEndTabItem();
    }
    if (igBeginTabItem("Many runs", NULL, 0)) {
      many_tab(a);
      igEndTabItem();
    }
    if (igBeginTabItem("Python", NULL, a->reveal_python ? ImGuiTabItemFlags_SetSelected : 0)) {
      python_tab(a);
      igEndTabItem();
    }
    a->sw.reveal = false;
    a->reveal_ics = false;
    a->reveal_python = false;
    igEndTabBar();
  }
  igEnd();
}
