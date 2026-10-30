/* The Analysis window's computations: XPPAUT's data-analysis tools applied
   to the last run (see the engine's xppe.h), and the phase-response recipe
   XPPAUT users follow by hand. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "file_utf8.h"
#include "app.h"

static const char *tool_names[AN_COUNT] = {
    "Spectrum",    "Fourier series", "Histogram",      "2-D histogram",
    "Correlation", "Statistics",     "Phase response", "H function",
    "Fit to data", "Ensemble of noisy runs", "Linearise"};

static bool ani_load(App *a, const char *path, bool quiet);

const char *app_analysis_name(int tool) {
  return tool >= 0 && tool < AN_COUNT ? tool_names[tool] : "";
}

static void free_linear(LinearSystem *L) {
  free(L->inputs);
  free(L->x); /* one block holds all the numbers */
  memset(L, 0, sizeof *L);
}

static void free_locus(RootLocusUI *R) {
  free(R->values); /* one block holds all the numbers */
  R->values = R->re = R->im = R->x = NULL;
  R->kind = LOCUS_NONE;
  R->steps = R->n = R->nx = 0;
  R->crossings[0] = '\0';
}

void app_analysis_free(App *a) {
  app_free_trajectory(&a->an.res);
  free_linear(&a->an.lin);
  free_locus(&a->an.locus);
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
    if ((f = app_fopen_utf8(ani, "r")) != NULL) {
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
  FILE *f = app_fopen_utf8(path, "r");
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

/* ---- Linearisation ---------------------------------------------------------- */

/* The inputs typed (names separated by commas or spaces) as the model's
   parameters, each once. False, with a message, for a name that is not a
   parameter. */
static bool parse_inputs(App *a, const char *typed, char (**out)[APP_NAME_LEN],
                         int *m) {
  char copy[512], *w;
  char (*names)[APP_NAME_LEN] = calloc((size_t)a->npars + 1, APP_NAME_LEN);
  int count = 0, i, k;
  if (!names) return false;
  snprintf(copy, sizeof copy, "%s", typed);
  for (w = strtok(copy, " ,;"); w; w = strtok(NULL, " ,;")) {
    for (i = 0; i < a->npars && SDL_strcasecmp(a->pars[i], w) != 0; i++)
      ;
    if (i == a->npars) {
      app_log(a, "error: no parameter '%s' to use as an input; type 'whos' "
                 "for the names", w);
      app_status(a, "Linearise: no parameter %s", w);
      free(names);
      return false;
    }
    for (k = 0; k < count && strcmp(names[k], a->pars[i]) != 0; k++)
      ;
    if (k == count) snprintf(names[count++], APP_NAME_LEN, "%s", a->pars[i]);
  }
  *out = names;
  *m = count;
  return true;
}

/* The poles to start pole placement from: the eigenvalues moved into the
   stable region (reflected into the left half-plane, or for a map
   inverted into the unit circle), in exact conjugate pairs. */
static void stable_start(LinearSystem *L) {
  int i, j;
  for (i = 0; i < L->n; i++) {
    double re = L->re[i], im = L->im[i];
    if (L->discrete) {
      double r = hypot(re, im), s = r > 1 ? 1 / (r * r) : r == 1 ? 0.5 : 1;
      re *= s;
      im *= s;
    } else if (re > 0) {
      re = -re;
    } else if (re == 0) {
      re = -1;
    }
    L->want_re[i] = isfinite(re) ? re : -1;
    L->want_im[i] = isfinite(im) ? im : 0;
  }
  for (i = 0; i < L->n; i++) {
    int best = -1;
    if (!(L->want_im[i] > 0)) continue;
    for (j = 0; j < L->n; j++)
      if (L->want_im[j] < 0 &&
          (best < 0 || fabs(L->want_re[j] - L->want_re[i]) + fabs(L->want_im[j] + L->want_im[i]) <
                           fabs(L->want_re[best] - L->want_re[i]) +
                               fabs(L->want_im[best] + L->want_im[i])))
        best = j;
    if (best >= 0) {
      L->want_re[best] = L->want_re[i];
      L->want_im[best] = -L->want_im[i];
    }
  }
}

/* lin = m.linearize(...): at the equilibrium is Python's default; the
   initial values are the script's own, written before the line. */
static void echo_linearize(App *a) {
  const LinearSystem *L = &a->an.lin;
  char args[4096], name[160];
  size_t k = 0;
  int i;
  args[0] = '\0';
  if (!L->at_eq) k += (size_t)snprintf(args, sizeof args, "at=m.initial_conditions");
  if (L->m > 0) {
    k += (size_t)snprintf(args + k, sizeof args - k, "%sinputs=[", k ? ", " : "");
    for (i = 0; i < L->m && k < sizeof args - 200; i++)
      k += (size_t)snprintf(args + k, sizeof args - k, "%s%s", i ? ", " : "",
                            echo_str(L->inputs[i], name, sizeof name));
    snprintf(args + k, sizeof args - k, "]");
  }
  echo_line(a, "lin = m.linearize(%s)", args);
  snprintf(a->echo.table, sizeof a->echo.table, "lin");
}

/* Model.linearize() in Python, with the same engine calls in the same
   order, so that the numbers are the same: the equilibrium (or the initial
   values), the right-hand side there, A and B, the eigenvalues, the rank. */
static bool linearize(App *a, bool outer) {
  AnalysisUI *an = &a->an;
  LinearSystem *L = &an->lin;
  xpp_engine_api *x = &a->eng->api;
  char (*inputs)[APP_NAME_LEN] = NULL;
  const char **names = NULL;
  double *f = NULL, *c = NULL, *block;
  int n = a->nvars, m = 0, i, rc;
  if (n < 1) return false;
  if (!parse_inputs(a, an->lin_inputs, &inputs, &m)) return false;
  free_linear(L);
  if (an->locus.kind == LOCUS_GAIN) free_locus(&an->locus); /* it was of the old A, B */
  app_free_trajectory(&an->res);
  an->have = false;
  an->summary[0] = '\0';
  block = malloc(sizeof(double) * (size_t)(n * (2 * n + 3 * m + 9) + 2 * m + 1));
  f = malloc(sizeof(double) * (size_t)n);
  names = malloc(sizeof(char *) * (size_t)(m + 1));
  c = malloc(sizeof(double) * (size_t)(n * n * m + 1));
  if (!block || !f || !names || !c) {
    free(block);
    free(inputs);
    rc = XPPE_ERR_RANGE;
    goto done;
  }
  L->n = n;
  L->m = m;
  L->inputs = inputs;
  L->at_eq = an->lin_at == 0;
  L->discrete = a->meth == 0;
  L->x = block;
  L->A = L->x + n;
  L->B = L->A + n * n;
  L->A_err = L->B + n * m;
  L->B_err = L->A_err + n * n;
  L->re = L->B_err + n * m;
  L->im = L->re + n;
  L->sv = L->im + n;
  L->u0 = L->sv + n;
  L->want_re = L->u0 + m;
  L->want_im = L->want_re + n;
  L->K = L->want_im + n;
  L->cl_re = L->K + n * m;
  L->cl_im = L->cl_re + n;
  L->Qw = L->cl_im + n;
  L->Rw = L->Qw + n;
  for (i = 0; i < n; i++) L->Qw[i] = 1;
  for (i = 0; i < m; i++) L->Rw[i] = 1;
  for (i = 0; i < m; i++) {
    names[i] = inputs[i];
    x->xppe_get(inputs[i], &L->u0[i]);
  }
  for (i = 0; i < n; i++) x->xppe_get(a->vars[i], &L->x[i]);
  if (L->at_eq && (rc = x->xppe_equilibrium(L->x, L->re, L->im)) != XPPE_OK) {
    app_log(a, "No equilibrium found from the initial values (%s). Start "
               "nearer one, or linearise at the initial values.",
            x->xppe_error());
    app_status(a, "Linearise: no equilibrium found");
    goto done;
  }
  if ((rc = x->xppe_rhs(0.0, L->x, f)) != XPPE_OK) goto failed;
  /* for a continuous model the equilibrium has f = 0; for a map x = F(x) */
  for (i = 0; i < n; i++) {
    double r = fabs(L->discrete ? f[i] - L->x[i] : f[i]);
    if (r > L->residual || isnan(r)) L->residual = r;
  }
  rc = x->xppe_linearize(L->x, m, names, 0.0, L->A, L->B, L->A_err, L->B_err);
  if (rc != XPPE_OK) goto failed;
  if (!L->at_eq && (rc = x->xppe_eigenvalues(n, L->A, L->re, L->im)) != XPPE_OK)
    goto failed;
  L->rank = 0;
  if (m > 0) {
    L->rank = x->xppe_controllability(n, m, L->A, L->B, 0.0, c, L->sv);
    if (L->rank < 0) {
      snprintf(L->rank_error, sizeof L->rank_error, "%s", x->xppe_error());
      L->rank = -1;
    }
  }
  for (i = 0; i < n * n + n * m; i++) {
    double e = i < n * n ? L->A_err[i] : L->B_err[i - n * n];
    if (e > L->worst) L->worst = e;
  }
  an->have = true;
  an->res_tool = AN_LINEARIZE;
  an->refit = true;
  stable_start(L);
  if (m > 0) app_place_poles(a);
  if (outer) echo_linearize(a);
  {
    char line[1024];
    int k = snprintf(line, sizeof line, "Linearised at %s:",
                     L->at_eq ? "the equilibrium" : "the initial values");
    for (i = 0; i < n && k < (int)sizeof line - 40; i++)
      k += snprintf(line + k, sizeof line - k, " %s=%.6g", a->vars[i], L->x[i]);
    app_log(a, "%s", line);
    k = snprintf(line, sizeof line, "  eigenvalues:");
    for (i = 0; i < n && k < (int)sizeof line - 40; i++)
      k += snprintf(line + k, sizeof line - k, " %.5g%+.5gi", L->re[i], L->im[i]);
    app_log(a, "%s", line);
    if (m > 0 && L->rank >= 0)
      app_log(a, "  controllability matrix: rank %d of %d (%s)", L->rank, n,
              L->rank == n ? "controllable" : "not controllable");
    else if (m > 0)
      app_log(a, "  the controllability rank could not be computed: %s",
              L->rank_error);
  }
  app_status(a, "Linearised at %s", L->at_eq ? "the equilibrium" : "the initial values");
  rc = XPPE_OK;
  goto done;
failed:
  app_log(a, "error: %s", x->xppe_error());
  app_status(a, "Linearise failed: %s", x->xppe_error());
done:
  if (rc != XPPE_OK) free_linear(L);
  free(f);
  free(names);
  free(c);
  return rc == XPPE_OK;
}

/* The columns xpplab's Linearization.to_csv() writes. */
bool app_export_linear_csv(App *a, const char *path) {
  const LinearSystem *L = &a->an.lin;
  FILE *f;
  char num[40];
  int i, j;
  if (!L->A) {
    app_log(a, "error: there is no linearisation to export");
    return false;
  }
  if (app_is_mat(path)) {
    app_log(a, "error: the linearisation has a column of names; export it as CSV");
    return false;
  }
  f = app_fopen_utf8(path, "w");
  if (!f) {
    app_log(a, "error: cannot write %s", path);
    return false;
  }
  fputs("state,at", f);
  for (j = 0; j < L->n; j++) fprintf(f, ",A_%s", a->vars[j]);
  for (j = 0; j < L->m; j++) fprintf(f, ",B_%s", L->inputs[j]);
  fputs(",eig_re,eig_im,rank,residual\n", f);
  for (i = 0; i < L->n; i++) {
    fputs(a->vars[i], f);
    app_format_double(num, sizeof num, L->x[i]);
    fprintf(f, ",%s", num);
    for (j = 0; j < L->n; j++) {
      app_format_double(num, sizeof num, L->A[i * L->n + j]);
      fprintf(f, ",%s", num);
    }
    for (j = 0; j < L->m; j++) {
      app_format_double(num, sizeof num, L->B[i * L->m + j]);
      fprintf(f, ",%s", num);
    }
    app_format_double(num, sizeof num, L->re[i]);
    fprintf(f, ",%s", num);
    app_format_double(num, sizeof num, L->im[i]);
    fprintf(f, ",%s", num);
    if (i == 0) {
      app_format_double(num, sizeof num, L->residual);
      fprintf(f, ",%d,%s\n", L->rank, num);
    } else {
      fputs(",,\n", f);
    }
  }
  fclose(f);
  app_log(a, "Exported the linearisation (%d states, %d inputs) to %s", L->n,
          L->m, path);
  return true;
}

/* ---- Pole placement -------------------------------------------------------- */

bool app_place_poles(App *a) {
  LinearSystem *L = &a->an.lin;
  xpp_engine_api *x;
  double *acl;
  int n = L->n, m = L->m, i, j, k;
  L->have_K = false;
  L->K_written = false;
  L->K_lqr = false;
  L->K_error[0] = '\0';
  if (!L->A || !L->want_re || m == 0 || !a->eng) return false;
  if (L->rank != n) {
    snprintf(L->K_error, sizeof L->K_error, "The inputs cannot steer every "
             "state, so not every pole can be placed.");
    return false;
  }
  /* -0 as 0, as the Python the echo writes reads it */
  for (i = 0; i < n; i++) {
    L->want_re[i] += 0.0;
    L->want_im[i] += 0.0;
  }
  x = &a->eng->api;
  if (x->xppe_place_poles(n, m, L->A, L->B, L->want_re, L->want_im, L->K) != XPPE_OK) {
    snprintf(L->K_error, sizeof L->K_error, "%s", x->xppe_error());
    return false;
  }
  acl = malloc(sizeof(double) * (size_t)(n * n));
  if (!acl) return false;
  for (i = 0; i < n; i++)
    for (j = 0; j < n; j++) {
      double s = L->A[i * n + j];
      for (k = 0; k < m; k++) s -= L->B[i * m + k] * L->K[k * n + j];
      acl[i * n + j] = s;
    }
  if (x->xppe_eigenvalues(n, acl, L->cl_re, L->cl_im) != XPPE_OK)
    for (i = 0; i < n; i++) L->cl_re[i] = L->cl_im[i] = NAN;
  free(acl);
  L->have_K = true;
  return true;
}

/* The poles as a Python list: [-2+1j, -2-1j, -3.5]. */
static void pole_text(const LinearSystem *L, int i, char *item, size_t len) {
  char re[64], im[64];
  echo_num(L->want_re[i], re, sizeof re);
  if (L->want_im[i] == 0)
    snprintf(item, len, "%s", re);
  else
    snprintf(item, len, "%s%s%sj", re, L->want_im[i] < 0 ? "-" : "+",
             echo_num(fabs(L->want_im[i]), im, sizeof im));
}

/* Numbers as a Python list for the echo, item(i) for i < n: in buf when
   the list fits on a line (buf of 4096 bytes or more); otherwise written
   as name = [...] over several lines, and buf gets name. */
static void echo_long_list(App *a, const char *name, int n,
                           void (*item)(const void *, int, char *, size_t),
                           const void *data,
                           char *buf, size_t len) {
  char text[160], line[200];
  size_t k = (size_t)snprintf(buf, len, "[");
  int i;
  for (i = 0; i < n; i++) {
    item(data, i, text, sizeof text);
    if (k + strlen(text) + 4 > 4000 || k + strlen(text) + 4 >= len) break;
    k += (size_t)snprintf(buf + k, len - k, "%s%s", i ? ", " : "", text);
  }
  if (i == n) {
    snprintf(buf + k, len - k, "]");
    return;
  }
  echo_line(a, "%s = [", name);
  k = 0;
  for (i = 0; i < n; i++) {
    item(data, i, text, sizeof text);
    if (k && k + strlen(text) + 2 > 72) {
      echo_line(a, "    %s,", line);
      k = 0;
    }
    k += (size_t)snprintf(line + k, sizeof line - k, "%s%s", k ? ", " : "", text);
  }
  if (k) echo_line(a, "    %s,", line);
  echo_line(a, "]");
  snprintf(buf, len, "%s", name);
}

static void pole_item(const void *L, int i, char *out, size_t len) {
  pole_text((const LinearSystem *)L, i, out, len);
}

/* The poles as a Python list: [-2+1j, -2-1j, -3.5], or poles = [...]
   written first when the list is long. */
static void echo_poles(App *a, const LinearSystem *L, char *buf, size_t len) {
  echo_long_list(a, "poles", L->n, pole_item, L, buf, len);
}

static void log_gain(App *a) {
  const LinearSystem *L = &a->an.lin;
  char line[2048];
  int i, j;
  for (i = 0; i < L->m; i++) {
    int k = snprintf(line, sizeof line, "  K[%s]:", L->inputs[i]);
    for (j = 0; j < L->n && k < (int)sizeof line - 40; j++)
      k += snprintf(line + k, sizeof line - k, " %s %.6g", a->vars[j], L->K[i * L->n + j]);
    app_log(a, "%s", line);
  }
}

/* A pole as written: -2, -2+1i, -2-1.5j, 3i. */
static bool read_pole(const char *w, double *re, double *im) {
  char *end;
  double v = strtod(w, &end);
  if (end == w) return false;
  if (*end == 'i' || *end == 'j') {
    *re = 0;
    *im = v;
    return end[1] == '\0';
  }
  *re = v;
  *im = 0;
  if (*end == '\0') return true;
  if (*end != '+' && *end != '-') return false;
  {
    const char *s = end;
    double b;
    if ((s[1] == 'i' || s[1] == 'j') && s[2] == '\0') {
      *im = *s == '-' ? -1 : 1;
      return true;
    }
    b = strtod(s, &end);
    if (end == s || (*end != 'i' && *end != 'j') || end[1] != '\0') return false;
    *im = b;
  }
  return true;
}

bool app_place_poles_text(App *a, const char *text) {
  LinearSystem *L = &a->an.lin;
  char copy[2048], *w, poles[4096];
  double *re, *im;
  int count = 0;
  bool outer, ok;
  if (!L->A || L->m == 0) {
    app_log(a, "error: linearise with inputs first, e.g. linearize inputs u");
    return false;
  }
  re = malloc(sizeof(double) * 2 * (size_t)L->n);
  if (!re) return false;
  im = re + L->n;
  snprintf(copy, sizeof copy, "%s", text);
  for (w = strtok(copy, " ,;"); w; w = strtok(NULL, " ,;")) {
    if (count == L->n) {
      app_log(a, "error: give %d poles, one per state", L->n);
      free(re);
      return false;
    }
    if (!read_pole(w, &re[count], &im[count])) {
      app_log(a, "error: cannot read the pole '%s'; write -2, -2+1i or -2-1i", w);
      free(re);
      return false;
    }
    count++;
  }
  if (count != L->n) {
    app_log(a, "error: give %d poles, one per state (got %d)", L->n, count);
    free(re);
    return false;
  }
  memcpy(L->want_re, re, sizeof(double) * (size_t)L->n);
  memcpy(L->want_im, im, sizeof(double) * (size_t)L->n);
  free(re);
  outer = echo_begin(a);
  ok = app_place_poles(a);
  if (ok) {
    app_log(a, "Gain for the poles:");
    log_gain(a);
    if (outer) {
      echo_poles(a, L, poles, sizeof poles);
      echo_line(a, "K = lin.place_poles(%s)", poles);
      L->K_written = true;
    }
  } else {
    app_log(a, "error: %s", L->K_error);
  }
  echo_end(a);
  return ok;
}

/* Numerical options in the order Python's Model.options lists them (and
   closed_loop() sets them). */
static const char *const option_list[] = {
    "TOTAL", "DT", "T0", "TRANS", "BOUND", "TOL", "ATOL", "DTMIN",
    "DTMAX", "NOUT", "MAXSTOR", "METH", "DELAY", "NEWT_TOL",
    "NEWT_ITER", "JAC_EPS", "POIMAP", "POIVAR", "POISGN", "POIPLN",
    "POISTOP", "NMESH"};
#define NOPTIONS ((int)(sizeof option_list / sizeof *option_list))
/* the letter XPPAUT reads for each method, by index (Python's METHODS) */
static const char method_letters[] = "demragvbqsc582y";

static bool is_input(const LinearSystem *L, const char *name) {
  int i;
  for (i = 0; i < L->m; i++)
    if (SDL_strcasecmp(L->inputs[i], name) == 0) return true;
  return false;
}

bool app_closed_loop(App *a, const char *path) {
  LinearSystem *L = &a->an.lin;
  xpp_engine_api *x;
  char out[1100], abs_path[2100], quoted[2300], poles[4096], load[2600];
  char *src = NULL, *text = NULL;
  const char **inames = NULL, **snames = NULL;
  char (*pnames)[APP_NAME_LEN] = NULL, (*vnames)[APP_NAME_LEN] = NULL;
  double *pv = NULL, *vv = NULL, ov[NOPTIONS];
  bool have_opt[NOPTIONS], outer, ok = false;
  int np = 0, nv = 0, i, j, len;
  FILE *f;
  if (!a->model_ok || !L->A) {
    app_log(a, "error: linearise with inputs and place the poles first");
    return false;
  }
  if (!L->have_K) {
    app_log(a, "error: no gain yet: %s", L->K_error[0] ? L->K_error
                                                       : "place the poles first");
    return false;
  }
  x = &a->eng->api;
  if (path && *path)
    snprintf(out, sizeof out, "%s", path);
  else if (!app_derived_path(a, "closed_loop", out, sizeof out)) {
    app_log(a, "error: no folder to write the closed loop to");
    return false;
  }
  src = SDL_LoadFile(a->model_path, NULL);
  inames = malloc(sizeof(char *) * (size_t)(L->m + L->n));
  pnames = calloc((size_t)a->npars + 1, APP_NAME_LEN);
  vnames = calloc((size_t)a->nvars + 1, APP_NAME_LEN);
  pv = malloc(sizeof(double) * (size_t)(a->npars + a->nvars + 1));
  if (!src || !inames || !pnames || !vnames || !pv) {
    app_log(a, "error: cannot read %s", a->model_path);
    goto done;
  }
  snames = inames + L->m;
  vv = pv + a->npars;
  for (i = 0; i < L->m; i++) inames[i] = L->inputs[i];
  for (i = 0; i < L->n; i++) snames[i] = a->vars[i];
  len = x->xppe_closed_loop(src, L->m, inames, L->u0, L->n, snames, L->x, L->K, NULL, 0);
  if (len < 0 || !(text = malloc((size_t)len + 1)) ||
      x->xppe_closed_loop(src, L->m, inames, L->u0, L->n, snames, L->x, L->K, text,
                          len + 1) != len) {
    app_log(a, "error: %s", len < 0 ? x->xppe_error() : "out of memory");
    goto done;
  }
  f = app_fopen_utf8(out, "wb");
  if (!f || fwrite(text, 1, (size_t)len, f) != (size_t)len) {
    if (f) fclose(f);
    app_log(a, "error: cannot write %s", out);
    goto done;
  }
  fclose(f);
  /* what the closed loop takes over, read before the model goes */
  for (i = 0; i < a->npars; i++)
    if (!is_input(L, a->pars[i]) && x->xppe_get(a->pars[i], &pv[np]) == XPPE_OK)
      snprintf(pnames[np++], APP_NAME_LEN, "%s", a->pars[i]);
  for (i = 0; i < a->nvars; i++)
    if (x->xppe_get(a->vars[i], &vv[nv]) == XPPE_OK)
      snprintf(vnames[nv++], APP_NAME_LEN, "%s", a->vars[i]);
  for (i = 0; i < NOPTIONS; i++) have_opt[i] = x->xppe_get(option_list[i], &ov[i]) == XPPE_OK;
  outer = echo_begin(a);
  echo_abs_path(out, abs_path, sizeof abs_path);
  snprintf(load, sizeof load, "m = lin.closed_loop(K, path=%s)",
           echo_str(abs_path, quoted, sizeof quoted));
  if (outer && !L->K_written) {
    echo_poles(a, L, poles, sizeof poles);
    echo_line(a, "K = lin.place_poles(%s)", poles);
  }
  if (!app_open_quiet(a, out, outer ? load : NULL)) {
    echo_end(a);
    goto done;
  }
  /* as closed_loop() does: parameters, initial values, numerical options */
  x = &a->eng->api;
  for (i = 0; i < np; i++)
    for (j = 0; j < a->npars; j++)
      if (strcmp(a->pars[j], pnames[i]) == 0) x->xppe_set_number(pnames[i], pv[i]);
  for (i = 0; i < nv; i++)
    for (j = 0; j < a->nvars; j++)
      if (strcmp(a->vars[j], vnames[i]) == 0) x->xppe_set_number(vnames[i], vv[i]);
  for (i = 0; i < NOPTIONS; i++) {
    double now;
    if (!have_opt[i] || x->xppe_get(option_list[i], &now) != XPPE_OK) continue;
    if (strcmp(option_list[i], "METH") == 0 && ov[i] >= 0 &&
        ov[i] < (double)(sizeof method_letters - 1)) {
      char letter[2] = {method_letters[(int)ov[i]], '\0'};
      x->xppe_set(option_list[i], letter);
    } else {
      x->xppe_set_number(option_list[i], ov[i]);
    }
  }
  app_refresh_values(a);
  app_log(a, "Wrote the closed loop to %s", out);
  if (app_run(a) && outer) echo_line(a, "sol = m.run()");
  echo_end(a);
  ok = true;
done:
  SDL_free(src);
  free(text);
  free(inames);
  free(pnames);
  free(vnames);
  free(pv);
  return ok;
}

/* ---- Optimal control (LQR) ------------------------------------------------- */

static void weight_item(const void *v, int i, char *out, size_t len) {
  echo_num(((const double *)v)[i], out, len);
}

/* A list of weights as Python: [10.0, 1.0], or name = [...] written first
   when it is long. */
static void echo_weights(App *a, const char *name, const double *v, int n, char *buf,
                         size_t len) {
  echo_long_list(a, name, n, weight_item, v, buf, len);
}

bool app_lqr(App *a) {
  LinearSystem *L = &a->an.lin;
  xpp_engine_api *x;
  double *Q, *R;
  int n = L->n, m = L->m, i, j;
  bool outer;
  if (!a->model_ok || !L->A || m == 0) {
    app_log(a, "error: linearise with inputs first, e.g. linearize inputs u");
    return false;
  }
  x = &a->eng->api;
  Q = calloc((size_t)(n * n + m * m) + 1, sizeof(double));
  if (!Q) return false;
  R = Q + n * n;
  for (i = 0; i < n; i++) Q[i * n + i] = L->Qw[i];
  for (i = 0; i < m; i++) R[i * m + i] = L->Rw[i];
  L->K_error[0] = '\0';
  if (x->xppe_lqr(n, m, L->A, L->B, Q, R, L->discrete, L->K, NULL) != XPPE_OK) {
    snprintf(L->K_error, sizeof L->K_error, "%s", x->xppe_error());
    app_log(a, "error: %s", L->K_error);
    L->have_K = false;
    free(Q);
    return false;
  }
  free(Q);
  /* the poles of A - B K, in exact conjugate pairs, become the poles shown */
  {
    double *acl = malloc(sizeof(double) * (size_t)(n * n) + 1);
    int k;
    if (!acl) return false;
    for (i = 0; i < n; i++)
      for (j = 0; j < n; j++) {
        double s = L->A[i * n + j];
        for (k = 0; k < m; k++) s -= L->B[i * m + k] * L->K[k * n + j];
        acl[i * n + j] = s;
      }
    if (x->xppe_eigenvalues(n, acl, L->cl_re, L->cl_im) != XPPE_OK)
      for (i = 0; i < n; i++) L->cl_re[i] = L->cl_im[i] = NAN;
    free(acl);
  }
  for (i = 0; i < n; i++) {
    L->want_re[i] = L->cl_re[i];
    L->want_im[i] = L->cl_im[i];
  }
  for (i = 0; i < n; i++)
    if (L->want_im[i] > 0)
      for (j = 0; j < n; j++)
        if (L->want_im[j] < 0 && fabs(L->want_re[j] - L->want_re[i]) <= 1e-9 * (1 + fabs(L->want_re[i])) &&
            fabs(L->want_im[j] + L->want_im[i]) <= 1e-9 * (1 + fabs(L->want_im[i]))) {
          L->want_re[j] = L->want_re[i];
          L->want_im[j] = -L->want_im[i];
          break;
        }
  L->have_K = true;
  L->K_lqr = true;
  L->K_written = false;
  outer = echo_begin(a);
  if (outer) {
    char q[4096], r[4096];
    echo_weights(a, "Q", L->Qw, n, q, sizeof q);
    echo_weights(a, "R", L->Rw, m, r, sizeof r);
    echo_line(a, "K = lin.lqr(Q=%s, R=%s).K", q, r);
    L->K_written = true;
  }
  echo_end(a);
  app_log(a, "LQR gain:");
  log_gain(a);
  {
    char line[1024];
    int k = snprintf(line, sizeof line, "  closed-loop poles:");
    for (i = 0; i < n && k < (int)sizeof line - 40; i++)
      k += snprintf(line + k, sizeof line - k, " %.5g%+.5gi", L->cl_re[i], L->cl_im[i]);
    app_log(a, "%s", line);
  }
  return true;
}

bool app_lqr_text(App *a, const char *text) {
  LinearSystem *L = &a->an.lin;
  char copy[2048], *w;
  double *target = NULL;
  int count = 0, limit = 0;
  if (!L->A || L->m == 0) {
    app_log(a, "error: linearise with inputs first, e.g. linearize inputs u");
    return false;
  }
  snprintf(copy, sizeof copy, "%s", text);
  for (w = strtok(copy, " ,;"); w; w = strtok(NULL, " ,;")) {
    char *end;
    double v;
    if (!strcmp(w, "Q") || !strcmp(w, "q")) {
      target = L->Qw;
      limit = L->n;
      count = 0;
      continue;
    }
    if (!strcmp(w, "R") || !strcmp(w, "r")) {
      target = L->Rw;
      limit = L->m;
      count = 0;
      continue;
    }
    v = strtod(w, &end);
    if (!target || *end || end == w || count >= limit) {
      app_log(a, "Write: lqr [Q q1 q2 ...] [R r1 ...], one weight per state "
                 "and per input (%d and %d), e.g. lqr Q 10 1 R 0.1", L->n, L->m);
      return false;
    }
    target[count++] = v;
    /* one number weighs them all alike */
    if (count == 1) {
      int i;
      for (i = 1; i < limit; i++) target[i] = v;
    }
  }
  return app_lqr(a);
}

/* ---- Root locus ------------------------------------------------------------ */

/* Where a branch crosses the edge of stability, as RootLocus.crossings()
   finds it: linearly between the two steps around the crossing. */
static void find_crossings(RootLocusUI *R) {
  size_t k = 0;
  int b, s, count = 0;
  R->crossings[0] = '\0';
  for (b = 0; b < R->n; b++)
    for (s = 0; s + 1 < R->steps; s++) {
      double r0 = R->re[b * R->steps + s], r1 = R->re[b * R->steps + s + 1];
      double i0 = R->im[b * R->steps + s], i1 = R->im[b * R->steps + s + 1];
      double d0 = R->discrete ? hypot(r0, i0) - 1 : r0;
      double d1 = R->discrete ? hypot(r1, i1) - 1 : r1;
      double f, v, zr, zi;
      if (!isfinite(d0) || !isfinite(d1) || d0 == d1 || (d0 < 0) == (d1 < 0)) continue;
      f = -d0 / (d1 - d0);
      v = R->values[s] + f * (R->values[s + 1] - R->values[s]);
      zr = r0 + f * (r1 - r0);
      zi = i0 + f * (i1 - i0);
      if (zi < 0) continue; /* the pair is told once, by its upper pole */
      if (count++ < 6 && k < sizeof R->crossings - 160) {
        if (zi > 0)
          k += (size_t)snprintf(R->crossings + k, sizeof R->crossings - k,
                                "%s%s = %.6g: a pair of poles becomes %s, at "
                                "%.4g +/- %.4gi",
                                k ? "\n" : "", R->name, v,
                                d1 > 0 ? "unstable" : "stable", zr, zi);
        else
          k += (size_t)snprintf(R->crossings + k, sizeof R->crossings - k,
                                "%s%s = %.6g: a pole becomes %s, at %.4g",
                                k ? "\n" : "", R->name, v,
                                d1 > 0 ? "unstable" : "stable", zr);
      }
    }
  if (count > 6 && k < sizeof R->crossings - 40)
    snprintf(R->crossings + k, sizeof R->crossings - k, "\n(and %d more)", count - 6);
}

static bool locus_alloc(RootLocusUI *R, int steps, int n, int nx) {
  free_locus(R);
  R->values = malloc(sizeof(double) * ((size_t)steps * (size_t)(2 * n + nx + 1) + 1));
  if (!R->values) return false;
  R->re = R->values + steps;
  R->im = R->re + (size_t)steps * n;
  R->x = R->im + (size_t)steps * n;
  R->steps = steps;
  R->n = n;
  R->nx = nx;
  return true;
}

static void log_locus(App *a) {
  const RootLocusUI *R = &a->an.locus;
  app_log(a, "Root locus over %s from %g to %g (%d steps)%s", R->name, R->values[0],
          R->values[R->steps - 1], R->steps,
          R->crossings[0] ? ":" : ": no pole crosses the edge of stability");
  if (R->crossings[0]) {
    char copy[sizeof R->crossings], *line;
    snprintf(copy, sizeof copy, "%s", R->crossings);
    for (line = strtok(copy, "\n"); line; line = strtok(NULL, "\n"))
      app_log(a, "  %s", line);
  }
}

bool app_gain_locus(App *a, int mode, int input, int state, double lo, double hi,
                    int steps) {
  LinearSystem *L = &a->an.lin;
  RootLocusUI *R = &a->an.locus;
  xpp_engine_api *x;
  double *K, *g, *re, *im;
  int n = L->n, m = L->m, s, b;
  bool outer;
  if (!a->model_ok || !L->A || m == 0) {
    app_log(a, "error: linearise with inputs first, e.g. linearize inputs u");
    return false;
  }
  if (mode == 0 && !L->have_K) {
    app_log(a, "error: no gain K yet: place the poles first (poles ...)");
    return false;
  }
  if (mode != 0 && (input < 0 || input >= m || state < 0 || state >= n)) {
    app_log(a, "error: choose an input and a state for the feedback");
    return false;
  }
  if (!isfinite(lo) || !isfinite(hi)) {
    app_log(a, "error: the gains must be finite numbers");
    return false;
  }
  if (steps < 2 || steps > 100000) steps = 201;
  x = &a->eng->api;
  K = calloc((size_t)(m * n) + (size_t)steps * (size_t)(2 * n + 1) + 1, sizeof(double));
  if (!K) return false;
  g = K + m * n;
  re = g + steps;
  im = re + (size_t)steps * n;
  if (mode == 0)
    memcpy(K, L->K, sizeof(double) * (size_t)(m * n));
  else
    K[input * n + state] = 1;
  if (x->xppe_gain_locus(n, m, L->A, L->B, K, lo, hi, steps, g, re, im) != XPPE_OK) {
    app_log(a, "error: %s", x->xppe_error());
    free(K);
    return false;
  }
  if (!locus_alloc(R, steps, n, 0)) {
    free(K);
    return false;
  }
  R->kind = LOCUS_GAIN;
  R->discrete = L->discrete;
  snprintf(R->name, sizeof R->name, "gain");
  memcpy(R->values, g, sizeof(double) * (size_t)steps);
  for (s = 0; s < steps; s++)
    for (b = 0; b < n; b++) {
      R->re[(size_t)b * steps + s] = re[(size_t)s * n + b];
      R->im[(size_t)b * steps + s] = im[(size_t)s * n + b];
    }
  find_crossings(R);
  outer = echo_begin(a);
  if (outer) {
    char klist[4096], num[64], lo_s[64], hi_s[64], poles[4096];
    if (mode == 0) {
      if (!L->K_written) {
        echo_poles(a, L, poles, sizeof poles);
        echo_line(a, "K = lin.place_poles(%s)", poles);
        L->K_written = true;
      }
      snprintf(klist, sizeof klist, "K");
    } else if (m * n <= 64) {
      size_t k = (size_t)snprintf(klist, sizeof klist, "[");
      int i, j;
      for (i = 0; i < m; i++) {
        k += (size_t)snprintf(klist + k, sizeof klist - k, "%s[", i ? ", " : "");
        for (j = 0; j < n; j++)
          k += (size_t)snprintf(klist + k, sizeof klist - k, "%s%s", j ? ", " : "",
                                echo_num(K[i * n + j], num, sizeof num));
        k += (size_t)snprintf(klist + k, sizeof klist - k, "]");
      }
      snprintf(klist + k, sizeof klist - k, "]");
    } else {
      /* too long to list: zeros and the one 1 */
      echo_line(a, "Kfb = [[0.0] * %d for _ in range(%d)]", n, m);
      echo_line(a, "Kfb[%d][%d] = 1.0", input, state);
      snprintf(klist, sizeof klist, "Kfb");
    }
    echo_line(a, "rl = lin.root_locus(%s, gain=(%s, %s), steps=%d)", klist,
              echo_num(lo, lo_s, sizeof lo_s), echo_num(hi, hi_s, sizeof hi_s), steps);
  }
  echo_end(a);
  free(K);
  a->an.refit = true;
  log_locus(a);
  return true;
}

bool app_parameter_locus(App *a, int par, double lo, double hi, int steps) {
  RootLocusUI *R = &a->an.locus;
  xpp_engine_api *x;
  double *guess, *col;
  int n = a->nvars, s, b, j;
  bool outer, ok = false;
  if (!a->model_ok || par < 0 || par >= a->npars) {
    app_log(a, "error: choose a parameter");
    return false;
  }
  if (!isfinite(lo) || !isfinite(hi)) {
    app_log(a, "error: the ends must be finite numbers");
    return false;
  }
  if (steps < 2 || steps > 100000) steps = 101;
  x = &a->eng->api;
  guess = malloc(sizeof(double) * (size_t)(n + steps + 1));
  if (!guess) return false;
  col = guess + n;
  for (j = 0; j < n; j++) x->xppe_get(a->vars[j], &guess[j]);
  outer = echo_begin(a);
  if (x->xppe_parameter_locus(a->pars[par], lo, hi, steps, guess) != XPPE_OK) {
    app_log(a, "error: %s", x->xppe_error());
    goto done;
  }
  /* the ODE variables only: Markov variables have no equilibrium equation,
     so the table holds the parameter, NODE states and NODE eigenvalues */
  n = (x->xppe_result_count() - 1) / 3;
  if (x->xppe_result_rows() != steps || n < 1 || n > a->nvars ||
      !locus_alloc(R, steps, n, n))
    goto done;
  R->kind = LOCUS_PARAMETER;
  R->discrete = a->meth == 0;
  snprintf(R->name, sizeof R->name, "%s", a->pars[par]);
  if (x->xppe_result_column(0, R->values, steps) != steps) goto failed;
  for (j = 0; j < n; j++) {
    if (x->xppe_result_column(1 + j, col, steps) != steps) goto failed;
    for (s = 0; s < steps; s++) R->x[(size_t)s * n + j] = col[s];
  }
  for (b = 0; b < n; b++)
    if (x->xppe_result_column(1 + n + 2 * b, R->re + (size_t)b * steps, steps) != steps ||
        x->xppe_result_column(2 + n + 2 * b, R->im + (size_t)b * steps, steps) != steps)
      goto failed;
  find_crossings(R);
  if (outer) {
    char name[160], lo_s[64], hi_s[64];
    echo_line(a, "rl = m.root_locus(%s, (%s, %s), steps=%d)",
              echo_str(a->pars[par], name, sizeof name), echo_num(lo, lo_s, sizeof lo_s),
              echo_num(hi, hi_s, sizeof hi_s), steps);
  }
  a->an.refit = true;
  log_locus(a);
  ok = true;
  goto done;
failed:
  app_log(a, "error: the root locus table is not complete");
  free_locus(R);
done:
  echo_end(a);
  free(guess);
  return ok;
}

bool app_write_locus_csv(App *a, const char *path) {
  const RootLocusUI *R = &a->an.locus;
  char num[40];
  FILE *f;
  int s, b, j;
  if (R->kind == LOCUS_NONE) {
    app_log(a, "error: there is no root locus to export");
    return false;
  }
  if (app_is_mat(path)) {
    /* the value, the equilibrium, then re1, im1, re2, ... */
    int nc = 1 + R->nx + 2 * R->n, steps = R->steps;
    const char **nm = calloc((size_t)nc, sizeof *nm);
    const double **col = calloc((size_t)nc, sizeof *col);
    char (*lab)[16] = calloc((size_t)(2 * R->n + 1), sizeof *lab);
    double *buf = calloc((size_t)nc * (size_t)(steps > 0 ? steps : 1), sizeof *buf);
    bool ok = nm && col && lab && buf;
    for (j = 0; ok && j < nc; j++) col[j] = buf + (size_t)j * steps;
    if (ok) nm[0] = R->name;
    for (j = 0; ok && j < R->nx; j++) nm[1 + j] = a->vars[j];
    for (b = 0; ok && b < R->n; b++) {
      snprintf(lab[2 * b], sizeof lab[0], "re%d", b + 1);
      snprintf(lab[2 * b + 1], sizeof lab[0], "im%d", b + 1);
      nm[1 + R->nx + 2 * b] = lab[2 * b];
      nm[2 + R->nx + 2 * b] = lab[2 * b + 1];
    }
    for (s = 0; ok && s < steps; s++) {
      buf[s] = R->values[s];
      for (j = 0; j < R->nx; j++) buf[(size_t)(1 + j) * steps + s] = R->x[(size_t)s * R->nx + j];
      for (b = 0; b < R->n; b++) {
        buf[(size_t)(1 + R->nx + 2 * b) * steps + s] = R->re[(size_t)b * steps + s];
        buf[(size_t)(2 + R->nx + 2 * b) * steps + s] = R->im[(size_t)b * steps + s];
      }
    }
    ok = ok && app_write_mat(a, path, nc, nm, (const double *const *)col, steps, 0, NULL, NULL);
    free(nm);
    free(col);
    free(lab);
    free(buf);
    if (!ok) return false;
    app_log(a, "Exported the root locus (%d steps) to %s", steps, path);
    return true;
  }
  f = app_fopen_utf8(path, "w");
  if (!f) {
    app_log(a, "error: cannot write %s", path);
    return false;
  }
  fputs(R->name, f);
  for (j = 0; j < R->nx; j++) fprintf(f, ",%s", a->vars[j]);
  for (b = 0; b < R->n; b++) fprintf(f, ",re%d,im%d", b + 1, b + 1);
  fputc('\n', f);
  for (s = 0; s < R->steps; s++) {
    app_format_double(num, sizeof num, R->values[s]);
    fputs(num, f);
    for (j = 0; j < R->nx; j++) {
      app_format_double(num, sizeof num, R->x[(size_t)s * R->nx + j]);
      fprintf(f, ",%s", num);
    }
    for (b = 0; b < R->n; b++) {
      app_format_double(num, sizeof num, R->re[(size_t)b * R->steps + s]);
      fprintf(f, ",%s", num);
      app_format_double(num, sizeof num, R->im[(size_t)b * R->steps + s]);
      fprintf(f, ",%s", num);
    }
    fputc('\n', f);
  }
  fclose(f);
  app_log(a, "Exported the root locus (%d steps) to %s", R->steps, path);
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
  if (n->tool == AN_LINEARIZE) return linearize(a, outer);
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
      int lr = x->xppe_lyapunov_integrate(0, &lyap);
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
