/*
 * xppe.h - the XPPAUT engine API.
 *
 * One loaded engine image holds one model, because XPPAUT keeps its state
 * in global variables. To work with several models, or to load a second
 * model, load another copy of the engine library (libxpp does this for
 * you; see xpp.h).
 *
 * Every call returns XPPE_OK (0) or a negative XPPE_ERR_* code. Text that
 * the engine printed during the last call is available from xppe_log().
 * If the engine hits a fatal error (where XPPAUT would have exited), the
 * call returns XPPE_ERR_FATAL and the image refuses further work: load a
 * fresh copy.
 */
#ifndef XPPE_H
#define XPPE_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(XPPE_BUILD_SHARED)
#define XPPE_API __declspec(dllexport)
#elif defined(__GNUC__) && defined(XPPE_BUILD_SHARED)
#define XPPE_API __attribute__((visibility("default")))
#else
#define XPPE_API
#endif

#define XPPE_API_VERSION 2

enum {
  XPPE_OK = 0,
  XPPE_ERR_STATE = -1,   /* no model loaded, already loaded, or broken */
  XPPE_ERR_LOAD = -2,    /* the model file could not be read or parsed */
  XPPE_ERR_FATAL = -3,   /* the engine aborted; this image is unusable */
  XPPE_ERR_NAME = -4,    /* no variable, parameter or option of that name */
  XPPE_ERR_RANGE = -5,   /* index or size out of range */
  XPPE_ERR_NUMERIC = -6  /* the computation did not converge or failed */
};

/* Name lists, for xppe_count() and xppe_name(). */
enum {
  XPPE_VARIABLES = 0,  /* state variables: ODEs/maps, then Markov variables */
  XPPE_PARAMETERS = 1, /* parameters declared with "par" */
  XPPE_COLUMNS = 2,    /* output columns: "T", variables, then auxiliaries */
  XPPE_SETS = 3        /* named parameter sets ("set" statements) */
};

XPPE_API int xppe_api_version(void);
XPPE_API const char *xppe_version(void);

/* Load a model file. options are extra XPPAUT command-line options, such
   as {"-setfile", "lecar.set"}; pass 0 and NULL for none. */
XPPE_API int xppe_load(const char *ode_path, int noptions,
                       const char *const *options);

/* Everything the engine printed during the last call (never NULL). */
XPPE_API const char *xppe_log(void);
/* A one-line description of the last error (never NULL). */
XPPE_API const char *xppe_error(void);

XPPE_API int xppe_count(int list);
XPPE_API const char *xppe_name(int list, int index);

/* Read a variable's initial condition, a parameter, or a numerical option
   (TOTAL, DT, T0, TRANS, BOUND, TOL, ATOL, DTMIN, DTMAX, NOUT, MAXSTOR,
   METH, DELAY, NEWT_TOL, NEWT_ITER, JAC_EPS, ...). Names ignore case. */
XPPE_API int xppe_get(const char *name, double *value);
/* Set an initial condition, parameter or option exactly as a "set"
   statement or an ODE-file "@" line would. value is text so that options
   like METH=stiff work. */
XPPE_API int xppe_set(const char *name, const char *value);
XPPE_API int xppe_set_number(const char *name, double value);
/* Apply the named set from the model file. */
XPPE_API int xppe_apply_set(const char *set_name);

/* Integrate from the current initial conditions, exactly as XPPAUT's batch
   mode does. On success the result has xppe_rows() rows. */
XPPE_API int xppe_run(void);
XPPE_API int xppe_rows(void);
/* Copy output column col (0 = time) into out, which holds n values.
   Returns the number of values copied. col = -1 always gives time, even
   when the model's "only" statement leaves time out of the columns. */
XPPE_API int xppe_column(int col, double *out, int n);

/* Newton's method for an equilibrium starting from x (length = number of
   ODE variables). On success x holds the equilibrium and re/im the
   eigenvalues of the Jacobian there. */
XPPE_API int xppe_equilibrium(double *x, double *re, double *im);

/* The right-hand side of the ODEs at time t and state x (one value per
   ODE variable). For maps it is the next state. */
XPPE_API int xppe_rhs(double t, const double *x, double *dxdt);

/* Nullcline of variable `which` (0: the x-variable, 1: the y-variable) in
   the plane of ODE variables ix and iy (0-based) over the given window, on
   a grid x grid mesh, by XPPAUT's own contouring. Writes up to
   max_segments line segments as (x1, y1, x2, y2) quadruples and returns
   their number. */
XPPE_API int xppe_nullcline(int which, int ix, int iy, double xlo, double xhi,
                            double ylo, double yhi, int grid, float *segments,
                            int max_segments);

/* Stable and unstable manifolds of a saddle. After xppe_equilibrium() finds
   a saddle with a one-dimensional stable or unstable direction, there are
   xppe_manifold_count() branches (two per direction). xppe_manifold(k)
   integrates branch k (backward in time for stable branches) and leaves it
   as the current run; *type is +1 for unstable, -1 for stable. */
XPPE_API int xppe_manifold_count(void);
XPPE_API int xppe_manifold(int k, int *type);

/* ---- Data analysis --------------------------------------------------------
 *
 * XPPAUT's Numerics > stocHastic tools and Numerics > Averaging, applied
 * to the current run by XPPAUT's own routines. Columns are named as in the
 * data browser: "T", a variable or an auxiliary (names ignore case).
 *
 * The tools that produce a table leave it in the result table, read with
 * xppe_result_rows(), xppe_result_count(), xppe_result_name() and
 * xppe_result_column(). The run itself is not changed.
 */

/* Fourier series of a column over the run: freq, cos, sin (Fourier). */
XPPE_API int xppe_fourier(const char *column);
/* The same as amplitude and phase: freq, amplitude, phase (Power). The log
   reports a0 and the L2 norm. */
XPPE_API int xppe_power(const char *column);
/* Spectral density by Welch's method, windows of window_length points with
   50% overlap (Spec.dns): type 0 power spectral density of column, 1 cross
   spectrum and 2 coherence of column and column2. window: 0 square,
   1 Parzen, 2 Hamming, 3 Bartlett, 4 Hann. Result: freq, then power,
   cross or coherence. */
XPPE_API int xppe_spectrum(int type, const char *column, const char *column2,
                           int window_length, int window);
/* Histogram of a column in nbins bins from lo to hi, counting only the
   points where condition (an expression; "" for all) is nonzero: bin,
   count (Histogram). */
XPPE_API int xppe_histogram(const char *column, int nbins, double lo,
                            double hi, const char *condition);
/* Two-dimensional histogram of columns x and y: x, y, count (2D-hist). */
XPPE_API int xppe_histogram2d(const char *x, int nx, double xlo, double xhi,
                              const char *y, int ny, double ylo, double yhi);
/* Histogram of all pairwise differences of a column: difference, count
   (stAutocor). */
XPPE_API int xppe_autocorrelation(const char *column, int nbins, double lo,
                                  double hi);
/* Cross-correlation of two columns (the same column twice gives the
   autocorrelation) over about nbins lags, computed directly or (use_fft)
   by FFT: lag, correlation (Xcorrel). */
XPPE_API int xppe_correlation(const char *column, const char *column2,
                              int nbins, int use_fft);
/* Mean and standard deviation of a column (Mean). */
XPPE_API int xppe_mean(const char *column, double *mean, double *sd);
/* The largest Liapunov exponent along the current run (Liapunov). */
XPPE_API int xppe_lyapunov(double *exponent);

/* The adjoint of the linearised equations along the current run, which
   must be exactly one period of a stable limit cycle (Averaging > New
   adjoint). Its column for a variable is the infinitesimal phase response
   curve to perturbations of that variable. Result: T, then one column per
   ODE variable. max_iterations and tolerance replace XPPAUT's defaults (20
   and 1e-3) when positive. */
XPPE_API int xppe_adjoint(int max_iterations, double tolerance);
/* The averaged interaction function H(phi) for weak coupling, after
   xppe_adjoint() on the same run (Averaging > Make H). coupling gives one
   formula per ODE variable, n of them; primed names such as V' stand for
   the other oscillator's variables. Result: phi, H, and for three or more
   columns also Hodd and Heven. */
XPPE_API int xppe_hfunction(int n, const char *const *coupling);

/* Least-squares fit of parameters and initial conditions to data with
   XPPAUT's Levenberg-Marquardt (stocHastic > fIt data). file holds npts
   rows of ncols numbers, time first. variables names the ODE variables to
   fit and columns their columns in the file (from 2), both comma-separated;
   parameters lists the parameters and initial conditions to vary.
   tolerance, epsilon and max_iterations replace XPPAUT's defaults (1e-3,
   1e-5, 20) when positive. On success the model holds the fitted values. */
XPPE_API int xppe_fit(const char *file, int npts, int ncols,
                      const char *variables, const char *columns,
                      const char *parameters, double tolerance,
                      double epsilon, int max_iterations);

/* The table left by the last analysis. */
XPPE_API int xppe_result_rows(void);
XPPE_API int xppe_result_count(void);
XPPE_API const char *xppe_result_name(int col);
XPPE_API int xppe_result_column(int col, double *out, int n);

/* ---- AUTO continuation ----------------------------------------------------
 *
 * The same continuation XPPAUT runs from its AUTO window, driven by calls:
 *
 *   xppe_auto_parameters("iapp", NULL);   choose what to vary
 *   xppe_auto_set("DS", 0.02);            numerics, as in the AutoNum dialog
 *   xppe_auto_run("s");                   start: steady-state branch
 *   xppe_auto_grab(2);                    pick labelled point 2 (say, HB)
 *   xppe_auto_run("p");                   periodic orbits from the Hopf
 *
 * xppe_auto_run(choice) behaves like XPPAUT's Run button. `choice` is the
 * key you would press in the menu XPPAUT shows next:
 *
 *   nothing grabbed:  s steady state, p periodic, b boundary value,
 *                     h homoclinic, e heteroclinic (maps need no choice)
 *   Hopf (HB):        p periodic, e extend, n new point, t two-parameter
 *   period doubling:  d doubling, t two-parameter, f fixed period, e extend
 *   torus (TR):       t two-parameter, f fixed period, e extend
 *   periodic point:   e extend, f fixed period
 *   branch point:     s switch, e extend, n new point, t two-parameter
 *   limit point and ordinary steady states need no choice.
 */

/* Numerical settings: NTST, NMAX, NPR, DS, DSMIN, DSMAX, PARMIN, PARMAX,
   NORMMIN, NORMMAX, NCOL, EPSL, EPSU, EPSS, IAD, MXBF, IID, ITMX, ITNW,
   NWTN, IADS, SUPPBP. */
XPPE_API int xppe_auto_get(const char *name, double *value);
XPPE_API int xppe_auto_set(const char *name, double value);
/* The main continuation parameter and, for two-parameter runs, the second
   one (may be NULL). */
XPPE_API int xppe_auto_parameters(const char *main_par, const char *second_par);
/* AUTO's scratch and output files (fort.*, model.b/.d/.s) go here. */
XPPE_API int xppe_auto_set_workdir(const char *dir);
/* Forget the bifurcation diagram. */
XPPE_API int xppe_auto_reset(void);
XPPE_API int xppe_auto_run(const char *choice);
/* Select the point with this label as the start of the next run. */
XPPE_API int xppe_auto_grab(int label);
XPPE_API int xppe_auto_ungrab(void);

/* AUTO works with up to 8 of the model's parameters ("slots"). */
XPPE_API int xppe_auto_parameter_count(void);
XPPE_API const char *xppe_auto_parameter_name(int slot);
/* Slot of the main (which=0) or secondary (which=1) parameter. */
XPPE_API int xppe_auto_main_slot(int which);

typedef struct {
  int branch;        /* AUTO branch number; negative for periodic orbits */
  int point;         /* point number on the branch; negative when stable */
  int type;          /* AUTO type code (itp) */
  int label;         /* label of a special or saved point, else 0 */
  int stable;        /* 1 if stable */
  int periodic;      /* 1 for a periodic orbit, 0 for an equilibrium */
  int two_parameter; /* nonzero for points of a two-parameter curve */
  int par1_slot, par2_slot;
  double par1, par2; /* values of the main and secondary parameters */
  double norm;       /* L2 norm of the solution, as AUTO reports it */
  double period;     /* period of the orbit (periodic points) */
  char type_name[4]; /* "HB", "LP", "BP", "PD", "TR", "EP", "UZ", "MX", "" */
} xppe_auto_point_info;

/* Per-point arrays, for xppe_auto_point_data(). */
enum {
  XPPE_AUTO_U0 = 0,     /* the solution (start of the orbit) */
  XPPE_AUTO_UMAX = 1,   /* maximum of each variable over the orbit */
  XPPE_AUTO_UMIN = 2,   /* minimum of each variable over the orbit */
  XPPE_AUTO_UMEAN = 3,  /* mean of each variable over the orbit */
  XPPE_AUTO_EIG_RE = 4, /* eigenvalues or Floquet multipliers, real part */
  XPPE_AUTO_EIG_IM = 5, /* ... imaginary part */
  XPPE_AUTO_PARAMS = 6  /* the values of all AUTO parameter slots */
};

XPPE_API int xppe_auto_count(void);
XPPE_API int xppe_auto_point(int index, xppe_auto_point_info *info);
XPPE_API int xppe_auto_point_data(int index, int what, double *out, int n);

#ifdef __cplusplus
}
#endif

#endif
