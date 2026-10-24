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
    "Fit to data"};

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
}

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
  for (k = nc - used; k < nc; k++)
    dev += fabs(tc[k] - tc[k - 1] - mean);
  if (dev / used > 0.01 * mean) return 0;
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

bool app_analysis_run(App *a) {
  xpp_engine_api *x;
  AnalysisUI *n = &a->an;
  const char *c1, *c2;
  int rc = XPPE_OK;
  if (!a->model_ok) return false;
  if (n->col < 0 || n->col >= a->ncols) n->col = 0;
  if (n->col2 < 0 || n->col2 >= a->ncols) n->col2 = n->col;
  x = &a->eng->api;
  if (!a->have_traj && n->tool != AN_FIT && n->tool != AN_PRC &&
      n->tool != AN_HFUNCTION && !app_run(a))
    return false;
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
