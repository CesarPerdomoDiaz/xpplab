/* The app's actions in one table (actions.h), and the command palette's
   search over them and over the command window's commands. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "actions.h"

/* ---- What the actions do -------------------------------------------------- */

/* Bring a window forward, as the command `show` does. */
static void show_window(App *a, const char *key) {
  snprintf(a->focus, sizeof a->focus, "%s", key);
}

static void act_new(App *a, int arg) { (void)arg, app_new_model(a); }
static void act_open(App *a, int arg) { (void)arg, ui_dialog_open(a); }
static void act_save(App *a, int arg) { (void)arg, app_apply_editor(a); }
static void act_save_as(App *a, int arg) { (void)arg, ui_dialog_save_as(a); }
static void act_export(App *a, int arg) { (void)arg, ui_dialog_export_csv(a); }
static void act_script(App *a, int arg) { (void)arg, ui_dialog_save_script(a); }
static void act_quit(App *a, int arg) { (void)arg, a->quit = true; }

static void act_run(App *a, int arg) { (void)arg, app_run(a); }
static void act_command(App *a, int arg);
static void act_equilibria_range(App *a, int arg) {
  (void)arg;
  a->sw.mode = SWEEP_EQUILIBRIA;
  a->sw.reveal = true;
  a->show_workspace = true;
}
static void act_linearise(App *a, int arg) {
  (void)arg;
  a->an.tool = AN_LINEARIZE;
  a->show_analysis = true;
  show_window(a, "analysis");
}
static void act_nullclines(App *a, int arg) {
  (void)arg;
  app_command(a, a->pp.nullclines ? "nullclines off" : "nullclines");
}
static void act_auto(App *a, int arg) { (void)arg, show_window(a, "auto"); }
static void act_range(App *a, int arg) {
  (void)arg;
  a->sw.reveal = true;
  a->show_workspace = true;
}
static void act_animator(App *a, int arg) { (void)arg, show_window(a, "animator"); }
static void act_kinescope(App *a, int arg) { (void)arg, show_window(a, "kinescope"); }
static void act_view3d(App *a, int arg) { (void)arg, show_window(a, "3d"); }
static void act_analysis(App *a, int arg) {
  a->an.tool = arg;
  a->show_analysis = true;
  show_window(a, "analysis");
}

static void act_reset_layout(App *a, int arg) { (void)arg, a->reset_layout = true; }
static void act_beside(App *a, int arg) {
  (void)arg;
  a->layout = !a->layout;
  a->reset_layout = true;
}
static void act_dark(App *a, int arg) {
  (void)arg;
  a->dark = !a->dark;
  ui_apply_theme(a);
}
static const float text_sizes[] = {0.85f, 1.0f, 1.15f, 1.3f, 1.5f, 1.75f, 2.0f};
static void act_text_size(App *a, int arg) { a->ui_scale = text_sizes[arg]; }
static bool chk_text_size(App *a, int arg) { return a->ui_scale == text_sizes[arg]; }
static void act_palette(App *a, int arg) { (void)arg, palette_open(a); }

static const char *help_pages[] = {"welcome", "tutorial", "neuron-tutorial", "auto",
                                   "odefile", "numerics", "commands", "xppaut",
                                   "matlab", "auto07p", "about"};
static void act_help(App *a, int arg) { ui_show_help(a, help_pages[arg]); }

/* An action that is a command of the command window. */
static const char *const action_commands[] = {"eq", "eq all", "erase", "bvp", "dirfield"};
static void act_command(App *a, int arg) { app_command(a, action_commands[arg]); }
enum { CMD_EQ, CMD_EQ_ALL, CMD_ERASE, CMD_BVP, CMD_DIRFIELD };

/* ---- When they are available ------------------------------------------------ */

static bool en_model(App *a, const char **why) {
  *why = "Open a model first.";
  return a->model_ok;
}
static bool en_save(App *a, const char **why) {
  *why = "Open or create a model first.";
  return a->model_ok || a->editor[0];
}
static bool en_save_as(App *a, const char **why) {
  *why = "There is no model in the editor.";
  return a->editor[0] != 0;
}
static bool en_data(App *a, const char **why) {
  *why = "Run the model first: there is no run to export.";
  return a->have_traj;
}
static bool en_script(App *a, const char **why) {
  *why = "Nothing has been done in this session yet.";
  return a->echo.len > 0;
}
static bool en_erase(App *a, const char **why) {
  *why = "There are no kept curves.";
  return a->nfrozen > 0;
}
static bool en_bvp(App *a, const char **why) {
  *why = !a->model_ok ? "Open a model first."
                      : "The model has no boundary conditions (bndry statements).";
  return a->model_ok && a->nbcs > 0;
}
static bool en_3d(App *a, const char **why) {
  *why = !a->model_ok ? "Open a model first."
                      : "The model needs at least three variables to show in 3-D.";
  return a->model_ok && a->ncols > 3;
}
static bool chk_nullclines(App *a, int arg) { return (void)arg, a->pp.nullclines; }
static bool chk_dirfield(App *a, int arg) { return (void)arg, a->pp.dirfield; }
static bool chk_beside(App *a, int arg) { return (void)arg, a->layout == 1; }
static bool chk_dark(App *a, int arg) { return (void)arg, a->dark; }
static bool chk_analysis(App *a, int arg) { return a->an.tool == arg; }

/* ---- The table ------------------------------------------------------------- */

#define ITEM(id, menu, sep, name, sc, desc, tip, run, arg, en, chk) \
  {id, menu, NULL, sep, name, sc, desc, tip, run, arg, -1, en, chk}
#define SUB(id, menu, sub, name, desc, run, arg, en, chk) \
  {id, menu, sub, false, name, NULL, desc, NULL, run, arg, -1, en, chk}
#define FLAG(id, menu, sep, name, sc, desc, tip, field) \
  {id, menu, NULL, sep, name, sc, desc, tip, NULL, 0, (long)offsetof(App, field), NULL, NULL}
#define WINDOW(id, name, field, sc) \
  FLAG(id, "View", false, name, sc, "Show or hide the " name " window.", NULL, field)
#define ANALYSIS(id, name, desc, tool) \
  SUB(id, "Run", "Analyse the run", name, desc, act_analysis, tool, en_model, chk_analysis)
#define SIZE(id, name, k) \
  SUB(id, "View", "Text size", name, "Set the size of the text in all windows.", act_text_size, k, NULL, chk_text_size)
#define HELP(id, sep, name, sc, desc, k) \
  ITEM(id, "Help", sep, name, sc, desc, NULL, act_help, k, NULL, NULL)

const Action app_actions[] = {
    /* File */
    ITEM("file.new", "File", false, "New model", MOD "N",
         "Start a new model from a template.", NULL, act_new, 0, NULL, NULL),
    ITEM("file.open", "File", false, "Open...", MOD "O",
         "Open an XPPAUT .ode file.", NULL, act_open, 0, NULL, NULL),
    ITEM("file.save", "File", true, "Save", MOD "S",
         "Save the model in the editor and reload it.", NULL, act_save, 0, en_save, NULL),
    ITEM("file.save_as", "File", false, "Save as...", NULL,
         "Save the model in the editor under another name.", NULL, act_save_as, 0,
         en_save_as, NULL),
    ITEM("file.export", "File", true, "Export data as CSV or MAT...", NULL,
         "Write the last run to a CSV or MATLAB .mat file.", NULL, act_export, 0, en_data,
         NULL),
    ITEM("file.script", "File", false, "Save session as Python script...", NULL,
         "Save what you have done as a Python script for the xpplab package.",
         "A script that repeats what you have done, number for number, with Python and "
         "the xpplab package (pip install xpplab).",
         act_script, 0, en_script, NULL),
    ITEM("file.quit", "File", true, "Quit", MOD "Q  /  F Q", "Leave the app.", NULL,
         act_quit, 0, NULL, NULL),

    /* Run */
    ITEM("run.integrate", "Run", false, "Integrate", "F5  /  I G",
         "Integrate from the current initial values.", NULL, act_run, 0, en_model, NULL),
    ITEM("run.equilibrium", "Run", false, "Find equilibrium", "S G",
         "Newton's method from the initial values, and the equilibrium's stability.", NULL,
         act_command, CMD_EQ, en_model, NULL),
    ITEM("run.equilibria", "Run", false, "Find all equilibria", NULL,
         "Every equilibrium in the phase plane's window, from random guesses.",
         "Every equilibrium in the phase plane's window, from random starting points "
         "(XPPAUT's Sing pts > Monte Carlo).",
         act_command, CMD_EQ_ALL, en_model, NULL),
    ITEM("run.equilibria_range", "Run", false, "Equilibria over a parameter...", NULL,
         "Follow the equilibrium as a parameter changes (the Range tool).", NULL,
         act_equilibria_range, 0, en_model, NULL),
    ITEM("run.linearise", "Run", false, "Linearise...", NULL,
         "A and B at an equilibrium, its eigenvalues, and whether parameters can steer it.",
         "A and B at the equilibrium, its eigenvalues in the complex plane, and whether "
         "chosen parameters can steer the state.",
         act_linearise, 0, en_model, NULL),
    ITEM("run.nullclines", "Run", false, "Nullclines", "N N",
         "Draw the curves where each derivative is zero in the phase plane.", NULL,
         act_nullclines, 0, en_model, chk_nullclines),
    ITEM("run.dirfield", "Run", false, "Direction field", "D D",
         "Draw the direction field in the phase plane.", NULL, act_command, CMD_DIRFIELD,
         en_model, chk_dirfield),
    FLAG("run.live", "Run", true, "Rerun after every change (live)", NULL,
         "Integrate again whenever a parameter or initial value changes.", NULL, live),
    FLAG("run.keep", "Run", false, "Keep previous curves", NULL,
         "Keep earlier trajectories on the plots to compare them.", NULL, keep_curves),
    ITEM("run.erase", "Run", false, "Erase curves", "E", "Remove the kept curves.", NULL,
         act_command, CMD_ERASE, en_erase, NULL),
    ITEM("run.auto", "Run", true, "Bifurcation diagram (AUTO)...", NULL,
         "Open the AUTO window: follow equilibria and periodic orbits as a parameter changes.",
         NULL, act_auto, 0, en_model, NULL),
    ITEM("run.bvp", "Run", false, "Boundary-value problem", NULL,
         "Solve the model's boundary conditions by shooting.",
         "Solve the model's boundary conditions by shooting (XPPAUT's Bndryval).", act_command,
         CMD_BVP, en_bvp, NULL),
    ITEM("run.range", "Run", false, "Range...", NULL,
         "Run over a range of a parameter or initial value.", NULL, act_range, 0, en_model,
         NULL),
    ITEM("run.animator", "Run", false, "Animator...", NULL,
         "Play an XPPAUT animation of the run.", NULL, act_animator, 0, en_model, NULL),
    ITEM("run.kinescope", "Run", false, "Kinescope...", NULL,
         "Capture pictures of the plots and play them back or save them as a film.", NULL,
         act_kinescope, 0, NULL, NULL),
    ITEM("run.view3d", "Run", false, "3D view", NULL,
         "Show three columns of the run in a rotating 3-D plot.", NULL, act_view3d, 0, en_3d,
         NULL),
    ANALYSIS("run.an.spectrum", "Spectrum",
             "Power spectral density, cross spectrum or coherence of the last run.",
             AN_SPECTRUM),
    ANALYSIS("run.an.fourier", "Fourier series",
             "Fourier series of a column of the last run.", AN_FOURIER),
    ANALYSIS("run.an.histogram", "Histogram", "Histogram of a column of the last run.",
             AN_HISTOGRAM),
    ANALYSIS("run.an.histogram2d", "2-D histogram", "Two-dimensional histogram of two columns.",
             AN_HISTOGRAM2D),
    ANALYSIS("run.an.correlation", "Correlation",
             "Auto- or cross-correlation of the last run.", AN_CORRELATION),
    ANALYSIS("run.an.statistics", "Statistics",
             "Mean, standard deviation and Liapunov exponent of a column.", AN_STATISTICS),
    ANALYSIS("run.an.prc", "Phase response",
             "Phase response curves of the limit cycle: one period and its adjoint.", AN_PRC),
    ANALYSIS("run.an.hfunction", "H function",
             "The weak-coupling interaction function of coupled oscillators.", AN_HFUNCTION),
    ANALYSIS("run.an.fit", "Fit to data", "Fit parameters to a data file by least squares.",
             AN_FIT),
    ANALYSIS("run.an.ensemble", "Ensemble of noisy runs",
             "Mean and variance over many runs of a noisy model.", AN_ENSEMBLE),
    ANALYSIS("run.an.linearize", "Linearise",
             "A and B, eigenvalues and controllability at a point.", AN_LINEARIZE),

    /* View */
    WINDOW("view.models", "Models", show_browser, NULL),
    WINDOW("view.workspace", "Workspace", show_workspace, NULL),
    WINDOW("view.editor", "Editor", show_editor, NULL),
    WINDOW("view.time", "Time Series", show_time, NULL),
    WINDOW("view.phase", "Phase Plane", show_phase, NULL),
    WINDOW("view.auto", "Bifurcation", show_auto, NULL),
    WINDOW("view.console", "Command Window", show_console, NULL),
    WINDOW("view.data", "Data", show_data, NULL),
    WINDOW("view.array", "Array Plot", show_array, NULL),
    WINDOW("view.analysis", "Analysis", show_analysis, NULL),
    WINDOW("view.view3d", "3D View", show_view3d, NULL),
    WINDOW("view.animator", "Animator", show_ani, NULL),
    WINDOW("view.kinescope", "Kinescope", show_kine, NULL),
    WINDOW("view.figure", "Figure", show_figure, NULL),
    WINDOW("view.help", "Help", show_help, "F1"),
    ITEM("view.palette", "View", false, "Command palette...", MOD "Shift+P",
         "Find any action or command by typing part of its name.", NULL, act_palette, 0,
         NULL, NULL),
    ITEM("view.reset_layout", "View", true, "Reset window layout", NULL,
         "Put the windows back where they start.", NULL, act_reset_layout, 0, NULL, NULL),
    ITEM("view.beside", "View", false, "Phase plane beside time series", NULL,
         "Show the phase plane next to the time series instead of in tabs.", NULL, act_beside,
         0, NULL, chk_beside),
    ITEM("view.dark", "View", false, "Dark theme", NULL, "Switch between the dark and the light theme.",
         NULL, act_dark, 0, NULL, chk_dark),
    SIZE("view.size.85", "85%", 0),
    SIZE("view.size.100", "100%", 1),
    SIZE("view.size.115", "115%", 2),
    SIZE("view.size.130", "130%", 3),
    SIZE("view.size.150", "150%", 4),
    SIZE("view.size.175", "175%", 5),
    SIZE("view.size.200", "200%", 6),
    FLAG("view.classic", "View", false, "XPPAUT keyboard commands", NULL,
         "Use XPPAUT's two-key commands (I G, N N, D D, ...).", NULL, classic_keys),
    FLAG("view.echo", "View", false, "Show actions as Python", NULL,
         "Write each action in the command window as the Python that does the same.",
         "Write each action in the command window as the Python that does the same (lines "
         "starting with >>>).",
         echo.console),
    FLAG("view.demo", "View", true, "ImGui/ImPlot demo (developers)", NULL,
         "Show the ImGui and ImPlot demo windows.", NULL, show_demo),

    /* Help */
    HELP("help.welcome", false, "Getting started", "F1", "The welcome page of the manual.", 0),
    HELP("help.tutorial", false, "Tutorial: your first model", NULL,
         "Write, run and explore a first model.", 1),
    HELP("help.neuron", false, "Tutorial: why does a neuron fire?", NULL,
         "A neuron model, its equilibria and its bifurcation.", 2),
    HELP("help.auto", false, "Bifurcation analysis with AUTO", NULL,
         "Follow equilibria and periodic orbits with AUTO.", 3),
    HELP("help.odefile", false, "The .ode file format", NULL,
         "Everything a model file can say.", 4),
    HELP("help.numerics", false, "Numerical methods", NULL,
         "The integrators and how to choose one.", 5),
    HELP("help.commands", false, "Commands and keyboard shortcuts", NULL,
         "Every command of the command window and every shortcut.", 6),
    HELP("help.xppaut", false, "Coming from XPPAUT", NULL,
         "Where XPPAUT's menus and keys are in XPP Lab.", 7),
    HELP("help.matlab", false, "Coming from MATLAB", NULL,
         "XPP Lab for MATLAB users.", 8),
    HELP("help.auto07p", false, "Coming from AUTO-07p", NULL,
         "XPP Lab for AUTO-07p users.", 9),
    HELP("help.about", true, "About XPP Lab", NULL, "The version, the licence and the credits.",
         10),
};
const int app_nactions = (int)(sizeof app_actions / sizeof app_actions[0]);

bool action_checked(App *a, const Action *x) {
  if (x->flag >= 0) return *(bool *)((char *)a + x->flag);
  return x->checked ? x->checked(a, x->arg) : false;
}

bool action_available(App *a, const Action *x, const char **reason) {
  static const char *none;
  if (!reason) reason = &none;
  *reason = "";
  return x->enabled ? x->enabled(a, reason) : true;
}

/* ---- Recent use ------------------------------------------------------------ */

static void remember(App *a, const char *key) {
  CmdPalette *p = &a->pal;
  int i, at = p->nrecent;
  for (i = 0; i < p->nrecent; i++)
    if (!strcmp(p->recent[i], key)) at = i;
  if (at == p->nrecent) {
    if (p->nrecent < APP_PAL_RECENT) p->nrecent++;
    at = p->nrecent - 1;
  }
  memmove(p->recent[1], p->recent[0], (size_t)at * sizeof p->recent[0]);
  snprintf(p->recent[0], sizeof p->recent[0], "%s", key);
}

/* 0 for the newest, a large number if it was not used. */
static int recency(App *a, const char *key) {
  int i;
  for (i = 0; i < a->pal.nrecent; i++)
    if (!strcmp(a->pal.recent[i], key)) return i;
  return 1000;
}

void action_run(App *a, const Action *x) {
  remember(a, x->id);
  if (x->flag >= 0) {
    bool *p = (bool *)((char *)a + x->flag);
    *p = !*p;
  } else {
    x->run(a, x->arg);
  }
}

/* ---- Palette entries ------------------------------------------------------- */

enum { E_ACTION, E_COMMAND, E_FILE };

static int entry_kind(App *a, int e, int *index) {
  (void)a;
  if (e < app_nactions) return *index = e, E_ACTION;
  e -= app_nactions;
  if (e < app_ncommands) return *index = e, E_COMMAND;
  return *index = e - app_ncommands, E_FILE;
}

int palette_entries(App *a) { return app_nactions + app_ncommands + a->nrecent; }

static const char *base_name(const char *path) {
  const char *s = strrchr(path, '/'), *b = strrchr(path, '\\');
  if (b && (!s || b > s)) s = b;
  return s ? s + 1 : path;
}

/* What the user searches in: an action's menu path, a command's name. */
static void match_name(App *a, int e, char *buf, size_t n) {
  int i, k = entry_kind(a, e, &i);
  if (k == E_ACTION) {
    const Action *x = &app_actions[i];
    if (x->submenu)
      snprintf(buf, n, "%s: %s", x->submenu, x->name);
    else if (!strcmp(x->menu, "Help"))
      snprintf(buf, n, "Help: %s", x->name);
    else
      snprintf(buf, n, "%s", x->name);
  } else if (k == E_COMMAND) {
    snprintf(buf, n, "%s", app_command_table[i].name);
  } else {
    snprintf(buf, n, "Open recent: %s", base_name(a->recent[i]));
  }
}

void palette_label(App *a, int e, char *buf, size_t n) {
  int i, k = entry_kind(a, e, &i);
  if (k == E_COMMAND && app_command_table[i].args)
    snprintf(buf, n, "%s %s", app_command_table[i].name, app_command_table[i].args);
  else
    match_name(a, e, buf, n);
}

const char *palette_description(App *a, int e) {
  int i, k = entry_kind(a, e, &i);
  return k == E_ACTION ? app_actions[i].description
         : k == E_COMMAND ? app_command_table[i].description
                          : a->recent[i];
}

const char *palette_shortcut(App *a, int e) {
  int i, k = entry_kind(a, e, &i);
  const char *s = k == E_ACTION ? app_actions[i].shortcut
                  : k == E_COMMAND ? app_command_table[i].shortcut
                                   : NULL;
  return s ? s : "";
}

bool palette_available(App *a, int e, const char **reason) {
  int i, k = entry_kind(a, e, &i);
  static const char *none;
  if (!reason) reason = &none;
  *reason = "";
  if (k == E_ACTION) return action_available(a, &app_actions[i], reason);
  if (k == E_FILE) return true;
  switch (app_command_table[i].needs) {
  case NEED_MODEL:
    *reason = "Open a model first.";
    return a->model_ok;
  case NEED_RUN:
    *reason = !a->model_ok ? "Open a model first." : "Run the model first.";
    return a->model_ok && a->have_traj;
  case NEED_BRANCH:
    *reason = !a->model_ok ? "Open a model first."
                           : "The bifurcation diagram has no branch yet: follow one with AUTO first.";
    return a->model_ok && a->au.count > 0;
  }
  return true;
}

/* ---- Searching ------------------------------------------------------------- */

static int sort_score[PALETTE_MAX_HITS * 2];
static int sort_rank[PALETTE_MAX_HITS * 2];

static int by_score(const void *p, const void *q) {
  int x = *(const int *)p, y = *(const int *)q;
  if (sort_score[x] != sort_score[y]) return sort_score[y] - sort_score[x];
  if (sort_rank[x] != sort_rank[y]) return sort_rank[x] - sort_rank[y];
  return x - y;
}

/* How well the words match; -1 if one is missing. A word counts most in the
   name, most of all at its start or at the start of one of its words. */
static int score_entry(const char *name, const char *desc, char (*word)[40], int nwords) {
  int total = 0, k;
  for (k = 0; k < nwords; k++) {
    const char *at = SDL_strcasestr(name, word[k]);
    if (at) {
      total += 4;
      if (at == name) total += 3;
      else if (at[-1] == ' ' || at[-1] == ':' || at[-1] == '-' || at[-1] == '(') total += 1;
    } else if (SDL_strcasestr(desc, word[k])) {
      total += 1;
    } else {
      return -1;
    }
  }
  return total;
}

int palette_search(App *a, const char *query, int *hits, int max) {
  static int order[PALETTE_MAX_HITS * 2];
  char word[8][40];
  char name[256], key[96];
  int nwords = 0, n = palette_entries(a), count = 0, e;
  const char *s = query ? query : "";
  if (n > PALETTE_MAX_HITS) n = PALETTE_MAX_HITS;
  while (*s && nwords < 8) {
    size_t len = 0;
    while (*s == ' ' || *s == '\t') s++;
    while (s[len] && s[len] != ' ' && s[len] != '\t') len++;
    if (!len) break;
    snprintf(word[nwords++], sizeof word[0], "%.*s", (int)len, s);
    s += len;
  }
  for (e = 0; e < n; e++) {
    int i, k = entry_kind(a, e, &i);
    match_name(a, e, name, sizeof name);
    sort_score[e] = score_entry(name, palette_description(a, e), word, nwords);
    if (sort_score[e] < 0) continue;
    if (k == E_ACTION) snprintf(key, sizeof key, "%s", app_actions[i].id);
    else if (k == E_COMMAND) snprintf(key, sizeof key, "cmd:%s", app_command_table[i].name);
    else snprintf(key, sizeof key, "file:%s", a->recent[i]);
    sort_rank[e] = recency(a, key);
    order[count++] = e;
  }
  qsort(order, (size_t)count, sizeof order[0], by_score);
  if (count > max) count = max;
  memcpy(hits, order, (size_t)count * sizeof *hits);
  return count;
}

/* ---- Running --------------------------------------------------------------- */

void palette_run(App *a, int e) {
  int i, k = entry_kind(a, e, &i);
  char key[1100];
  a->pal.open = false;
  if (k == E_ACTION) {
    action_run(a, &app_actions[i]);
  } else if (k == E_COMMAND) {
    const CommandInfo *c = &app_command_table[i];
    snprintf(key, sizeof key, "cmd:%s", c->name);
    remember(a, key);
    if (c->args) {
      char text[96];
      snprintf(text, sizeof text, "%s ", c->name);
      ui_console_prefill(a, text);
    } else {
      /* Typed, it would be logged so (xpplab-batch logs it itself). */
      if (!a->batch) app_log(a, ">> %s", c->name);
      app_command(a, c->name);
    }
  } else {
    char path[1024];
    snprintf(path, sizeof path, "%s", a->recent[i]);
    snprintf(key, sizeof key, "file:%s", path);
    remember(a, key);
    app_open(a, path);
  }
}

void palette_open(App *a) {
  a->pal.open = true;
  a->pal.focus = true;
  a->pal.sel = 0;
  a->pal.query[0] = '\0';
}

/* ---- The `palette` command --------------------------------------------------- */

static int check_fail(App *a, const char *fmt, const char *x, const char *y) {
  char msg[400];
  snprintf(msg, sizeof msg, fmt, x, y);
  app_log(a, "error: palette: %s", msg);
  return 1;
}

/* Is there a table command called `word`, or one that starts with it? */
static bool has_command(const char *word) {
  size_t n = strlen(word);
  int i;
  for (i = 0; i < app_ncommands; i++) {
    const char *c = app_command_table[i].name;
    if (!strncmp(c, word, n) && (c[n] == '\0' || c[n] == ' ')) return true;
  }
  return false;
}

/* The table is consistent, every command that `help` lists is in it, and
   every entry is found by searching for its own name. */
static int palette_check(App *a) {
  static int hits[PALETTE_MAX_HITS];
  static const char *const menus[] = {"File", "Run", "View", "Help"};
  int bad = 0, i, j, n;
  const char *u = app_command_usage();
  char label[256];

  for (i = 0; i < app_nactions; i++) {
    const Action *x = &app_actions[i];
    bool known = false;
    for (j = 0; j < 4; j++) known |= !strcmp(x->menu, menus[j]);
    if (!known) bad += check_fail(a, "%s: no such menu '%s'", x->id, x->menu);
    if (!x->name || !x->name[0] || !x->description || !x->description[0])
      bad += check_fail(a, "%s: no name or description", x->id, "");
    if (!x->run && x->flag < 0) bad += check_fail(a, "%s: does nothing", x->id, "");
    for (j = 0; j < i; j++)
      if (!strcmp(app_actions[j].id, x->id)) bad += check_fail(a, "%s: id used twice", x->id, "");
      else if (!strcmp(app_actions[j].menu, x->menu) &&
               !strcmp(app_actions[j].name, x->name) &&
               (app_actions[j].submenu == x->submenu ||
                (app_actions[j].submenu && x->submenu &&
                 !strcmp(app_actions[j].submenu, x->submenu))))
        bad += check_fail(a, "%s: same menu item as %s", x->id, app_actions[j].id);
    /* A submenu's items follow one another. */
    if (x->submenu && i > 0 && app_actions[i - 1].submenu != x->submenu &&
        (!app_actions[i - 1].submenu || strcmp(app_actions[i - 1].submenu, x->submenu))) {
      for (j = 0; j < i - 1; j++)
        if (app_actions[j].submenu && !strcmp(app_actions[j].submenu, x->submenu))
          bad += check_fail(a, "%s: its submenu is split", x->id, "");
    }
  }
  for (i = 0; i < app_ncommands; i++) {
    const CommandInfo *c = &app_command_table[i];
    if (!c->description || !c->description[0])
      bad += check_fail(a, "%s: no description", c->name, "");
    for (j = 0; j < i; j++)
      if (!strcmp(app_command_table[j].name, c->name))
        bad += check_fail(a, "%s: listed twice", c->name, "");
  }
  /* Each first word of a line of `help` that starts a command. */
  for (; *u; u = strchr(u, '\n') ? strchr(u, '\n') + 1 : u + strlen(u)) {
    char word[40];
    size_t len = 0;
    if (u[0] != ' ' || u[1] != ' ' || u[2] == ' ' || u[2] == '|') continue;
    while (u[2 + len] && u[2 + len] != ' ' && u[2 + len] != '\n' && len < sizeof word - 1) len++;
    snprintf(word, sizeof word, "%.*s", (int)len, u + 2);
    if (!has_command(word)) bad += check_fail(a, "the command '%s' of help is not in the table", word, "");
  }
  n = palette_entries(a);
  for (i = 0; i < n; i++) {
    int found, k;
    if (i >= app_nactions && i < app_nactions + app_ncommands)
      snprintf(label, sizeof label, "%s", app_command_table[i - app_nactions].name);
    else
      match_name(a, i, label, sizeof label);
    found = palette_search(a, label, hits, PALETTE_MAX_HITS);
    for (k = 0; k < found && hits[k] != i; k++) {}
    if (k == found) bad += check_fail(a, "'%s' is not found by its own name", label, "");
  }
  if (!bad)
    app_log(a, "palette: %d actions and %d commands, all consistent", app_nactions,
            app_ncommands);
  return bad;
}

void palette_command(App *a, char *rest) {
  static int hits[PALETTE_MAX_HITS];
  while (*rest == ' ') rest++;
  if (!*rest) {
    palette_open(a);
  } else if (!strcmp(rest, "check")) {
    palette_check(a);
  } else if (!strcmp(rest, "list")) {
    int e, n = palette_entries(a);
    char label[256];
    for (e = 0; e < n; e++) {
      const char *kind = e < app_nactions ? "action"
                         : e < app_nactions + app_ncommands ? "command" : "file";
      palette_label(a, e, label, sizeof label);
      app_log(a, "palette: %s | %s | %s | %s", kind, label, palette_shortcut(a, e),
              palette_description(a, e));
    }
  } else if (!strncmp(rest, "run", 3) && (rest[3] == '\0' || rest[3] == ' ')) {
    const char *text = rest + 3, *why;
    int found;
    while (*text == ' ') text++;
    found = palette_search(a, text, hits, PALETTE_MAX_HITS);
    if (!found) {
      app_log(a, "error: nothing in the palette matches '%s'", text);
    } else if (!palette_available(a, hits[0], &why)) {
      char label[256];
      palette_label(a, hits[0], label, sizeof label);
      app_log(a, "error: %s is not available: %s", label, why);
    } else {
      palette_run(a, hits[0]);
    }
  } else {
    const char *text = !strncmp(rest, "type", 4) && (rest[4] == '\0' || rest[4] == ' ') ? rest + 4 : rest;
    while (*text == ' ') text++;
    palette_open(a);
    snprintf(a->pal.query, sizeof a->pal.query, "%s", text);
  }
}
