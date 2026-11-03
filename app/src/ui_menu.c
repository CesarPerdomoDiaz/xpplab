/* Main menu, toolbar and status bar. */
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "actions.h"
#include "ui.h"

/* A menu item of the action table (actions.c). */
static void draw_item(App *a, const Action *x) {
  const char *why;
  bool ok = action_available(a, x, &why);
  if (igMenuItem_Bool(x->name, x->shortcut, action_checked(a, x), ok)) action_run(a, x);
  if (igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!ok && why[0])
      igSetTooltip("%s", why);
    else if (x->tip)
      igSetTooltip("%s", x->tip);
  }
}

static void menu_open_recent(App *a) {
  int i;
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
}

/* The items of one menu, in the table's order. Items with the same submenu
   follow one another and make a submenu. */
static void draw_menu(App *a, const char *menu) {
  int i = 0;
  while (i < app_nactions) {
    const Action *x = &app_actions[i];
    if (strcmp(x->menu, menu) != 0) {
      i++;
      continue;
    }
    if (x->sep_before) igSeparator();
    if (x->submenu) {
      const char *why;
      if (igBeginMenu(x->submenu, action_available(a, x, &why))) {
        for (; i < app_nactions && app_actions[i].submenu &&
               !strcmp(app_actions[i].submenu, x->submenu);
             i++)
          draw_item(a, &app_actions[i]);
        igEndMenu();
      } else {
        for (; i < app_nactions && app_actions[i].submenu &&
               !strcmp(app_actions[i].submenu, x->submenu);
             i++) {}
      }
      continue;
    }
    draw_item(a, x);
    if (!strcmp(x->id, "file.open")) menu_open_recent(a);
    i++;
  }
}

void ui_menu_bar(App *a) {
  if (!igBeginMainMenuBar()) return;
  if (igBeginMenu("File", true)) {
    draw_menu(a, "File");
    igEndMenu();
  }
  if (igBeginMenu("Run", true)) {
    draw_menu(a, "Run");
    igEndMenu();
  }
  if (igBeginMenu("View", true)) {
    draw_menu(a, "View");
    igEndMenu();
  }
  if (igBeginMenu("Help", true)) {
    draw_menu(a, "Help");
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
           "response curves and curve fitting on the last run; the "
           "linearisation at an equilibrium.",
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
