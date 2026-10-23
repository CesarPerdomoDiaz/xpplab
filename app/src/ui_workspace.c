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
  igTableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 95, 0);
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
    igEndTabBar();
  }
  igEnd();
}
