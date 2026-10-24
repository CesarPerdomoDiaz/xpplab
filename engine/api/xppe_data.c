/*
 * Data analysis for the engine API (see xppe.h): XPPAUT's Numerics >
 * stocHastic tools (Fourier, power, spectral density, histograms,
 * correlations, mean, Liapunov exponent, curve fitting) and Numerics >
 * Averaging (adjoint and H function), applied to the current run.
 *
 * Each tool runs XPPAUT's own routine. XPPAUT then shows the result in its
 * data browser in place of the run; here the result is copied into a
 * result table and the browser is pointed back at the run, so the run
 * itself is never changed.
 */
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "xppe.h"
#include "xppe_internal.h"

#include "xpplim.h"
#include "do_fit.h"

extern float **storage;
extern int storind, MAXSTOR;
extern int NEQ, NODE;
extern double NEWT_ERR;
extern double constants[];
extern double last_ic[MAXODE];
int plintf(char *fmt, ...);

/* histogram.c */
extern float *my_hist[MAXODE + 1], *my_four[MAXODE + 1];
extern int hist_len, four_len, HIST_HERE, FOUR_HERE;
extern int spec_col, spec_col2, spec_wid, spec_win, spec_type;
void new_four(int nmodes, int col);
void just_sd(int flag);
void new_hist(int nbins, double zlo, double zhi, int col, int col2,
              char *condition, int which);
int two_d_hist(int col1, int col2, int ndat, int n1, int n2, double xlo,
               double xhi, double ylo, double yhi);

/* adj2.c */
extern float **my_adj, **my_h;
extern int adj_len, h_len, ADJ_HERE, H_HERE, HODD_EV, ADJ_MAXIT;
extern double ADJ_ERR;
extern char *coup_string[MAXODE];
void data_back(void);
void new_adjoint(void);
void new_h_fun(int silent);
int hrw_liapunov(double *liap, int batch, double eps);

/* do_fit.c (its functions are declared in do_fit.h), browse.c */
extern FITINFO fin;
void find_variable(char *s, int *col);
float *get_data_col(int c);

/* ---- The result table ------------------------------------------------ */

#define MAX_RESULT_COLS (MAXODE + 1) /* an adjoint has one per variable */

static double *res[MAX_RESULT_COLS];
static char res_names[MAX_RESULT_COLS][32];
static int res_ncols, res_nrows;

static void result_clear(void) {
  int i;
  for (i = 0; i < res_ncols; i++) free(res[i]);
  res_ncols = res_nrows = 0;
}

/* Start a table of nrows rows with the given column names. */
static int result_begin(int nrows, int ncols, const char *const *names) {
  int i;
  result_clear();
  if (nrows < 0) nrows = 0;
  for (i = 0; i < ncols && i < MAX_RESULT_COLS; i++) {
    res[i] = calloc((size_t)nrows + 1, sizeof(double));
    if (!res[i]) {
      res_ncols = i;
      result_clear();
      xppe_set_error("out of memory", NULL);
      return XPPE_ERR_RANGE;
    }
    snprintf(res_names[i], sizeof res_names[i], "%s", names[i]);
  }
  res_ncols = i;
  res_nrows = nrows;
  return XPPE_OK;
}

static void result_fill(int col, const float *src) {
  int i;
  for (i = 0; i < res_nrows; i++) res[col][i] = (double)src[i];
}

int xppe_result_rows(void) { return res_nrows; }
int xppe_result_count(void) { return res_ncols; }

const char *xppe_result_name(int col) {
  return col >= 0 && col < res_ncols ? res_names[col] : "";
}

int xppe_result_column(int col, double *out, int n) {
  int i;
  if (col < 0 || col >= res_ncols) {
    xppe_set_error("no such result column", NULL);
    return XPPE_ERR_RANGE;
  }
  if (n > res_nrows) n = res_nrows;
  for (i = 0; i < n; i++) out[i] = res[col][i];
  return n;
}

/* ---- Helpers ----------------------------------------------------------- */

/* The current run must exist: at least two stored points. */
static int require_run(void) {
  int rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (storind < 2) {
    xppe_set_error("integrate the model first (the analysis uses the "
                   "current run)", NULL);
    return XPPE_ERR_STATE;
  }
  return XPPE_OK;
}

/* A data-browser column by name: "T", a variable or an auxiliary. */
static int column_index(const char *name, int *col) {
  char buf[64];
  if (!name || !name[0] || strlen(name) >= sizeof buf) {
    xppe_set_error("no such column: %s", name ? name : "");
    return XPPE_ERR_NAME;
  }
  strcpy(buf, name);
  find_variable(buf, col);
  if (*col < 0 || *col > NEQ) {
    xppe_set_error("no such column: %s", name);
    return XPPE_ERR_NAME;
  }
  return XPPE_OK;
}

/* Free XPPAUT's result arrays after copying them, and show the run again
   (XPPAUT's data_back; it does not free them itself). */
static void free_four(void) {
  if (FOUR_HERE) {
    free(my_four[0]);
    free(my_four[1]);
    free(my_four[2]);
    FOUR_HERE = 0;
  }
  data_back();
}

static void free_hist(void) {
  if (HIST_HERE) {
    free(my_hist[0]);
    free(my_hist[1]);
    if (HIST_HERE == 2) free(my_hist[2]);
    HIST_HERE = 0;
  }
  data_back();
}

/* ---- Fourier series and spectra ------------------------------------------ */

/* Mirrors compute_fourier() (flag 0) and just_fourier(1)/compute_power()
   (flag 1). */
static int fourier(const char *column, int power) {
  static const char *cs[] = {"freq", "cos", "sin"};
  static const char *ap[] = {"freq", "amplitude", "phase"};
  int rc, col, i;
  if ((rc = require_run()) != XPPE_OK) return rc;
  if ((rc = column_index(column, &col)) != XPPE_OK) return rc;
  if (NEQ < 2) {
    xppe_set_error("Need at least three data columns", NULL);
    return XPPE_ERR_STATE;
  }
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  new_four(storind / 2 - 1, col);
  XPPE_GUARD_END;
  if (!FOUR_HERE) {
    xppe_end_call();
    xppe_set_error("could not compute the Fourier series", NULL);
    return XPPE_ERR_NUMERIC;
  }
  if (power) {
    float *datx = get_data_col(1), *daty = get_data_col(2), ptot = 0;
    for (i = 0; i < four_len; i++) {
      double c = datx[i], s = daty[i];
      datx[i] = sqrt(s * s + c * c);
      daty[i] = atan2(s, c);
      ptot += (datx[i] * datx[i]);
    }
    plintf("a0=%g L2norm= %g  \n", datx[0], sqrt(ptot));
  }
  rc = result_begin(four_len, 3, power ? ap : cs);
  if (rc == XPPE_OK)
    for (i = 0; i < 3; i++) result_fill(i, my_four[i]);
  free_four();
  xppe_end_call();
  return rc;
}

int xppe_fourier(const char *column) { return fourier(column, 0); }
int xppe_power(const char *column) { return fourier(column, 1); }

/* Mirrors compute_sd()/just_sd(). */
int xppe_spectrum(int type, const char *column, const char *column2,
                  int window_length, int window) {
  static const char *names[3][2] = {
      {"freq", "power"}, {"freq", "cross"}, {"freq", "coherence"}};
  int rc, col, col2 = 1;
  if ((rc = require_run()) != XPPE_OK) return rc;
  if (type < 0 || type > 2 || window < 0 || window > 4) {
    xppe_set_error("type must be 0-2 and window 0-4", NULL);
    return XPPE_ERR_RANGE;
  }
  if ((rc = column_index(column, &col)) != XPPE_OK) return rc;
  if (type > 0 && (rc = column_index(column2, &col2)) != XPPE_OK) return rc;
  if (window_length < 2 ||
      (storind - window_length + 1) / (window_length / 2) < 1) {
    xppe_set_error("the window must be at least 2 points and shorter than "
                   "the run", NULL);
    return XPPE_ERR_RANGE;
  }
  spec_col = col;
  spec_col2 = col2;
  spec_wid = window_length;
  spec_win = window;
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  just_sd(type);
  XPPE_GUARD_END;
  rc = result_begin(HIST_HERE ? hist_len : 0, 2, names[type]);
  if (rc == XPPE_OK && HIST_HERE) {
    result_fill(0, my_hist[0]);
    result_fill(1, my_hist[1]);
  }
  free_hist();
  xppe_end_call();
  return rc;
}

/* ---- Histograms and correlations ----------------------------------------- */

/* Runs new_hist() (which 0: histogram, 1: stationary autocorrelation by
   differences, 2/3: cross-correlation directly or by FFT). */
static int histogram(int which, int nbins, double lo, double hi, int col,
                     int col2, const char *condition, const char *const *names) {
  char cond[256];
  int rc;
  snprintf(cond, sizeof cond, "%s", condition ? condition : "");
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  new_hist(nbins, lo, hi, col, col2, cond, which);
  XPPE_GUARD_END;
  rc = result_begin(HIST_HERE ? hist_len : 0, 2, names);
  if (rc == XPPE_OK && HIST_HERE) {
    result_fill(0, my_hist[0]);
    result_fill(1, my_hist[1]);
  }
  free_hist();
  xppe_end_call();
  return rc;
}

/* Mirrors compute_hist(). */
int xppe_histogram(const char *column, int nbins, double lo, double hi,
                   const char *condition) {
  static const char *names[] = {"bin", "count"};
  int rc, col;
  if ((rc = require_run()) != XPPE_OK) return rc;
  if ((rc = column_index(column, &col)) != XPPE_OK) return rc;
  if (nbins < 1 || !(hi > lo)) {
    xppe_set_error("need at least one bin and hi > lo", NULL);
    return XPPE_ERR_RANGE;
  }
  return histogram(0, nbins, lo, hi, col, 0, condition, names);
}

/* Mirrors compute_stacor(). */
int xppe_autocorrelation(const char *column, int nbins, double lo,
                         double hi) {
  static const char *names[] = {"difference", "count"};
  int rc, col;
  if ((rc = require_run()) != XPPE_OK) return rc;
  if ((rc = column_index(column, &col)) != XPPE_OK) return rc;
  if (nbins < 1 || !(hi > lo)) {
    xppe_set_error("need at least one bin and hi > lo", NULL);
    return XPPE_ERR_RANGE;
  }
  return histogram(1, nbins, lo, hi, col, 0, "", names);
}

/* Mirrors compute_correl(), including its choice of an odd number of lags
   and of the lag axis. */
int xppe_correlation(const char *column, const char *column2, int nbins,
                     int use_fft) {
  static const char *names[] = {"lag", "correlation"};
  int rc, col, col2, lag;
  float total, dta;
  if ((rc = require_run()) != XPPE_OK) return rc;
  if ((rc = column_index(column, &col)) != XPPE_OK) return rc;
  if ((rc = column_index(column2, &col2)) != XPPE_OK) return rc;
  total = storage[0][storind - 1] - storage[0][0];
  dta = total / (float)(storind - 1);
  if (nbins > (storind / 2 - 1)) nbins = storind / 2 - 2;
  nbins = 2 * (nbins / 2) + 1;
  if (nbins < 1) {
    xppe_set_error("the run is too short for a correlation", NULL);
    return XPPE_ERR_RANGE;
  }
  lag = nbins / 2;
  return histogram(use_fft ? 3 : 2, nbins, -lag * dta, lag * dta, col, col2,
                   "", names);
}

/* Mirrors twod_hist(). */
int xppe_histogram2d(const char *x, int nx, double xlo, double xhi,
                     const char *y, int ny, double ylo, double yhi) {
  static const char *names[] = {"x", "y", "count"};
  int rc, col, col2, length, i;
  if ((rc = require_run()) != XPPE_OK) return rc;
  if (NEQ < 2 || storind < 3) {
    xppe_set_error("Need more data and at least 3 columns", NULL);
    return XPPE_ERR_STATE;
  }
  if ((rc = column_index(x, &col)) != XPPE_OK) return rc;
  if ((rc = column_index(y, &col2)) != XPPE_OK) return rc;
  if (nx < 2 || ny < 2 || !(xhi > xlo) || !(yhi > ylo)) {
    xppe_set_error("need at least 2 bins each way and hi > lo", NULL);
    return XPPE_ERR_RANGE;
  }
  length = nx * ny;
  if (length >= MAXSTOR) length = MAXSTOR - 1;
  xppe_begin_call();
  free_hist();
  my_hist[0] = malloc(sizeof(float) * (size_t)length);
  my_hist[1] = malloc(sizeof(float) * (size_t)length);
  my_hist[2] = malloc(sizeof(float) * (size_t)length);
  if (!my_hist[0] || !my_hist[1] || !my_hist[2]) {
    free(my_hist[0]);
    free(my_hist[1]);
    free(my_hist[2]);
    xppe_end_call();
    xppe_set_error("out of memory", NULL);
    return XPPE_ERR_RANGE;
  }
  HIST_HERE = 2;
  for (i = 3; i <= NEQ; i++) my_hist[i] = storage[i];
  hist_len = length;
  XPPE_GUARD_BEGIN((void)0);
  two_d_hist(col, col2, storind, nx, ny, xlo, xhi, ylo, yhi);
  XPPE_GUARD_END;
  rc = result_begin(hist_len, 3, names);
  if (rc == XPPE_OK)
    for (i = 0; i < 3; i++) result_fill(i, my_hist[i]);
  free_hist();
  xppe_end_call();
  return rc;
}

/* Mirrors column_mean(). */
int xppe_mean(const char *column, double *mean, double *sd) {
  int rc, col, i;
  double sum = 0, sum2 = 0, ss, m;
  if ((rc = require_run()) != XPPE_OK) return rc;
  if ((rc = column_index(column, &col)) != XPPE_OK) return rc;
  for (i = 0; i < storind; i++) {
    ss = storage[col][i];
    sum += ss;
    sum2 += (ss * ss);
  }
  m = sum / (double)storind;
  if (mean) *mean = m;
  if (sd) *sd = sqrt(sum2 / (double)storind - m * m);
  return XPPE_OK;
}

/* ---- Liapunov exponent ------------------------------------------------ */

/* hrw_liapunov() in batch mode: the largest exponent along the current
   run, from one-step integrations of a perturbation of size NEWT_ERR. */
int xppe_lyapunov(double *exponent) {
  int rc, ok = 0;
  double z = 0;
  if ((rc = require_run()) != XPPE_OK) return rc;
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  ok = hrw_liapunov(&z, 1, NEWT_ERR);
  XPPE_GUARD_END;
  xppe_end_call();
  if (!ok) {
    xppe_set_error("the Liapunov exponent could not be computed", NULL);
    return XPPE_ERR_NUMERIC;
  }
  if (exponent) *exponent = z;
  return XPPE_OK;
}

/* ---- Averaging: adjoint and H function ----------------------------------- */

static void copy_table(float **data, int len, int ncols,
                       const char *const *names) {
  int i;
  if (result_begin(len, ncols, names) != XPPE_OK) return;
  for (i = 0; i < ncols; i++) result_fill(i, data[i]);
}

/* new_adjoint(): the adjoint of the linearisation along the current run,
   which must be exactly one period of a stable limit cycle. */
int xppe_adjoint(int max_iterations, double tolerance) {
  const char *names[MAXODE + 1];
  extern char uvar_names[MAXODE][12];
  int rc, i;
  if ((rc = require_run()) != XPPE_OK) return rc;
  if (max_iterations > 0) ADJ_MAXIT = max_iterations;
  if (tolerance > 0) ADJ_ERR = tolerance;
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  new_adjoint();
  XPPE_GUARD_END;
  if (!ADJ_HERE) {
    data_back();
    xppe_end_call();
    xppe_set_error("the adjoint did not converge: is the run exactly one "
                   "period of a stable limit cycle?", NULL);
    return XPPE_ERR_NUMERIC;
  }
  names[0] = "T";
  for (i = 0; i < NODE && i < MAX_RESULT_COLS - 1; i++)
    names[i + 1] = uvar_names[i];
  copy_table(my_adj, adj_len, i + 1, names);
  data_back();
  xppe_end_call();
  return XPPE_OK;
}

/* new_h_fun(): H(phi) for the coupling formulas given (one per ODE, in
   which primed names such as V' are the other oscillator's variables).
   Needs xppe_adjoint() on the same run first. */
int xppe_hfunction(int n, const char *const *coupling) {
  static const char *names[] = {"phi", "H", "Hodd", "Heven"};
  int rc, i;
  if ((rc = require_run()) != XPPE_OK) return rc;
  if (!ADJ_HERE || storind != adj_len) {
    xppe_set_error("compute the adjoint of this run first", NULL);
    return XPPE_ERR_STATE;
  }
  if (n != NODE) {
    xppe_set_error("give one coupling formula per ODE variable", NULL);
    return XPPE_ERR_RANGE;
  }
  for (i = 0; i < NODE; i++) {
    snprintf(coup_string[i], 80, "%s",
             coupling[i] && coupling[i][0] ? coupling[i] : "0");
  }
  xppe_begin_call();
  xpps_answers_clear(); /* make_h() keeps the strings when not prompted */
  XPPE_GUARD_BEGIN((void)0);
  new_h_fun(0);
  XPPE_GUARD_END;
  if (!H_HERE) {
    data_back();
    xppe_end_call();
    xppe_set_error("the H function could not be computed: check the "
                   "coupling formulas", NULL);
    return XPPE_ERR_NUMERIC;
  }
  copy_table(my_h, h_len, HODD_EV ? 4 : 2, names);
  data_back();
  xppe_end_call();
  return XPPE_OK;
}

/* ---- Curve fitting ---------------------------------------------------- */

/* test_fit() without its dialog: the same checks, then run_fit(). */
int xppe_fit(const char *file, int npts, int ncols, const char *variables,
             const char *columns, const char *parameters, double tolerance,
             double epsilon, int max_iterations) {
  char varlist[256], collist[256], parlist[512];
  double a[1000], y0[1000], *yfit;
  int nvars = 0, npars = 0, i, ok, rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (!file || !variables || !columns || !parameters || npts < 1 ||
      ncols < 2) {
    xppe_set_error("give the data file, its number of rows and columns, "
                   "the variables, their columns and the parameters", NULL);
    return XPPE_ERR_RANGE;
  }
  snprintf(varlist, sizeof varlist, "%s", variables);
  snprintf(collist, sizeof collist, "%s", columns);
  snprintf(parlist, sizeof parlist, "%s", parameters);
  parse_collist(collist, fin.icols, &nvars);
  if (nvars <= 0) {
    xppe_set_error("No columns...", NULL);
    return XPPE_ERR_RANGE;
  }
  fin.nvars = nvars;
  nvars = 0;
  parse_varlist(varlist, fin.ivar, &nvars);
  if (fin.nvars != nvars) {
    xppe_set_error(" # columns != # fitted variables", NULL);
    return XPPE_ERR_RANGE;
  }
  parse_parlist(parlist, fin.ipar, &npars);
  if (npars <= 0) {
    xppe_set_error(" No parameters!", NULL);
    return XPPE_ERR_NAME;
  }
  fin.npars = npars;
  for (i = 0; i < npars; i++)
    if (fin.ipar[i] >= 0 && fin.ipar[i] >= NODE) {
      xppe_set_error(" Cant vary auxiliary/markov variables! ", NULL);
      return XPPE_ERR_NAME;
    }
  for (i = 0; i < fin.nvars; i++) {
    if (fin.icols[i] < 2) {
      xppe_set_error(" Illegal column must be >= 2", NULL);
      return XPPE_ERR_RANGE;
    }
    if (fin.ivar[i] < 0 || fin.ivar[i] >= NODE) {
      xppe_set_error(" Fit only to variables! ", NULL);
      return XPPE_ERR_NAME;
    }
  }
  fin.npts = npts;
  fin.dim = ncols;
  if (tolerance > 0) fin.tol = tolerance;
  if (epsilon > 0) fin.eps = epsilon;
  if (max_iterations > 0) fin.maxiter = max_iterations;
  yfit = malloc((size_t)fin.npts * (size_t)fin.nvars * sizeof(double));
  if (!yfit) {
    xppe_set_error("out of memory", NULL);
    return XPPE_ERR_RANGE;
  }
  for (i = 0; i < NODE; i++) y0[i] = last_ic[i];
  for (i = 0; i < fin.npars; i++)
    a[i] = fin.ipar[i] < 0 ? constants[-fin.ipar[i]] : last_ic[fin.ipar[i]];
  xppe_begin_call();
  plintf(" Running the fit...\n");
  XPPE_GUARD_BEGIN(free(yfit));
  ok = run_fit((char *)file, fin.npts, fin.npars, fin.nvars, fin.maxiter,
               fin.dim, fin.eps, fin.tol, fin.ipar, fin.ivar, fin.icols, y0,
               a, yfit);
  XPPE_GUARD_END;
  free(yfit);
  xppe_end_call();
  if (!ok) {
    xppe_set_error("the fit did not converge; see the log", NULL);
    return XPPE_ERR_NUMERIC;
  }
  /* The fitted values become the model's parameters and initial data. */
  for (i = 0; i < npars; i++) {
    if (fin.ipar[i] < 0)
      constants[-fin.ipar[i]] = a[i];
    else
      last_ic[fin.ipar[i]] = a[i];
  }
  return XPPE_OK;
}
