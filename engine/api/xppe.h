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

#define XPPE_API_VERSION 1

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
   Returns the number of values copied. */
XPPE_API int xppe_column(int col, double *out, int n);

/* Newton's method for an equilibrium starting from x (length = number of
   ODE variables). On success x holds the equilibrium and re/im the
   eigenvalues of the Jacobian there. */
XPPE_API int xppe_equilibrium(double *x, double *re, double *im);

#ifdef __cplusplus
}
#endif

#endif
