/* The Analysis window's computations: XPPAUT's data-analysis tools applied
   to the last run (see the engine's xppe.h), and the phase-response recipe
   XPPAUT users follow by hand. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "app.h"

static const char *tool_names[AN_COUNT] = {
    "Spectrum",    "Fourier series", "Histogram",      "2-D histogram",
    "Correlation", "Statistics",     "Phase response", "H function",
    "Fit to data", "Ensemble of noisy runs"};

static bool ani_load(App *a, const char *path, bool quiet);

const char *app_analysis_name(int tool) {
  return tool >= 0 && tool < AN_COUNT ? tool_names[tool] : "";
}

void app_analysis_free(App *a) {
  app_free_trajectory(&a->an.res);
  free(a->an.coupling);
  a->an.coupling = NULL;
  a->an.have = false;
}

void app_analysis_reset(App *a) {
  AnalysisUI *n = &a->an;
  int tool = n->tool, i, first;
  app_analysis_free(a);
  memset(n, 0, sizeof *n);
  n->tool = tool >= 0 && tool < AN_COUNT ? tool : AN_SPECTRUM;
  /* the first two columns after time */
  first = a->ncols > 1 && SDL_strcasecmp(a->cols[0], "T") == 0 ? 1 : 0;
  n->col = first < a->ncols ? first : 0;
  n->col2 = first + 1 < a->ncols ? first + 1 : n->col;
  n->spec_len = 512;
  n->spec_window = 2; /* Hamming */
  n->bins = 50;
  n->trials = 50;
  n->bins2 = 20;
  n->auto_range = true;
  n->lags = 100;
  n->show_col = 1;
  n->log_y = true; /* spectra span decades */
  n->coupling = calloc((size_t)(a->nvars > 0 ? a->nvars : 1), sizeof *n->coupling);
  /* Diffusive coupling through the first variable, as in XPPAUT's examples. */
  for (i = 0; i < a->nvars; i++)
    snprintf(n->coupling[i], sizeof n->coupling[i], "0");
  if (a->nvars > 0)
    snprintf(n->coupling[0], sizeof n->coupling[0], "%s'-%s", a->vars[0],
             a->vars[0]);
  if (a->nvars > 0) snprintf(n->fit_vars, sizeof n->fit_vars, "%s", a->vars[0]);
  snprintf(n->fit_cols, sizeof n->fit_cols, "2");

  /* The other per-model tools: boundary conditions, the 3D view and an
     animation that comes with the model (lorenz.ode, lorenz.ani). */
  a->nbcs = a->eng ? a->eng->api.xppe_bvp_count() : 0;
  app_view3d_reset(a);
  memset(&a->ani, 0, sizeof a->ani);
  a->ani.speed = 60;
  a->ani.loop = true;
  {
    char ani[1100];
    const char *dot = strrchr(a->model_path, '.');
    size_t stem = dot ? (size_t)(dot - a->model_path) : strlen(a->model_path);
    FILE *f;
    snprintf(ani, sizeof ani, "%.*s.ani", (int)stem, a->model_path);
    if ((f = fopen(ani, "r")) != NULL) {
      fclose(f);
      /* Quietly: some .ani files belong to another model (lorenz.ani is
         for lorenz2.ode). */
      if (ani_load(a, ani, true))
        app_log(a, "This model comes with an animation: press Animate, or "
                   "open View > Animator.");
    }
  }
}

/* ---- Boundary-value problems, the 3D view, the animator ---------------- */

bool app_bvp(App *a) {
  xpp_engine_api *x;
  int i;
  char buf[256];
  size_t len = 0;
  bool outer;
  if (!a->model_ok) return false;
  x = &a->eng->api;
  if (a->nbcs <= 0) {
    app_log(a, "This model has no boundary conditions. Add bndry statements, "
               "e.g. 'bndry x-1' (at the start) or 'bndry y'-2' (a primed "
               "name means the value at the end).");
    return false;
  }
  outer = echo_begin(a);
  if (x->xppe_bvp(0, 0.0, 0.0) != XPPE_OK) {
    app_log(a, "error: %s", x->xppe_error());
    app_status(a, "Boundary-value problem: %s", x->xppe_error());
    echo_end(a);
    return false;
  }
  if (outer) echo_line(a, "sol = m.bvp()");
  echo_end(a);
  app_refresh_values(a);
  app_fetch_current(a);
  for (i = 0; i < a->nvars && len < sizeof buf - 40; i++)
    len += (size_t)snprintf(buf + len, sizeof buf - len, " %s=%.6g",
                            a->vars[i], a->ic[i]);
  app_log(a, "Solved the boundary-value problem by shooting. Initial "
             "values:%s", buf);
  app_status(a, "Boundary-value problem solved");
  return true;
}

/* A plot option from the model's @ lines. xppe_get() looks among the
   variables and parameters first, so an option a model also uses as a name
   cannot be read that way (lor2.ode has variables xp, yp and zp, and "XP"
   gave the initial value of xp, -8): the default stays. */
static void get_option(App *a, const char *name, double *value) {
  int i;
  for (i = 0; i < a->nvars; i++)
    if (SDL_strcasecmp(a->vars[i], name) == 0) return;
  for (i = 0; i < a->npars; i++)
    if (SDL_strcasecmp(a->pars[i], name) == 0) return;
  a->eng->api.xppe_get(name, value);
}

/* A column for the 3D view: the one asked for if it exists, else fallback. */
static int column_or(const App *a, double asked, int fallback) {
  return asked >= 0 && asked < a->ncols ? (int)asked : fallback;
}

void app_view3d_reset(App *a) {
  static const char *keys[6] = {"XMIN", "XMAX", "YMIN", "YMAX", "ZMIN", "ZMAX"};
  View3D *v = &a->v3;
  double xp = 1, yp = 2, zp = 3, axes = 0, th = 45, ph = 45;
  int i;
  memset(v, 0, sizeof *v);
  v->zoom = 1;
  v->auto_box = true;
  if (!a->eng || a->ncols < 2) return;
  get_option(a, "XP", &xp);
  get_option(a, "YP", &yp);
  get_option(a, "ZP", &zp);
  get_option(a, "AXES", &axes);
  get_option(a, "VIEW_THETA", &th);
  get_option(a, "VIEW_PHI", &ph);
  /* XP, YP and ZP number the columns from 0 for time, as App.cols does. */
  v->x = column_or(a, xp, 1);
  v->y = column_or(a, yp, a->ncols > 2 ? 2 : 1);
  v->z = zp >= 1 && zp != xp ? column_or(a, zp, a->ncols > 3 ? 3 : 0)
                             : (a->ncols > 3 ? 3 : 0);
  v->theta = (float)th;
  v->phi = (float)ph;
  if (axes == 5) {
    for (i = 0; i < 6; i++) get_option(a, keys[i], &v->box[i]);
    v->auto_box = !(v->box[0] < v->box[1] && v->box[2] < v->box[3] &&
                    v->box[4] < v->box[5]);
    a->show_view3d = true; /* the model asks for a 3D plot */
    v->front = 3;          /* once the windows exist */
  }
}

static bool ani_load(App *a, const char *path, bool quiet) {
  xpp_engine_api *x;
  AniUI *v = &a->ani;
  int n;
  if (!a->model_ok) return false;
  x = &a->eng->api;
  n = x->xppe_ani_load(path);
  if (n < 0) {
    if (!quiet) {
      app_log(a, "error: %s", x->xppe_error());
      app_log(a, "%s", x->xppe_log());
    }
    v->loaded = false;
    return false;
  }
  snprintf(v->path, sizeof v->path, "%s", path);
  v->loaded = true;
  v->ncommands = n;
  x->xppe_ani_window(&v->xlo, &v->ylo, &v->xhi, &v->yhi);
  v->row = 0;
  v->last_row = -1;
  v->ncomets = 0;
  v->playing = false;
  if (v->speed <= 0) v->speed = 60;
  if (!quiet) app_log(a, "Loaded the animation %s (%d commands).", path, n);
  return true;
}

bool app_ani_load(App *a, const char *path) { return ani_load(a, path, false); }

/* ---- Helpers -------------------------------------------------------------- */

/* A column of the engine's current run, by name; returns the number of
   values copied, or -1. */
static int engine_column(App *a, const char *name, double *out, int n) {
  xpp_engine_api *x = &a->eng->api;
  int j;
  for (j = 0; j < a->ncols; j++)
    if (SDL_strcasecmp(a->cols[j], name) == 0)
      return x->xppe_column(a->time_missing ? j - 1 : j, out, n);
  return -1;
}

/* Start the next run from the end of the current one. */
static void start_from_end(App *a) {
  xpp_engine_api *x = &a->eng->api;
  int rows = x->xppe_rows(), i;
  double *buf;
  if (rows < 1) return;
  buf = malloc(sizeof(double) * (size_t)rows);
  for (i = 0; i < a->nvars; i++)
    if (engine_column(a, a->vars[i], buf, rows) == rows)
      x->xppe_set_number(a->vars[i], buf[rows - 1]);
  free(buf);
}

static void data_range(App *a, int col, double *lo, double *hi) {
  int i;
  *lo = 0;
  *hi = 1;
  if (!a->have_traj || col < 0 || col >= a->traj.ncols || a->traj.nrows < 1)
    return;
  *lo = *hi = a->traj.cols[col][0];
  for (i = 1; i < a->traj.nrows; i++) {
    double v = a->traj.cols[col][i];
    if (v < *lo) *lo = v;
    if (v > *hi) *hi = v;
  }
  if (*hi <= *lo) *hi = *lo + 1;
}

/* Copy the engine's result table into a->an.res. With phase_of > 0, a
   first column "phase" = (T - T0) / phase_of is added. */
static void fetch_result(App *a, double phase_of) {
  xpp_engine_api *x = &a->eng->api;
  AnalysisUI *n = &a->an;
  int rows = x->xppe_result_rows(), ncols = x->xppe_result_count(), j, k;
  int extra = phase_of > 0 ? 1 : 0;
  app_free_trajectory(&n->res);
  n->res.nrows = rows;
  n->res.ncols = ncols + extra;
  n->res.names = calloc((size_t)(ncols + extra), APP_NAME_LEN);
  n->res.cols = calloc((size_t)(ncols + extra), sizeof(double *));
  for (j = 0; j < ncols; j++) {
    snprintf(n->res.names[j + extra], APP_NAME_LEN, "%s", x->xppe_result_name(j));
    n->res.cols[j + extra] = malloc(sizeof(double) * (size_t)(rows > 0 ? rows : 1));
    x->xppe_result_column(j, n->res.cols[j + extra], rows);
  }
  if (extra) {
    snprintf(n->res.names[0], APP_NAME_LEN, "phase");
    n->res.cols[0] = malloc(sizeof(double) * (size_t)(rows > 0 ? rows : 1));
    for (k = 0; k < rows; k++)
      n->res.cols[0][k] =
          ncols > 0 ? (n->res.cols[1][k] - n->res.cols[1][0]) / phase_of : 0;
  }
  n->have = rows > 0;
  n->refit = true;
  if (n->show_col < 1 || n->show_col >= n->res.ncols) n->show_col = 1;
  /* Phase response: phase, T, then the variables; show the first one. */
  if (extra && (n->show_col < 2 || n->show_col >= n->res.ncols))
    n->show_col = n->res.ncols > 2 ? 2 : 1;
}

/* The period from upward crossings of the middle of the range, averaged
   over the last few cycles. Returns 0 if there is no steady oscillation. */
static double measure_period(const double *t, const double *v, int n) {
  double lo = v[0], hi = v[0], mid, tc[64], mean = 0, dev = 0;
  int i, nc = 0, k, used;
  for (i = 1; i < n; i++) {
    if (v[i] < lo) lo = v[i];
    if (v[i] > hi) hi = v[i];
  }
  if (hi - lo <= 0) return 0;
  mid = 0.5 * (lo + hi);
  for (i = 0; i + 1 < n; i++)
    if (v[i] < mid && v[i + 1] >= mid) {
      double s0 = v[i] - mid, s1 = v[i + 1] - mid;
      if (nc == 64) {
        memmove(tc, tc + 1, sizeof tc[0] * 63);
        nc = 63;
      }
      tc[nc++] = t[i] - s0 * (t[i + 1] - t[i]) / (s1 - s0);
    }
  if (nc < 3) return 0;
  used = nc - 1 < 5 ? nc - 1 : 5;
  for (k = nc - used; k < nc; k++) mean += tc[k] - tc[k - 1];
  mean /= used;
  for (k = nc - used; k < nc; k++) {
    double d = tc[k] - tc[k - 1] - mean;
    dev += d * d;
  }
  /* Steady when the periods' standard deviation is within 1% of their
     mean: the test xpplab's Model.limit_cycle() makes, so that both find
     the same period and a session's script repeats it. */
  if (sqrt(dev / used) > 0.01 * mean) return 0;
  return mean;
}

/* One period of the limit cycle, then XPPAUT's adjoint along it. The run
   is left as that period, which the H function needs. */
static char an_error[256]; /* a failure found here rather than in the engine */

static int phase_response(App *a) {
  xpp_engine_api *x = &a->eng->api;
  AnalysisUI *n = &a->an;
  double total = a->total, settle, length, period = 0, *t = NULL, *v = NULL;
  double *ics = malloc(sizeof(double) * (size_t)(a->nvars + 1));
  int rows, rc = XPPE_OK, i, attempt;
  for (i = 0; i < a->nvars; i++) x->xppe_get(a->vars[i], &ics[i]);
  settle = n->transient > 0 ? n->transient : total;
  length = total > settle ? total : settle;
  /* Settle, then look for a few cycles; if there are not enough, keep
     going with twice the time (the period is not known in advance). */
  for (attempt = 0; attempt < 6 && period <= 0; attempt++) {
    x->xppe_set_number("TOTAL", settle);
    if ((rc = x->xppe_run()) != XPPE_OK) goto done;
    start_from_end(a);
    x->xppe_set_number("TOTAL", length);
    if ((rc = x->xppe_run()) != XPPE_OK) goto done;
    rows = x->xppe_rows();
    free(t);
    free(v);
    t = malloc(sizeof(double) * (size_t)(rows + 1));
    v = malloc(sizeof(double) * (size_t)(rows + 1));
    x->xppe_column(-1, t, rows);
    if (a->nvars < 1 || engine_column(a, a->vars[0], v, rows) != rows) break;
    /* The next attempt, or the period below, starts at the end of this run,
       as in Model.limit_cycle(). */
    start_from_end(a);
    period = measure_period(t, v, rows);
    settle *= 2;
    length *= 2;
  }
  if (period <= 0) {
    snprintf(an_error, sizeof an_error,
             "no steady oscillation of %s found. Check that the model "
             "oscillates, or give a longer transient.",
             a->nvars > 0 ? a->vars[0] : "?");
    rc = XPPE_ERR_NUMERIC;
    goto done;
  }
  start_from_end(a);
  x->xppe_set_number("TOTAL", period);
  if ((rc = x->xppe_run()) != XPPE_OK) goto done;
  rc = x->xppe_adjoint(0, 0.0);
done:
  x->xppe_set_number("TOTAL", total);
  for (i = 0; i < a->nvars; i++) x->xppe_set_number(a->vars[i], ics[i]);
  free(ics);
  free(t);
  free(v);
  n->period = rc == XPPE_OK ? period : 0;
  return rc;
}

/* Rows and numbers per row of a data file. */
static bool file_shape(const char *path, int *rows, int *cols) {
  FILE *f = fopen(path, "r");
  char line[4096];
  *rows = *cols = 0;
  if (!f) return false;
  while (fgets(line, sizeof line, f)) {
    char *p = line, *end;
    int k = 0;
    for (;;) {
      strtod(p, &end);
      if (end == p) break;
      k++;
      p = end;
    }
    if (k == 0) continue;
    if (*cols == 0) *cols = k;
    (*rows)++;
  }
  fclose(f);
  return *rows > 0;
}

/* ---- Running a tool ------------------------------------------------------ */

/* A list of names or numbers typed with commas or spaces, as Python: words
   quoted, numbers as they are. */
static void echo_list(const char *typed, bool quote, char *out, size_t len) {
  char copy[512], *w, item[160];
  size_t k = 0;
  snprintf(copy, sizeof copy, "%s", typed);
  out[0] = '\0';
  for (w = strtok(copy, " ,"); w && k < len - 1; w = strtok(NULL, " ,"))
    k += (size_t)snprintf(out + k, len - k, "%s%s", k ? ", " : "",
                          quote ? echo_str(w, item, sizeof item) : w);
}

/* The tool's line, with every argument the engine was given. */
static void echo_analysis(App *a, const char *c1, const char *c2, int spec_len,
                          bool lyap_ok) {
  static const char *windows[] = {"square", "parzen", "hamming", "bartlett", "hann"};
  static const char *kinds[] = {"psd", "cross", "coherence"};
  AnalysisUI *n = &a->an;
  char s1[160], s2[160], v[4][64], extra[600], win[32];
  const char *table = "";
  int i;
  echo_str(c1, s1, sizeof s1);
  echo_str(c2, s2, sizeof s2);
  switch (n->tool) {
  case AN_SPECTRUM:
    table = "spec";
    if (n->spec_window >= 0 && n->spec_window <= 4)
      snprintf(win, sizeof win, "\"%s\"", windows[n->spec_window]);
    else
      snprintf(win, sizeof win, "%d", n->spec_window);
    if (n->spec_kind <= 0 || n->spec_kind > 2)
      echo_line(a, "spec = m.spectrum(%s, length=%d, window=%s)", s1, spec_len, win);
    else
      echo_line(a, "spec = m.spectrum(%s, %s, kind=\"%s\", length=%d, window=%s)", s1,
                s2, kinds[n->spec_kind], spec_len, win);
    break;
  case AN_FOURIER:
    table = "fourier";
    echo_line(a, "fourier = m.%s(%s)", n->polar ? "power" : "fourier", s1);
    break;
  case AN_HISTOGRAM:
    table = "hist";
    extra[0] = '\0';
    if (n->condition[0]) {
      char c[300];
      snprintf(extra, sizeof extra, ", condition=%s",
               echo_str(n->condition, c, sizeof c));
    }
    echo_line(a, "hist = m.histogram(%s, bins=%d, range=(%s, %s)%s)", s1, n->bins,
              echo_num(n->lo, v[0], 64), echo_num(n->hi, v[1], 64), extra);
    break;
  case AN_HISTOGRAM2D:
    table = "hist2d";
    echo_line(a, "hist2d = m.histogram2d(%s, %s, bins=(%d, %d), range=((%s, %s), "
                 "(%s, %s)))", s1, s2, n->bins, n->bins2, echo_num(n->lo, v[0], 64),
              echo_num(n->hi, v[1], 64), echo_num(n->lo2, v[2], 64),
              echo_num(n->hi2, v[3], 64));
    break;
  case AN_CORRELATION:
    table = "corr";
    if (n->col2 == n->col)
      echo_line(a, "corr = m.correlation(%s, lags=%d%s)", s1, n->lags,
                n->use_fft ? ", fft=True" : "");
    else
      echo_line(a, "corr = m.correlation(%s, %s, lags=%d%s)", s1, s2, n->lags,
                n->use_fft ? ", fft=True" : "");
    break;
  case AN_STATISTICS:
    echo_line(a, "stats = m.mean(%s)", s1);
    if (lyap_ok) echo_line(a, "lyap = m.lyapunov()");
    break;
  case AN_PRC:
  case AN_HFUNCTION:
    table = n->tool == AN_PRC ? "prc" : "h";
    if (n->transient > 0)
      echo_line(a, "prc = m.phase_response(transient=%s)",
                echo_num(n->transient, v[0], 64));
    else
      echo_line(a, "prc = m.phase_response()");
    if (n->tool == AN_HFUNCTION) {
      char list[1200], f[300];
      size_t k = 0;
      list[0] = '\0';
      for (i = 0; i < a->nvars && k < sizeof list - 320; i++)
        k += (size_t)snprintf(list + k, sizeof list - k, "%s%s", i ? ", " : "",
                              echo_str(n->coupling[i], f, sizeof f));
      echo_line(a, "h = m.h_function([%s])", list);
    }
    echo_line(a, "sol = m.last_run()");
    break;
  case AN_FIT: {
    char p[2100], q[2200], vars[600], cols[300], pars[800];
    echo_abs_path(n->fit_file, p, sizeof p);
    echo_list(n->fit_vars, true, vars, sizeof vars);
    echo_list(n->fit_cols, false, cols, sizeof cols);
    echo_list(n->fit_pars, true, pars, sizeof pars);
    echo_line(a, "fitted = m.fit(%s, variables=[%s], columns=[%s], parameters=[%s])",
              echo_str(p, q, sizeof q), vars, cols, pars);
    echo_line(a, "sol = m.run()");
    break;
  }
  case AN_ENSEMBLE:
    table = "ens";
    echo_line(a, "ens = m.ensemble(%d)", n->trials);
    echo_line(a, "sol = m.last_run()");
    break;
  default:
    break;
  }
  snprintf(a->echo.table, sizeof a->echo.table, "%s", table);
}

/* XPPAUT's stocHastic > Compute: the runs' mean and variance at each time,
   as a table of time, then V_mean, V_var, ... (Ensemble.to_table()). */
static bool ensemble(App *a, bool outer) {
  AnalysisUI *n = &a->an;
  xpp_engine_api *x = &a->eng->api;
  Trajectory *r = &n->res;
  int rows = x->xppe_ensemble(n->trials), j, w;
  if (rows < 0) {
    app_log(a, "error: %s", x->xppe_error());
    app_status(a, "Ensemble failed: %s", x->xppe_error());
    app_fetch_current(a);
    return false;
  }
  app_free_trajectory(r);
  r->nrows = rows;
  r->ncols = 1 + 2 * (a->ncols - 1);
  r->names = calloc((size_t)r->ncols, APP_NAME_LEN);
  r->cols = calloc((size_t)r->ncols, sizeof(double *));
  for (j = 0; j < r->ncols; j++)
    r->cols[j] = malloc(sizeof(double) * (size_t)(rows > 0 ? rows : 1));
  snprintf(r->names[0], APP_NAME_LEN, "T");
  x->xppe_ensemble_column(-1, 0, r->cols[0], rows);
  for (j = 1; j < a->ncols; j++)
    for (w = 0; w < 2; w++) {
      int k = 2 * j - 1 + w;
      snprintf(r->names[k], APP_NAME_LEN, "%s_%s", a->cols[j], w ? "var" : "mean");
      x->xppe_ensemble_column(a->time_missing ? j - 1 : j, w, r->cols[k], rows);
    }
  n->have = true;
  n->res_tool = AN_ENSEMBLE;
  n->show_col = 1;
  n->refit = true;
  app_fetch_current(a); /* the last of the runs */
  if (outer) echo_analysis(a, a->cols[0], a->cols[0], 0, false);
  snprintf(n->summary, sizeof n->summary,
           "Mean and variance of %d runs at each of %d times; the band is one "
           "standard deviation. The run shown elsewhere is the last of them.",
           n->trials, rows);
  app_log(a, "Ensemble of %d runs: %d times.", n->trials, rows);
  app_status(a, "Ensemble computed");
  return true;
}

static bool analysis_run(App *a, bool outer);

bool app_analysis_run(App *a) {
  bool outer, ok;
  if (!a->model_ok) return false;
  outer = echo_begin(a);
  ok = analysis_run(a, outer);
  echo_end(a);
  return ok;
}

static bool analysis_run(App *a, bool outer) {
  xpp_engine_api *x;
  AnalysisUI *n = &a->an;
  const char *c1, *c2;
  int rc = XPPE_OK, spec_len = 0;
  if (n->col < 0 || n->col >= a->ncols) n->col = 0;
  if (n->col2 < 0 || n->col2 >= a->ncols) n->col2 = n->col;
  x = &a->eng->api;
  if (n->tool == AN_ENSEMBLE) return ensemble(a, outer);
  if (!a->have_traj && n->tool != AN_FIT && n->tool != AN_PRC &&
      n->tool != AN_HFUNCTION) {
    if (!app_run(a)) return false;
    if (outer) echo_line(a, "sol = m.run()");
  }
  c1 = a->cols[n->col];
  c2 = a->cols[n->col2];
  n->summary[0] = '\0';
  an_error[0] = '\0';
  switch (n->tool) {
  case AN_SPECTRUM: {
    /* A window longer than the run gives nothing: shorten it. */
    int rows = x->xppe_rows(), len = n->spec_len;
    while (len > 4 && (rows - len + 1) / (len / 2) < 1) len /= 2;
    if (len != n->spec_len)
      app_log(a, "note: the run has %d points, so the window is %d points "
                 "instead of %d.", rows, len, n->spec_len);
    rc = x->xppe_spectrum(n->spec_kind, c1, c2, len, n->spec_window);
    spec_len = len;
    break;
  }
  case AN_FOURIER:
    rc = n->polar ? x->xppe_power(c1) : x->xppe_fourier(c1);
    if (rc == XPPE_OK && n->polar) {
      const char *log = x->xppe_log(), *p = strstr(log, "a0=");
      if (p) snprintf(n->summary, sizeof n->summary, "%.*s",
                      (int)strcspn(p, "\r\n"), p);
    }
    break;
  case AN_HISTOGRAM:
    if (n->auto_range) {
      data_range(a, n->col, &n->lo, &n->hi);
    }
    rc = x->xppe_histogram(c1, n->bins, n->lo, n->hi, n->condition);
    break;
  case AN_HISTOGRAM2D:
    if (n->auto_range) {
      data_range(a, n->col, &n->lo, &n->hi);
      data_range(a, n->col2, &n->lo2, &n->hi2);
    }
    rc = x->xppe_histogram2d(c1, n->bins, n->lo, n->hi, c2, n->bins2, n->lo2,
                             n->hi2);
    break;
  case AN_CORRELATION:
    rc = x->xppe_correlation(c1, c2, n->lags, n->use_fft);
    break;
  case AN_STATISTICS: {
    double mean = 0, sd = 0, lyap = 0;
    rc = x->xppe_mean(c1, &mean, &sd);
    if (rc == XPPE_OK) {
      int lr = x->xppe_lyapunov(&lyap);
      if (outer) echo_analysis(a, c1, c2, 0, lr == XPPE_OK);
      if (lr == XPPE_OK)
        snprintf(n->summary, sizeof n->summary,
                 "%s: mean %.6g, standard deviation %.6g.   Largest "
                 "Liapunov exponent of the run: %.4g", c1, mean, sd, lyap);
      else
        snprintf(n->summary, sizeof n->summary,
                 "%s: mean %.6g, standard deviation %.6g", c1, mean, sd);
      app_log(a, "%s", n->summary);
    }
    app_free_trajectory(&n->res);
    n->have = false;
    n->res_tool = n->tool;
    return rc == XPPE_OK;
  }
  case AN_PRC:
    rc = phase_response(a);
    break;
  case AN_HFUNCTION: {
    const char **f = malloc(sizeof(char *) * (size_t)(a->nvars + 1));
    int i;
    /* The H function needs the adjoint of the current run: recompute it,
       since the model may have been run again since. */
    rc = phase_response(a);
    for (i = 0; i < a->nvars; i++) f[i] = n->coupling[i];
    if (rc == XPPE_OK) rc = x->xppe_hfunction(a->nvars, f);
    free(f);
    break;
  }
  case AN_FIT: {
    int rows, cols;
    if (!file_shape(n->fit_file, &rows, &cols)) {
      app_log(a, "error: cannot read the data file '%s'", n->fit_file);
      return false;
    }
    rc = x->xppe_fit(n->fit_file, rows, cols, n->fit_vars, n->fit_cols,
                     n->fit_pars, 0, 0, 0);
    if (rc == XPPE_OK) {
      char *p, list[256];
      size_t len = 0;
      if (outer) echo_analysis(a, c1, c2, 0, false);
      app_refresh_values(a);
      snprintf(list, sizeof list, "%s", n->fit_pars);
      len = (size_t)snprintf(n->summary, sizeof n->summary, "Fitted:");
      for (p = strtok(list, " ,"); p && len < sizeof n->summary; p = strtok(NULL, " ,")) {
        double v = 0;
        x->xppe_get(p, &v);
        len += (size_t)snprintf(n->summary + len, sizeof n->summary - len,
                                " %s = %.6g", p, v);
      }
      app_log(a, "%s", n->summary);
      app_run(a); /* show the fitted solution */
    }
    app_free_trajectory(&n->res);
    n->have = false;
    n->res_tool = n->tool;
    if (rc != XPPE_OK) {
      app_log(a, "error: %s", x->xppe_error());
      app_status(a, "Fit failed: %s", x->xppe_error());
    }
    return rc == XPPE_OK;
  }
  default:
    return false;
  }
  if (rc != XPPE_OK) {
    const char *why = an_error[0] ? an_error : x->xppe_error();
    app_log(a, "error: %s", why);
    app_status(a, "%s failed: %s", app_analysis_name(n->tool), why);
    if (n->tool == AN_PRC || n->tool == AN_HFUNCTION) app_fetch_current(a);
    return false;
  }
  fetch_result(a, n->tool == AN_PRC ? n->period : 0);
  n->res_tool = n->tool;
  if (outer) echo_analysis(a, c1, c2, spec_len, false);
  if (n->tool == AN_PRC || n->tool == AN_HFUNCTION) {
    /* The run is now one period of the cycle: show it. */
    app_fetch_current(a);
    app_refresh_values(a);
    if (n->tool == AN_PRC)
      snprintf(n->summary, sizeof n->summary,
               "Period %.6g. The run is now one period of the limit cycle; "
               "each column is the phase response to kicks in that variable.",
               n->period);
  }
  if (n->tool == AN_PRC || n->tool == AN_HFUNCTION)
    app_log(a, "%s: %d points over one period (%.6g).",
            app_analysis_name(n->tool), n->res.nrows, n->period);
  else
    app_log(a, "%s of %s: %d rows.", app_analysis_name(n->tool), c1,
            n->res.nrows);
  app_status(a, "%s computed", app_analysis_name(n->tool));
  return true;
}
