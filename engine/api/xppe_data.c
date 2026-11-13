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
#include <limits.h>
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
void norm_vec(double *v, double *mu, int n);

/* The integration (integrate.c, numerics, delay, Volterra, noise) */
extern double DELTA_T, TEND, T0, TRANS, BOUND;
extern int METHOD, NJMP, DelayFlag, NKernel, NMarkov, NWiener;
extern int NFlags, TORUS; /* global flags, @ fold */
extern int (*solver)();   /* the fixed-step method */
extern double *WORK;
void get_ic(int it, double *x);
void evaluate_derived(void);
void end_cv(void);

/* do_fit.c (its functions are declared in do_fit.h), browse.c */
extern FITINFO fin;
void find_variable(char *s, int *col);
float *get_data_col(int c);

/* ---- The result table ------------------------------------------------ */

/* an adjoint has one per variable; a phase model also phase, H, H_odd,
   H_even, locked and stable */
#define MAX_RESULT_COLS (MAXODE + 6)

static double *res[MAX_RESULT_COLS];
static char res_names[MAX_RESULT_COLS][32];
static int res_ncols, res_nrows;

static void result_clear(void) {
  int i;
  for (i = 0; i < res_ncols; i++) {
    free(res[i]);
    res[i] = NULL;
  }
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

int xppe_result_start(int nrows, int ncols, const char *const *names) {
  if (ncols > MAX_RESULT_COLS) {
    xppe_set_error("too many columns for the result table", NULL);
    return XPPE_ERR_RANGE;
  }
  return result_begin(nrows, ncols, names);
}

void xppe_result_put(int col, int row, double value) {
  if (col >= 0 && col < res_ncols && row >= 0 && row < res_nrows) res[col][row] = value;
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

/* hrw_liapunov()'s method carried out during an integration from the initial
   conditions, in double precision. A copy of the state perturbed by eps goes
   along with it; after each output step (DT times NOUT) the distance between
   the two is measured, the logarithm of its growth added up, and the copy
   put back at the distance eps in the direction it has moved to. Both are
   stepped as lyap_step() says: the adaptive methods with XPPAUT's
   one_step_int(), as in hrw_liapunov(), the fixed-step ones as integrate()
   steps them. In hrw_liapunov() each step starts from the stored run, which
   is in single precision, times included: once the time or a variable is
   large the rounding of the stored values and of the step length is
   comparable to eps, and a folded variable jumps by its period. Steps that
   start before TRANS carry the perturbation along without counting, and the
   integration stops, as a run does, when a variable passes BOUND. Nothing is
   stored. */
enum {
  METHOD_DISCRETE = 0,
  METHOD_GEAR = 5,
  METHOD_RKQS = 8,
  METHOD_STIFF = 9,
  METHOD_CVODE = 10,
  METHOD_DP5 = 11,
  METHOD_DP83 = 12,
  METHOD_RB23 = 13
};

/* 1 for the methods that integrate() steps with its default: branch, NJMP
   steps of DT in one solver() call; 0 for the adaptive ones, which go
   through one_step_int(). */
static int method_is_fixed_step(void) {
  return !(METHOD == METHOD_GEAR || METHOD == METHOD_RKQS ||
           METHOD == METHOD_STIFF || METHOD == METHOD_CVODE ||
           METHOD == METHOD_DP5 || METHOD == METHOD_DP83 ||
           METHOD == METHOD_RB23);
}

/* One output step from t to t1 (1 on success). The fixed-step methods take
   NJMP steps of DT as integrate() does, with one solver() call: a step to
   the rounded end time t1 could be a short one, which adams() cannot take.
   The adaptive methods go through one_step_int(). The two states take turns
   on the one solver state (Adams history, Gear and CVODE workspaces), so
   each call starts the method afresh; a saved copy of that state for each
   trajectory is not available. */
static int lyap_step(double *y, double t, double t1, int *istart, int fixed) {
  if (fixed) return solver(y, &t, DELTA_T, NJMP, NODE, istart, WORK) >= 0;
  return one_step_int(y, t, t1, istart);
}

int xppe_lyapunov_integrate(double eps, double *exponent) {
  double y[MAXODE], y2[MAXODE], dy[MAXODE];
  double h, t, t1, nrm, sum = 0, span = 0;
  int rc, i, k, nsteps, istart, fixed, ok = 1, out = 0;
  const char *why = NULL;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (DelayFlag || NKernel > 0) {
    xppe_set_error("the Liapunov exponent needs a model without delays or "
                   "integrals: the perturbed copy would need a history of "
                   "its own", NULL);
    return XPPE_ERR_STATE;
  }
  if (NWiener > 0 || NMarkov > 0) {
    xppe_set_error("the Liapunov exponent needs a model without noise "
                   "(wiener or Markov variables)", NULL);
    return XPPE_ERR_STATE;
  }
  if (!(eps > 0)) eps = NEWT_ERR;
  /* The output steps of integrate(): a map takes steps of 1 (or -1). */
  if (METHOD == METHOD_DISCRETE) {
    nsteps = (int)TEND;
    h = DELTA_T / fabs(DELTA_T);
  } else {
    nsteps = (int)((TEND + fabs(DELTA_T) * .1) / fabs(DELTA_T));
    h = DELTA_T;
  }
  nsteps = (nsteps + NJMP - 1) / NJMP;
  h *= NJMP;
  fixed = method_is_fixed_step();
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  evaluate_derived();
  get_ic(2, y);
  for (i = 0; i < NODE; i++) dy[i] = 0;
  dy[0] = eps;
  for (k = 0; k < nsteps; k++) {
    t = T0 + k * h;
    t1 = T0 + (k + 1) * h;
    for (i = 0; i < NODE; i++) y2[i] = y[i] + dy[i];
    /* Each call starts the method afresh, since the two states take turns
       (gear() takes 0 for a fresh start, as integrate() gives it). */
    istart = METHOD == METHOD_GEAR ? 0 : 1;
    ok = lyap_step(y, t, t1, &istart, fixed);
    istart = METHOD == METHOD_GEAR ? 0 : 1;
#ifdef CVODE_YES
    if (METHOD == METHOD_CVODE) end_cv(); /* start_cv() would leak it */
#endif
    if (ok) ok = lyap_step(y2, t, t1, &istart, fixed);
#ifdef CVODE_YES
    if (METHOD == METHOD_CVODE) end_cv();
#endif
    if (!ok) {
      why = "the integration failed";
      break;
    }
    for (i = 0; i < NODE && fabs(y[i]) <= BOUND; i++)
      ;
    if (i < NODE) { /* out of bounds or NaN: the run would end here */
      out = 1;
      break;
    }
    for (i = 0; i < NODE; i++) dy[i] = y2[i] - y[i];
    norm_vec(dy, &nrm, NODE);
    if (!(nrm > 0) || !isfinite(nrm)) {
      why = nrm == 0 ? "the perturbation shrank to nothing (an exponent of "
                       "minus infinity)"
                     : "the perturbed copy blew up";
      ok = 0;
      break;
    }
    if (fabs(t) >= TRANS) {
      sum += log(nrm / eps);
      span += t1 - t;
    }
    for (i = 0; i < NODE; i++) dy[i] *= eps;
  }
  XPPE_GUARD_END;
  xppe_end_call();
  if (!ok) {
    xppe_set_error("the Liapunov exponent could not be computed: %s", why);
    return XPPE_ERR_NUMERIC;
  }
  if (span == 0) {
    xppe_set_error(out ? "a variable passed BOUND before there was a step "
                         "to average over"
                       : "no output step after the transient (TRANS) to "
                         "average over", NULL);
    return XPPE_ERR_RANGE;
  }
  if (exponent) *exponent = sum / span;
  return XPPE_OK;
}

/* ---- Averaging: adjoint and H function ----------------------------------- */

static int copy_table(float **data, int len, int ncols,
                      const char *const *names) {
  int i, rc;
  if ((rc = result_begin(len, ncols, names)) != XPPE_OK) return rc;
  for (i = 0; i < ncols; i++) result_fill(i, data[i]);
  return XPPE_OK;
}

/* XPPAUT keeps ADJ_HERE set after later runs, so a run of the same length
   (after a parameter change, or from another start on the cycle) passed
   every check and was combined with the adjoint of the old one. The stored
   run is fingerprinted when the adjoint is computed (FNV-1a over its length
   and the bits of time and the ODE variables), and the functions that use
   the adjoint compare. */
static unsigned long long adj_run_fp;
static int adj_run_fp_set;

static unsigned long long run_fingerprint(void) {
  unsigned long long h = 14695981039346656037ULL;
  int i, k;
  size_t b;
  const unsigned char *p;
  int head[2];
  head[0] = storind;
  head[1] = NODE;
  for (p = (const unsigned char *)head, b = 0; b < sizeof head; b++)
    h = (h ^ p[b]) * 1099511628211ULL;
  for (i = 0; i <= NODE; i++)
    for (k = 0; k < storind; k++)
      for (p = (const unsigned char *)&storage[i][k], b = 0; b < sizeof(float);
           b++)
        h = (h ^ p[b]) * 1099511628211ULL;
  return h;
}

/* The adjoint is there and belongs to the stored run (XPPE_OK), or an error
   saying which is not so. */
static int require_current_adjoint(const char *what) {
  if (!ADJ_HERE || !adj_run_fp_set) {
    xppe_set_error("%s needs the adjoint of one period of a stable limit "
                   "cycle: run exactly one period and compute the adjoint "
                   "(an unstable cycle has none)", what);
    return XPPE_ERR_STATE;
  }
  if (storind != adj_len || run_fingerprint() != adj_run_fp) {
    xppe_set_error("%s: the run is not the one the adjoint was computed "
                   "from (it was run again since): compute the adjoint again",
                   what);
    return XPPE_ERR_STATE;
  }
  return XPPE_OK;
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
  for (i = 0; i < NODE && i < MAXODE; i++)
    names[i + 1] = uvar_names[i];
  rc = copy_table(my_adj, adj_len, i + 1, names);
  data_back();
  adj_run_fp = run_fingerprint();
  adj_run_fp_set = 1;
  xppe_end_call();
  return rc;
}

/* new_h_fun(): H(phi) for the coupling formulas given (one per ODE, in
   which primed names such as V' are the other oscillator's variables).
   Needs xppe_adjoint() on the same run first. */
int xppe_hfunction(int n, const char *const *coupling) {
  static const char *names[] = {"phi", "H", "Hodd", "Heven"};
  int rc, i;
  if ((rc = require_run()) != XPPE_OK) return rc;
  if ((rc = require_current_adjoint("the H function")) != XPPE_OK) return rc;
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
  rc = copy_table(my_h, h_len, HODD_EV ? 4 : 2, names);
  data_back();
  xppe_end_call();
  return rc;
}

/* ---- The phase-reduced model --------------------------------------------- */

/* Catmull-Rom cubic through the N values v[0..N-1] of a periodic function on
   a grid of spacing 1 (index N is index 0): the value at u, and its
   derivative in *dv. The only numerics of xppe_phase_model that are not
   XPPAUT's. */
static double periodic_cubic(const double *v, int N, double u, double *dv) {
  double fl = floor(u), s = u - fl, s2 = s * s, s3 = s2 * s;
  int k = (int)fl % N;
  double p0, p1, p2, p3, m1, m2;
  if (k < 0) k += N;
  p0 = v[(k + N - 1) % N];
  p1 = v[k];
  p2 = v[(k + 1) % N];
  p3 = v[(k + 2) % N];
  m1 = 0.5 * (p2 - p0);
  m2 = 0.5 * (p3 - p1);
  if (dv)
    *dv = (6 * s2 - 6 * s) * p1 + (3 * s2 - 4 * s + 1) * m1 +
          (-6 * s2 + 6 * s) * p2 + (3 * s2 - 2 * s) * m2;
  return (2 * s3 - 3 * s2 + 1) * p1 + (s3 - 2 * s2 + s) * m1 +
         (-2 * s3 + 3 * s2) * p2 + (s3 - s2) * m2;
}

/* The zeros of the periodic grid function v[0..N-1] where it changes sign:
   an exact zero at a node counts when its two neighbours differ in sign, an
   interval whose ends differ in sign holds one zero, found by a few secant
   steps (false position, so it stays in the interval) on the cubic. root[]
   gets the position in grid units, slope[] the slope there (per grid unit):
   for an interval, the rise of v across it, since the cubic can overshoot
   and hold three zeros in one bracket, and false position may settle on the
   middle one, whose derivative has the other sign; at a node, the cubic's
   derivative. Returns the number of zeros (at most N). */
static int sign_change_roots(const double *v, int N, double *root,
                             double *slope) {
  int k, i, nr = 0;
  double vmax = 0;
  for (k = 0; k < N; k++)
    if (fabs(v[k]) > vmax) vmax = fabs(v[k]);
  for (k = 0; k < N; k++) {
    double x, d;
    double next = v[(k + 1) % N];
    if (v[k] == 0) {
      double prev = v[(k + N - 1) % N];
      if (!(prev * next < 0)) continue;
      x = k;
    } else if (v[k] * next < 0) {
      double a = k, b = k + 1, fa = v[k], fb = next, f;
      x = a;
      for (i = 0; i < 12; i++) {
        x = b - fb * (b - a) / (fb - fa);
        f = periodic_cubic(v, N, x, NULL);
        if (f == 0 || fabs(f) <= 1e-14 * vmax) break;
        if ((f < 0) == (fa < 0)) {
          a = x;
          fa = f;
        } else {
          b = x;
          fb = f;
        }
      }
    } else {
      continue;
    }
    if (v[k] == 0)
      periodic_cubic(v, N, x, &d);
    else
      d = next - v[k];
    root[nr] = x;
    slope[nr] = d;
    nr++;
  }
  return nr;
}

/* Method D of docs/design/isochrons.md. H comes from xppe_hfunction (its
   column, not a second average); Z from the adjoint. H_odd and H_even are
   its columns when it returns them (NEQ > 2), else 0.5 (H[k] -/+ H[n-1-k]),
   which is what make_h() does for them. */
int xppe_phase_model(int n, const char *const *coupling) {
  static const char *fixed_names[] = {"phase", "H", "H_odd", "H_even",
                                      "locked", "stable"};
  const char *names[MAX_RESULT_COLS];
  extern char uvar_names[MAXODE][12];
  static char znames[MAXODE][16]; /* kept off the stack */
  double *h = NULL, *hodd = NULL, *heven = NULL, *v = NULL, *root = NULL,
         *slope = NULL;
  double step, period;
  int rc, i, k, m, nr, ncols, nz;
  if ((rc = require_run()) != XPPE_OK) return rc;
  if (DelayFlag || NKernel > 0) {
    xppe_set_error("the phase model needs a model without delays or "
                   "integrals", NULL);
    return XPPE_ERR_STATE;
  }
  if (NWiener > 0 || NMarkov > 0) {
    xppe_set_error("the phase model needs a model without noise (wiener or "
                   "Markov variables)", NULL);
    return XPPE_ERR_STATE;
  }
  if (METHOD == METHOD_DISCRETE) {
    xppe_set_error("the phase model needs differential equations, not a map",
                   NULL);
    return XPPE_ERR_STATE;
  }
  if ((rc = require_current_adjoint("the phase model")) != XPPE_OK) return rc;
  if (adj_len < 5) {
    xppe_set_error("the phase model needs a cycle of at least 5 stored "
                   "points", NULL);
    return XPPE_ERR_STATE;
  }
  step = DELTA_T * NJMP; /* the spacing of the stored cycle and of Z */
  if (!(step > 0)) {
    xppe_set_error("the phase model needs a positive output step", NULL);
    return XPPE_ERR_STATE;
  }
  m = adj_len;
  period = step * (m - 1);
  /* DT or NOUT changed since the run: the stored cycle has another spacing */
  if (fabs((double)storage[0][m - 1] - (double)storage[0][0] - period) >
      1e-5 * period + 1e-6 * fabs((double)storage[0][m - 1])) {
    xppe_set_error("DT or NOUT is not what the stored cycle was made with: "
                   "run one period again and compute the adjoint again", NULL);
    return XPPE_ERR_STATE;
  }
  nz = NODE;
  ncols = 1 + nz + 3 + 2;
  if (ncols > MAX_RESULT_COLS) {
    xppe_set_error("too many variables for the phase model table", NULL);
    return XPPE_ERR_RANGE;
  }
  if ((rc = xppe_hfunction(n, coupling)) != XPPE_OK) return rc;
  if (res_ncols < 2 || res_nrows != m) {
    xppe_set_error("the H function's table is not one value per stored "
                   "point of the cycle", NULL);
    return XPPE_ERR_NUMERIC;
  }
  for (k = 0; k < m; k++)
    if (!isfinite(res[1][k])) {
      xppe_set_error("H is not a number at some phases: check the coupling "
                     "formulas", NULL);
      return XPPE_ERR_NUMERIC;
    }
  /* xppe_hfunction's table is in the result: keep its columns, then replace
     the table. */
  h = malloc(sizeof(double) * (size_t)m * 4);
  v = h + m;
  hodd = v + m;
  heven = hodd + m;
  root = malloc(sizeof(double) * (size_t)m * 2);
  if (!h || !root) {
    free(h);
    free(root);
    xppe_set_error("out of memory", NULL);
    return XPPE_ERR_RANGE;
  }
  slope = root + m;
  for (k = 0; k < m; k++) h[k] = res[1][k];
  for (k = 0; k < m; k++) {
    if (res_ncols >= 4) {
      hodd[k] = res[2][k];
      heven[k] = res[3][k];
    } else {
      hodd[k] = 0.5 * (h[k] - h[m - 1 - k]);
      heven[k] = 0.5 * (h[k] + h[m - 1 - k]);
    }
  }
  /* The roots are those of H_odd on the periodic grid of the m-1 distinct
     points, where H_odd(-phi) = -H_odd(phi) holds exactly, so that 0 (and
     T/2 on an even grid) are zeros and not rounding noise. */
  for (k = 0; k < m - 1; k++)
    v[k] = 0.5 * (h[k] - h[(m - 1 - k) % (m - 1)]);
  nr = sign_change_roots(v, m - 1, root, slope);
  names[0] = fixed_names[0];
  for (i = 0; i < nz; i++) {
    snprintf(znames[i], sizeof znames[i], "Z_%s", uvar_names[i]);
    names[1 + i] = znames[i];
  }
  for (i = 1; i < 6; i++) names[nz + i] = fixed_names[i];
  if ((rc = result_begin(m, ncols, names)) != XPPE_OK) {
    free(h);
    free(root);
    return rc;
  }
  for (k = 0; k < m; k++) {
    res[0][k] = k * step;
    for (i = 0; i < nz; i++) res[1 + i][k] = (double)my_adj[i + 1][k];
    res[nz + 1][k] = h[k];
    res[nz + 2][k] = hodd[k];
    res[nz + 3][k] = heven[k];
    res[nz + 4][k] = res[nz + 5][k] = NAN;
  }
  for (k = 0; k < nr; k++) {
    res[nz + 4][k] = root[k] * step;
    res[nz + 5][k] = slope[k] > 0 ? 1 : 0;
  }
  free(h);
  free(root);
  return XPPE_OK;
}

/* ---- Asymptotic phase ---------------------------------------------------- */

/* The point of the stored cycle (a polyline through the stored points, which
   run from t = 0 to the period) nearest to y: its distance, the position
   k + u (u in [0, 1]) in units of the output step, and the cycle point and
   Z there, both interpolated linearly between the stored points k and k+1. */
static double cycle_nearest(const double *y, double *pos, double *gam,
                            double *z) {
  double best = HUGE_VAL;
  int i, k;
  for (k = 0; k + 1 < adj_len; k++) {
    double dd = 0, yd = 0, u, d2 = 0;
    for (i = 0; i < NODE; i++) {
      double a = storage[i + 1][k], d = storage[i + 1][k + 1] - a;
      dd += d * d;
      yd += (y[i] - a) * d;
    }
    u = dd > 0 ? yd / dd : 0;
    u = u < 0 ? 0 : (u > 1 ? 1 : u);
    for (i = 0; i < NODE; i++) {
      double a = storage[i + 1][k], p = a + u * (storage[i + 1][k + 1] - a);
      d2 += (y[i] - p) * (y[i] - p);
    }
    if (d2 < best) {
      best = d2;
      *pos = k + u;
      for (i = 0; i < NODE; i++) {
        double a = storage[i + 1][k], b = my_adj[i + 1][k];
        gam[i] = a + u * (storage[i + 1][k + 1] - a);
        z[i] = b + u * (my_adj[i + 1][k + 1] - b);
      }
    }
  }
  return sqrt(best);
}

/* Method A of docs/design/isochrons.md. The state is stepped with
   XPPAUT's own routines, one output step at a time (one_step_int() for the
   adaptive methods, as in xppe_lyapunov_integrate(); the fixed-step solver
   as integrate() calls it for the others), without restarting between steps, until it lies within delta of the stored cycle. With theta_c the
   time position of the nearest cycle point, the phase is
   theta_c/T + Z(theta_c) . (end state - gamma(theta_c))/T - s/T, mod 1. */
int xppe_asymptotic_phase(const double *x, double delta, double tmax,
                          double *phase, double *elapsed) {
  double y[MAXODE], gam[MAXODE], z[MAXODE];
  double h, t, t1, pos, dist, period, theta, corr = 0, s = 0, ph, q;
  int rc, i, k, nmax, istart, fixed, ok = 1, out = 0;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (DelayFlag || NKernel > 0) {
    xppe_set_error("the asymptotic phase needs a model without delays or "
                   "integrals", NULL);
    return XPPE_ERR_STATE;
  }
  if (NWiener > 0 || NMarkov > 0) {
    xppe_set_error("the asymptotic phase needs a model without noise "
                   "(wiener or Markov variables)", NULL);
    return XPPE_ERR_STATE;
  }
  if (METHOD == METHOD_DISCRETE) {
    xppe_set_error("the asymptotic phase needs differential equations, not "
                   "a map", NULL);
    return XPPE_ERR_STATE;
  }
  if (NFlags > 0 || TORUS == 1) {
    xppe_set_error("the asymptotic phase needs a model without global flags "
                   "or folded variables (@ fold): the stored cycle is reset "
                   "or folded there, the free integration is not", NULL);
    return XPPE_ERR_STATE;
  }
  if ((rc = require_current_adjoint("the asymptotic phase")) != XPPE_OK)
    return rc;
  if (adj_len < 2) {
    xppe_set_error("the asymptotic phase needs a cycle of at least 2 stored "
                   "points", NULL);
    return XPPE_ERR_STATE;
  }
  if (!x || !(delta > 0) || !(tmax > 0)) {
    xppe_set_error("give a state, a distance delta > 0 and a time limit "
                   "tmax > 0", NULL);
    return XPPE_ERR_RANGE;
  }
  h = DELTA_T * NJMP; /* the spacing of the stored cycle and of Z */
  if (!(h > 0)) {
    xppe_set_error("the asymptotic phase needs a positive output step", NULL);
    return XPPE_ERR_STATE;
  }
  period = h * (adj_len - 1);
  /* DT or NOUT changed since the run: the stored cycle has another spacing */
  if (fabs((double)storage[0][adj_len - 1] - (double)storage[0][0] - period) >
      1e-5 * period + 1e-6 * fabs((double)storage[0][adj_len - 1])) {
    xppe_set_error("DT or NOUT is not what the stored cycle was made with: "
                   "run one period again and compute the adjoint again", NULL);
    return XPPE_ERR_STATE;
  }
  q = floor(tmax / h + 1e-9);
  nmax = q >= INT_MAX ? INT_MAX : (int)q;
  fixed = method_is_fixed_step();
  for (i = 0; i < NODE; i++) y[i] = x[i];
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  evaluate_derived();
  /* One start for the whole integration, as integrate() does (gear() takes
     0 for a fresh start). */
  istart = METHOD == METHOD_GEAR ? 0 : 1;
  t = T0;
  for (k = 0;; k++) {
    for (i = 0; i < NODE && fabs(y[i]) <= BOUND; i++)
      ;
    if (i < NODE) { /* out of bounds or NaN: a run would end here */
      out = 1;
      ok = 0;
      break;
    }
    dist = cycle_nearest(y, &pos, gam, z);
    if (dist < delta) break;
    if (k >= nmax) {
      ok = 0;
      break;
    }
    if (fixed) {
      /* NJMP steps of DT, as integrate() takes them: a step to a rounded
         end time could be a short one, which adams() cannot take. */
      if (solver(y, &t, DELTA_T, NJMP, NODE, &istart, WORK) < 0) {
        ok = -1;
        break;
      }
    } else {
      t1 = T0 + (k + 1) * h;
      if (!one_step_int(y, T0 + k * h, t1, &istart)) {
        ok = -1;
        break;
      }
    }
  }
  XPPE_GUARD_END;
#ifdef CVODE_YES
  if (METHOD == METHOD_CVODE) end_cv(); /* safe to call twice */
#endif
  xppe_end_call();
  if (ok == -1) {
    xppe_set_error("the integration failed", NULL);
    return XPPE_ERR_NUMERIC;
  }
  if (!ok) {
    if (out)
      xppe_set_error("the state left the bounds (BOUND) before it came near "
                     "the cycle: the asymptotic phase is undefined here", NULL);
    else
      xppe_set_error("the state did not come within delta of the cycle by "
                     "tmax: the asymptotic phase is undefined here (outside "
                     "the basin of the cycle, or tmax is too short)", NULL);
    return XPPE_ERR_NUMERIC;
  }
  s = k * h;
  for (i = 0; i < NODE; i++) corr += z[i] * (y[i] - gam[i]);
  theta = pos * h;
  ph = (theta + corr - s) / period;
  ph -= floor(ph);
  if (ph >= 1) ph = 0; /* a tiny negative value rounds up to 1 */
  if (phase) *phase = ph;
  if (elapsed) *elapsed = s;
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
