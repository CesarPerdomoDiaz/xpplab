/* Workspace: parameters, initial values, numerics and parameter sets. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

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
  if (Button("From end of last run")) app_command(a, "ic last");
  Tooltip("Start the next run where the last one ended (XPPAUT: I L).");
  igSameLine(0, -1);
  if (Button("From equilibrium") && a->neq > 0) {
    for (i = 0; i < a->nvars; i++) app_set_ic(a, i, a->eq[a->neq - 1].x[i]);
  }
  Tooltip("Use the last equilibrium found as the initial value.");
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
  if (s->result_mode != SWEEP_OVERLAY && s->n > 0 &&
      ImPlot_BeginPlot("##sweep", V2(-1, -1), ImPlotFlags_NoTitle | ImPlotFlags_NoLegend)) {
    const char *xl = s->over_ic ? a->vars[s->par] : a->pars[s->par];
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
        ImPlot_SetupAxes(s->eqt.names[0], a->vars[col - 1], ImPlotAxisFlags_AutoFit,
                         ImPlotAxisFlags_AutoFit);
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
      ImPlot_SetupAxes(xl, "Liapunov exponent", ImPlotAxisFlags_AutoFit,
                       ImPlotAxisFlags_AutoFit);
      ImPlot_PlotInfLines_doublePtr("##zero", &zero, 1,
                                    Spec(V4(0.5f, 0.5f, 0.5f, 0.6f), 1.0f));
      ImPlot_PlotLine_doublePtrdoublePtr("lyap", s->x, s->ymax, s->n,
                                         SpecMarker(HEX(0xd62728), ImPlotMarker_Circle, 2.5f));
    } else {
      ImPlot_SetupAxes(xl, a->vars[s->var], ImPlotAxisFlags_AutoFit,
                       ImPlotAxisFlags_AutoFit);
      ImPlot_PlotScatter_doublePtrdoublePtr("max", s->x, s->ymax, s->n,
                                            SpecMarker(HEX(0x1f77b4), ImPlotMarker_Circle, 2.5f));
      ImPlot_PlotScatter_doublePtrdoublePtr("min", s->x, s->ymin, s->n,
                                            SpecMarker(HEX(0xd62728), ImPlotMarker_Circle, 2.5f));
    }
    ImPlot_EndPlot();
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
    if (igBeginTabItem("Initial values", NULL, 0)) {
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
    a->sw.reveal = false;
    igEndTabBar();
  }
  igEnd();
}
