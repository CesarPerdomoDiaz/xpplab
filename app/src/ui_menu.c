/* Main menu, toolbar and status bar. */
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "ui.h"

static void menu_file(App *a) {
  int i;
  if (igMenuItem_Bool("New model", MOD "N", false, true)) app_new_model(a);
  if (igMenuItem_Bool("Open...", MOD "O", false, true)) ui_dialog_open(a);
  if (igBeginMenu("Open recent", a->nrecent > 0)) {
    for (i = 0; i < a->nrecent; i++)
      if (igMenuItem_Bool(a->recent[i], NULL, false, true)) {
        char p[1024];
        snprintf(p, sizeof p, "%s", a->recent[i]);
        app_open(a, p);
        break;
      }
    igEndMenu();
  }
  igSeparator();
  if (igMenuItem_Bool("Save", MOD "S", false, a->model_ok || a->editor[0]))
    app_apply_editor(a);
  if (igMenuItem_Bool("Save as...", NULL, false, a->editor[0] != 0))
    ui_dialog_save_as(a);
  igSeparator();
  if (igMenuItem_Bool("Export data as CSV...", NULL, false, a->have_traj))
    ui_dialog_export_csv(a);
  if (igMenuItem_Bool("Save session as Python script...", NULL, false,
                      a->echo.len > 0))
    ui_dialog_save_script(a);
  if (igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    igSetTooltip("A script that repeats what you have done, number for number, "
                 "with Python and the xpplab package (pip install xpplab).");
  igSeparator();
  if (igMenuItem_Bool("Quit", MOD "Q  /  F Q", false, true)) a->quit = true;
}

static void menu_run(App *a) {
  bool ok = a->model_ok;
  if (igMenuItem_Bool("Integrate", "F5  /  I G", false, ok)) app_run(a);
  if (igMenuItem_Bool("Find equilibrium", "S G", false, ok)) app_command(a, "eq");
  if (igMenuItem_Bool("Find all equilibria", NULL, false, ok)) app_command(a, "eq all");
  if (igIsItemHovered(0))
    igSetTooltip("Every equilibrium in the phase plane's window, from random "
                 "starting points (XPPAUT's Sing pts > Monte Carlo).");
  if (igMenuItem_Bool("Equilibria over a parameter...", NULL, false, ok)) {
    a->sw.mode = SWEEP_EQUILIBRIA;
    a->sw.reveal = true;
    a->show_workspace = true;
  }
  if (igMenuItem_Bool("Nullclines", "N N", a->pp.nullclines, ok))
    app_command(a, a->pp.nullclines ? "nullclines off" : "nullclines");
  if (igMenuItem_Bool("Direction field", "D D", a->pp.dirfield, ok))
    app_command(a, "dirfield");
  igSeparator();
  igMenuItem_BoolPtr("Rerun after every change (live)", NULL, &a->live, true);
  igMenuItem_BoolPtr("Keep previous curves", NULL, &a->keep_curves, true);
  if (igMenuItem_Bool("Erase curves", "E", false, a->nfrozen > 0))
    app_command(a, "erase");
  igSeparator();
  if (igMenuItem_Bool("Bifurcation diagram (AUTO)...", NULL, false, ok)) {
    a->show_auto = true;
    igSetWindowFocus_Str(WIN_AUTO);
  }
  if (igMenuItem_Bool("Boundary-value problem", NULL, false, ok && a->nbcs > 0))
    app_command(a, "bvp");
  if (igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    igSetTooltip(a->nbcs > 0 ? "Solve the model's boundary conditions by "
                               "shooting (XPPAUT's Bndryval)."
                             : "The model has no boundary conditions (bndry "
                               "statements).");
  if (igMenuItem_Bool("Range...", NULL, false, ok)) {
    a->sw.reveal = true;
    a->show_workspace = true;
  }
  if (igMenuItem_Bool("Animator...", NULL, false, ok)) {
    a->show_ani = true;
    igSetWindowFocus_Str(WIN_ANI);
  }
  if (igMenuItem_Bool("Kinescope...", NULL, false, true)) {
    a->show_kine = true;
    igSetWindowFocus_Str(WIN_KINE);
  }
  if (igMenuItem_Bool("3D view", NULL, false, ok && a->ncols > 3)) {
    a->show_view3d = true;
    igSetWindowFocus_Str(WIN_VIEW3D);
  }
  if (igBeginMenu("Analyse the run", ok)) {
    int t;
    for (t = 0; t < AN_COUNT; t++)
      if (igMenuItem_Bool(app_analysis_name(t), NULL, a->an.tool == t, true)) {
        a->an.tool = t;
        a->show_analysis = true;
        igSetWindowFocus_Str(WIN_ANALYSIS);
      }
    igEndMenu();
  }
}

static void menu_view(App *a) {
  igMenuItem_BoolPtr(WIN_BROWSER, NULL, &a->show_browser, true);
  igMenuItem_BoolPtr(WIN_WORKSPACE, NULL, &a->show_workspace, true);
  igMenuItem_BoolPtr(WIN_EDITOR, NULL, &a->show_editor, true);
  igMenuItem_BoolPtr(WIN_TIME, NULL, &a->show_time, true);
  igMenuItem_BoolPtr(WIN_PHASE, NULL, &a->show_phase, true);
  igMenuItem_BoolPtr(WIN_AUTO, NULL, &a->show_auto, true);
  igMenuItem_BoolPtr(WIN_CONSOLE, NULL, &a->show_console, true);
  igMenuItem_BoolPtr(WIN_DATA, NULL, &a->show_data, true);
  igMenuItem_BoolPtr(WIN_ARRAY, NULL, &a->show_array, true);
  igMenuItem_BoolPtr(WIN_ANALYSIS, NULL, &a->show_analysis, true);
  igMenuItem_BoolPtr(WIN_VIEW3D, NULL, &a->show_view3d, true);
  igMenuItem_BoolPtr(WIN_ANI, NULL, &a->show_ani, true);
  igMenuItem_BoolPtr(WIN_KINE, NULL, &a->show_kine, true);
  igMenuItem_BoolPtr(WIN_HELP, "F1", &a->show_help, true);
  igSeparator();
  if (igMenuItem_Bool("Reset window layout", NULL, false, true))
    a->reset_layout = true;
  if (igMenuItem_Bool("Phase plane beside time series", NULL, a->layout == 1, true)) {
    a->layout = !a->layout;
    a->reset_layout = true;
  }
  if (igMenuItem_BoolPtr("Dark theme", NULL, &a->dark, true)) ui_apply_theme(a);
  if (igBeginMenu("Text size", true)) {
    static const float sizes[] = {0.85f, 1.0f, 1.15f, 1.3f, 1.5f, 1.75f, 2.0f};
    int i;
    for (i = 0; i < 7; i++) {
      char label[32];
      snprintf(label, sizeof label, "%d%%", (int)(sizes[i] * 100 + 0.5f));
      if (igMenuItem_Bool(label, NULL, a->ui_scale == sizes[i], true))
        a->ui_scale = sizes[i];
    }
    igEndMenu();
  }
  igMenuItem_BoolPtr("XPPAUT keyboard commands", NULL, &a->classic_keys, true);
  igMenuItem_BoolPtr("Show actions as Python", NULL, &a->echo.console, true);
  if (igIsItemHovered(0))
    igSetTooltip("Write each action in the command window as the Python that "
                 "does the same (lines starting with >>>).");
  igSeparator();
  igMenuItem_BoolPtr("ImGui/ImPlot demo (developers)", NULL, &a->show_demo,
                     true);
}

static void menu_help(App *a) {
  if (igMenuItem_Bool("Getting started", "F1", false, true))
    ui_show_help(a, "welcome");
  if (igMenuItem_Bool("Tutorial: your first model", NULL, false, true))
    ui_show_help(a, "tutorial");
  if (igMenuItem_Bool("Bifurcation analysis with AUTO", NULL, false, true))
    ui_show_help(a, "auto");
  if (igMenuItem_Bool("The .ode file format", NULL, false, true))
    ui_show_help(a, "odefile");
  if (igMenuItem_Bool("Numerical methods", NULL, false, true))
    ui_show_help(a, "numerics");
  if (igMenuItem_Bool("Commands and keyboard shortcuts", NULL, false, true))
    ui_show_help(a, "commands");
  if (igMenuItem_Bool("Coming from XPPAUT", NULL, false, true))
    ui_show_help(a, "xppaut");
  igSeparator();
  if (igMenuItem_Bool("About XPP Lab", NULL, false, true))
    ui_show_help(a, "about");
}

void ui_menu_bar(App *a) {
  if (!igBeginMainMenuBar()) return;
  if (igBeginMenu("File", true)) {
    menu_file(a);
    igEndMenu();
  }
  if (igBeginMenu("Run", true)) {
    menu_run(a);
    igEndMenu();
  }
  if (igBeginMenu("View", true)) {
    menu_view(a);
    igEndMenu();
  }
  if (igBeginMenu("Help", true)) {
    menu_help(a);
    igEndMenu();
  }
  igEndMainMenuBar();
}

static bool tool(const char *label, const char *tip, bool enabled) {
  bool clicked;
  igBeginDisabled(!enabled);
  clicked = igButton(label, V2(0, 0));
  igEndDisabled();
  if (igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled |
                      ImGuiHoveredFlags_DelayShort) &&
      igBeginTooltip()) {
    igPushTextWrapPos(igGetFontSize() * 28.0f);
    igTextUnformatted(tip, NULL);
    igPopTextWrapPos();
    igEndTooltip();
  }
  igSameLine(0, 6);
  return clicked;
}

void ui_toolbar(App *a, float height) {
  bool ok = a->model_ok;
  (void)height;
  if (tool("New", "Start a new model from a template (" MOD "N).", true))
    app_new_model(a);
  if (tool("Open", "Open an XPPAUT .ode file (" MOD "O).", true)) ui_dialog_open(a);
  if (tool("Save", "Save the model and reload it (" MOD "S).", a->editor[0] != 0))
    app_apply_editor(a);
  igTextDisabled("|");
  igSameLine(0, 6);
  igPushStyleColor_Vec4(ImGuiCol_Button, HEX(0x2e7d32));
  igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, HEX(0x388e3c));
  igPushStyleColor_Vec4(ImGuiCol_Text, HEX(0xffffff));
  if (tool("  Run  ", "Integrate from the current initial values (F5, or I G).",
           ok))
    app_run(a);
  igPopStyleColor(3);
  if (tool("Equilibrium",
           "Find a steady state near the current initial values with "
           "Newton's method, and show its stability (S G).",
           ok))
    app_command(a, "eq");
  if (tool("Nullclines",
           "Draw the curves where dx/dt = 0 and dy/dt = 0 in the phase plane "
           "(N N). Equilibria sit where they cross.",
           ok && a->nvars > 1))
    app_command(a, a->pp.nullclines ? "nullclines off" : "nullclines");
  if (tool("Flow", "Show the direction field in the phase plane (D D).",
           ok && a->nvars > 1))
    app_command(a, "dirfield");
  if (tool("Bifurcation",
           "Open the AUTO window: follow equilibria and periodic orbits as a "
           "parameter changes.",
           ok)) {
    a->show_auto = true;
    igSetWindowFocus_Str(WIN_AUTO);
  }
  if (tool("Analysis",
           "Spectra, histograms, correlations, Liapunov exponents, phase "
           "response curves and curve fitting on the last run.",
           ok)) {
    a->show_analysis = true;
    igSetWindowFocus_Str(WIN_ANALYSIS);
  }
  if (ok && a->nbcs > 0 &&
      tool("Solve BVP",
           "Solve the boundary-value problem: shoot from the initial values "
           "until the model's bndry conditions hold (XPPAUT's Bndryval).",
           true))
    app_command(a, "bvp");
  if (ok && a->ani.loaded &&
      tool("Animate", "Play the model's animation (.ani file) over the run.",
           true)) {
    a->show_ani = true;
    igSetWindowFocus_Str(WIN_ANI);
  }
  igTextDisabled("|");
  igSameLine(0, 6);
  if (ok) {
    double total = a->total;
    igTextUnformatted("Total time", NULL);
    igSameLine(0, 4);
    igSetNextItemWidth(90);
    if (InputDouble("##total", &total, "%g")) {
      char v[64];
      snprintf(v, sizeof v, "%.17g", total);
      app_set_option(a, "TOTAL", v);
    }
    Tooltip("How long to integrate (the TOTAL option).");
    igSameLine(0, 10);
    igTextUnformatted("Method", NULL);
    igSameLine(0, 4);
    igSetNextItemWidth(120);
    if (igBeginCombo("##meth", app_method_name(a->meth), 0)) {
      int i;
      for (i = 0; i < 15; i++)
        if (igSelectable_Bool(app_method_name(i), i == a->meth, 0, V2(0, 0)))
          app_set_option(a, "METH", app_method_name(i));
      igEndCombo();
    }
    Tooltip("The integrator. See Help > Numerical methods for advice.");
    igSameLine(0, 10);
    igCheckbox("Live", &a->live);
    Tooltip("Rerun automatically whenever you change a parameter or an "
            "initial value.");
    igSameLine(0, 6);
    igCheckbox("Keep curves", &a->keep_curves);
    Tooltip("Keep earlier trajectories on the plots so you can compare "
            "them. Use E (erase) to clear.");
  }
}

void ui_status_bar(App *a, float height) {
  (void)height;
  if (a->model_ok)
    igTextColored(HEX(0x2e7d32), "%s", a->model_name);
  else if (a->load_error[0])
    igTextColored(HEX(0xc62828), "%s (errors)", a->model_name);
  else
    igTextDisabled("no model");
  igSameLine(0, 16);
  igTextUnformatted(a->status, NULL);
  if (a->classic_prefix) {
    igSameLine(0, 16);
    igTextColored(HEX(0x1565c0), "XPP: %c ...", a->classic_prefix - 32);
  }
}
