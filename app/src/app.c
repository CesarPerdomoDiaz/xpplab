/* The application's actions; see app.h. */
#include "app.h"
#include "export.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

App *g_app;

static const char *method_names[] = {
    "discrete", "euler", "modeuler", "rungekutta", "adams",
    "gear",     "volterra", "backeul", "qualrk", "stiff",
    "cvode",    "5dp",   "83dp",     "2rb",    "ymp"};

/* ---- Logging ----------------------------------------------------------- */

void app_log(App *a, const char *fmt, ...) {
  char buf[4096];
  va_list ap;
  size_t n;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  n = strlen(buf);
  if (a->log_len + n + 2 > a->log_cap) {
    /* Keep the newest half when the log grows too large. */
    if (a->log_len > a->log_cap / 2) {
      size_t drop = a->log_len - a->log_cap / 2;
      memmove(a->log, a->log + drop, a->log_len - drop + 1);
      a->log_len -= drop;
    }
    if (a->log_len + n + 2 > a->log_cap) return;
  }
  memcpy(a->log + a->log_len, buf, n);
  a->log_len += n;
  if (n == 0 || buf[n - 1] != '\n') a->log[a->log_len++] = '\n';
  a->log[a->log_len] = '\0';
  a->log_scroll = true;
}

void app_status(App *a, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(a->status, sizeof a->status, fmt, ap);
  va_end(ap);
}

/* ---- Small helpers ------------------------------------------------------ */

static char *read_file(const char *path, size_t *len) {
  FILE *f = fopen(path, "rb");
  long n;
  char *buf;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) n = 0;
  buf = malloc((size_t)n + 1);
  if (buf) {
    n = (long)fread(buf, 1, (size_t)n, f);
    buf[n] = '\0';
    if (len) *len = (size_t)n;
  }
  fclose(f);
  return buf;
}

static void set_editor_text(App *a, const char *text) {
  size_t n = strlen(text);
  size_t cap = n + 65536;
  if (cap < 1 << 20) cap = 1 << 20;
  if (cap > a->editor_cap) {
    free(a->editor);
    a->editor = malloc(cap);
    a->editor_cap = cap;
  }
  memcpy(a->editor, text, n + 1);
  a->editor_dirty = false;
}

static void dir_of(const char *path, char *out, size_t len) {
  const char *slash = NULL, *p;
  for (p = path; *p; p++)
    if (*p == '/' || *p == '\\') slash = p;
  if (!slash) {
    snprintf(out, len, ".");
    return;
  }
  snprintf(out, len, "%.*s", (int)(slash - path), path);
}

static const char *base_of(const char *path) {
  const char *b = path, *p;
  for (p = path; *p; p++)
    if (*p == '/' || *p == '\\') b = p + 1;
  return b;
}

static void free_traj(Trajectory *t) {
  int j;
  if (t->cols)
    for (j = 0; j < t->ncols; j++) free(t->cols[j]);
  free(t->cols);
  free(t->names);
  memset(t, 0, sizeof *t);
}

static void free_found(App *a) {
  FoundEquilibria *f = &a->found;
  free(f->x);
  free(f->re);
  free(f->im);
  free(f->unstable);
  memset(f, 0, sizeof *f);
}

/* The engine's result table as a Trajectory (its columns, by name). */
static void fetch_table(App *a, Trajectory *t) {
  xpp_engine_api *x = &a->eng->api;
  int rows = x->xppe_result_rows(), ncols = x->xppe_result_count(), j;
  free_traj(t);
  t->nrows = rows;
  t->ncols = ncols;
  t->names = calloc((size_t)ncols + 1, APP_NAME_LEN);
  t->cols = calloc((size_t)ncols + 1, sizeof(double *));
  for (j = 0; j < ncols; j++) {
    snprintf(t->names[j], APP_NAME_LEN, "%s", x->xppe_result_name(j));
    t->cols[j] = malloc(sizeof(double) * (size_t)(rows > 0 ? rows : 1));
    x->xppe_result_column(j, t->cols[j], rows);
  }
}

static void free_frozen_nullclines(PhasePlane *p);

static void free_model_cache(App *a) {
  int i;
  free(a->vars);
  free(a->pars);
  free(a->cols);
  free(a->sets);
  free(a->par);
  free(a->ic);
  free(a->plot_col);
  a->vars = a->pars = a->cols = a->sets = NULL;
  a->par = NULL;
  a->ic = NULL;
  a->plot_col = NULL;
  a->nvars = a->npars = a->ncols = a->nsets = 0;
  free_traj(&a->traj);
  app_clear_curves(a);
  app_clear_equilibria(a);
  a->have_traj = false;
  free(a->pp.ncx);
  free(a->pp.ncy);
  a->pp.ncx = a->pp.ncy = NULL;
  a->pp.ncx_n = a->pp.ncy_n = 0;
  free_frozen_nullclines(&a->pp);
  if (a->ks.film.on) { /* a film of the model being closed stops */
    free(a->ks.film.ics);
    a->ks.film.ics = NULL;
    a->ks.film.on = false;
    echo_end(a);
  }
  free(a->au.info);
  free(a->au.umax);
  free(a->au.umin);
  a->au.info = NULL;
  a->au.umax = a->au.umin = NULL;
  a->au.count = 0;
  a->au.selected = -1;
}

/* ---- Lifecycle ---------------------------------------------------------- */

void app_init(App *a, const char *base_dir, bool ephemeral) {
  memset(a, 0, sizeof *a);
  g_app = a;
  snprintf(a->base_dir, sizeof a->base_dir, "%s", base_dir);
  snprintf(a->engine_path, sizeof a->engine_path, "%s%s", base_dir,
           xpp_instance_library_name());
  snprintf(a->examples_dir, sizeof a->examples_dir, "%sexamples", base_dir);
  a->log_cap = 512 * 1024;
  a->log = malloc(a->log_cap);
  a->log[0] = '\0';
  a->show_browser = a->show_workspace = a->show_editor = a->show_time =
      a->show_phase = a->show_auto = a->show_console = a->show_help = true;
  a->show_data = false;
  a->live = true;
  a->ui_scale = 1.0f;
  a->classic_keys = true;
  a->pp.grid = 100;
  a->pp.click_to_start = true;
  a->ks.ms = 250;
  a->ks.loop = true;
  a->ks.source = FIG_PHASE;
  a->pp.show_equilibria = true;
  a->au.selected = -1;
  a->au.yvar = 0;
  a->echo.console = true;
  a->echo.sweep_mode = -1;
  snprintf(a->help_page, sizeof a->help_page, "welcome");
  app_fig_reset(a);
  set_editor_text(a, "");
  a->ephemeral = ephemeral;
  /* Until the user picks a theme, follow the system's light or dark mode. */
  if (!ephemeral) a->dark = SDL_GetSystemTheme() == SDL_SYSTEM_THEME_DARK;
  if (!ephemeral) app_load_settings(a);
  app_log(a, "XPP Lab %s - XPPAUT 8.0 engine. Type 'help' for commands.",
          XPPS_VERSION);
  app_status(a, "Open a model (File > Open) or pick an example on the left.");
}

void app_shutdown(App *a) {
  if (!a->ephemeral) app_save_settings(a);
  free_model_cache(a);
  app_kine_clear(a);
  free(a->sw.x);
  free(a->sw.ymax);
  free(a->sw.ymin);
  free(a->sw.x2);
  free_traj(&a->sw.eqt);
  free_found(a);
  app_analysis_free(a);
  app_many_free(a);
  fig_free(&a->fg.fig);
  if (a->eng) xpp_instance_close(a->eng);
  a->eng = NULL;
  free(a->editor);
  free(a->log);
  free(a->echo.text);
  free(a->echo.par);
  free(a->echo.ic);
}

/* ---- Loading ----------------------------------------------------------- */

/* Find the line of the editor that an engine error refers to. */
static int guess_error_line(App *a, const char *log) {
  const char *p = strstr(log, "line ");
  const char *err;
  if (p && isdigit((unsigned char)p[5])) return atoi(p + 5);
  /* "Error in <statement>" or "Illegal syntax" after an echoed statement:
     look for the statement in the editor. */
  err = strstr(log, "Error in ");
  if (err) {
    char stmt[128];
    int k = 0, line = 1;
    const char *q, *e = a->editor;
    err += 9;
    while (*err && *err != '\n' && *err != '=' && *err != ' ' && k < 120)
      stmt[k++] = *err++;
    stmt[k] = '\0';
    if (k > 0)
      for (q = e; *q; q++) {
        if (strncmp(q, stmt, (size_t)k) == 0) return line;
        if (*q == '\n') line++;
      }
  }
  return 0;
}

static void copy_names(xpp_instance *e, int kind, int n,
                       char (*out)[APP_NAME_LEN]) {
  int i;
  for (i = 0; i < n; i++) {
    const char *s = e->api.xppe_name(kind, i);
    snprintf(out[i], APP_NAME_LEN, "%s", s ? s : "?");
  }
}

static double get_value(App *a, const char *name) {
  double v = 0;
  if (a->eng) a->eng->api.xppe_get(name, &v);
  return v;
}

void app_refresh_values(App *a) {
  int i;
  if (!a->model_ok) return;
  for (i = 0; i < a->npars; i++) a->par[i].value = get_value(a, a->pars[i]);
  for (i = 0; i < a->nvars; i++) a->ic[i] = get_value(a, a->vars[i]);
  a->total = get_value(a, "TOTAL");
  a->dt = get_value(a, "DT");
  a->t0 = get_value(a, "T0");
  a->trans = get_value(a, "TRANS");
  a->bound = get_value(a, "BOUND");
  a->tol = get_value(a, "TOL");
  a->atol = get_value(a, "ATOL");
  a->nout = (int)get_value(a, "NOUT");
  a->meth = (int)get_value(a, "METH");
  a->poimap = (int)get_value(a, "POIMAP");
  a->poivar = (int)get_value(a, "POIVAR");
  a->poisgn = (int)get_value(a, "POISGN");
  a->poistop = (int)get_value(a, "POISTOP");
  a->poipln = get_value(a, "POIPLN");
}

static void reset_slider_ranges(App *a) {
  int i;
  for (i = 0; i < a->npars; i++) {
    double v = a->par[i].value;
    if (v > 0) {
      a->par[i].lo = 0;
      a->par[i].hi = 2 * v;
    } else if (v < 0) {
      a->par[i].lo = 2 * v;
      a->par[i].hi = 0;
    } else {
      a->par[i].lo = -1;
      a->par[i].hi = 1;
    }
  }
}

static void read_auto_settings(App *a) {
  xpp_engine_api *x = &a->eng->api;
  double v;
  const char *name;
  int i;
  x->xppe_auto_get("DS", &a->au.ds);
  x->xppe_auto_get("DSMIN", &a->au.dsmin);
  x->xppe_auto_get("DSMAX", &a->au.dsmax);
  x->xppe_auto_get("PARMIN", &a->au.parmin);
  x->xppe_auto_get("PARMAX", &a->au.parmax);
  x->xppe_auto_get("NORMMIN", &a->au.normmin);
  x->xppe_auto_get("NORMMAX", &a->au.normmax);
  x->xppe_auto_get("NMAX", &v);
  a->au.nmax = (int)v;
  x->xppe_auto_get("NPR", &v);
  a->au.npr = (int)v;
  x->xppe_auto_get("NTST", &v);
  a->au.ntst = (int)v;
  a->au.main_par = a->au.second_par = -1;
  name = x->xppe_auto_parameter_name(x->xppe_auto_main_slot(0));
  if (name) a->au.main_par = app_find_par(a, name);
  name = x->xppe_auto_parameter_name(x->xppe_auto_main_slot(1));
  if (name) a->au.second_par = app_find_par(a, name);
  if (a->au.main_par < 0 && a->npars > 0) a->au.main_par = 0;
  if (a->au.second_par < 0 || a->au.second_par == a->au.main_par)
    for (i = 0; i < a->npars; i++)
      if (i != a->au.main_par) {
        a->au.second_par = i;
        break;
      }
}

static bool load_engine(App *a) {
  char err[512], dir[1024];
  int rc;
  free_model_cache(a);
  if (a->eng) xpp_instance_close(a->eng);
  a->eng = NULL;
  a->model_ok = false;
  a->echo.loaded = false;
  a->load_error[0] = '\0';
  a->error_line = 0;

  a->eng = xpp_instance_open(a->engine_path, err, sizeof err);
  if (!a->eng) {
    snprintf(a->load_error, sizeof a->load_error, "%s", err);
    app_log(a, "error: %s", err);
    return false;
  }
  /* XPPAUT opens tables and other files relative to the model. */
  dir_of(a->model_path, dir, sizeof dir);
  if (chdir(dir) != 0) app_log(a, "warning: cannot change to %s", dir);
  rc = a->eng->api.xppe_load(base_of(a->model_path), 0, NULL);
  if (rc != XPPE_OK) {
    const char *log = a->eng->api.xppe_log();
    snprintf(a->load_error, sizeof a->load_error, "%s",
             a->eng->api.xppe_error());
    a->error_line = guess_error_line(a, log);
    app_log(a, "error: %s", a->load_error);
    xpp_instance_close(a->eng);
    a->eng = NULL;
    return false;
  }
  a->model_ok = true;
  a->nvars = a->eng->api.xppe_count(XPPE_VARIABLES);
  a->npars = a->eng->api.xppe_count(XPPE_PARAMETERS);
  a->ncols = a->eng->api.xppe_count(XPPE_COLUMNS);
  a->nsets = a->eng->api.xppe_count(XPPE_SETS);
  a->vars = calloc((size_t)a->nvars + 1, APP_NAME_LEN);
  a->pars = calloc((size_t)a->npars + 1, APP_NAME_LEN);
  a->cols = calloc((size_t)a->ncols + 1, APP_NAME_LEN);
  a->sets = calloc((size_t)a->nsets + 1, APP_NAME_LEN);
  a->par = calloc((size_t)a->npars + 1, sizeof *a->par);
  a->ic = calloc((size_t)a->nvars + 1, sizeof *a->ic);
  a->plot_col = calloc((size_t)a->ncols + 1, sizeof *a->plot_col);
  copy_names(a->eng, XPPE_VARIABLES, a->nvars, a->vars);
  copy_names(a->eng, XPPE_PARAMETERS, a->npars, a->pars);
  copy_names(a->eng, XPPE_COLUMNS, a->ncols, a->cols);
  /* An "only" statement can leave time out of the output: put it back as
     column 0 (the engine serves it as column -1). */
  a->time_missing = a->ncols == 0 || SDL_strcasecmp(a->cols[0], "T") != 0;
  if (a->time_missing) {
    char(*c)[APP_NAME_LEN] = calloc((size_t)a->ncols + 2, APP_NAME_LEN);
    bool *pc = calloc((size_t)a->ncols + 2, sizeof *pc);
    memcpy(c + 1, a->cols, (size_t)a->ncols * APP_NAME_LEN);
    snprintf(c[0], APP_NAME_LEN, "T");
    free(a->cols);
    free(a->plot_col);
    a->cols = c;
    a->plot_col = pc;
    a->ncols++;
  }
  copy_names(a->eng, XPPE_SETS, a->nsets, a->sets);
  /* By default plot the state variables, not the auxiliaries. */
  {
    int j;
    for (j = 1; j < a->ncols; j++) a->plot_col[j] = j <= a->nvars && j <= 6;
  }
  app_refresh_values(a);
  reset_slider_ranges(a);
  a->pp.xvar = 0;
  a->pp.yvar = a->nvars > 1 ? 1 : 0;
  a->pp.nc_dirty = true;
  a->pp.fr_steps = 0; /* the Freeze form starts afresh for this model */
  /* Start the phase plane where the model file says (@ xp, yp, xlo, ...),
     as XPPAUT does. XP and YP count columns from 1; 0 means time. */
  {
    int xp = (int)get_value(a, "XP"), yp = (int)get_value(a, "YP");
    double xlo = get_value(a, "XLO"), xhi = get_value(a, "XHI");
    double ylo = get_value(a, "YLO"), yhi = get_value(a, "YHI");
    if (xp >= 1 && xp <= a->nvars && yp >= 1 && yp <= a->nvars && xp != yp) {
      a->pp.xvar = xp - 1;
      a->pp.yvar = yp - 1;
      if (xlo < xhi && ylo < yhi) {
        a->pp.win_xlo = xlo;
        a->pp.win_xhi = xhi;
        a->pp.win_ylo = ylo;
        a->pp.win_yhi = yhi;
        a->pp.set_window = true;
      }
    }
  }
  a->au.yvar = 0;
  read_auto_settings(a);
  /* Sweep defaults: the first parameter over its slider range. */
  free(a->sw.x);
  free(a->sw.ymax);
  free(a->sw.ymin);
  free(a->sw.x2);
  free_traj(&a->sw.eqt);
  memset(&a->sw, 0, sizeof a->sw);
  free_found(a);
  if (a->npars > 0) {
    a->sw.from = a->par[0].lo;
    a->sw.to = a->par[0].hi;
  }
  a->sw.steps = 40;
  a->sw.mode = SWEEP_MAXMIN;
  a->sw.transient = 0.5;
  app_analysis_reset(a);
  app_many_reset(a);
  echo_model_loaded(a);
  return true;
}

static bool open_model(App *a, const char *path, bool run, const char *load_line);

bool app_open(App *a, const char *path) { return open_model(a, path, true, NULL); }

bool app_open_quiet(App *a, const char *path, const char *load_line) {
  return open_model(a, path, false, load_line);
}

static bool open_model(App *a, const char *path, bool run, const char *load_line) {
  size_t len;
  char *text = read_file(path, &len);
  bool outer;
  if (!text) {
    app_log(a, "error: cannot read %s", path);
    app_status(a, "Cannot read %s", path);
    return false;
  }
  /* Keep the absolute path: the working directory follows the model. */
  {
#ifdef _WIN32
    char *full = _fullpath(NULL, path, 0);
#else
    char *full = realpath(path, NULL);
#endif
    snprintf(a->model_path, sizeof a->model_path, "%s", full ? full : path);
    free(full);
  }
  snprintf(a->model_name, sizeof a->model_name, "%s", base_of(path));
  set_editor_text(a, text);
  free(text);
  app_add_recent(a, a->model_path);
  outer = echo_begin(a);
  a->echo.load_line = load_line;
  if (!load_engine(a)) {
    a->echo.load_line = NULL;
    app_status(a, "%s has errors - see the editor", a->model_name);
    a->show_editor = true;
    echo_end(a);
    return false;
  }
  a->echo.load_line = NULL;
  app_log(a, "Loaded %s: %d variables, %d parameters.", a->model_name,
          a->nvars, a->npars);
  app_status(a, "Loaded %s", a->model_name);
  snprintf(a->focus, sizeof a->focus, "time");
  if (run && app_run(a) && outer) echo_line(a, "sol = m.run()");
  echo_end(a);
  return true;
}

bool app_open_text(App *a, const char *text, const char *name) {
  char path[1024];
  char *pref = app_pref_dir();
  FILE *f;
  snprintf(path, sizeof path, "%s%s", pref ? pref : "", name);
  SDL_free(pref);
  f = fopen(path, "wb");
  if (!f) {
    app_log(a, "error: cannot write %s", path);
    return false;
  }
  fputs(text, f);
  fclose(f);
  return app_open(a, path);
}

/* ---- Copies of the examples ----------------------------------------------- */

/* The examples come with the app: inside the bundle on macOS, next to the
   program elsewhere. Saving one in place would change the app itself (on
   macOS it breaks the app's signature, and run from the disk image the save
   fails), so an edited example is saved as a copy in Documents/XPP Lab. */

#ifdef _WIN32
#define SEP "\\"
#else
#define SEP "/"
#endif

bool app_is_example(const App *a, const char *path) {
  size_t n;
  bool in;
#ifdef _WIN32
  char *dir = _fullpath(NULL, a->examples_dir, 0);
#else
  char *dir = realpath(a->examples_dir, NULL);
#endif
  if (!dir) return false;
  n = strlen(dir);
#if defined(_WIN32) || defined(__APPLE__) /* case-insensitive file systems */
  in = SDL_strncasecmp(path, dir, n) == 0;
#else
  in = strncmp(path, dir, n) == 0;
#endif
  in = in && (path[n] == '/' || path[n] == '\\');
  free(dir);
  return in;
}

/* The folder for the copies, made if need be. XPPLAB_DOCUMENTS, if set,
   stands for Documents (tests/parity.py points it at its own folder). */
static bool copies_dir(char *out, size_t len) {
  const char *docs = SDL_getenv("XPPLAB_DOCUMENTS");
  size_t n;
  if (!docs || !*docs) docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
  if (!docs) docs = SDL_GetUserFolder(SDL_FOLDER_HOME);
  if (!docs) return false;
  n = strlen(docs);
  snprintf(out, len, "%s%sXPP Lab", docs,
           n && (docs[n - 1] == '/' || docs[n - 1] == '\\') ? "" : SEP);
  return SDL_CreateDirectory(out);
}

bool app_derived_path(const App *a, const char *suffix, char *out, size_t len) {
  char dir[1024];
  const char *name = base_of(a->model_path), *dot = strrchr(name, '.');
  int stem = dot ? (int)(dot - name) : (int)strlen(name);
  int k = (int)(name - a->model_path);
  if (app_is_example(a, a->model_path)) {
    if (!copies_dir(dir, sizeof dir)) return false;
  } else if (k > 0) {
    snprintf(dir, sizeof dir, "%.*s", k - 1, a->model_path);
  } else {
    snprintf(dir, sizeof dir, ".");
  }
  snprintf(out, len, "%s" SEP "%.*s_%s.ode", dir, stem, name, suffix);
  return true;
}

bool app_example_copy_path(const App *a, char *out, size_t len) {
  char dir[1024];
  const char *name = base_of(a->model_path), *dot = strrchr(name, '.');
  int stem = dot ? (int)(dot - name) : (int)strlen(name), k;
  if (!copies_dir(dir, sizeof dir)) return false;
  /* A name not yet taken: lecar.ode, then lecar 2.ode, lecar 3.ode, ... */
  for (k = 1; k < 1000; k++) {
    if (k == 1)
      snprintf(out, len, "%s" SEP "%s", dir, name);
    else
      snprintf(out, len, "%s" SEP "%.*s %d%s", dir, stem, name, k, dot ? dot : "");
    if (!SDL_GetPathInfo(out, NULL)) return true;
  }
  return false;
}

typedef struct {
  const App *a;
  const char *dest, *name;
  size_t stem;
} Companions;

/* Bring along the files an example reads next to it, unless the folder has
   them already: those named like the model (lorenz.ani, lecar.ode.set) and
   those its text names (pendx.ani in pendx.ode, a compiled library). */
static SDL_EnumerationResult SDLCALL copy_companion(void *ud, const char *dir,
                                                    const char *file) {
  const Companions *c = ud;
  char from[1100], to[1100];
  size_t n = strlen(file);
  if (strcmp(file, c->name) == 0 || (n && file[n - 1] == '~')) /* backups */
    return SDL_ENUM_CONTINUE;
  if (!(strncmp(file, c->name, c->stem) == 0 && file[c->stem] == '.') &&
      !strstr(c->a->editor, file))
    return SDL_ENUM_CONTINUE;
  snprintf(from, sizeof from, "%s%s", dir, file);
  snprintf(to, sizeof to, "%s" SEP "%s", c->dest, file);
  if (!SDL_GetPathInfo(to, NULL)) SDL_CopyFile(from, to);
  return SDL_ENUM_CONTINUE;
}

static void copy_companions(const App *a, const char *copy) {
  char src[1024], dest[1024];
  const char *dot;
  Companions c;
  dir_of(a->model_path, src, sizeof src);
  dir_of(copy, dest, sizeof dest);
  c.a = a;
  c.dest = dest;
  c.name = base_of(a->model_path);
  dot = strrchr(c.name, '.');
  c.stem = dot ? (size_t)(dot - c.name) : strlen(c.name);
  SDL_EnumerateDirectory(src, copy_companion, &c);
}

bool app_save_editor(App *a, const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) {
    app_log(a, "error: cannot write %s", path);
    return false;
  }
  fputs(a->editor, f);
  fclose(f);
  a->editor_dirty = false;
  if (path != a->model_path) {
    snprintf(a->model_path, sizeof a->model_path, "%s", path);
    snprintf(a->model_name, sizeof a->model_name, "%s", base_of(path));
    app_add_recent(a, path);
  }
  app_log(a, "Saved %s", path);
  return true;
}

bool app_apply_editor(App *a) {
  bool outer;
  if (!a->model_path[0]) {
    char *pref = app_pref_dir();
    snprintf(a->model_path, sizeof a->model_path, "%suntitled.ode",
             pref ? pref : "");
    SDL_free(pref);
    snprintf(a->model_name, sizeof a->model_name, "untitled.ode");
  }
  if (app_is_example(a, a->model_path)) {
    char copy[1024];
    if (!app_example_copy_path(a, copy, sizeof copy)) {
      app_log(a, "error: cannot make a folder for your copy in Documents: %s",
              SDL_GetError());
      return false;
    }
    copy_companions(a, copy);
    app_log(a, "note: the examples come with the app and stay as they are; "
               "your version of %s is a copy in %s.", a->model_name, copy);
    if (!app_save_editor(a, copy)) return false;
  } else if (!app_save_editor(a, a->model_path)) {
    return false;
  }
  outer = echo_begin(a);
  if (!load_engine(a)) {
    app_status(a, "The model has errors: %s", a->load_error);
    echo_end(a);
    return false;
  }
  app_log(a, "Reloaded %s.", a->model_name);
  app_status(a, "Reloaded %s", a->model_name);
  if (app_run(a) && outer) echo_line(a, "sol = m.run()");
  echo_end(a);
  return true;
}

void app_new_model(App *a) {
  static const char *tmpl =
      "# A new model. Edit it, then press Apply (Ctrl+Enter).\n"
      "#\n"
      "# The FitzHugh-Nagumo neuron:\n"
      "#   v is the membrane potential, w a recovery variable.\n"
      "\n"
      "v' = v - v^3/3 - w + I\n"
      "w' = eps*(v + a - b*w)\n"
      "\n"
      "par I=0.5, a=0.7, b=0.8, eps=0.08\n"
      "init v=-1, w=1\n"
      "\n"
      "@ total=200, dt=0.05, xp=v, yp=w\n"
      "@ xlo=-2.5, xhi=2.5, ylo=-1, yhi=2.5\n"
      "done\n";
  app_open_text(a, tmpl, "untitled.ode");
}

/* ---- Values -------------------------------------------------------------- */

int app_find_var(App *a, const char *name) {
  int i;
  for (i = 0; i < a->nvars; i++)
    if (SDL_strcasecmp(a->vars[i], name) == 0) return i;
  return -1;
}

int app_find_par(App *a, const char *name) {
  int i;
  for (i = 0; i < a->npars; i++)
    if (SDL_strcasecmp(a->pars[i], name) == 0) return i;
  return -1;
}

static void after_change(App *a) {
  a->pp.nc_dirty = true;
  a->echo.dirty = true;
  if (a->live) a->needs_run = true;
}

bool app_set_param(App *a, int i, double v) {
  if (!a->model_ok || i < 0 || i >= a->npars) return false;
  if (a->eng->api.xppe_set_number(a->pars[i], v) != XPPE_OK) return false;
  a->par[i].value = get_value(a, a->pars[i]);
  after_change(a);
  return true;
}

bool app_set_ic(App *a, int i, double v) {
  if (!a->model_ok || i < 0 || i >= a->nvars) return false;
  if (a->eng->api.xppe_set_number(a->vars[i], v) != XPPE_OK) return false;
  a->ic[i] = get_value(a, a->vars[i]);
  a->echo.dirty = true;
  if (a->live) a->needs_run = true;
  return true;
}

bool app_set_option(App *a, const char *name, const char *value) {
  if (!a->model_ok) return false;
  if (a->eng->api.xppe_set(name, value) != XPPE_OK) {
    app_log(a, "error: %s", a->eng->api.xppe_error());
    return false;
  }
  /* The options the echo follows are written when they differ; any other
     is written as it was given. */
  if (a->echo.depth == 0 && a->echo.loaded && !echo_knows(a, name)) {
    char low[64], v[600];
    int k;
    for (k = 0; name[k] && k < 63; k++) low[k] = (char)tolower((unsigned char)name[k]);
    low[k] = '\0';
    echo_line(a, "m.set(%s=%s)", low, echo_str(value, v, sizeof v));
  }
  app_refresh_values(a);
  after_change(a);
  return true;
}

bool app_apply_set(App *a, const char *name) {
  bool outer;
  char s[160];
  if (!a->model_ok) return false;
  outer = echo_begin(a);
  if (a->eng->api.xppe_apply_set(name) != XPPE_OK) {
    app_log(a, "error: %s", a->eng->api.xppe_error());
    echo_end(a);
    return false;
  }
  if (outer) echo_line(a, "m.apply_set(%s)", echo_str(name, s, sizeof s));
  echo_end(a);
  app_refresh_values(a);
  reset_slider_ranges(a);
  after_change(a);
  if (outer) a->echo.dirty = false; /* the set's values are the script's too */
  app_log(a, "Applied set %s.", name);
  return true;
}

/* ---- Integration ------------------------------------------------------- */

void app_clear_curves(App *a) {
  int i;
  for (i = 0; i < a->nfrozen; i++) free_traj(&a->frozen[i]);
  a->nfrozen = 0;
}

void app_freeze(App *a) {
  Trajectory *t;
  if (!a->have_traj || a->traj.nrows == 0) return;
  if (a->nfrozen == APP_MAX_FROZEN) {
    free_traj(&a->frozen[0]);
    memmove(&a->frozen[0], &a->frozen[1],
            sizeof a->frozen[0] * (APP_MAX_FROZEN - 1));
    a->nfrozen--;
  }
  t = &a->frozen[a->nfrozen++];
  *t = a->traj; /* take ownership of the arrays */
  memset(&a->traj, 0, sizeof a->traj);
  a->have_traj = false;
}

/* Copy the engine's current run into t. */
static void fetch_run(App *a, Trajectory *t) {
  xpp_engine_api *x = &a->eng->api;
  int rows = x->xppe_rows(), j;
  t->nrows = rows;
  t->ncols = a->ncols;
  t->names = calloc((size_t)a->ncols, APP_NAME_LEN);
  t->cols = calloc((size_t)a->ncols, sizeof(double *));
  for (j = 0; j < a->ncols; j++) {
    snprintf(t->names[j], APP_NAME_LEN, "%s", a->cols[j]);
    t->cols[j] = malloc(sizeof(double) * (size_t)(rows > 0 ? rows : 1));
    x->xppe_column(a->time_missing ? j - 1 : j, t->cols[j], rows);
  }
}

void app_fetch_current(App *a) {
  free_traj(&a->traj);
  fetch_run(a, &a->traj);
  snprintf(a->traj.label, sizeof a->traj.label, "run %d", a->nfrozen + 1);
  a->have_traj = a->traj.nrows > 0;
}

void app_free_trajectory(Trajectory *t) { free_traj(t); }

void app_fetch_run(App *a, Trajectory *t) {
  free_traj(t);
  fetch_run(a, t);
}

/* After a call that returned XPPE_ERR_FATAL: XPPAUT's code ended the
   program (exit()), and the engine refuses all further work. Rather than
   leave every button failing, open the model again in a fresh engine with
   the parameters, initial values and AUTO settings it had. The bifurcation
   diagram starts afresh. */
static void recover_engine(App *a, const char *what) {
  int np = a->npars, nv = a->nvars, i;
  double *par = malloc(sizeof(double) * (size_t)(np + 1));
  double *ic = malloc(sizeof(double) * (size_t)(nv + 1));
  AutoUI keep = a->au; /* its settings; its point arrays are rebuilt */
  if (!par || !ic) {
    free(par);
    free(ic);
    return;
  }
  memcpy(par, a->par, sizeof(double) * (size_t)np);
  memcpy(ic, a->ic, sizeof(double) * (size_t)nv);
  app_log(a, "%s: XPPAUT's code stopped the engine. Reopening %s with your "
             "values ...", what, a->model_name);
  if (load_engine(a) && a->npars == np && a->nvars == nv) {
    for (i = 0; i < np; i++) a->eng->api.xppe_set_number(a->pars[i], par[i]);
    for (i = 0; i < nv; i++) a->eng->api.xppe_set_number(a->vars[i], ic[i]);
    a->au.yvar = keep.yvar;
    a->au.main_par = keep.main_par;
    a->au.second_par = keep.second_par;
    a->au.ds = keep.ds;
    a->au.dsmin = keep.dsmin;
    a->au.dsmax = keep.dsmax;
    a->au.parmin = keep.parmin;
    a->au.parmax = keep.parmax;
    a->au.normmin = keep.normmin;
    a->au.normmax = keep.normmax;
    a->au.nmax = keep.nmax;
    a->au.npr = keep.npr;
    a->au.ntst = keep.ntst;
    app_auto_refresh(a);
    app_refresh_values(a);
    echo_values(a); /* the session reopens the model too, then sets them */
    app_log(a, "Reopened %s with your parameters, initial values and AUTO "
               "settings; the bifurcation diagram starts afresh.", a->model_name);
  }
  free(par);
  free(ic);
}

static bool run(App *a) {
  int rows, rc;
  xpp_engine_api *x;
  a->needs_run = false;
  if (!a->model_ok) return false;
  x = &a->eng->api;
  if ((rc = x->xppe_run()) != XPPE_OK) {
    app_log(a, "error: %s", x->xppe_error());
    app_status(a, "Integration failed: %s", x->xppe_error());
    if (rc == XPPE_ERR_FATAL) recover_engine(a, "Run");
    return false;
  }
  if (a->keep_curves && a->have_traj)
    app_freeze(a);
  else
    free_traj(&a->traj);
  fetch_run(a, &a->traj);
  rows = a->traj.nrows;
  snprintf(a->traj.label, sizeof a->traj.label, "run %d", a->nfrozen + 1);
  a->have_traj = true;
  {
    const char *log = x->xppe_log();
    if (strstr(log, "not completed")) {
      /* Name the culprit: auxiliary quantities count towards BOUND too. */
      const char *ob = strstr(log, "out of bounds at t");
      if (ob) {
        const char *b = ob, *e;
        char who[96];
        size_t n;
        while (b > log && b[-1] != '\n') b--;
        e = ob;
        while (*e && *e != '\n') e++;
        n = (size_t)(e - b);
        if (n >= sizeof who) n = sizeof who - 1;
        memcpy(who, b, n);
        who[n] = 0;
        app_log(a, "warning: the integration stopped early:%s. A variable "
                   "or an 'aux' quantity is larger than BOUND; raise it "
                   "(@ bound=1e6) if that is expected.", who);
      } else
        app_log(a, "warning: the integration stopped early (see 'BOUND' and "
                   "'DT', or try another method).");
    }
  }
  app_status(a, "Integrated %s: %d points to t = %g", a->model_name, rows,
             rows > 0 ? a->traj.cols[0][rows - 1] : 0.0);
  return true;
}

bool app_run(App *a) {
  bool outer = echo_begin(a), ok = run(a);
  if (outer && ok) echo_line(a, "sol = m.run()");
  echo_end(a);
  return ok;
}

/* Initial conds > Mouse: a run from a point of the phase plane. With Keep
   each (mIce) the earlier runs stay on the plots. */
bool app_click_start(App *a, double x, double y) {
  bool live = a->live, keep = a->keep_curves, ok;
  if (!a->model_ok) return false;
  a->live = false;
  app_set_ic(a, a->pp.xvar, x);
  app_set_ic(a, a->pp.yvar, y);
  a->live = live;
  if (a->pp.click_keep) a->keep_curves = true;
  ok = app_run(a);
  a->keep_curves = keep;
  return ok;
}

bool app_run_live(App *a) {
  bool ok;
  if (a->echo.depth > 0) return app_run(a);
  a->echo.live = true;
  ok = app_run(a);
  a->echo.live = false;
  if (ok) a->echo.pending_run = true;
  return ok;
}

bool app_ic_last(App *a, bool shift) {
  int i, j;
  bool live = a->live, outer;
  if (!a->have_traj || a->traj.nrows == 0) {
    app_log(a, "There is no run yet.");
    return false;
  }
  outer = echo_begin(a);
  a->live = false;
  for (i = 0; i < a->nvars; i++)
    for (j = 1; j < a->traj.ncols; j++)
      if (SDL_strcasecmp(a->traj.names[j], a->vars[i]) == 0)
        app_set_ic(a, i, a->traj.cols[j][a->traj.nrows - 1]);
  /* Shift: the time goes on from where the run ended. */
  if (shift) a->eng->api.xppe_set_number("T0", a->traj.cols[0][a->traj.nrows - 1]);
  a->live = live;
  /* The values as the run holds them, which Model.ic_last() would take
     from the engine in another precision: written as values. */
  if (outer) echo_values(a);
  echo_end(a);
  app_refresh_values(a);
  a->reveal_ics = true;
  if (shift)
    app_log(a, "Initial values set to the end of the last run, and the start "
               "time T0 to %g.", a->t0);
  else
    app_log(a, "Initial values set to the end of the last run.");
  return true;
}

/* After the engine changed initial values itself: the app's copy, the
   echo's line, and a live rerun. */
static void ics_changed(App *a, bool outer, const char *line) {
  if (outer) echo_line(a, "%s", line);
  a->reveal_ics = true;
  app_refresh_values(a);
  echo_end(a);
  if (a->live) a->needs_run = true;
}

bool app_ic_read(App *a, const char *path) {
  char p[2100], line[2200];
  bool outer;
  if (!a->model_ok) return false;
  outer = echo_begin(a);
  if (a->eng->api.xppe_ic_file(path) < 0) {
    app_log(a, "error: %s", a->eng->api.xppe_error());
    echo_end(a);
    return false;
  }
  snprintf(line, sizeof line, "m.read_ics(%s)", echo_str(path, p, sizeof p));
  ics_changed(a, outer, line);
  app_log(a, "Initial values read from %s.", path);
  return true;
}

bool app_ic_write(App *a, const char *path) {
  char p[2100], num[64];
  bool outer;
  int i;
  FILE *f;
  if (!a->model_ok) return false;
  if ((f = fopen(path, "w")) == NULL) {
    app_log(a, "error: cannot write %s", path);
    return false;
  }
  /* Exactly, one per line: Model.read_ics() and xppe_ic_file() read it. */
  for (i = 0; i < a->nvars; i++) {
    app_format_double(num, sizeof num, a->ic[i]);
    fprintf(f, "%s\n", num);
  }
  fclose(f);
  outer = echo_begin(a);
  if (outer) echo_line(a, "m.write_ics(%s)", echo_str(path, p, sizeof p));
  echo_end(a);
  app_log(a, "Initial values written to %s.", path);
  return true;
}

bool app_ic_formula(App *a, const char *target, const char *formula) {
  char t[300], f[2100], line[2600];
  bool outer;
  int n;
  if (!a->model_ok) return false;
  outer = echo_begin(a);
  if ((n = a->eng->api.xppe_ic_formula(target, formula)) < 0) {
    app_log(a, "error: %s", a->eng->api.xppe_error());
    echo_end(a);
    return false;
  }
  snprintf(line, sizeof line, "m.ic_formula(%s, %s)", echo_str(target, t, sizeof t),
           echo_str(formula, f, sizeof f));
  ics_changed(a, outer, line);
  app_log(a, "%d initial value%s set from %s.", n, n == 1 ? "" : "s", formula);
  return true;
}

/* XPPAUT's Continue: integrate further from where the last run ended and
   append the result, leaving the initial values and T0 as they were. */
static bool continue_run(App *a, double more) {
  Trajectory old;
  double t_end, *ic_saved;
  double t0 = a->t0, total = a->total;
  bool keep = a->keep_curves;
  int i, j;
  old = a->traj;
  memset(&a->traj, 0, sizeof a->traj);
  a->have_traj = false;
  t_end = old.cols[0][old.nrows - 1];
  ic_saved = malloc(sizeof(double) * (size_t)a->nvars);
  memcpy(ic_saved, a->ic, sizeof(double) * (size_t)a->nvars);
  a->eng->api.xppe_set_number("T0", t_end);
  a->eng->api.xppe_set_number("TOTAL", more);
  for (i = 0; i < a->nvars; i++)
    for (j = 1; j < old.ncols; j++)
      if (SDL_strcasecmp(old.names[j], a->vars[i]) == 0)
        a->eng->api.xppe_set_number(a->vars[i], old.cols[j][old.nrows - 1]);
  a->keep_curves = false;
  if (!run(a)) {
    a->traj = old;
    a->have_traj = true;
  } else {
    /* Join: old rows, then the new ones without their duplicate first row. */
    int n = old.nrows + (a->traj.nrows > 0 ? a->traj.nrows - 1 : 0);
    for (j = 0; j < a->traj.ncols && j < old.ncols; j++) {
      double *c = realloc(old.cols[j], sizeof(double) * (size_t)n);
      if (!c) continue;
      if (a->traj.nrows > 1)
        memcpy(c + old.nrows, a->traj.cols[j] + 1,
               sizeof(double) * (size_t)(a->traj.nrows - 1));
      old.cols[j] = c;
    }
    old.nrows = n;
    free_traj(&a->traj);
    a->traj = old;
    a->have_traj = true;
    app_status(a, "Continued to t = %g", a->traj.cols[0][n - 1]);
  }
  a->keep_curves = keep;
  a->eng->api.xppe_set_number("T0", t0);
  a->eng->api.xppe_set_number("TOTAL", total);
  for (i = 0; i < a->nvars; i++) a->eng->api.xppe_set_number(a->vars[i], ic_saved[i]);
  free(ic_saved);
  app_refresh_values(a);
  return true;
}

bool app_continue(App *a, double more) {
  bool outer, ok, fresh;
  char num[64];
  if (!a->model_ok) return false;
  if (more <= 0) more = a->total;
  fresh = !a->have_traj || a->traj.nrows == 0;
  outer = echo_begin(a);
  ok = fresh ? run(a) : continue_run(a, more);
  if (outer && ok) {
    if (fresh)
      echo_line(a, "sol = m.run()");
    else
      echo_line(a, "sol = m.continue_run(sol, %s)", echo_num(more, num, sizeof num));
  }
  echo_end(a);
  return ok;
}

/* ---- Parameter sweeps ------------------------------------------------------ */

static bool sweep_equilibria(App *a);

static void set_swept(App *a, bool over_ic, int i, double v) {
  if (over_ic)
    app_set_ic(a, i, v);
  else
    app_set_param(a, i, v);
}

/* The Range's values as its loop makes them, then Model.sweep() (both
   values together: a tuple of names) or Model.sweep_grid(), whose measure
   takes the max and min over the same rows. overlay: the runs kept. */
static void echo_sweep(App *a, bool overlay, bool two, bool grid, int n1, int n2,
                       const char *what, const char *what2) {
  SweepUI *s = &a->sw;
  char nm[160], nm2[160], var[160], f[64], t[64], tr[64], call[512], meas[400];
  const char *cont = s->continue_runs ? ", continue_runs=True" : "";
  echo_str(what, nm, sizeof nm);
  echo_line(a, "sweep_values = [%s + (%s - %s) * k / %d for k in range(%d)]",
            echo_num(s->from, f, sizeof f), echo_num(s->to, t, sizeof t), f,
            n1 - 1, n1);
  if (two) {
    echo_str(what2, nm2, sizeof nm2);
    echo_line(a, "sweep_values2 = [%s + (%s - %s) * k / %d for k in range(%d)]",
              echo_num(s->from2, f, sizeof f), echo_num(s->to2, t, sizeof t), f,
              (grid ? n2 : n1) - 1, grid ? n2 : n1);
  }
  meas[0] = '\0';
  if (overlay) {
    /* the runs themselves */
  } else if (s->mode == SWEEP_LYAPUNOV) {
    snprintf(meas, sizeof meas, ", measure=lambda s: m.lyapunov()");
  } else {
    echo_str(a->vars[s->var], var, sizeof var);
    echo_num(s->transient, tr, sizeof tr);
    snprintf(meas, sizeof meas, ", measure=lambda s: (s[%s][int(len(s) * %s):].max(), "
                                "s[%s][int(len(s) * %s):].min())", var, tr, var, tr);
  }
  if (!two)
    snprintf(call, sizeof call, "m.sweep(%s, sweep_values%s%s)", nm, meas, cont);
  else if (grid)
    snprintf(call, sizeof call, "m.sweep_grid(%s, sweep_values, %s, sweep_values2%s%s)",
             nm, nm2, meas, cont);
  else
    snprintf(call, sizeof call, "m.sweep((%s, %s), list(zip(sweep_values, sweep_values2))%s%s)",
             nm, nm2, meas, cont);
  if (overlay) {
    echo_line(a, "runs = %s", call);
    echo_line(a, grid ? "sol = runs[-1][-1]" : "sol = runs[-1]");
  } else {
    echo_line(a, "sweep = %s", call);
    echo_line(a, "sol = m.run()");
  }
  a->echo.sweep_mode = overlay ? -1 : s->mode;
  a->echo.sweep_kind = grid ? 2 : two ? 1 : 0;
  snprintf(a->echo.sweep_name, sizeof a->echo.sweep_name, "%s", what);
  snprintf(a->echo.sweep_name2, sizeof a->echo.sweep_name2, "%s", two ? what2 : "");
  snprintf(a->echo.sweep_var, sizeof a->echo.sweep_var, "%s",
           s->mode == SWEEP_MAXMIN ? a->vars[s->var] : "");
}

/* The runs go through the first value's steps for each of the second's
   (a grid, row by row), or through both together (a line), setting the
   first value then the second, as XPPAUT's do_range() does. */
bool app_sweep(App *a) {
  SweepUI *s = &a->sw;
  double old, old2 = 0, *ics;
  bool live = a->live, keep = a->keep_curves, outer, two, grid;
  int k, n, n1, n2, col = -1, j;
  const char *what, *what2 = NULL;
  if (!a->model_ok) return false;
  if (s->mode == SWEEP_EQUILIBRIA) return sweep_equilibria(a);
  if (s->over_ic ? (s->par < 0 || s->par >= a->nvars)
                 : (s->par < 0 || s->par >= a->npars))
    return false;
  two = s->two && (s->over_ic2 ? s->par2 >= 0 && s->par2 < a->nvars
                               : s->par2 >= 0 && s->par2 < a->npars) &&
        !(s->over_ic2 == s->over_ic && s->par2 == s->par);
  grid = two && s->grid;
  what = s->over_ic ? a->vars[s->par] : a->pars[s->par];
  if (two) what2 = s->over_ic2 ? a->vars[s->par2] : a->pars[s->par2];
  s->overlay = s->mode == SWEEP_OVERLAY;
  n1 = s->steps < 2 ? 2 : s->steps;
  n2 = grid ? (s->steps2 < 2 ? 2 : s->steps2) : 1;
  if (s->overlay && n1 * n2 > APP_MAX_FROZEN) {
    if (grid) {
      app_log(a, "error: a grid that keeps each run holds at most %d runs: use "
                 "fewer steps, or measure a variable or the Liapunov exponent",
              APP_MAX_FROZEN);
      return false;
    }
    n1 = APP_MAX_FROZEN;
  }
  n = n1 * n2;
  outer = echo_begin(a);
  old = s->over_ic ? a->ic[s->par] : a->par[s->par].value;
  if (two) old2 = s->over_ic2 ? a->ic[s->par2] : a->par[s->par2].value;
  ics = malloc(sizeof(double) * (size_t)(a->nvars + 1));
  memcpy(ics, a->ic, sizeof(double) * (size_t)a->nvars);
  a->live = false;
  if (s->overlay) {
    app_clear_curves(a);
    free_traj(&a->traj);
    a->have_traj = false;
    a->keep_curves = true;
  } else {
    free(s->x);
    free(s->x2);
    free(s->ymax);
    free(s->ymin);
    s->x = malloc(sizeof(double) * (size_t)n);
    s->x2 = malloc(sizeof(double) * (size_t)n);
    s->ymax = malloc(sizeof(double) * (size_t)n);
    s->ymin = malloc(sizeof(double) * (size_t)n);
    s->n = 0;
    s->n1 = n1;
    s->n2 = n2;
    s->result_mode = s->mode;
    s->result_kind = grid ? 2 : two ? 1 : 0;
  }
  for (k = 0; k < n; k++) {
    int i1 = k % n1, j2 = k / n1;
    double v = s->from + (s->to - s->from) * i1 / (n1 - 1), v2 = 0;
    if (two)
      v2 = grid ? s->from2 + (s->to2 - s->from2) * j2 / (n2 - 1)
                : s->from2 + (s->to2 - s->from2) * i1 / (n1 - 1);
    /* "Use old ICs" = No: start where the last run ended (Initial conds >
       Last, in full precision). */
    if (s->continue_runs && k > 0) {
      a->eng->api.xppe_ic_last();
      app_refresh_values(a);
    }
    set_swept(a, s->over_ic, s->par, v);
    if (two) set_swept(a, s->over_ic2, s->par2, v2);
    if (!app_run(a)) break;
    if (s->mode == SWEEP_LYAPUNOV) {
      double z = 0;
      if (a->eng->api.xppe_lyapunov_integrate(0, &z) != XPPE_OK) z = NAN;
      s->x[s->n] = v;
      s->x2[s->n] = v2;
      s->ymax[s->n] = s->ymin[s->n] = z;
      s->n++;
    } else if (!s->overlay) {
      const Trajectory *t = &a->traj;
      int start = (int)(t->nrows * s->transient);
      double hi = -1e300, lo = 1e300;
      if (col < 0)
        for (j = 1; j < t->ncols; j++)
          if (SDL_strcasecmp(t->names[j], a->vars[s->var]) == 0) col = j;
      if (col < 0) break;
      for (j = start; j < t->nrows; j++) {
        double y = t->cols[col][j];
        if (y > hi) hi = y;
        if (y < lo) lo = y;
      }
      s->x[s->n] = v;
      s->x2[s->n] = v2;
      s->ymax[s->n] = hi;
      s->ymin[s->n] = lo;
      s->n++;
    }
  }
  for (j = 0; j < a->nvars; j++) app_set_ic(a, j, ics[j]);
  free(ics);
  set_swept(a, s->over_ic, s->par, old);
  if (two) set_swept(a, s->over_ic2, s->par2, old2);
  a->live = live;
  a->keep_curves = keep;
  if (!s->overlay) {
    app_run(a);
    s->reveal = true;
    a->show_workspace = true;
    snprintf(a->focus, sizeof a->focus, "workspace");
  }
  if (outer && k == n) echo_sweep(a, s->overlay, two, grid, n1, n2, what, what2);
  echo_end(a);
  if (grid)
    app_log(a, "Range over %s from %g to %g and %s from %g to %g: a grid of %d "
               "runs%s%s.", what, s->from, s->to, what2, s->from2, s->to2, n,
            s->continue_runs ? ", each starting where the last ended" : "",
            s->overlay ? " (trajectories kept on the plots)"
                       : s->mode == SWEEP_LYAPUNOV ? " (Liapunov exponents)" : "");
  else
    app_log(a, "Range over %s from %g to %g%s%s%s%s in %d runs%s%s.", what, s->from,
            s->to, two ? ", with " : "", two ? what2 : "", two ? " moving with it" : "",
            "", n, s->continue_runs ? ", each starting where the last ended" : "",
            s->overlay ? " (trajectories kept on the plots)"
                       : s->mode == SWEEP_LYAPUNOV ? " (Liapunov exponents)" : "");
  app_status(a, "Range over %s done", what);
  return true;
}

/* ---- Filming the Range (XPPAUT's Range with Movie = Yes) ------------------------
   The same runs as app_sweep() keeping each trajectory, one per frame: each
   is drawn alone and the Kinescope captures the figure before the next. */

static bool film_two(App *a) {
  SweepUI *s = &a->sw;
  return s->two &&
         (s->over_ic2 ? s->par2 >= 0 && s->par2 < a->nvars : s->par2 >= 0 && s->par2 < a->npars) &&
         !(s->over_ic2 == s->over_ic && s->par2 == s->par);
}

bool app_film_range(App *a, int figure) {
  SweepUI *s = &a->sw;
  Kinescope *k = &a->ks;
  if (!a->model_ok || k->film.on) return false;
  if (s->over_ic ? (s->par < 0 || s->par >= a->nvars) : (s->par < 0 || s->par >= a->npars))
    return false;
  k->film.two = film_two(a);
  k->film.grid = k->film.two && s->grid;
  k->film.n1 = s->steps < 2 ? 2 : s->steps;
  k->film.n2 = k->film.grid ? (s->steps2 < 2 ? 2 : s->steps2) : 1;
  k->film.n = k->film.n1 * k->film.n2;
  if (k->film.n > APP_MAX_FRAMES) {
    app_log(a, "error: a film holds at most %d frames: use fewer steps.", APP_MAX_FRAMES);
    return false;
  }
  k->film.ics = malloc(sizeof(double) * (size_t)(a->nvars + 1));
  if (!k->film.ics) return false;
  memcpy(k->film.ics, a->ic, sizeof(double) * (size_t)a->nvars);
  k->film.old = s->over_ic ? a->ic[s->par] : a->par[s->par].value;
  k->film.old2 = !k->film.two ? 0 : s->over_ic2 ? a->ic[s->par2] : a->par[s->par2].value;
  k->film.live = a->live;
  k->film.keep = a->keep_curves;
  /* The runs write no lines of their own: the sweep is written at the end. */
  k->film.outer = echo_begin(a);
  a->live = false;
  a->keep_curves = false;
  app_clear_curves(a);
  app_kine_clear(a); /* XPPAUT starts the film afresh */
  k->source = figure;
  k->failed = false;
  k->film.k = 0;
  k->film.waiting = false;
  k->film.on = true;
  s->overlay = true;
  a->show_kine = true;
  return true;
}

static void film_end(App *a, bool done) {
  SweepUI *s = &a->sw;
  Kinescope *k = &a->ks;
  const char *what = s->over_ic ? a->vars[s->par] : a->pars[s->par];
  const char *what2 = !k->film.two ? NULL : s->over_ic2 ? a->vars[s->par2] : a->pars[s->par2];
  int j;
  for (j = 0; j < a->nvars; j++) app_set_ic(a, j, k->film.ics[j]);
  free(k->film.ics);
  k->film.ics = NULL;
  set_swept(a, s->over_ic, s->par, k->film.old);
  if (k->film.two) set_swept(a, s->over_ic2, s->par2, k->film.old2);
  a->live = k->film.live;
  a->keep_curves = k->film.keep;
  k->film.on = false;
  if (done && k->film.outer)
    echo_sweep(a, true, k->film.two, k->film.grid, k->film.n1, k->film.n2, what, what2);
  echo_end(a);
  if (done) {
    app_log(a, "Filmed %d runs over %s%s%s: %d frames in the Kinescope.", k->film.n, what,
            what2 ? " and " : "", what2 ? what2 : "", k->n);
    app_status(a, "Filmed %d runs over %s", k->film.n, what);
    k->cur = 0;
  } else {
    app_log(a, "The film stopped after %d of %d runs.", k->n, k->film.n);
    app_status(a, "The film stopped");
    k->save_after[0] = k->frames_after[0] = '\0';
  }
}

/* Once a frame: after the last run's picture is taken, the next run. */
void app_film_step(App *a) {
  SweepUI *s = &a->sw;
  Kinescope *k = &a->ks;
  int i1, j2;
  double v, v2 = 0;
  if (!k->film.on) return;
  if (k->film.waiting) {
    if (export_capture_waiting()) return;
    k->film.waiting = false;
    if (k->failed) {
      film_end(a, false);
      return;
    }
    k->film.k++;
  }
  if (k->film.k >= k->film.n) {
    film_end(a, true);
    return;
  }
  i1 = k->film.k % k->film.n1;
  j2 = k->film.k / k->film.n1;
  v = s->from + (s->to - s->from) * i1 / (k->film.n1 - 1);
  if (k->film.two)
    v2 = k->film.grid ? s->from2 + (s->to2 - s->from2) * j2 / (k->film.n2 - 1)
                      : s->from2 + (s->to2 - s->from2) * i1 / (k->film.n1 - 1);
  if (s->continue_runs && k->film.k > 0) {
    a->eng->api.xppe_ic_last();
    app_refresh_values(a);
  }
  set_swept(a, s->over_ic, s->par, v);
  if (k->film.two) set_swept(a, s->over_ic2, s->par2, v2);
  if (!app_run(a)) {
    film_end(a, false);
    return;
  }
  app_status(a, "Filming: run %d of %d", k->film.k + 1, k->film.n);
  app_kine_capture(a, k->source);
  k->film.waiting = true;
}

/* XPPAUT's Sing pts > Range over the Range tab's values: the equilibrium
   followed from the initial values as the parameter changes. */
static bool sweep_equilibria(App *a) {
  SweepUI *s = &a->sw;
  xpp_engine_api *x = &a->eng->api;
  int n = s->steps < 2 ? 2 : s->steps, k, rc, stable = 0, unstable = 0, failed = 0;
  int col;
  double *values;
  bool outer;
  if (s->over_ic || s->par < 0 || s->par >= a->npars) {
    app_log(a, "error: equilibria are followed over a parameter, not an "
               "initial value");
    return false;
  }
  values = malloc(sizeof(double) * (size_t)n);
  if (!values) return false;
  for (k = 0; k < n; k++) values[k] = s->from + (s->to - s->from) * k / (n - 1);
  outer = echo_begin(a);
  rc = x->xppe_equilibrium_sweep(a->pars[s->par], values, n, a->ic);
  free(values);
  if (rc != XPPE_OK) {
    app_log(a, "error: %s", x->xppe_error());
    echo_end(a);
    return false;
  }
  fetch_table(a, &s->eqt);
  free(s->x);
  free(s->ymax);
  free(s->ymin);
  s->n = s->eqt.nrows;
  s->x = malloc(sizeof(double) * (size_t)(s->n + 1));
  s->ymax = malloc(sizeof(double) * (size_t)(s->n + 1));
  s->ymin = malloc(sizeof(double) * (size_t)(s->n + 1));
  col = s->var >= 0 && s->var < a->nvars ? s->var + 1 : 1;
  for (k = 0; k < s->n && s->eqt.ncols > a->nvars + 1; k++) {
    s->x[k] = s->eqt.cols[0][k];
    s->ymax[k] = s->eqt.cols[col][k];
    s->ymin[k] = s->eqt.cols[a->nvars + 1][k];
    if (isnan(s->ymin[k]))
      failed++;
    else if (s->ymin[k] > 0)
      unstable++;
    else
      stable++;
  }
  s->overlay = false;
  s->result_mode = SWEEP_EQUILIBRIA;
  s->result_kind = 0;
  s->reveal = true;
  a->show_workspace = true;
  snprintf(a->focus, sizeof a->focus, "workspace");
  if (failed)
    app_log(a, "Equilibria over %s from %g to %g at %d values: %d stable, %d "
               "unstable, none found at %d.", a->pars[s->par], s->from, s->to,
            n, stable, unstable, failed);
  else
    app_log(a, "Equilibria over %s from %g to %g at %d values: %d stable, %d "
               "unstable.", a->pars[s->par], s->from, s->to, n, stable, unstable);
  app_status(a, "Equilibria over %s done", a->pars[s->par]);
  if (outer) {
    char nm[160], f[64], t[64];
    echo_line(a, "sweep_values = [%s + (%s - %s) * k / %d for k in range(%d)]",
              echo_num(s->from, f, sizeof f), echo_num(s->to, t, sizeof t), f,
              n - 1, n);
    echo_line(a, "eq_sweep = m.equilibrium_sweep(%s, sweep_values)",
              echo_str(a->pars[s->par], nm, sizeof nm));
    a->echo.sweep_mode = SWEEP_EQUILIBRIA;
    snprintf(a->echo.sweep_name, sizeof a->echo.sweep_name, "%s", a->pars[s->par]);
    a->echo.sweep_var[0] = '\0';
  }
  echo_end(a);
  return true;
}

/* ---- Equilibria ---------------------------------------------------------- */

static void classify(Equilibrium *e, int n, bool discrete) {
  int i, unstable = 0, complex_pair = 0;
  for (i = 0; i < n; i++) {
    double mag = sqrt(e->re[i] * e->re[i] + e->im[i] * e->im[i]);
    if (discrete ? mag > 1 : e->re[i] > 0) unstable++;
    if (fabs(e->im[i]) > 1e-10) complex_pair = 1;
  }
  e->unstable = unstable;
  if (discrete)
    snprintf(e->kind, sizeof e->kind, "%s", unstable ? "unstable" : "stable");
  else if (unstable > 0 && unstable < n)
    snprintf(e->kind, sizeof e->kind, complex_pair ? "saddle-focus" : "saddle");
  else
    snprintf(e->kind, sizeof e->kind, "%s %s",
             unstable ? "unstable" : "stable", complex_pair ? "focus" : "node");
}

static void free_equilibrium(Equilibrium *e) {
  int k;
  free(e->x);
  free(e->re);
  free(e->im);
  for (k = 0; k < e->nmanifolds; k++) free_traj(&e->manifold[k]);
  memset(e, 0, sizeof *e);
}

void app_clear_equilibria(App *a) {
  int i;
  for (i = 0; i < a->neq; i++) free_equilibrium(&a->eq[i]);
  a->neq = 0;
}

static bool equilibrium(App *a, const double *guess);

/* eq = m.equilibrium(), or for a saddle, whose manifolds are drawn too,
   eq, manifolds = m.manifolds(); with the guess as a dict. */
static void echo_equilibrium(App *a, const double *guess, bool saddle) {
  char g[2048], name[160], num[64];
  size_t k = 0;
  int i;
  g[0] = '\0';
  if (guess) {
    k += (size_t)snprintf(g, sizeof g, "{");
    for (i = 0; i < a->nvars && k < sizeof g - 240; i++)
      k += (size_t)snprintf(g + k, sizeof g - k, "%s%s: %s", i ? ", " : "",
                            echo_str(a->vars[i], name, sizeof name),
                            echo_num(guess[i], num, sizeof num));
    snprintf(g + k, sizeof g - k, "}");
  }
  if (saddle)
    echo_line(a, "eq, manifolds = m.manifolds(%s)", g);
  else
    echo_line(a, "eq = m.equilibrium(%s)", g);
}

bool app_equilibrium(App *a, const double *guess) {
  bool outer = echo_begin(a), ok = equilibrium(a, guess);
  if (outer && ok) {
    const Equilibrium *e = &a->eq[a->neq - 1];
    echo_equilibrium(a, guess, e->unstable > 0 && e->unstable < a->nvars && a->nvars > 1);
  }
  echo_end(a);
  return ok;
}

static bool equilibrium(App *a, const double *guess) {
  Equilibrium *e;
  int i, n = a->nvars;
  if (!a->model_ok) return false;
  if (a->neq == APP_MAX_EQUILIBRIA) {
    free_equilibrium(&a->eq[0]);
    memmove(&a->eq[0], &a->eq[1], sizeof a->eq[0] * (APP_MAX_EQUILIBRIA - 1));
    a->neq--;
  }
  e = &a->eq[a->neq];
  memset(e, 0, sizeof *e);
  e->x = malloc(sizeof(double) * (size_t)n);
  e->re = calloc((size_t)n, sizeof(double));
  e->im = calloc((size_t)n, sizeof(double));
  for (i = 0; i < n; i++) e->x[i] = guess ? guess[i] : a->ic[i];
  if (a->eng->api.xppe_equilibrium(e->x, e->re, e->im) != XPPE_OK) {
    app_log(a, "No equilibrium found near that point (%s).",
            a->eng->api.xppe_error());
    app_status(a, "No equilibrium found near that point");
    free_equilibrium(e);
    return false;
  }
  classify(e, n, a->meth == 0);
  a->neq++;
  /* Saddles: XPPAUT's invariant sets, one branch on each side of every
     one-dimensional stable or unstable direction. */
  if (e->unstable > 0 && e->unstable < n && n > 1) {
    int k, m = a->eng->api.xppe_manifold_count();
    for (k = 0; k < m && k < 8; k++) {
      int type = 0;
      if (a->eng->api.xppe_manifold(k, &type) != XPPE_OK) continue;
      fetch_run(a, &e->manifold[e->nmanifolds]);
      e->manifold_type[e->nmanifolds++] = type;
    }
  }
  {
    char line[1024];
    int k = 0;
    k += snprintf(line + k, sizeof line - k, "Equilibrium (%s):", e->kind);
    for (i = 0; i < n && k < (int)sizeof line - 40; i++)
      k += snprintf(line + k, sizeof line - k, " %s=%.6g", a->vars[i], e->x[i]);
    app_log(a, "%s", line);
    k = snprintf(line, sizeof line, "  eigenvalues:");
    for (i = 0; i < n && k < (int)sizeof line - 40; i++)
      k += snprintf(line + k, sizeof line - k, " %.5g%+.5gi", e->re[i],
                    e->im[i]);
    app_log(a, "%s", line);
    if (e->nmanifolds)
      app_log(a, "  %d branches of its stable and unstable manifolds drawn in "
                 "the phase plane.", e->nmanifolds);
  }
  app_status(a, "Found %s %s", strchr("aeiou", e->kind[0]) ? "an" : "a",
             e->kind);
  return true;
}

/* The phase plane's window as it is shown, or as the model file set it
   when the plot has not been drawn yet. */
static bool shown_window(App *a, double w[4]) {
  const FigureState *g = &g_figures[FIG_PHASE];
  w[0] = a->pp.xlo;
  w[1] = a->pp.xhi;
  w[2] = a->pp.ylo;
  w[3] = a->pp.yhi;
  if (g->valid) {
    w[0] = g->xmin;
    w[1] = g->xmax;
    w[2] = g->ymin;
    w[3] = g->ymax;
  } else if (a->pp.set_window || !(w[1] > w[0] && w[3] > w[2])) {
    w[0] = a->pp.win_xlo;
    w[1] = a->pp.win_xhi;
    w[2] = a->pp.win_ylo;
    w[3] = a->pp.win_yhi;
  }
  return w[1] > w[0] && w[3] > w[2];
}

bool app_phase_window(App *a, double w[4]) {
  if (shown_window(a, w)) return true;
  w[0] = get_value(a, "XLO"); /* XPPAUT's plot window */
  w[1] = get_value(a, "XHI");
  w[2] = get_value(a, "YLO");
  w[3] = get_value(a, "YHI");
  return w[1] > w[0] && w[3] > w[2];
}

bool app_find_equilibria(App *a, int guesses) {
  xpp_engine_api *x;
  FoundEquilibria *f = &a->found;
  const double tol = 0.001;
  double *lo, *hi;
  int n = a->nvars, i, k, count;
  bool outer, discrete;
  if (!a->model_ok || n < 1) return false;
  if (guesses < 1) guesses = 100;
  x = &a->eng->api;
  lo = malloc(sizeof(double) * (size_t)n);
  hi = malloc(sizeof(double) * (size_t)n);
  if (!lo || !hi) {
    free(lo);
    free(hi);
    return false;
  }
  /* The phase plane's window for its two variables. For the others, the
     range the last run covered, widened by a quarter on each side (the
     equilibria that matter are usually near it), else XPPAUT's default
     box. */
  for (i = 0; i < n; i++) {
    int j, k;
    lo[i] = -10;
    hi[i] = 10;
    for (j = 1; a->have_traj && j < a->traj.ncols; j++) {
      double mn, mx;
      if (SDL_strcasecmp(a->traj.names[j], a->vars[i]) != 0 || a->traj.nrows < 1) continue;
      mn = mx = a->traj.cols[j][0];
      for (k = 1; k < a->traj.nrows; k++) {
        mn = fmin(mn, a->traj.cols[j][k]);
        mx = fmax(mx, a->traj.cols[j][k]);
      }
      if (isfinite(mn) && isfinite(mx)) {
        double d = mx > mn ? 0.25 * (mx - mn) : fmax(1.0, 0.25 * fabs(mx));
        lo[i] = mn - d;
        hi[i] = mx + d;
      }
    }
  }
  if (n > 1 && a->pp.xvar != a->pp.yvar) {
    double w[4];
    if (shown_window(a, w)) {
      lo[a->pp.xvar] = w[0];
      hi[a->pp.xvar] = w[1];
      lo[a->pp.yvar] = w[2];
      hi[a->pp.yvar] = w[3];
    }
  }
  outer = echo_begin(a);
  count = x->xppe_find_equilibria(guesses, tol, lo, hi);
  if (count < 0) {
    app_log(a, "error: %s", x->xppe_error());
    echo_end(a);
    free(lo);
    free(hi);
    return false;
  }
  free_found(a);
  f->n = count;
  f->guesses = guesses;
  f->x = malloc(sizeof(double) * (size_t)(count * n + 1));
  f->re = malloc(sizeof(double) * (size_t)(count * n + 1));
  f->im = malloc(sizeof(double) * (size_t)(count * n + 1));
  f->unstable = calloc((size_t)count + 1, sizeof(int));
  discrete = a->meth == 0;
  app_clear_equilibria(a);
  for (k = 0; k < count; k++) {
    double *xk = f->x + (size_t)k * n, *rk = f->re + (size_t)k * n,
           *ik = f->im + (size_t)k * n;
    x->xppe_found_equilibrium(k, xk, rk, ik);
    /* Counted as xpplab's Equilibrium.unstable_count does. */
    for (i = 0; i < n; i++)
      if (discrete ? hypot(rk[i], ik[i]) > 1 : rk[i] > 0) f->unstable[k]++;
    if (a->neq < APP_MAX_EQUILIBRIA) {
      Equilibrium *e = &a->eq[a->neq];
      memset(e, 0, sizeof *e);
      e->x = malloc(sizeof(double) * (size_t)n);
      e->re = malloc(sizeof(double) * (size_t)n);
      e->im = malloc(sizeof(double) * (size_t)n);
      memcpy(e->x, xk, sizeof(double) * (size_t)n);
      memcpy(e->re, rk, sizeof(double) * (size_t)n);
      memcpy(e->im, ik, sizeof(double) * (size_t)n);
      classify(e, n, discrete);
      a->neq++;
    }
  }
  app_log(a, "Found %d equilibri%s from %d random guesses in the box:", count,
          count == 1 ? "um" : "a", guesses);
  for (k = 0; k < a->neq; k++) {
    char line[1024];
    int m = snprintf(line, sizeof line, "  %s:", a->eq[k].kind);
    for (i = 0; i < n && m < (int)sizeof line - 40; i++)
      m += snprintf(line + m, sizeof line - m, " %s=%.6g", a->vars[i], a->eq[k].x[i]);
    app_log(a, "%s", line);
  }
  if (count > a->neq)
    app_log(a, "  ... and %d more (export them with export FILE.csv equilibria)",
            count - a->neq);
  app_status(a, "Found %d equilibri%s", count, count == 1 ? "um" : "a");
  a->show_phase = true;
  if (outer) {
    char box[4096], name[160], v[2][64];
    size_t m = 0;
    for (i = 0; i < n && m < sizeof box - 300; i++)
      m += (size_t)snprintf(box + m, sizeof box - m, "%s%s: (%s, %s)", i ? ", " : "",
                            echo_str(a->vars[i], name, sizeof name),
                            echo_num(lo[i], v[0], 64), echo_num(hi[i], v[1], 64));
    echo_line(a, "eqs = m.find_equilibria(guesses=%d, box={%s}, tol=%s)", guesses, box,
              echo_num(tol, v[0], 64));
  }
  echo_end(a);
  free(lo);
  free(hi);
  return true;
}

/* ---- Nullclines ------------------------------------------------------------ */

bool app_nullclines(App *a, double xlo, double xhi, double ylo, double yhi) {
  PhasePlane *p = &a->pp;
  const int max = 40000;
  if (!a->model_ok || p->xvar == p->yvar || a->nvars < 2) return false;
  if (!p->ncx) p->ncx = malloc(sizeof(float) * 4 * max);
  if (!p->ncy) p->ncy = malloc(sizeof(float) * 4 * max);
  p->ncx_n = a->eng->api.xppe_nullcline(0, p->xvar, p->yvar, xlo, xhi, ylo,
                                        yhi, p->grid, p->ncx, max);
  p->ncy_n = a->eng->api.xppe_nullcline(1, p->xvar, p->yvar, xlo, xhi, ylo,
                                        yhi, p->grid, p->ncy, max);
  /* Written once when they are switched on, not every time a pan or zoom
     computes them again. */
  if (!a->echo.nc_written) {
    char x[160], y[160], n[4][64];
    if (echo_begin(a)) {
      echo_line(a, "nc = m.nullclines(%s, %s, (%s, %s), (%s, %s), grid=%d)",
                echo_str(a->vars[p->xvar], x, sizeof x),
                echo_str(a->vars[p->yvar], y, sizeof y), echo_num(xlo, n[0], 64),
                echo_num(xhi, n[1], 64), echo_num(ylo, n[2], 64),
                echo_num(yhi, n[3], 64), p->grid);
      a->echo.nc_written = true;
    }
    echo_end(a);
  }
  if (p->ncx_n < 0) p->ncx_n = 0;
  if (p->ncy_n < 0) p->ncy_n = 0;
  p->xlo = xlo;
  p->xhi = xhi;
  p->ylo = ylo;
  p->yhi = yhi;
  p->nc_dirty = false;
  return true;
}

/* ---- Frozen nullclines ------------------------------------------------------- */

static void free_frozen_nullclines(PhasePlane *p) {
  int k;
  for (k = 0; k < p->nfrozen; k++) {
    free(p->frozen[k].x);
    free(p->frozen[k].y);
  }
  free(p->frozen);
  p->frozen = NULL;
  p->nfrozen = 0;
  p->show_frozen = 0;
  p->play = false;
}

/* Both nullclines over window w, as XPPAUT computes them, kept as set k. */
static bool freeze_at(App *a, const double w[4], const char *label) {
  PhasePlane *p = &a->pp;
  FrozenNullclines *f;
  const int max = 40000;
  float *buf;
  int nx, ny;
  if (p->nfrozen >= APP_MAX_FROZEN_NC) {
    app_log(a, "error: %d frozen nullclines are the most kept; delete them first.",
            APP_MAX_FROZEN_NC);
    return false;
  }
  if (!p->frozen) p->frozen = calloc(APP_MAX_FROZEN_NC, sizeof *p->frozen);
  buf = malloc(sizeof(float) * 8 * max);
  if (!p->frozen || !buf) {
    free(buf);
    return false;
  }
  nx = a->eng->api.xppe_nullcline(0, p->xvar, p->yvar, w[0], w[1], w[2], w[3], p->grid,
                                  buf, max);
  ny = a->eng->api.xppe_nullcline(1, p->xvar, p->yvar, w[0], w[1], w[2], w[3], p->grid,
                                  buf + 4 * max, max);
  if (nx < 0) nx = 0;
  if (ny < 0) ny = 0;
  f = &p->frozen[p->nfrozen];
  f->x = malloc(sizeof(float) * 4 * (size_t)(nx + 1));
  f->y = malloc(sizeof(float) * 4 * (size_t)(ny + 1));
  if (!f->x || !f->y) {
    free(f->x);
    free(f->y);
    free(buf);
    return false;
  }
  memcpy(f->x, buf, sizeof(float) * 4 * (size_t)nx);
  memcpy(f->y, buf + 4 * max, sizeof(float) * 4 * (size_t)ny);
  f->nx = nx;
  f->ny = ny;
  snprintf(f->label, sizeof f->label, "%s", label);
  p->nfrozen++;
  free(buf);
  return true;
}

/* The script's list of frozen nullclines, started once. */
static void echo_frozen_list(App *a) {
  if (!a->echo.frozen_list) echo_line(a, "frozen = []");
  a->echo.frozen_list = true;
}

static void echo_plane(App *a, const double w[4], char *buf, size_t len) {
  char x[160], y[160], n[4][64];
  snprintf(buf, len, "%s, %s, (%s, %s), (%s, %s), grid=%d",
           echo_str(a->vars[a->pp.xvar], x, sizeof x),
           echo_str(a->vars[a->pp.yvar], y, sizeof y), echo_num(w[0], n[0], 64),
           echo_num(w[1], n[1], 64), echo_num(w[2], n[2], 64), echo_num(w[3], n[3], 64),
           a->pp.grid);
}

bool app_freeze_nullclines(App *a) {
  char label[64], args[800];
  double w[4];
  bool outer, ok;
  if (!a->model_ok || a->nvars < 2 || a->pp.xvar == a->pp.yvar) return false;
  if (!app_phase_window(a, w)) return false;
  outer = echo_begin(a);
  snprintf(label, sizeof label, "#%d", a->pp.nfrozen + 1);
  ok = freeze_at(a, w, label);
  if (ok && outer) {
    echo_frozen_list(a);
    echo_plane(a, w, args, sizeof args);
    echo_line(a, "frozen.append(m.nullclines(%s))", args);
  }
  echo_end(a);
  if (ok) {
    a->pp.nullclines = true; /* to see them against the live ones */
    a->pp.nc_dirty = true;
    app_status(a, "Froze the nullclines (%d kept).", a->pp.nfrozen);
  }
  return ok;
}

bool app_freeze_nullcline_range(App *a, int par, double from, double to, int n) {
  char label[64], args[800], nm[160], num[2][64];
  double w[4], old;
  bool outer, ok = true;
  int k;
  if (!a->model_ok || a->nvars < 2 || a->pp.xvar == a->pp.yvar) return false;
  if (par < 0 || par >= a->npars || n < 1) return false;
  if (a->pp.nfrozen + n > APP_MAX_FROZEN_NC) {
    app_log(a, "error: at most %d frozen nullclines are kept.", APP_MAX_FROZEN_NC);
    return false;
  }
  if (!app_phase_window(a, w)) return false;
  outer = echo_begin(a);
  old = get_value(a, a->pars[par]);
  for (k = 0; k < n && ok; k++) {
    /* As Python computes the values the script lists. */
    double v = n > 1 ? from + (to - from) * k / (n - 1) : from;
    a->eng->api.xppe_set_number(a->pars[par], v);
    snprintf(label, sizeof label, "%s = %g", a->pars[par], v);
    ok = freeze_at(a, w, label);
  }
  a->eng->api.xppe_set_number(a->pars[par], old);
  if (ok && outer) {
    echo_frozen_list(a);
    echo_line(a, "nc_values = [%s + (%s - %s) * k / %d for k in range(%d)]",
              echo_num(from, num[0], 64), echo_num(to, num[1], 64), num[0],
              n > 1 ? n - 1 : 1, n);
    echo_plane(a, w, args, sizeof args);
    echo_line(a, "frozen += m.nullcline_sweep(%s, nc_values, %s)",
              echo_str(a->pars[par], nm, sizeof nm), args);
  }
  echo_end(a);
  if (ok) {
    a->pp.nullclines = true;
    a->pp.nc_dirty = true;
    app_status(a, "Froze the nullclines for %d values of %s.", n, a->pars[par]);
  }
  return ok;
}

void app_unfreeze_nullclines(App *a) {
  bool outer;
  if (a->pp.nfrozen == 0) return;
  free_frozen_nullclines(&a->pp);
  outer = echo_begin(a);
  if (outer && a->echo.frozen_list) echo_line(a, "frozen = []");
  echo_end(a);
}

/* One row per segment: the set (0 the nullclines shown, 1, 2, ... those
   frozen), which nullcline (1 the x variable's, 2 the y variable's, as
   XPPAUT's Nullcline > Save numbers them), and its two ends. */
bool app_export_nullclines_csv(App *a, const char *path) {
  PhasePlane *p = &a->pp;
  char pa[2100], args[800], num[64];
  double w[4];
  bool outer, now;
  int k, s, i;
  FILE *f;
  if (!a->model_ok || a->nvars < 2 || p->xvar == p->yvar) return false;
  now = p->nullclines && app_phase_window(a, w);
  if (!now && p->nfrozen == 0) {
    app_log(a, "error: no nullclines to save; switch them on or freeze some.");
    return false;
  }
  /* The ones shown, computed afresh over the window as the script will
     (its line is written below). */
  if (now) {
    a->echo.nc_written = true;
    app_nullclines(a, w[0], w[1], w[2], w[3]);
  }
  if ((f = fopen(path, "w")) == NULL) {
    app_log(a, "error: cannot write %s", path);
    return false;
  }
  fprintf(f, "set,nullcline,x1,y1,x2,y2\n");
  for (k = now ? -1 : 0; k < p->nfrozen; k++) {
    const float *seg[2] = {k < 0 ? p->ncx : p->frozen[k].x, k < 0 ? p->ncy : p->frozen[k].y};
    int cnt[2] = {k < 0 ? p->ncx_n : p->frozen[k].nx, k < 0 ? p->ncy_n : p->frozen[k].ny};
    for (s = 0; s < 2; s++)
      for (i = 0; i < 4 * cnt[s]; i++) {
        if (i % 4 == 0) fprintf(f, "%d.0,%d.0", k + 1, s + 1);
        app_format_double(num, sizeof num, (double)seg[s][i]);
        fprintf(f, ",%s%s", num, i % 4 == 3 ? "\n" : "");
      }
  }
  fclose(f);
  outer = echo_begin(a);
  if (outer) {
    echo_abs_path(path, pa, sizeof pa);
    if (now) {
      echo_plane(a, w, args, sizeof args);
      echo_line(a, "nc = m.nullclines(%s)", args);
    } else {
      echo_frozen_list(a);
    }
    echo_line(a, "xs.Table.from_nullclines(%s%s).to_csv(%s)",
              now ? "[nc] + frozen" : "frozen", now ? "" : ", first=1",
              echo_str(pa, args, sizeof args));
  }
  echo_end(a);
  app_log(a, "Wrote %s.", path);
  return true;
}

/* ---- AUTO -------------------------------------------------------------------- */

void app_auto_refresh(App *a) {
  xpp_engine_api *x;
  int i, n;
  if (!a->model_ok) return;
  x = &a->eng->api;
  n = x->xppe_auto_count();
  free(a->au.info);
  free(a->au.umax);
  free(a->au.umin);
  a->au.count = n;
  a->au.info = calloc((size_t)n + 1, sizeof *a->au.info);
  a->au.umax = calloc((size_t)(n + 1) * (size_t)a->nvars, sizeof(double));
  a->au.umin = calloc((size_t)(n + 1) * (size_t)a->nvars, sizeof(double));
  for (i = 0; i < n; i++) {
    x->xppe_auto_point(i, &a->au.info[i]);
    x->xppe_auto_point_data(i, XPPE_AUTO_UMAX, a->au.umax + (size_t)i * a->nvars,
                            a->nvars);
    x->xppe_auto_point_data(i, XPPE_AUTO_UMIN, a->au.umin + (size_t)i * a->nvars,
                            a->nvars);
  }
  if (a->au.selected >= n) a->au.selected = -1;
}

bool app_auto_push_settings(App *a) {
  xpp_engine_api *x = &a->eng->api;
  const char *second =
      a->au.second_par >= 0 ? a->pars[a->au.second_par] : NULL;
  if (a->au.main_par < 0) {
    app_log(a, "AUTO: choose a parameter to vary first.");
    return false;
  }
  if (x->xppe_auto_parameters(a->pars[a->au.main_par], second) != XPPE_OK) {
    app_log(a, "AUTO: %s", x->xppe_error());
    return false;
  }
  x->xppe_auto_set("DS", a->au.ds);
  x->xppe_auto_set("DSMIN", a->au.dsmin);
  x->xppe_auto_set("DSMAX", a->au.dsmax);
  x->xppe_auto_set("PARMIN", a->au.parmin);
  x->xppe_auto_set("PARMAX", a->au.parmax);
  x->xppe_auto_set("NORMMIN", a->au.normmin);
  x->xppe_auto_set("NORMMAX", a->au.normmax);
  x->xppe_auto_set("NMAX", a->au.nmax);
  x->xppe_auto_set("NPR", a->au.npr);
  x->xppe_auto_set("NTST", a->au.ntst);
  return true;
}

/* Before an AUTO action: the parameters and numerics that
   app_auto_push_settings() is about to give the engine, where they differ
   from what it holds (the script's model holds the same). */
static void echo_auto_setup(App *a) {
  static const char *keys[] = {"DS",      "DSMIN",   "DSMAX", "PARMIN", "PARMAX",
                               "NORMMIN", "NORMMAX", "NMAX",  "NPR",    "NTST"};
  xpp_engine_api *x = &a->eng->api;
  double want[10];
  const char *main, *second, *m1, *m2;
  char line[1200], name[2][160], num[64], low[16];
  size_t k = 0;
  int i, j;
  if (a->au.main_par < 0) return;
  want[0] = a->au.ds;
  want[1] = a->au.dsmin;
  want[2] = a->au.dsmax;
  want[3] = a->au.parmin;
  want[4] = a->au.parmax;
  want[5] = a->au.normmin;
  want[6] = a->au.normmax;
  want[7] = a->au.nmax;
  want[8] = a->au.npr;
  want[9] = a->au.ntst;
  main = a->pars[a->au.main_par];
  second = a->au.second_par >= 0 ? a->pars[a->au.second_par] : NULL;
  m1 = x->xppe_auto_parameter_name(x->xppe_auto_main_slot(0));
  m2 = x->xppe_auto_parameter_name(x->xppe_auto_main_slot(1));
  if (!m1 || SDL_strcasecmp(m1, main) != 0 ||
      (second && (!m2 || SDL_strcasecmp(m2, second) != 0))) {
    echo_str(main, name[0], sizeof name[0]);
    if (second)
      echo_line(a, "m.auto.parameters(%s, %s)", name[0],
                echo_str(second, name[1], sizeof name[1]));
    else
      echo_line(a, "m.auto.parameters(%s)", name[0]);
  }
  for (i = 0; i < 10; i++) {
    double now = 0;
    x->xppe_auto_get(keys[i], &now);
    if (now == want[i]) continue;
    for (j = 0; keys[i][j] && j < 15; j++) low[j] = (char)tolower((unsigned char)keys[i][j]);
    low[j] = '\0';
    k += (size_t)snprintf(line + k, sizeof line - k, "%s%s=%s", k ? ", " : "", low,
                          echo_num(want[i], num, sizeof num));
  }
  if (k) echo_line(a, "m.auto.set(%s)", line);
}

/* The name xpplab's Auto.continue_from() gives the action of a key. */
static const char *auto_action(const char *type, const char *key) {
  static const struct { const char *key, *name; } names[] = {
      {"p", "periodic"}, {"t", "two-parameter"}, {"s", "switch"},
      {"d", "doubling"}, {"f", "fixed-period"},  {"e", "extend"}, {"n", "new"}};
  size_t i;
  /* From a fold, AUTO follows it in two parameters whatever the key. */
  if (strcmp(type, "LP") == 0) return "two-parameter";
  for (i = 0; i < sizeof names / sizeof names[0]; i++)
    if (strcmp(key, names[i].key) == 0) return names[i].name;
  /* "": a fold's two-parameter curve, or an ordinary point's branch */
  return strcmp(type, "LP") == 0 ? "two-parameter" : "extend";
}

static void report_auto(App *a, int before) {
  int i, specials = 0;
  /* AUTO's code ended its run where XPPAUT itself would have closed (see
     go_go_auto_guarded() in the engine): say so. */
  if (strstr(a->eng->api.xppe_log(), "AUTO stopped here"))
    app_log(a, "AUTO stopped early: it met a singular matrix (at a branch "
               "point, or where steady states are not isolated), where "
               "XPPAUT itself would have closed. The points so far are kept.");
  for (i = before; i < a->au.count; i++) {
    xppe_auto_point_info *p = &a->au.info[i];
    if (p->label && p->type_name[0] && strcmp(p->type_name, "EP") != 0) {
      if (specials < 12)
        app_log(a, "  %-2s #%d at %s = %.6g (%s%s)", p->type_name, p->label,
                a->eng->api.xppe_auto_parameter_name(p->par1_slot), p->par1,
                p->stable ? "stable" : "unstable",
                p->periodic ? " periodic orbit" : " steady state");
      specials++;
    }
  }
  if (specials > 12) app_log(a, "  ... and %d more special points", specials - 12);
  app_log(a, "AUTO computed %d points.", a->au.count - before);
}

static bool auto_equilibria(App *a);

bool app_auto_equilibria(App *a) {
  bool outer, ok;
  char name[160];
  if (!a->model_ok) return false;
  outer = echo_begin(a);
  if (outer) echo_auto_setup(a);
  ok = auto_equilibria(a);
  if (outer && ok)
    echo_line(a, "m.auto.equilibria(%s)",
              echo_str(a->pars[a->au.main_par], name, sizeof name));
  echo_end(a);
  return ok;
}

static bool auto_equilibria(App *a) {
  xpp_engine_api *x;
  int before, rc;
  if (!a->model_ok || !app_auto_push_settings(a)) return false;
  x = &a->eng->api;
  /* Start from a nearby equilibrium, as one does in XPPAUT. */
  {
    double *g = malloc(sizeof(double) * (size_t)a->nvars);
    double *re = malloc(sizeof(double) * (size_t)a->nvars);
    double *im = malloc(sizeof(double) * (size_t)a->nvars);
    int i;
    memcpy(g, a->ic, sizeof(double) * (size_t)a->nvars);
    if (x->xppe_equilibrium(g, re, im) == XPPE_OK)
      for (i = 0; i < a->nvars; i++) x->xppe_set_number(a->vars[i], g[i]);
    else
      app_log(a, "AUTO: no steady state was found near the initial values "
                 "(Newton's method did not converge); AUTO starts from them "
                 "anyway.");
    free(g);
    free(re);
    free(im);
  }
  x->xppe_auto_ungrab();
  before = x->xppe_auto_count();
  app_log(a, "AUTO: steady states as %s varies ...", a->pars[a->au.main_par]);
  if ((rc = x->xppe_auto_run("s")) != XPPE_OK) {
    app_log(a, "AUTO: %s", x->xppe_error());
    if (rc == XPPE_ERR_FATAL)
      recover_engine(a, "AUTO");
    else
      app_auto_refresh(a);
    return false;
  }
  app_auto_refresh(a);
  app_refresh_values(a);
  report_auto(a, before);
  /* Stopped at once without converging: say why and what to try. */
  if (a->au.count - before <= 3 && a->au.count > before &&
      strcmp(a->au.info[a->au.count - 1].type_name, "MX") == 0)
    app_log(a, "AUTO stopped at once (MX: no convergence). It follows a "
               "steady state, so it must start from one: run the model until "
               "it settles, take its end as the initial values (ic last, or "
               "I L), and try again. If no steady state can be found at all, "
               "the model's steady states may not be isolated: some variables "
               "(a position, a running total) can rest at any value, and AUTO "
               "cannot follow such steady states.");
  app_status(a, "AUTO: %d points", a->au.count);
  return true;
}

int app_auto_actions(App *a, int point, const char **labels, const char **keys,
                     int max) {
  xppe_auto_point_info *p;
  const char *t;
  int n = 0;
#define ADD(l, k)                                                          \
  if (n < max) {                                                           \
    labels[n] = l;                                                         \
    keys[n] = k;                                                           \
    n++;                                                                   \
  }
  if (point < 0 || point >= a->au.count) return 0;
  p = &a->au.info[point];
  t = p->type_name;
  if (strcmp(t, "HB") == 0) {
    ADD("Periodic orbits from this Hopf point", "p");
    ADD("Two-parameter curve of Hopf points", "t");
    ADD("Extend the steady-state branch", "e");
  } else if (strcmp(t, "LP") == 0) {
    ADD("Two-parameter curve of folds", "");
  } else if (strcmp(t, "BP") == 0) {
    ADD("Switch to the other branch", "s");
    ADD("Extend this branch", "e");
    ADD("Two-parameter curve of branch points", "t");
  } else if (strcmp(t, "PD") == 0) {
    ADD("Period-doubled branch", "d");
    ADD("Two-parameter curve of period doublings", "t");
    ADD("Extend this branch", "e");
  } else if (strcmp(t, "TR") == 0) {
    ADD("Two-parameter curve of torus points", "t");
    ADD("Extend this branch", "e");
  } else if (p->periodic) {
    ADD("Extend the branch of periodic orbits", "e");
    ADD("Continue at fixed period", "f");
  } else {
    ADD("Extend the steady-state branch", "");
  }
#undef ADD
  return n;
}

static bool auto_continue(App *a, int point, const char *key);

bool app_auto_continue(App *a, int point, const char *key) {
  bool outer, ok;
  char type[16], name[2][160];
  const char *action;
  int label;
  if (!a->model_ok || point < 0 || point >= a->au.count) return false;
  /* The point's array is rebuilt by the run: keep what the line needs. */
  label = a->au.info[point].label;
  snprintf(type, sizeof type, "%s", a->au.info[point].type_name);
  action = auto_action(type, key);
  outer = echo_begin(a);
  if (outer && label) echo_auto_setup(a);
  ok = auto_continue(a, point, key);
  if (outer && ok) {
    if (strcmp(action, "two-parameter") == 0 && a->au.second_par >= 0)
      echo_line(a, "m.auto.continue_from(%d, \"two-parameter\", parameter=%s, second=%s)",
                label, echo_str(a->pars[a->au.main_par], name[0], sizeof name[0]),
                echo_str(a->pars[a->au.second_par], name[1], sizeof name[1]));
    else
      echo_line(a, "m.auto.continue_from(%d, \"%s\")", label, action);
  }
  echo_end(a);
  return ok;
}

static bool auto_continue(App *a, int point, const char *key) {
  xpp_engine_api *x;
  xppe_auto_point_info *p;
  int before, rc;
  if (!a->model_ok || point < 0 || point >= a->au.count) return false;
  p = &a->au.info[point];
  if (!p->label) {
    app_log(a, "AUTO: only labelled points can be continued; pick a point "
               "with a number.");
    return false;
  }
  if (!app_auto_push_settings(a)) return false;
  x = &a->eng->api;
  if (x->xppe_auto_grab(p->label) != XPPE_OK) {
    app_log(a, "AUTO: %s", x->xppe_error());
    return false;
  }
  before = x->xppe_auto_count();
  app_log(a, "AUTO: continuing from %s #%d ...",
          p->type_name[0] ? p->type_name : "point", p->label);
  if ((rc = x->xppe_auto_run(key)) != XPPE_OK) {
    app_log(a, "AUTO: %s", x->xppe_error());
    if (rc == XPPE_ERR_FATAL)
      recover_engine(a, "AUTO");
    else
      app_auto_refresh(a);
    return false;
  }
  app_auto_refresh(a);
  app_refresh_values(a);
  report_auto(a, before);
  app_status(a, "AUTO: %d points in the diagram", a->au.count);
  return true;
}

void app_auto_reset(App *a) {
  bool outer;
  if (!a->model_ok) return;
  outer = echo_begin(a);
  a->eng->api.xppe_auto_reset();
  if (outer) echo_line(a, "m.auto.reset()");
  echo_end(a);
  a->au.selected = -1;
  app_auto_refresh(a);
  app_log(a, "AUTO: diagram cleared.");
}

/* AUTO > File > Save diagram, Load diagram and Import orbit (the engine's
   xppe_auto_save, _load and _orbit). */
bool app_auto_save(App *a, const char *path) {
  char p[2100], q[2200];
  bool outer;
  if (!a->model_ok) return false;
  echo_abs_path(path, p, sizeof p);
  outer = echo_begin(a);
  if (a->eng->api.xppe_auto_save(p) != XPPE_OK) {
    app_log(a, "error: %s", a->eng->api.xppe_error());
    echo_end(a);
    return false;
  }
  if (outer) echo_line(a, "m.auto.save(%s)", echo_str(p, q, sizeof q));
  echo_end(a);
  app_log(a, "Saved the bifurcation diagram (%d points) to %s", a->au.count, p);
  app_status(a, "Saved the diagram to %s", p);
  return true;
}

bool app_auto_load(App *a, const char *path) {
  char p[2100], q[2200];
  bool outer;
  if (!a->model_ok) return false;
  echo_abs_path(path, p, sizeof p);
  outer = echo_begin(a);
  if (a->eng->api.xppe_auto_load(p) != XPPE_OK) {
    app_log(a, "error: %s", a->eng->api.xppe_error());
    echo_end(a);
    return false;
  }
  if (outer) echo_line(a, "m.auto.load(%s)", echo_str(p, q, sizeof q));
  echo_end(a);
  a->au.selected = -1;
  app_auto_refresh(a);
  read_auto_settings(a); /* the file brings AUTO's settings too */
  a->show_auto = true;
  app_log(a, "Loaded a bifurcation diagram of %d points from %s; its labelled "
             "points can be continued.", a->au.count, p);
  app_status(a, "Loaded the diagram: %d points", a->au.count);
  return true;
}

bool app_auto_orbit(App *a, int point) {
  int label;
  bool outer;
  if (!a->model_ok || point < 0 || point >= a->au.count) return false;
  label = a->au.info[point].label;
  outer = echo_begin(a);
  if (a->eng->api.xppe_auto_orbit(label) != XPPE_OK) {
    app_log(a, "error: %s", a->eng->api.xppe_error());
    echo_end(a);
    return false;
  }
  if (outer) echo_line(a, "sol = m.auto.orbit(%d)", label);
  echo_end(a);
  if (a->keep_curves && a->have_traj) app_freeze(a);
  app_fetch_current(a);
  app_log(a, "The periodic orbit at point %d is now the run: one period, %d "
             "points.", label, a->traj.nrows);
  app_status(a, "Showing the orbit at point %d", label);
  return true;
}

/* ---- Files and settings -------------------------------------------------- */

/* A table as CSV: a header of names, then rows of exact numbers. */
static bool write_table(App *a, const char *path, const Trajectory *t) {
  FILE *f;
  char num[40];
  int i, j;
  f = fopen(path, "w");
  if (!f) {
    app_log(a, "error: cannot write %s", path);
    return false;
  }
  for (j = 0; j < t->ncols; j++) fprintf(f, "%s%s", j ? "," : "", t->names[j]);
  fputc('\n', f);
  for (i = 0; i < t->nrows; i++) {
    for (j = 0; j < t->ncols; j++) {
      app_format_double(num, sizeof num, t->cols[j][i]);
      fprintf(f, "%s%s", j ? "," : "", num);
    }
    fputc('\n', f);
  }
  fclose(f);
  app_log(a, "Exported %d rows to %s", t->nrows, path);
  return true;
}

/* The line that writes the same file from the script. */
static void echo_export(App *a, const char *what, const char *path) {
  char p[2100], q[2200];
  if (!echo_begin(a)) {
    echo_end(a);
    return;
  }
  echo_abs_path(path, p, sizeof p);
  echo_line(a, "%s.to_csv(%s)", what, echo_str(p, q, sizeof q));
  echo_end(a);
}

/* XPPAUT's output.dat: every column, 8 significant digits, no header, as
   xppaut -silent and Trajectory.save() write it. */
static bool write_dat(App *a, const char *path, const Trajectory *t) {
  FILE *f = fopen(path, "w");
  int i, j;
  if (!f) {
    app_log(a, "error: cannot write %s", path);
    return false;
  }
  for (i = 0; i < t->nrows; i++)
    for (j = 0; j < t->ncols; j++)
      fprintf(f, j + 1 < t->ncols ? "%.8g " : "%.8g\n", t->cols[j][i]);
  fclose(f);
  app_log(a, "Exported %d rows to %s (XPPAUT's format)", t->nrows, path);
  return true;
}

bool app_export_csv(App *a, const char *path) {
  size_t n = strlen(path);
  bool dat = n > 4 && SDL_strcasecmp(path + n - 4, ".dat") == 0;
  if (!a->have_traj) {
    app_log(a, "error: there is no run to export");
    return false;
  }
  if (!(dat ? write_dat(a, path, &a->traj) : write_table(a, path, &a->traj))) return false;
  if (dat) {
    char p[2100], q[2200];
    if (echo_begin(a)) {
      echo_abs_path(path, p, sizeof p);
      echo_line(a, "sol.save(%s)", echo_str(p, q, sizeof q));
    }
    echo_end(a);
  } else {
    echo_export(a, "sol", path);
  }
  return true;
}

bool app_export_analysis_csv(App *a, const char *path) {
  if (a->an.have && a->an.res_tool == AN_LINEARIZE) {
    if (!app_export_linear_csv(a, path)) return false;
    echo_export(a, "lin", path);
    return true;
  }
  if (!a->an.have || !a->echo.table[0]) {
    app_log(a, "error: there is no Analysis result to export (statistics and "
               "fits are shown in the command window)");
    return false;
  }
  if (!write_table(a, path, &a->an.res)) return false;
  echo_export(a, a->echo.table, path);
  return true;
}

bool app_export_range_csv(App *a, const char *path) {
  SweepUI *s = &a->sw;
  Trajectory t;
  char names[4][APP_NAME_LEN], p[2100], q[2200], nm[160];
  double *c[4];
  bool ok;
  int kind;
  if (s->overlay || s->n <= 0 || a->echo.sweep_mode < 0) {
    app_log(a, "error: export a Range that measures a variable or the "
               "Liapunov exponent (a Range that keeps the runs has no table)");
    return false;
  }
  if (s->result_mode == SWEEP_EQUILIBRIA) {
    if (!write_table(a, path, &s->eqt)) return false;
    echo_export(a, "eq_sweep", path);
    return true;
  }
  /* One row per run, in the order run: the swept value or values, then
     the measure. A grid's rows go through the first value for each of the
     second's. */
  memset(&t, 0, sizeof t);
  kind = s->result_kind;
  snprintf(names[0], APP_NAME_LEN, "%s", a->echo.sweep_name);
  c[0] = s->x;
  t.ncols = 1;
  if (kind) {
    snprintf(names[1], APP_NAME_LEN, "%s", a->echo.sweep_name2);
    c[1] = s->x2;
    t.ncols = 2;
  }
  if (a->echo.sweep_mode == SWEEP_LYAPUNOV) {
    snprintf(names[t.ncols], APP_NAME_LEN, "lyapunov");
    c[t.ncols++] = s->ymax;
  } else {
    snprintf(names[t.ncols], APP_NAME_LEN, "%.56s_max", a->echo.sweep_var);
    c[t.ncols++] = s->ymax;
    snprintf(names[t.ncols], APP_NAME_LEN, "%.56s_min", a->echo.sweep_var);
    c[t.ncols++] = s->ymin;
  }
  t.nrows = s->n;
  t.names = names;
  t.cols = c;
  if (!(ok = write_table(a, path, &t))) return false;
  if (echo_begin(a)) {
    const bool lyap = a->echo.sweep_mode == SWEEP_LYAPUNOV;
    char cols[600], n2[160];
    echo_abs_path(path, p, sizeof p);
    echo_str(names[0], nm, sizeof nm);
    echo_str(a->echo.sweep_name2, n2, sizeof n2);
    if (lyap)
      snprintf(cols, sizeof cols, "\"lyapunov\": sweep");
    else
      snprintf(cols, sizeof cols, "%s: sweep[..., 0], %s: sweep[..., 1]",
               echo_str(names[kind ? 2 : 1], q, sizeof q),
               echo_str(names[kind ? 3 : 2], q + 1100, sizeof q - 1100));
    if (kind == 2)
      echo_line(a, "xs.Table.from_grid(%s, sweep_values, %s, sweep_values2, {%s}).to_csv(%s)",
                nm, n2, cols, echo_str(p, q, sizeof q));
    else if (kind == 1)
      echo_line(a, "xs.Table({%s: sweep_values, %s: sweep_values2, %s}).to_csv(%s)", nm, n2,
                cols, echo_str(p, q, sizeof q));
    else
      echo_line(a, "xs.Table({%s: sweep_values, %s}).to_csv(%s)", nm, cols,
                echo_str(p, q, sizeof q));
  }
  echo_end(a);
  return true;
}

/* The columns xpplab's EquilibriumList.to_csv() writes. */
bool app_export_equilibria_csv(App *a, const char *path) {
  FoundEquilibria *f = &a->found;
  FILE *fp;
  char num[40];
  int n = a->nvars, i, k;
  if (!a->model_ok || f->n <= 0) {
    app_log(a, "error: no equilibria found yet (eq all)");
    return false;
  }
  fp = fopen(path, "w");
  if (!fp) {
    app_log(a, "error: cannot write %s", path);
    return false;
  }
  for (i = 0; i < n; i++) fprintf(fp, "%s,", a->vars[i]);
  fputs("unstable", fp);
  for (i = 1; i <= n; i++) fprintf(fp, ",re%d,im%d", i, i);
  fputc('\n', fp);
  for (k = 0; k < f->n; k++) {
    for (i = 0; i < n; i++) {
      app_format_double(num, sizeof num, f->x[(size_t)k * n + i]);
      fprintf(fp, "%s,", num);
    }
    fprintf(fp, "%d", f->unstable[k]);
    for (i = 0; i < n; i++) {
      app_format_double(num, sizeof num, f->re[(size_t)k * n + i]);
      fprintf(fp, ",%s", num);
      app_format_double(num, sizeof num, f->im[(size_t)k * n + i]);
      fprintf(fp, ",%s", num);
    }
    fputc('\n', fp);
  }
  fclose(fp);
  app_log(a, "Exported %d equilibria to %s", f->n, path);
  echo_export(a, "eqs", path);
  return true;
}

/* The points of the bifurcation diagram, with the columns xpplab's
   Diagram.to_csv() writes. */
bool app_export_auto_csv(App *a, const char *path) {
  xpp_engine_api *x;
  FILE *f;
  char num[40];
  int i, j;
  if (!a->model_ok || a->au.count == 0) {
    app_log(a, "error: the bifurcation diagram has no points yet");
    return false;
  }
  x = &a->eng->api;
  f = fopen(path, "w");
  if (!f) {
    app_log(a, "error: cannot write %s", path);
    return false;
  }
  fputs("branch,point,type,label,stable,periodic,two_parameter,parameter,value,"
        "parameter2,value2,norm,period", f);
  for (j = 0; j < a->nvars; j++) fprintf(f, ",%s_max", a->vars[j]);
  for (j = 0; j < a->nvars; j++) fprintf(f, ",%s_min", a->vars[j]);
  fputc('\n', f);
  for (i = 0; i < a->au.count; i++) {
    const xppe_auto_point_info *p = &a->au.info[i];
    const char *n1 = x->xppe_auto_parameter_name(p->par1_slot);
    const char *n2 = p->two_parameter ? x->xppe_auto_parameter_name(p->par2_slot) : NULL;
    fprintf(f, "%d,%d,%s,%d,%d,%d,%d,%s,", p->branch, p->point, p->type_name, p->label,
            p->stable ? 1 : 0, p->periodic ? 1 : 0, p->two_parameter ? 1 : 0,
            n1 ? n1 : "");
    app_format_double(num, sizeof num, p->par1);
    fprintf(f, "%s,%s,", num, n2 ? n2 : "");
    if (n2) {
      app_format_double(num, sizeof num, p->par2);
      fputs(num, f);
    }
    app_format_double(num, sizeof num, p->norm);
    fprintf(f, ",%s,", num);
    app_format_double(num, sizeof num, p->period);
    fputs(num, f);
    for (j = 0; j < a->nvars; j++) {
      app_format_double(num, sizeof num, a->au.umax[(size_t)i * a->nvars + j]);
      fprintf(f, ",%s", num);
    }
    for (j = 0; j < a->nvars; j++) {
      app_format_double(num, sizeof num, a->au.umin[(size_t)i * a->nvars + j]);
      fprintf(f, ",%s", num);
    }
    fputc('\n', f);
  }
  fclose(f);
  app_log(a, "Exported %d points of the bifurcation diagram to %s", a->au.count, path);
  echo_export(a, "m.auto.diagram", path);
  return true;
}

void app_add_recent(App *a, const char *path) {
  int i, k = -1;
  for (i = 0; i < a->nrecent; i++)
    if (strcmp(a->recent[i], path) == 0) k = i;
  if (k < 0) {
    if (a->nrecent < APP_MAX_RECENT) a->nrecent++;
    k = a->nrecent - 1;
  }
  for (i = k; i > 0; i--) memcpy(a->recent[i], a->recent[i - 1], sizeof a->recent[0]);
  snprintf(a->recent[0], sizeof a->recent[0], "%s", path);
}

char *app_pref_dir(void) {
  static const char *carry[] = {"settings.ini", "layout.ini"};
  static bool checked = false;
  char *pref = SDL_GetPrefPath("XPPLab", "XPPLab");
  if (pref && !checked) {
    /* Until 0.1.1 the app was XPP Studio: the old folder is the same path
       with XPPStudio for XPPLab. Its settings and layout come along once. */
    char old[1024], from[1100], to[1100], *o = old, *end = old + sizeof old - 1;
    const char *s = pref;
    size_t i;
    checked = true;
    while (*s && o < end) {
      if (strncmp(s, "XPPLab", 6) == 0 && end - o >= 9) {
        memcpy(o, "XPPStudio", 9);
        o += 9;
        s += 6;
      } else {
        *o++ = *s++;
      }
    }
    *o = '\0';
    for (i = 0; i < SDL_arraysize(carry); i++) {
      snprintf(from, sizeof from, "%s%s", old, carry[i]);
      snprintf(to, sizeof to, "%s%s", pref, carry[i]);
      if (SDL_GetPathInfo(from, NULL) && !SDL_GetPathInfo(to, NULL))
        SDL_CopyFile(from, to);
    }
  }
  return pref;
}

static void settings_path(char *out, size_t len) {
  char *pref = app_pref_dir();
  snprintf(out, len, "%ssettings.ini", pref ? pref : "");
  SDL_free(pref);
}

void app_save_settings(App *a) {
  char path[1100];
  FILE *f;
  int i;
  settings_path(path, sizeof path);
  f = fopen(path, "w");
  if (!f) return;
  fprintf(f, "dark=%d\nscale=%g\nclassic_keys=%d\nlive=%d\necho=%d\n", a->dark,
          (double)a->ui_scale, a->classic_keys, a->live, a->echo.console);
  for (i = a->nrecent - 1; i >= 0; i--) fprintf(f, "recent=%s\n", a->recent[i]);
  if (a->python[0]) fprintf(f, "python=%s\n", a->python);
  fclose(f);
}

void app_load_settings(App *a) {
  char path[1100], line[1100];
  FILE *f;
  settings_path(path, sizeof path);
  f = fopen(path, "r");
  if (!f) return;
  while (fgets(line, sizeof line, f)) {
    char *eq = strchr(line, '='), *nl = strchr(line, '\n');
    if (nl) *nl = '\0';
    if (!eq) continue;
    *eq = '\0';
    if (strcmp(line, "dark") == 0) a->dark = atoi(eq + 1) != 0;
    if (strcmp(line, "scale") == 0) a->ui_scale = (float)atof(eq + 1);
    if (strcmp(line, "classic_keys") == 0) a->classic_keys = atoi(eq + 1) != 0;
    if (strcmp(line, "live") == 0) a->live = atoi(eq + 1) != 0;
    if (strcmp(line, "echo") == 0) a->echo.console = atoi(eq + 1) != 0;
    if (strcmp(line, "recent") == 0) app_add_recent(a, eq + 1);
    if (strcmp(line, "python") == 0) snprintf(a->python, sizeof a->python, "%s", eq + 1);
  }
  fclose(f);
  if (a->ui_scale < 0.5f || a->ui_scale > 3.0f) a->ui_scale = 1.0f;
}

const char *app_method_name(int i) {
  return i >= 0 && i < (int)(sizeof method_names / sizeof method_names[0])
             ? method_names[i]
             : "?";
}
