/* Commands typed in the command window. Toolbar buttons and XPPAUT's
   keyboard commands run through here too, so both always behave alike. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "app.h"

void ui_show_help(App *a, const char *page);

static const char *usage =
    "Commands (XPPAUT keyboard equivalents in brackets):\n"
    "  run                      integrate from the initial values [I G]\n"
    "  continue [TIME]          integrate further from the end of the last\n"
    "                           run and append to it [C C]\n"
    "  set NAME=VALUE ...       set parameters, initial values or options\n"
    "                           e.g.  set I=0.5 total=200 meth=stiff\n"
    "  get NAME                 show a value\n"
    "  whos                     list variables and parameters\n"
    "  ic last                  start the next run where the last ended [I L]\n"
    "  eq                       find an equilibrium and its stability [S G]\n"
    "  nullclines [off]         draw nullclines in the phase plane [N N]\n"
    "  dirfield [off]           draw the direction field [D D]\n"
    "  erase                    remove kept curves [E]\n"
    "  use SET                  apply a named parameter set\n"
    "  auto steady [PAR]        follow steady states as PAR varies\n"
    "  auto from LABEL ACTION   continue from a labelled point; ACTION is\n"
    "                           periodic | two | extend | switch | doubling\n"
    "  auto set DS=0.01 ...     AUTO numerics (ds, dsmin, dsmax, nmax, npr,\n"
    "                           parmin, parmax, ntst)\n"
    "  auto reset               clear the bifurcation diagram\n"
    "  open FILE | new | save   models\n"
    "  export FILE.csv          save the last run as CSV\n"
    "  show WINDOW              bring a window forward: time, phase, auto,\n"
    "                           editor, data, help, console, workspace\n"
    "  help [TOPIC]             open the manual (topics: tutorial, auto,\n"
    "                           odefile, numerics, commands, xppaut)\n";

static char *skip_space(char *s) {
  while (*s && isspace((unsigned char)*s)) s++;
  return s;
}

static char *next_word(char **s) {
  char *w = skip_space(*s), *e;
  if (!*w) return NULL;
  e = w;
  while (*e && !isspace((unsigned char)*e)) e++;
  if (*e) *e++ = '\0';
  *s = e;
  return w;
}

static void cmd_set(App *a, char *rest) {
  char *tok = strtok(rest, " ,;");
  while (tok) {
    char *eq = strchr(tok, '=');
    if (!eq) {
      app_log(a, "error: write set NAME=VALUE (got '%s')", tok);
    } else {
      int i;
      *eq = '\0';
      i = app_find_par(a, tok);
      if (i >= 0)
        app_set_param(a, i, atof(eq + 1));
      else if ((i = app_find_var(a, tok)) >= 0)
        app_set_ic(a, i, atof(eq + 1));
      else
        app_set_option(a, tok, eq + 1);
    }
    tok = strtok(NULL, " ,;");
  }
}

static void cmd_whos(App *a) {
  int i;
  char line[4096];
  int k = 0;
  if (!a->model_ok) {
    app_log(a, "No model loaded.");
    return;
  }
  k = snprintf(line, sizeof line, "Variables (initial value):");
  for (i = 0; i < a->nvars && k < (int)sizeof line - 64; i++)
    k += snprintf(line + k, sizeof line - k, " %s=%g", a->vars[i], a->ic[i]);
  app_log(a, "%s", line);
  k = snprintf(line, sizeof line, "Parameters:");
  for (i = 0; i < a->npars && k < (int)sizeof line - 64; i++)
    k += snprintf(line + k, sizeof line - k, " %s=%g", a->pars[i],
                  a->par[i].value);
  app_log(a, "%s", line);
  app_log(a, "Output columns: %d. Method %s, total %g, dt %g.", a->ncols,
          app_method_name(a->meth), a->total, a->dt);
}

static void cmd_ic_last(App *a) {
  int i, j;
  if (!a->have_traj || a->traj.nrows == 0) {
    app_log(a, "There is no run yet.");
    return;
  }
  for (i = 0; i < a->nvars; i++)
    for (j = 1; j < a->traj.ncols; j++)
      if (SDL_strcasecmp(a->traj.names[j], a->vars[i]) == 0)
        app_set_ic(a, i, a->traj.cols[j][a->traj.nrows - 1]);
  app_log(a, "Initial values set to the end of the last run.");
}

static const char *action_key(App *a, int point, const char *word) {
  const xppe_auto_point_info *p = &a->au.info[point];
  if (!strcmp(word, "periodic")) return "p";
  if (!strcmp(word, "two") || !strcmp(word, "two-parameter"))
    return strcmp(p->type_name, "LP") == 0 ? "" : "t";
  if (!strcmp(word, "extend")) return (p->periodic || !strcmp(p->type_name, "HB") ||
                                       !strcmp(p->type_name, "BP") ||
                                       !strcmp(p->type_name, "PD") ||
                                       !strcmp(p->type_name, "TR"))
                                          ? "e"
                                          : "";
  if (!strcmp(word, "switch")) return "s";
  if (!strcmp(word, "doubling")) return "d";
  if (!strcmp(word, "fixed")) return "f";
  return NULL;
}

static void cmd_auto(App *a, char *rest) {
  char *w = next_word(&rest);
  if (!a->model_ok) {
    app_log(a, "No model loaded.");
    return;
  }
  if (!w || !strcmp(w, "steady") || !strcmp(w, "eq")) {
    char *par = next_word(&rest);
    if (par) {
      int i = app_find_par(a, par);
      if (i < 0) {
        app_log(a, "error: no parameter %s", par);
        return;
      }
      a->au.main_par = i;
    }
    a->show_auto = true;
    app_auto_equilibria(a);
  } else if (!strcmp(w, "from")) {
    char *lab = next_word(&rest), *act = next_word(&rest);
    int i, label = lab ? atoi(lab) : 0, point = -1;
    const char *key;
    for (i = 0; i < a->au.count; i++)
      if (a->au.info[i].label == label) point = i;
    if (point < 0) {
      app_log(a, "error: no point labelled %s", lab ? lab : "?");
      return;
    }
    key = action_key(a, point, act ? act : "extend");
    if (!key) {
      app_log(a, "error: unknown action %s", act);
      return;
    }
    a->au.selected = point;
    app_auto_continue(a, point, key);
  } else if (!strcmp(w, "reset")) {
    app_auto_reset(a);
  } else if (!strcmp(w, "set")) {
    char *tok = strtok(rest, " ,;");
    while (tok) {
      char *eq = strchr(tok, '=');
      if (eq) {
        double v = atof(eq + 1);
        *eq = '\0';
        if (!SDL_strcasecmp(tok, "ds")) a->au.ds = v;
        else if (!SDL_strcasecmp(tok, "dsmin")) a->au.dsmin = v;
        else if (!SDL_strcasecmp(tok, "dsmax")) a->au.dsmax = v;
        else if (!SDL_strcasecmp(tok, "nmax")) a->au.nmax = (int)v;
        else if (!SDL_strcasecmp(tok, "npr")) a->au.npr = (int)v;
        else if (!SDL_strcasecmp(tok, "ntst")) a->au.ntst = (int)v;
        else if (!SDL_strcasecmp(tok, "parmin")) a->au.parmin = v;
        else if (!SDL_strcasecmp(tok, "parmax")) a->au.parmax = v;
        else app_log(a, "error: unknown AUTO setting %s", tok);
      }
      tok = strtok(NULL, " ,;");
    }
  } else {
    app_log(a, "error: unknown auto command '%s'; try 'help'", w);
  }
}

void app_command(App *a, const char *line_in) {
  char line[1024], *rest = line, *cmd;
  snprintf(line, sizeof line, "%s", line_in);
  cmd = next_word(&rest);
  if (!cmd) return;
  for (char *c = cmd; *c; c++) *c = (char)tolower((unsigned char)*c);

  if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
    char *topic = next_word(&rest);
    if (topic)
      ui_show_help(a, topic);
    else
      app_log(a, "%s", usage);
  } else if (!strcmp(cmd, "run") || !strcmp(cmd, "go") || !strcmp(cmd, "ig")) {
    app_run(a);
  } else if (!strcmp(cmd, "continue") || !strcmp(cmd, "cc")) {
    char *w = next_word(&rest);
    app_continue(a, w ? atof(w) : 0.0);
  } else if (!strcmp(cmd, "set")) {
    cmd_set(a, rest);
    if (a->needs_run) app_run(a);
  } else if (!strcmp(cmd, "get")) {
    char *n = next_word(&rest);
    double v = 0;
    if (n && a->model_ok && a->eng->api.xppe_get(n, &v) == XPPE_OK)
      app_log(a, "%s = %.10g", n, v);
    else
      app_log(a, "error: no value named %s", n ? n : "?");
  } else if (!strcmp(cmd, "whos") || !strcmp(cmd, "who") || !strcmp(cmd, "list")) {
    cmd_whos(a);
  } else if (!strcmp(cmd, "ic")) {
    char *w = next_word(&rest);
    if (w && !strcmp(w, "last")) {
      bool live = a->live;
      a->live = false;
      cmd_ic_last(a);
      a->live = live;
    } else {
      app_log(a, "Use 'ic last', or 'set x=...' for one variable.");
    }
  } else if (!strcmp(cmd, "eq") || !strcmp(cmd, "equilibrium") ||
             !strcmp(cmd, "sg")) {
    app_equilibrium(a, NULL);
  } else if (!strcmp(cmd, "nullclines") || !strcmp(cmd, "nn")) {
    char *w = next_word(&rest);
    a->pp.nullclines = !(w && !strcmp(w, "off"));
    a->pp.nc_dirty = true;
    a->show_phase = true;
  } else if (!strcmp(cmd, "dirfield") || !strcmp(cmd, "dd")) {
    char *w = next_word(&rest);
    a->pp.dirfield = w ? strcmp(w, "off") != 0 : !a->pp.dirfield;
    a->show_phase = true;
  } else if (!strcmp(cmd, "erase") || !strcmp(cmd, "e")) {
    app_clear_curves(a);
    a->neq = 0;
    app_status(a, "Erased kept curves and equilibria.");
  } else if (!strcmp(cmd, "freeze")) {
    app_freeze(a);
  } else if (!strcmp(cmd, "fit") || !strcmp(cmd, "wf")) {
    app_status(a, "Double-click a plot to fit it to the data.");
  } else if (!strcmp(cmd, "use")) {
    char *s = next_word(&rest);
    if (s) app_apply_set(a, s);
    if (a->needs_run) app_run(a);
  } else if (!strcmp(cmd, "sets")) {
    int i;
    for (i = 0; i < a->nsets; i++) app_log(a, "  %s", a->sets[i]);
    if (!a->nsets) app_log(a, "No parameter sets in this model.");
  } else if (!strcmp(cmd, "auto")) {
    cmd_auto(a, rest);
  } else if (!strcmp(cmd, "open")) {
    char *p = skip_space(rest);
    if (*p) app_open(a, p);
  } else if (!strcmp(cmd, "new")) {
    app_new_model(a);
  } else if (!strcmp(cmd, "save")) {
    char *p = skip_space(rest);
    if (*p)
      app_save_editor(a, p);
    else
      app_apply_editor(a);
  } else if (!strcmp(cmd, "export")) {
    char *p = skip_space(rest);
    if (*p) app_export_csv(a, p);
  } else if (!strcmp(cmd, "show")) {
    char *w = next_word(&rest);
    if (w) snprintf(a->focus, sizeof a->focus, "%s", w);
  } else if (!strcmp(cmd, "theme")) {
    char *w = next_word(&rest);
    a->dark = w && !strcmp(w, "dark");
  } else if (!strcmp(cmd, "clc") || !strcmp(cmd, "clear")) {
    a->log_len = 0;
    a->log[0] = '\0';
  } else if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit") || !strcmp(cmd, "fq")) {
    a->quit = true;
  } else {
    app_log(a, "error: unknown command '%s'. Type 'help' for the list.", cmd);
  }
}
