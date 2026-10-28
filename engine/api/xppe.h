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

#define XPPE_API_VERSION 11

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
   METH, DELAY, NEWT_TOL, NEWT_ITER, JAC_EPS, ...) or plot setting from the
   ODE file (XLO, XHI, YLO, YHI, XP, YP; for 3D also ZP, AXES (5 = 3D),
   VIEW_THETA, VIEW_PHI, XMIN ... ZMAX). Names ignore case. */
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
/* Start the next run where the last one ended (Initial conds > Last): the
   final state, in double precision, becomes the initial condition. */
XPPE_API int xppe_ic_last(void);
/* Initial conds > Shift: as xppe_ic_last(), and T0 becomes the time the
   last run ended, so that the time goes on (it matters for models that
   depend on t). Since API version 5. */
XPPE_API int xppe_ic_shift(void);
/* Initial conds > File: read the initial conditions from a text file, the
   first numbers in it, one per ODE variable in order (as XPPAUT reads
   them; commas and semicolons may separate them too). Returns the number
   of values read; a file with fewer numbers than variables changes nothing
   and gives XPPE_ERR_RANGE, one that cannot be opened XPPE_ERR_LOAD. Since
   API version 5. */
XPPE_API int xppe_ic_file(const char *path);
/* Initial conds > formUla: give a variable ("V") or a range of them in
   XPPAUT's array form ("u[1..20]") the value of a formula in the model's
   parameters and functions. In the array form, [j] in the formula stands
   for the index ([j+1] and [j-1] work too), so "u[1..20]" with
   "sin(2*pi*[j]/20)" sets u1 ... u20. Returns the number of values set; a
   name that is not a variable, or a formula XPPAUT cannot evaluate,
   changes nothing. Since API version 5. */
XPPE_API int xppe_ic_formula(const char *target, const char *formula);

/* Numerics > stocHastic > Compute: run the model `trials` times (at least
   2) from the same initial conditions, the random numbers of wiener and
   ran() going on from run to run, and keep the mean and the variance of
   every output column at each time, as XPPAUT's batch mode computes them
   with @ stoch=1 (in single precision). Returns the number of rows; the
   last run stays as the current run. It uses XPPAUT's range options
   (RANGEOVER, RANGESTEP, ...), which keep the values it gives them. Since
   API version 6. */
XPPE_API int xppe_ensemble(int trials);
/* Column col of the last ensemble, numbered as for xppe_column(): its mean
   (which = 0) or its variance (which = 1). Since API version 6. */
XPPE_API int xppe_ensemble_column(int col, int which, double *out, int n);

/* Newton's method for an equilibrium starting from x (length = number of
   ODE variables). On success x holds the equilibrium and re/im the
   eigenvalues of the Jacobian there. */
XPPE_API int xppe_equilibrium(double *x, double *re, double *im);

/* The right-hand side of the ODEs at time t and state x (one value per
   ODE variable). For maps it is the next state. */
XPPE_API int xppe_rhs(double t, const double *x, double *dxdt);

/* The output columns (as xppe_name(XPPE_COLUMNS, i) names them: T, the
   variables, the auxiliaries) at time t and state x (one value per ODE
   variable; Markov variables take their initial values), computed as
   XPPAUT computes a stored row. out holds n >= xppe_count(XPPE_COLUMNS)
   values. Returns the number of columns written. Since API version 8. */
XPPE_API int xppe_columns_at(double t, const double *x, double *out, int n);

/* Global flags, the model's "global sign condition {events}" lines, in
   their order. xppe_flag_condition() gives flag i's condition as written
   (NULL if there is no such flag) and its sign: 1 when the condition
   crosses zero going up, -1 going down, 0 either way. xppe_halt_flag()
   tells which flag stopped the last xppe_run() or xppe_manifold() run by a
   halting event ({arret=1}): its number counted from 1, or 0 when the run
   was not stopped by a flag. Since API version 8. */
XPPE_API int xppe_flag_count(void);
XPPE_API const char *xppe_flag_condition(int i, int *sign);
XPPE_API int xppe_halt_flag(void);

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

/* Equilibria as a parameter changes (XPPAUT's Sing pts > Range): for each
   of the n values in turn, the parameter takes it and Newton's method
   starts from the last equilibrium found (the first time from guess, one
   value per ODE variable). Result table: the parameter, each variable,
   then "unstable" and "stable", the numbers of eigenvalues with positive
   and negative real parts (for maps, outside and inside the unit circle).
   A row where Newton's method failed holds NaN after the parameter. The
   parameter is restored afterwards. Since API version 3. */
XPPE_API int xppe_equilibrium_sweep(const char *parameter, const double *values,
                                    int n, const double *guess);
/* Root locus over a parameter: the same steps as xppe_equilibrium_sweep()
   for steps values evenly from from to to (the last exactly to), keeping
   the eigenvalues at each equilibrium. Each step's eigenvalues are put in
   the order that follows on from the last good step (the closest pairs
   first), so that a column follows one eigenvalue. Result table: the
   parameter, each variable, then re1, im1, re2, im2, ...; NaN after the
   parameter where Newton's method failed. The parameter is restored
   afterwards. Since API version 11. */
XPPE_API int xppe_parameter_locus(const char *parameter, double from, double to,
                                  int steps, const double *guess);

/* Every equilibrium Newton's method finds from `guesses` random starting
   points in a box (XPPAUT's Sing pts > Monte Carlo): lo and hi give the
   box's edges, one value per ODE variable. Two equilibria closer than
   `tolerance` (the sum of the absolute differences) count as one. Returns
   how many were found, at most 400; xppe_found_equilibrium(i, ...) gives
   equilibrium i and the eigenvalues there. The random numbers come from
   XPPAUT's generator, so the same calls find the same equilibria. Since
   API version 3. */
XPPE_API int xppe_find_equilibria(int guesses, double tolerance,
                                  const double *lo, const double *hi);
XPPE_API int xppe_found_equilibrium(int i, double *x, double *re, double *im);

/* ---- Linearisation and control ------------------------------------------
 *
 * The linear system dx' = A dx + B du around a point of the model, for
 * control design. Matrices are row-major. Since API version 7.
 */

/* Linearise the right-hand side at the state x (one value per variable,
   n = xppe_count(XPPE_VARIABLES), at t = 0; Markov variables get zero rows
   and columns). A (n x n) receives the Jacobian, B (n x ninputs) the
   derivatives with respect to the parameters named in inputs, one column
   each. They are central differences extrapolated to zero step (Ridders'
   method, from three starting steps, each entry keeping the estimate with
   the smallest error), so no step has to be chosen; A_error and B_error
   (either may be NULL) receive those error estimates. step > 0 takes a
   single central difference with that relative step instead, and the
   errors are NaN. For a map, A and B are the derivatives of the map. The
   parameters keep their values. */
XPPE_API int xppe_linearize(const double *x, int ninputs,
                            const char *const *inputs, double step, double *A,
                            double *B, double *A_error, double *B_error);
/* The eigenvalues of the n x n matrix a, by XPPAUT's own routine (EISPACK's
   orthes and hqr, as Sing pts uses), in the order it finds them. */
XPPE_API int xppe_eigenvalues(int n, const double *a, double *re, double *im);
/* The eigenvector of the n x n matrix a for its real eigenvalue lambda, by
   XPPAUT's inverse iteration (get_evec in gear.c, which Sing pts uses to
   draw invariant sets), scaled to unit length. Its sign is whatever the
   iteration gives. Since API version 8. */
XPPE_API int xppe_eigenvector(int n, const double *a, double lambda, double *v);
/* The controllability matrix C = [B, AB, ..., A^(n-1) B] of n states and m
   inputs (A n x n, B n x m): C is n x (n m). sv receives its n singular
   values, largest first. Returns the rank: the number of singular values
   above tol, or with tol <= 0 above max(n, n m) * 1e-8 times the largest
   (the derivatives are good to about 1e-10). With m = 0 the rank is 0 and
   nothing is written. */
XPPE_API int xppe_controllability(int n, int m, const double *A,
                                  const double *B, double tol, double *C,
                                  double *sv);
/* The feedback gain K (m x n) for which A - B K has the n poles re + i im
   (complex ones in conjugate pairs; for a map, inside the unit circle for
   a stable loop). With one input it is Ackermann's formula, the only gain
   there is; with several, the method of Kautsky, Nichols and Van Dooren,
   which spends the spare inputs on well-conditioned eigenvectors (a pole
   may then be repeated up to m times). Refuses, with XPPE_ERR_NUMERIC, a
   pair (A, B) that is not controllable by xppe_controllability()'s rank.
   Since API version 10. */
XPPE_API int xppe_place_poles(int n, int m, const double *A, const double *B,
                              const double *re, const double *im, double *K);
/* The closed-loop model: the .ode text source with each of the m inputs
   taken off its parameter line (p, par, param or parameters) and defined
   instead as the fixed variable u = u0 - K (x - x*), where x are the n
   states (their names in states) and x* the point. The laws use only the
   states, so they stand first, after the comments that open the file, and
   every fixed variable that uses an input sees them; a law longer than
   XPPAUT reads on one line is written as partial sums (fixed variables
   named kf1a1, kf1a2, ..., with x's added to kf if the model has such
   names). The rest of the text is kept. Writes at most size bytes of the text, ending
   with a NUL, to out (size 0 writes nothing) and returns the text's full
   length, like snprintf; or an error: XPPE_ERR_NAME for an input not on a
   parameter line, XPPE_ERR_RANGE for one on a line continued with a
   backslash. Since API version 10. */
XPPE_API int xppe_closed_loop(const char *source, int m,
                              const char *const *inputs, const double *u0,
                              int n, const char *const *states,
                              const double *x, const double *K, char *out,
                              int size);
/* Root locus over a gain: the eigenvalues of A - g B K (K m x n) for steps
   gains g evenly from from to to (the last exactly to), written to gains
   (may be NULL) and to re and im, steps x n each, row by row. Each row is
   put in the order that follows on from the row before (the closest pairs
   first), so that a column follows one eigenvalue. Since API version 11. */
XPPE_API int xppe_gain_locus(int n, int m, const double *A, const double *B,
                             const double *K, double from, double to, int steps,
                             double *gains, double *re, double *im);

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
/* The largest Liapunov exponent along the current run (Liapunov). XPPAUT
   computes it from the stored run, which is in single precision, times
   included, so it goes wrong in long runs (Lorenz to t = 1000), for large
   or folded variables and with some methods; xppe_lyapunov_integrate()
   does not. */
XPPE_API int xppe_lyapunov(double *exponent);
/* The largest Liapunov exponent of a run from the initial conditions,
   computed during the integration in double precision by XPPAUT's method:
   a copy perturbed by eps (JAC_EPS when eps <= 0) is stepped along with the
   state by the current method, and after each output step (DT times NOUT)
   the log of its growth is added up and the copy put back at the distance
   eps. Steps before TRANS do not count; the integration stops where a run
   would stop, when a variable passes BOUND. The last run is left as it
   was. Refuses delay equations, Volterra integrals and noise (wiener,
   Markov variables). Since API version 9. */
XPPE_API int xppe_lyapunov_integrate(double eps, double *exponent);

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

/* ---- Boundary-value problems ------------------------------------------------
 *
 * For models with "bndry" statements (the number is xppe_bvp_count()),
 * XPPAUT's shooting method adjusts the initial conditions so that the
 * solution over [T0, T0 + TOTAL] satisfies the boundary conditions
 * (Bndryval > No show). On success the initial conditions hold the answer
 * and the solution is the current run. max_iterations, tolerance and
 * epsilon replace BVP_MAXIT, BVP_TOL and BVP_EPS when positive.
 */
XPPE_API int xppe_bvp_count(void);
XPPE_API int xppe_bvp(int max_iterations, double tolerance, double epsilon);

/* ---- The animator -----------------------------------------------------------
 *
 * XPPAUT's .ani files, parsed by XPPAUT. xppe_ani_frame(row) evaluates every
 * command at that row of the current run (row 0 also evaluates the
 * "permanent" ones) and returns the number of shapes, read with
 * xppe_ani_get(). Coordinates are in the animation's window, from
 * xppe_ani_window() ("dimension"; 0..1 by default), with y upwards.
 */
enum {
  XPPE_ANI_LINE = 0,    /* (x1,y1)-(x2,y2) */
  XPPE_ANI_RECT = 1,    /* corners (x1,y1) and (x2,y2) */
  XPPE_ANI_ELLIPSE = 2, /* centre (x1,y1), radii x2 and y2 (circles too) */
  XPPE_ANI_TEXT = 3,    /* text at (x1,y1) */
  XPPE_ANI_COMET = 4    /* the newest point (x1,y1) of a trail of `trail` */
};

typedef struct {
  int shape;     /* XPPE_ANI_* */
  int command;   /* index of the command in the file; comets keep trails by it */
  int filled;
  double x1, y1, x2, y2;
  int color;     /* XPPAUT colour: 0 black, 20-29 red ... purple; -1: see level */
  double level;  /* for computed colours: 0..1 along XPPAUT's colour scale */
  int thickness; /* line width; for comets below 0, dots of that radius */
  int trail;
  int font_size; /* 0-4 */
  int serif;
  char text[128];
} xppe_ani_item;

XPPE_API int xppe_ani_load(const char *path); /* returns the command count */
XPPE_API int xppe_ani_window(double *xlo, double *ylo, double *xhi,
                             double *yhi);
XPPE_API int xppe_ani_frame(int row);
XPPE_API int xppe_ani_get(int i, xppe_ani_item *out);

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
/* Like xppe_auto_run, and answers the dialog some starts open (homoclinic and
   heteroclinic: "Homoclinic info") with `form`, "label=value;label=value",
   for example "dim unstable=1;dim stable=1;X_L=0.14;Y_L=0". */
XPPE_API int xppe_auto_run_form(const char *choice, const char *form);
/* Select the point with this label as the start of the next run. */
XPPE_API int xppe_auto_grab(int label);
XPPE_API int xppe_auto_ungrab(void);

/* XPPAUT's AUTO > File > Save diagram and Load diagram: an .auto file holds
   AUTO's settings, the diagram and AUTO's restart data, so a loaded
   diagram's labelled points can be grabbed and continued. Loading replaces
   the diagram. Files open in XPPAUT both ways. Since API version 4. */
XPPE_API int xppe_auto_save(const char *path);
XPPE_API int xppe_auto_load(const char *path);
/* Import orbit: the periodic orbit at a labelled point, one period from
   AUTO's restart data, becomes the current run (read it with
   xppe_column()). Since API version 4. */
XPPE_API int xppe_auto_orbit(int label);

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
