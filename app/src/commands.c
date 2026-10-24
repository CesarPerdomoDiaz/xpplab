/* Commands typed in the command window. Toolbar buttons and XPPAUT's
   keyboard commands run through here too, so both always behave alike. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "app.h"
#include "export.h"

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
    "  range NAME FROM TO N [VAR|lyap] [continue]\n"
    "                           XPPAUT's Range: N runs over a parameter or\n"
    "                           initial value; with VAR plot its max and min\n"
    "                           (a brute-force diagram), with lyap the\n"
    "                           Liapunov exponent; continue starts each run\n"
    "                           where the last ended (also: sweep)\n"
    "  bvp                      solve the boundary-value problem (bndry)\n"
    "  ani FILE.ani             open an XPPAUT animation of the run;\n"
    "                           ani play, ani stop, ani row N\n"
    "  view3d X Y Z             show three columns in the 3-D view\n"
    "Analysis of the last run (XPPAUT's Numerics > stocHastic, Averaging):\n"
    "  spectrum COL [COL2] [psd|cross|coherence] [N]\n"
    "                           spectral density, windows of N points\n"
    "  fourier COL | power COL  Fourier series: cos/sin, amplitude/phase\n"
    "  hist COL [BINS] [LO HI]  histogram of a column\n"
    "  hist2d X Y [BINS]        two-dimensional histogram\n"
    "  xcorr COL [COL2] [LAGS]  auto- or cross-correlation\n"
    "  mean COL                 mean, deviation, Liapunov exponent\n"
    "  prc                      phase response curves of the limit cycle\n"
    "  hfun F1; F2; ...         H function: one coupling per variable,\n"
    "                           primed names are the other cell (V'-V)\n"
    "  fitdata FILE VARS COLS PARS\n"
    "                           fit parameters to data (comma lists)\n"
    "  open FILE | new | save   models\n"
    "  export FILE.csv          save the last run as CSV\n"
    "  export FILE.svg [WHICH]  save a figure (time, phase, auto, auto2);\n"
    "                           .png saves it as it looks on screen\n"
    "  show WINDOW              bring a window forward: time, phase, auto,\n"
    "                           editor, data, analysis, 3d, animator, help,\n"
    "                           console, workspace\n"
    "  help [TOPIC]             open the manual (topics: tutorial, auto,\n"
    "                           analysis, odefile, numerics, commands, xppaut)\n";

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

static int col_index(App *a, const char *name) {
  int j;
  for (j = 0; j < a->ncols; j++)
    if (SDL_strcasecmp(a->cols[j], name) == 0) return j;
  app_log(a, "error: no column '%s'; type 'whos' for the names", name);
  return -1;
}

static bool is_number(const char *s) {
  char *end;
  if (!s) return false;
  strtod(s, &end);
  return end != s && *end == '\0';
}

/* spectrum, fourier, power, hist, hist2d, xcorr, mean, prc, hfun, fitdata */
static void cmd_analysis(App *a, const char *cmd, char *rest) {
  AnalysisUI *n = &a->an;
  char *w = NULL;
  if (!a->model_ok) {
    app_log(a, "error: open a model first");
    return;
  }
  if (!strcmp(cmd, "prc")) {
    n->tool = AN_PRC;
    if ((w = next_word(&rest)) && is_number(w)) n->transient = atof(w);
  } else if (!strcmp(cmd, "hfun")) {
    int i = 0;
    char *f;
    n->tool = AN_HFUNCTION;
    for (f = strtok(rest, ";"); f && i < a->nvars; f = strtok(NULL, ";"), i++)
      snprintf(n->coupling[i], sizeof n->coupling[i], "%s", skip_space(f));
    if (i > 0 && i < a->nvars)
      app_log(a, "note: %d of %d coupling formulas given; the rest are kept",
              i, a->nvars);
  } else if (!strcmp(cmd, "fitdata")) {
    char *file = next_word(&rest), *vars = next_word(&rest),
         *cols = next_word(&rest), *pars = next_word(&rest);
    if (!pars) {
      app_log(a, "Write: fitdata FILE VARS COLS PARS, e.g. fitdata v.dat V 2 "
                 "gca,gk");
      return;
    }
    n->tool = AN_FIT;
    snprintf(n->fit_file, sizeof n->fit_file, "%s", file);
    snprintf(n->fit_vars, sizeof n->fit_vars, "%s", vars);
    snprintf(n->fit_cols, sizeof n->fit_cols, "%s", cols);
    snprintf(n->fit_pars, sizeof n->fit_pars, "%s", pars);
  } else {
    int c;
    if (!(w = next_word(&rest))) {
      app_log(a, "error: give a column, e.g. %s V", cmd);
      return;
    }
    if ((c = col_index(a, w)) < 0) return;
    n->col = c;
    n->col2 = c;
    if (!strcmp(cmd, "spectrum") || !strcmp(cmd, "psd")) {
      n->tool = AN_SPECTRUM;
      n->spec_kind = 0;
      while ((w = next_word(&rest))) {
        if (!strcmp(w, "psd")) n->spec_kind = 0;
        else if (!strcmp(w, "cross")) n->spec_kind = 1;
        else if (!strcmp(w, "coherence")) n->spec_kind = 2;
        else if (is_number(w)) n->spec_len = atoi(w);
        else if ((c = col_index(a, w)) >= 0) {
          n->col2 = c;
          if (n->spec_kind == 0) n->spec_kind = 1;
        } else
          return;
      }
    } else if (!strcmp(cmd, "fourier") || !strcmp(cmd, "fft") ||
               !strcmp(cmd, "power")) {
      n->tool = AN_FOURIER;
      n->polar = !strcmp(cmd, "power");
    } else if (!strcmp(cmd, "hist")) {
      char *lo, *hi;
      n->tool = AN_HISTOGRAM;
      if ((w = next_word(&rest))) n->bins = atoi(w);
      lo = next_word(&rest);
      hi = next_word(&rest);
      n->auto_range = !(lo && hi);
      if (lo && hi) {
        n->lo = atof(lo);
        n->hi = atof(hi);
      }
    } else if (!strcmp(cmd, "hist2d")) {
      n->tool = AN_HISTOGRAM2D;
      if (!(w = next_word(&rest)) || (c = col_index(a, w)) < 0) {
        app_log(a, "error: write hist2d X Y [BINS]");
        return;
      }
      n->col2 = c;
      if ((w = next_word(&rest))) n->bins = n->bins2 = atoi(w);
      n->auto_range = true;
    } else if (!strcmp(cmd, "xcorr")) {
      n->tool = AN_CORRELATION;
      while ((w = next_word(&rest))) {
        if (is_number(w)) n->lags = atoi(w);
        else if ((c = col_index(a, w)) >= 0) n->col2 = c;
        else return;
      }
    } else {
      n->tool = AN_STATISTICS;
    }
  }
  a->show_analysis = true;
  snprintf(a->focus, sizeof a->focus, "analysis");
  app_analysis_run(a);
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
    app_clear_equilibria(a);
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
  } else if (!strcmp(cmd, "spectrum") || !strcmp(cmd, "psd") ||
             !strcmp(cmd, "fourier") || !strcmp(cmd, "fft") ||
             !strcmp(cmd, "power") || !strcmp(cmd, "hist") ||
             !strcmp(cmd, "hist2d") || !strcmp(cmd, "xcorr") ||
             !strcmp(cmd, "mean") || !strcmp(cmd, "prc") ||
             !strcmp(cmd, "hfun") || !strcmp(cmd, "fitdata")) {
    cmd_analysis(a, cmd, rest);
  } else if (!strcmp(cmd, "bvp")) {
    app_bvp(a);
  } else if (!strcmp(cmd, "ani")) {
    char *p = skip_space(rest);
    if (!strncmp(p, "play", 4) || !strncmp(p, "row ", 4) ||
        !strncmp(p, "stop", 4)) {
      /* ani play | ani stop | ani row N: drive the animator from scripts */
      a->ani.playing = p[0] == 'p';
      if (p[0] == 'r') a->ani.row = atoi(p + 4);
      a->show_ani = true;
      snprintf(a->focus, sizeof a->focus, "animator");
    } else if (!*p && a->ani.loaded) {
      a->show_ani = true;
      snprintf(a->focus, sizeof a->focus, "animator");
    } else if (!*p) {
      app_log(a, "Write: ani FILE.ani (the file's path, or its name next to "
                 "the model)");
    } else if (app_ani_load(a, p)) {
      a->show_ani = true;
      snprintf(a->focus, sizeof a->focus, "animator");
    }
  } else if (!strcmp(cmd, "view3d")) {
    char *w[3];
    int k, c;
    for (k = 0; k < 3; k++) w[k] = next_word(&rest);
    for (k = 0; k < 3 && w[k]; k++) {
      if ((c = col_index(a, w[k])) < 0) break;
      if (k == 0) a->v3.x = c;
      if (k == 1) a->v3.y = c;
      if (k == 2) a->v3.z = c;
    }
    a->show_view3d = true;
    snprintf(a->focus, sizeof a->focus, "3d");
  } else if (!strcmp(cmd, "sweep") || !strcmp(cmd, "range")) {
    /* range NAME FROM TO [STEPS] [VAR | lyap] [continue]: NAME is a
       parameter or a variable (its initial value); with VAR the max/min of
       VAR, with lyap the Liapunov exponent, otherwise keep the runs. */
    char *par = next_word(&rest), *from = next_word(&rest), *to = next_word(&rest);
    char *w;
    int p = par ? app_find_par(a, par) : -1, ic = -1;
    if (p < 0 && par) ic = app_find_var(a, par);
    if ((p < 0 && ic < 0) || !from || !to) {
      app_log(a, "Write: range NAME FROM TO [STEPS] [VAR | lyap] [continue]   "
                 "(NAME: a parameter or initial value; no VAR keeps the runs)");
    } else {
      a->sw.over_ic = p < 0;
      a->sw.par = p >= 0 ? p : ic;
      a->sw.from = atof(from);
      a->sw.to = atof(to);
      a->sw.steps = 20;
      a->sw.mode = SWEEP_OVERLAY;
      a->sw.continue_runs = false;
      while ((w = next_word(&rest))) {
        if (is_number(w))
          a->sw.steps = atoi(w);
        else if (!strcmp(w, "continue"))
          a->sw.continue_runs = true;
        else if (!strcmp(w, "lyap") || !strcmp(w, "liapunov"))
          a->sw.mode = SWEEP_LYAPUNOV;
        else if (app_find_var(a, w) >= 0) {
          a->sw.mode = SWEEP_MAXMIN;
          a->sw.var = app_find_var(a, w);
        } else
          app_log(a, "note: ignored '%s'", w);
      }
      app_sweep(a);
    }
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
    /* export FILE.csv | export FILE.svg|FILE.png [time|phase|auto|auto2] */
    char *file = next_word(&rest), *which = next_word(&rest);
    size_t n = file ? strlen(file) : 0;
    if (!file) {
      app_log(a, "Write export FILE.csv, or export FILE.svg phase.");
    } else if (n > 4 && (!SDL_strcasecmp(file + n - 4, ".svg") ||
                         !SDL_strcasecmp(file + n - 4, ".png"))) {
      FigureKind k = FIG_PHASE;
      if (which && !strcmp(which, "time")) k = FIG_TIME;
      if (which && !strcmp(which, "auto")) k = FIG_AUTO;
      if (which && !strcmp(which, "auto2")) k = FIG_AUTO2;
      if (!SDL_strcasecmp(file + n - 4, ".svg"))
        export_svg(a, k, file);
      else
        export_png(k, file);
    } else {
      app_export_csv(a, file);
    }
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
