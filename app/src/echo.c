/* The session as Python. Each action that changes the model or computes a
   result is written as the line of Python, with the xpplab package, that
   does the same. The command window shows the lines as they come, and
   File > Save session as Python script writes them out as a script that
   repeats the session's computations, number for number.

   The lines follow the engine's state rather than the clicks. Before an
   action, the numerical options, parameters and initial values that differ
   from what the lines so far have set are written; then the action; then
   the engine's state becomes the script's (echo_end). So a value typed,
   dragged on a slider or brought in by a set is written once and exactly,
   whatever way it came. A value change, or a live rerun while a slider is
   dragged, is written once the control is let go (echo_frame). */
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL3/SDL.h>

#include "app.h"
#include "export.h"

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

/* The numerical options that xpplab's Model.options reads and m.set()
   sets. Not MAXSTOR: XPPAUT doubles it whenever a run fills the storage,
   which the script's runs do too; one set by hand is written as given
   (app_set_option). */
static const char *option_names[ECHO_NOPTS] = {
    "TOTAL",   "DT",       "T0",       "TRANS",   "BOUND",  "TOL",    "ATOL",
    "DTMIN",   "DTMAX",    "NOUT",     "METH",    "DELAY",  "NEWT_TOL",
    "NEWT_ITER", "JAC_EPS", "POIMAP",  "POIVAR",  "POISGN", "POIPLN", "POISTOP",
    "NMESH"};

/* ---- Numbers, strings and paths ------------------------------------------ */

/* The shortest text that reads back as the same double, written as Python
   writes floats: plain digits unless the exponent is below -4 or above 15. */
void app_format_double(char *buf, size_t len, double v) {
  char e[40];
  int p, exp10;
  if (isnan(v)) {
    snprintf(buf, len, "nan");
    return;
  }
  if (isinf(v)) {
    snprintf(buf, len, v > 0 ? "inf" : "-inf");
    return;
  }
  for (p = 0; p < 16; p++) {
    snprintf(e, sizeof e, "%.*e", p, v);
    if (strtod(e, NULL) == v) break;
  }
  if (p == 16) snprintf(e, sizeof e, "%.16e", v);
  exp10 = atoi(strchr(e, 'e') + 1);
  if (exp10 < -4 || exp10 >= 16)
    snprintf(buf, len, "%s", e);
  else
    snprintf(buf, len, "%.*f", p > exp10 ? p - exp10 : 0, v);
}

const char *echo_num(double v, char *buf, size_t len) {
  if (isnan(v))
    snprintf(buf, len, "float(\"nan\")");
  else if (isinf(v))
    snprintf(buf, len, v > 0 ? "float(\"inf\")" : "-float(\"inf\")");
  else {
    app_format_double(buf, len, v);
    /* A float literal, so that -0.0 keeps its sign. */
    if (!strpbrk(buf, ".eE") && strlen(buf) + 3 < len) strcat(buf, ".0");
  }
  return buf;
}

const char *echo_str(const char *s, char *buf, size_t len) {
  size_t k = 0;
  if (len < 3) {
    if (len) buf[0] = '\0';
    return buf;
  }
  buf[k++] = '"';
  for (; *s && k + 6 < len; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '\\' || c == '"') {
      buf[k++] = '\\';
      buf[k++] = (char)c;
    } else if (c < 32 || c == 127) {
      k += (size_t)snprintf(buf + k, len - k, "\\x%02x", c);
    } else {
      buf[k++] = (char)c; /* UTF-8 passes through: scripts are UTF-8 */
    }
  }
  buf[k++] = '"';
  buf[k] = '\0';
  return buf;
}

void echo_abs_path(const char *path, char *out, size_t len) {
#ifdef _WIN32
  if (!_fullpath(out, path, len)) snprintf(out, len, "%s", path);
#else
  char cwd[1024];
  if (path[0] == '/' || !getcwd(cwd, sizeof cwd))
    snprintf(out, len, "%s", path);
  else
    snprintf(out, len, "%s/%s", cwd, path);
#endif
}

/* ---- The lines -------------------------------------------------------------- */

static void append(Echo *e, const char *line) {
  size_t n = strlen(line);
  if (e->len + n + 2 > e->cap) {
    size_t cap = e->cap ? e->cap : 4096;
    char *t;
    while (e->len + n + 2 > cap) cap *= 2;
    t = realloc(e->text, cap);
    if (!t) return;
    e->text = t;
    e->cap = cap;
  }
  memcpy(e->text + e->len, line, n);
  e->len += n;
  e->text[e->len++] = '\n';
  e->text[e->len] = '\0';
}

void echo_line(App *a, const char *fmt, ...) {
  char buf[8192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  append(&a->echo, buf);
  a->echo.wrote = true;
  if (a->echo.console) app_log(a, ">>> %s", buf);
}

/* ---- Values ----------------------------------------------------------------- */

static bool is_model_name(const App *a, const char *name) {
  int i;
  for (i = 0; i < a->nvars; i++)
    if (SDL_strcasecmp(a->vars[i], name) == 0) return true;
  for (i = 0; i < a->npars; i++)
    if (SDL_strcasecmp(a->pars[i], name) == 0) return true;
  return false;
}

bool echo_knows(App *a, const char *name) {
  int k;
  if (is_model_name(a, name)) return true;
  for (k = 0; k < ECHO_NOPTS; k++)
    if (SDL_strcasecmp(option_names[k], name) == 0) return true;
  return false;
}

static double value_of(App *a, const char *name) {
  double v = 0;
  a->eng->api.xppe_get(name, &v);
  return v;
}

/* xppe_get() looks among the variables and parameters first, so an option
   that a model also uses as a name cannot be read (see get_option() in
   app_analysis.c): it is left alone. */
static double option_value(App *a, int k) {
  return is_model_name(a, option_names[k]) ? NAN : value_of(a, option_names[k]);
}

static bool same(double x, double y) { return x == y || (isnan(x) && isnan(y)); }

/* The engine's state becomes the script's. */
static void sync_script(App *a) {
  Echo *e = &a->echo;
  int i;
  if (!a->model_ok || !e->loaded) return;
  for (i = 0; i < e->npar && i < a->npars; i++) e->par[i] = value_of(a, a->pars[i]);
  for (i = 0; i < e->nic && i < a->nvars; i++) e->ic[i] = value_of(a, a->vars[i]);
  for (i = 0; i < ECHO_NOPTS; i++) e->opt[i] = option_value(a, i);
  e->dirty = false;
}

/* Parameters (target "m.parameters") or initial values
   ("m.initial_conditions"): one change is an assignment, several are
   update() calls of a readable length. */
static void write_named(App *a, const char *target, char (*names)[APP_NAME_LEN],
                        double *script, int n) {
  char line[1200], item[300], name[160], num[64];
  size_t k = 0;
  int i, changed = 0, last = -1;
  for (i = 0; i < n; i++)
    if (!same(script[i], value_of(a, names[i]))) {
      changed++;
      last = i;
    }
  if (changed == 1) {
    script[last] = value_of(a, names[last]);
    echo_line(a, "%s[%s] = %s", target, echo_str(names[last], name, sizeof name),
              echo_num(script[last], num, sizeof num));
    return;
  }
  for (i = 0; i < n && changed > 1; i++) {
    double v = value_of(a, names[i]);
    if (same(script[i], v)) continue;
    snprintf(item, sizeof item, "%s: %s", echo_str(names[i], name, sizeof name),
             echo_num(v, num, sizeof num));
    if (k > 0 && k + strlen(item) > 72) {
      echo_line(a, "%s.update({%s})", target, line);
      k = 0;
    }
    k += (size_t)snprintf(line + k, sizeof line - k, "%s%s", k ? ", " : "", item);
    script[i] = v;
  }
  if (k) echo_line(a, "%s.update({%s})", target, line);
}

/* An option's value as XPPAUT reads it: most as numbers, but the Poincare
   section by its kind and its variable by name. */
static const char *option_text(App *a, int k, double v, char *buf, size_t len) {
  static const char *poimap[] = {"0", "section", "max", "period"};
  int m = (int)v;
  if (strcmp(option_names[k], "POIMAP") == 0) {
    snprintf(buf, len, "\"%s\"", m >= 0 && m < 4 ? poimap[m] : "0");
    return buf;
  }
  if (strcmp(option_names[k], "POIVAR") == 0)
    return echo_str(m >= 1 && m <= a->nvars ? a->vars[m - 1] : "T", buf, len);
  return echo_num(v, buf, len);
}

/* Choosing a method sets DT (discrete) or NOUT (the adaptive methods) in
   XPPAUT, so after a new method both are written as they are now. */
static void write_options(App *a) {
  Echo *e = &a->echo;
  char line[1200], num[200], low[32];
  size_t k = 0;
  int i, j, meth = -1;
  for (i = 0; i < ECHO_NOPTS; i++)
    if (strcmp(option_names[i], "METH") == 0) meth = i;
  if (meth >= 0) {
    double v = option_value(a, meth);
    if (!same(e->opt[meth], v)) {
      echo_line(a, "m.set(meth=\"%s\")", app_method_name((int)v));
      e->opt[meth] = v;
      for (i = 0; i < ECHO_NOPTS; i++)
        if (!strcmp(option_names[i], "DT") || !strcmp(option_names[i], "NOUT"))
          e->opt[i] = NAN;
    }
  }
  for (i = 0; i < ECHO_NOPTS; i++) {
    double v = option_value(a, i);
    if (i == meth || same(e->opt[i], v) || (isnan(v) && !isnan(e->opt[i]))) continue;
    for (j = 0; option_names[i][j] && j < 31; j++)
      low[j] = (char)tolower((unsigned char)option_names[i][j]);
    low[j] = '\0';
    k += (size_t)snprintf(line + k, sizeof line - k, "%s%s=%s", k ? ", " : "", low,
                          option_text(a, i, v, num, sizeof num));
    e->opt[i] = v;
  }
  if (k) echo_line(a, "m.set(%s)", line);
}

void echo_values(App *a) {
  Echo *e = &a->echo;
  if (!a->model_ok || !e->loaded) return;
  write_options(a);
  write_named(a, "m.parameters", a->pars, e->par, e->npar < a->npars ? e->npar : a->npars);
  write_named(a, "m.initial_conditions", a->vars, e->ic, e->nic < a->nvars ? e->nic : a->nvars);
  e->dirty = false;
}

/* What is waiting: changed values, then a live rerun. */
static void flush(App *a) {
  Echo *e = &a->echo;
  if (!a->model_ok || !e->loaded) return;
  echo_values(a);
  if (e->pending_run) {
    e->pending_run = false;
    echo_line(a, "sol = m.run()");
  }
}

/* ---- Actions ---------------------------------------------------------------- */

bool echo_begin(App *a) {
  Echo *e = &a->echo;
  if (e->depth++ > 0 || e->live) return false;
  flush(a);
  e->wrote = false;
  return true;
}

/* Only after an action that wrote its line: one that failed may still have
   changed values, which the next line then writes. */
void echo_end(App *a) {
  Echo *e = &a->echo;
  if (e->depth > 0) e->depth--;
  if (e->depth == 0 && !e->live && e->wrote) sync_script(a);
}

void echo_frame(App *a, bool busy) {
  Echo *e = &a->echo;
  if (busy || e->depth > 0 || !(e->pending_run || e->dirty)) return;
  flush(a);
  sync_script(a);
}

void echo_model_loaded(App *a) {
  Echo *e = &a->echo;
  char path[2100];
  free(e->par);
  free(e->ic);
  e->npar = a->npars;
  e->nic = a->nvars;
  e->par = calloc((size_t)e->npar + 1, sizeof(double));
  e->ic = calloc((size_t)e->nic + 1, sizeof(double));
  e->pending_run = false;
  e->nc_written = false;
  e->frozen_list = false;
  e->table[0] = '\0';
  e->sweep_mode = -1;
  e->loaded = e->par && e->ic;
  echo_line(a, "m = xs.load(%s)", echo_str(a->model_path, path, sizeof path));
  sync_script(a);
}

/* Figures are drawn again with xpplab's matplotlib plots: the same data,
   not the same pixels. The phase plane leaves out the trajectory and the
   equilibrium that Model.phase_plane() would compute afresh, and draws the
   run as it is. */
void echo_figure(App *a, int figure, const char *path) {
  char p[2100], q[2200], x[160], y[160], n[4][64], cols[1024];
  bool outer;
  size_t k = 0;
  int j;
  if (!a->model_ok || !a->echo.loaded) return;
  outer = echo_begin(a);
  echo_abs_path(path, p, sizeof p);
  echo_str(p, q, sizeof q);
  if (outer) {
    a->echo.plt = true;
    if (figure == FIG_TIME) {
      cols[0] = '\0';
      for (j = 1; j < a->ncols; j++)
        if (a->plot_col[j])
          k += (size_t)snprintf(cols + k, sizeof cols - k, "%s%s", k ? ", " : "",
                                echo_str(a->cols[j], x, sizeof x));
      echo_line(a, "sol.plot(%s).figure.savefig(%s)", cols, q);
    } else if (figure == FIG_PHASE && a->nvars > 1) {
      const FigureState *g = &g_figures[FIG_PHASE];
      double lim[4] = {a->pp.xlo, a->pp.xhi, a->pp.ylo, a->pp.yhi};
      if (g->valid) {
        lim[0] = g->xmin;
        lim[1] = g->xmax;
        lim[2] = g->ymin;
        lim[3] = g->ymax;
      }
      const char *fr = a->pp.nfrozen > 0 && a->echo.frozen_list ? ", frozen=frozen" : "";
      echo_str(a->vars[a->pp.xvar], x, sizeof x);
      echo_str(a->vars[a->pp.yvar], y, sizeof y);
      if (lim[1] > lim[0] && lim[3] > lim[2])
        echo_line(a, "ax = m.phase_plane(%s, %s, xlim=(%s, %s), ylim=(%s, %s), "
                     "trajectory=False, equilibrium=False%s)",
                  x, y, echo_num(lim[0], n[0], 64), echo_num(lim[1], n[1], 64),
                  echo_num(lim[2], n[2], 64), echo_num(lim[3], n[3], 64), fr);
      else
        echo_line(a, "ax = m.phase_plane(%s, %s, trajectory=False, "
                     "equilibrium=False%s)", x, y, fr);
      echo_line(a, "ax.plot(sol[%s], sol[%s])", x, y);
      echo_line(a, "ax.figure.savefig(%s)", q);
    } else if (figure == FIG_AUTO) {
      int v = a->au.yvar;
      echo_line(a, "m.auto.diagram.plot(%s).figure.savefig(%s)",
                v >= 0 && v < a->nvars ? echo_str(a->vars[v], x, sizeof x) : "\"norm\"", q);
    } else if (figure == FIG_AUTO2) {
      echo_line(a, "m.auto.diagram.plot().figure.savefig(%s)", q);
    }
  }
  echo_end(a);
}

bool echo_save(App *a, const char *path) {
  Echo *e = &a->echo;
  char date[64];
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  FILE *f;
  if (e->depth == 0) {
    flush(a);
    sync_script(a);
  }
  f = fopen(path, "wb");
  if (!f) {
    app_log(a, "error: cannot write %s", path);
    return false;
  }
  if (!tm || !strftime(date, sizeof date, "%Y-%m-%d %H:%M", tm)) date[0] = '\0';
  fprintf(f, "# An XPP Lab session%s%s.\n", date[0] ? ", saved " : "", date);
  fputs("# Running this script with Python repeats the session's computations,\n"
        "# number for number. It needs the xpplab package: pip install xpplab\n",
        f);
  if (e->plt) fputs("# The figures also need matplotlib: pip install matplotlib\n", f);
  fputs("\nimport xpplab as xs\n\n", f);
  if (e->text) fwrite(e->text, 1, e->len, f);
  fclose(f);
  app_log(a, "Saved the session as a Python script: %s", path);
  app_status(a, "Saved the session as %s", path);
  return true;
}
