/*
 * The XPPAUT engine API (see xppe.h).
 *
 * This file is the only bridge between the outside world and XPPAUT's
 * globals. Wherever possible it calls the same upstream functions that
 * XPPAUT's batch mode calls, in the same order, so results stay identical
 * to xppaut -silent.
 */
#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xppe.h"
#include "xppe_internal.h"

#include "browse.h"
#include "xpplim.h"

/* ---- XPPAUT globals and functions used here ---------------------------- */

typedef struct {
  char *name;
  char *does;
  unsigned int use;
} INTERN_SET; /* as declared in load_eqn.c */

extern int NODE, NMarkov, NEQ, NUPAR, METHOD, NJMP, NMESH, MAXSTOR;
extern int EVEC_ITER, POIMAP, POIVAR, POISGN, SOS, DelayFlag, SuppressOut;
extern int N_plist, *plotlist, Nintern_set;
extern double DELTA_T, TEND, T0, TRANS, BOUND, DELAY, TOLER, ATOLER, HMIN,
    HMAX, NEWT_ERR, EVEC_ERR, POIPLN;
extern double MY_XLO, MY_XHI, MY_YLO, MY_YHI;
extern int IXPLT, IYPLT, IZPLT, AXES;
extern double THETA0, PHI0, x_3d[2], y_3d[2], z_3d[2];
extern char uvar_names[MAXODE][12];
extern char upar_names[MAXPAR][11];
extern double last_ic[MAXODE];
extern BROWSER my_browser;
extern INTERN_SET intern_set[];

void do_main(int argc, char **argv);
void batch_integrate_once(void);
void do_intern_set(char *name, char *value);
void extract_internset(int j);
void chk_delay(void);
void redo_stuff(void);
int reallocstor(int ncol, int nrow);
int get_val(char *name, double *value);
void convert(char *source, char *dest);
void do_sing_info(double *x, double eps, double err, double big, int maxit,
                  int n, double *er, double *em, int *ierr);

/* ---- State and error handling ------------------------------------------ */

static int loaded;      /* a model is loaded and usable */
static int broken;      /* the engine aborted; the image must be replaced */
static char *last_log;  /* output of the last call */
static char last_error[512];

void xppe_set_error(const char *fmt, const char *arg) {
  snprintf(last_error, sizeof last_error, fmt, arg ? arg : "");
}

void xppe_begin_call(void) {
  xpps_capture_reset();
  last_error[0] = '\0';
}

void xppe_end_call(void) {
  free(last_log);
  last_log = xpps_capture_take();
}

void xppe_mark_broken(void) {
  broken = 1;
  loaded = 0;
  snprintf(last_error, sizeof last_error,
           "the engine stopped (exit code %d); see the log",
           xpps_guard_code());
}

int xppe_loaded(void) { return loaded; }

int xppe_require_model(void) {
  if (broken) {
    xppe_set_error("the engine stopped earlier; load the model in a new engine",
                   NULL);
    return XPPE_ERR_STATE;
  }
  if (!loaded) {
    xppe_set_error("no model loaded", NULL);
    return XPPE_ERR_STATE;
  }
  return XPPE_OK;
}

int xppe_same_name(const char *a, const char *b) {
  while (*a && *b) {
    if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
    a++;
    b++;
  }
  return *a == *b;
}

/* ---- Version ------------------------------------------------------------ */

int xppe_api_version(void) { return XPPE_API_VERSION; }

const char *xppe_version(void) { return "XPPAUT 8.0 engine (XPP Lab)"; }

const char *xppe_log(void) { return last_log ? last_log : ""; }

const char *xppe_error(void) { return last_error; }

/* ---- Loading ------------------------------------------------------------ */

/* The most informative line of the log after a failed load: the last line
   that looks like an error message, else the last non-empty line. */
static void load_failed(const char *path) {
  static const char *markers[] = {"rror", "llegal", "uplicate", "terminate",
                                  "Can't", "Cant", "nknown", "Problem",
                                  "Must", "missing", "Not ", "not "};
  const char *log = last_log ? last_log : "", *p = log, *best = NULL,
             *last = NULL;
  size_t best_len = 0, last_len = 0;
  while (*p) {
    const char *end = strchr(p, '\n');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    size_t k;
    int blank = 1;
    for (k = 0; k < len; k++)
      if (!isspace((unsigned char)p[k])) blank = 0;
    if (!blank) {
      last = p;
      last_len = len;
      for (k = 0; k < sizeof markers / sizeof markers[0]; k++) {
        size_t m = strlen(markers[k]), j;
        for (j = 0; j + m <= len; j++)
          if (strncmp(p + j, markers[k], m) == 0) break;
        if (j + m <= len) {
          best = p;
          best_len = len;
          break;
        }
      }
    }
    if (!end) break;
    p = end + 1;
  }
  if (!best) {
    best = last;
    best_len = last_len;
  }
  while (best && best_len > 0 && isspace((unsigned char)*best)) {
    best++;
    best_len--;
  }
  while (best && best_len > 0 && isspace((unsigned char)best[best_len - 1]))
    best_len--;
  if (best && best_len > 0)
    snprintf(last_error, sizeof last_error, "could not load %s: %.*s", path,
             (int)(best_len > 300 ? 300 : best_len), best);
  else
    snprintf(last_error, sizeof last_error, "could not load %s", path);
}

int xppe_load(const char *ode_path, int noptions, const char *const *options) {
  char **argv;
  int argc = 0, i;
  FILE *probe;

  xppe_begin_call();
  if (broken || loaded) {
    xppe_set_error("this engine already holds a model; use a new engine", NULL);
    xppe_end_call();
    return XPPE_ERR_STATE;
  }
  if (noptions < 0 || (noptions > 0 && options == NULL)) {
    xppe_set_error("bad option list", NULL);
    xppe_end_call();
    return XPPE_ERR_RANGE;
  }
  /* XPPAUT asks interactively for a new model when the file cannot be
     opened, so check first. */
  probe = fopen(ode_path, "r");
  if (!probe) {
    xppe_set_error("cannot open %s", ode_path);
    xppe_end_call();
    return XPPE_ERR_LOAD;
  }
  fclose(probe);

  argv = calloc((size_t)noptions + 4, sizeof *argv);
  if (!argv) {
    xppe_set_error("out of memory", NULL);
    xppe_end_call();
    return XPPE_ERR_RANGE;
  }
  argv[argc++] = "xppaut";
  argv[argc++] = (char *)ode_path;
  argv[argc++] = "-silent";
  for (i = 0; i < noptions; i++) argv[argc++] = (char *)options[i];

  xpps_embedded = 1;
  {
    /* XPPAUT exits when the model file has an error. Report that as a load
       error that quotes the parser's message. */
    jmp_buf jb;
    if (setjmp(jb)) {
      free(argv);
      broken = 1;
      xppe_end_call();
      load_failed(ode_path);
      return XPPE_ERR_LOAD;
    }
    xpps_guard_set(&jb);
    do_main(argc, argv); /* returns early when embedded; see main.c */
    xpps_guard_set(NULL);
  }
  free(argv);

  if (NODE + NMarkov <= 0) {
    broken = 1;
    xppe_end_call();
    load_failed(ode_path);
    return XPPE_ERR_LOAD;
  }
  loaded = 1;
  xppe_end_call();
  return XPPE_OK;
}

/* ---- Names -------------------------------------------------------------- */

int xppe_count(int list) {
  if (!loaded) return 0;
  switch (list) {
  case XPPE_VARIABLES:
    return NODE + NMarkov;
  case XPPE_PARAMETERS:
    return NUPAR;
  case XPPE_COLUMNS:
    return N_plist > 0 ? N_plist : NEQ + 1;
  case XPPE_SETS:
    return Nintern_set;
  }
  return 0;
}

static const char *column_name(int j) {
  return j == 0 ? "T" : uvar_names[j - 1];
}

const char *xppe_name(int list, int index) {
  if (!loaded || index < 0 || index >= xppe_count(list)) return NULL;
  switch (list) {
  case XPPE_VARIABLES:
    return uvar_names[index];
  case XPPE_PARAMETERS:
    return upar_names[index];
  case XPPE_COLUMNS:
    return column_name(N_plist > 0 ? plotlist[index] : index);
  case XPPE_SETS:
    return intern_set[index].name;
  }
  return NULL;
}

/* ---- Values ------------------------------------------------------------- */

typedef struct {
  const char *name;
  double *d;
  int *i;
} option_ref;

/* Numerical options by their ODE-file names (see set_option in load_eqn.c). */
static const option_ref option_table[] = {
    {"TOTAL", &TEND, NULL},     {"DT", &DELTA_T, NULL},
    {"T0", &T0, NULL},          {"TRANS", &TRANS, NULL},
    {"BOUND", &BOUND, NULL},    {"TOL", &TOLER, NULL},
    {"ATOL", &ATOLER, NULL},    {"DTMIN", &HMIN, NULL},
    {"DTMAX", &HMAX, NULL},     {"DELAY", &DELAY, NULL},
    {"NEWT_TOL", &EVEC_ERR, NULL}, {"JAC_EPS", &NEWT_ERR, NULL},
    {"POIPLN", &POIPLN, NULL},  {"NOUT", NULL, &NJMP},
    {"NJMP", NULL, &NJMP},      {"NMESH", NULL, &NMESH},
    {"MAXSTOR", NULL, &MAXSTOR}, {"METH", NULL, &METHOD},
    {"NEWT_ITER", NULL, &EVEC_ITER}, {"POIMAP", NULL, &POIMAP},
    {"POIVAR", NULL, &POIVAR},  {"POISGN", NULL, &POISGN},
    {"POISTOP", NULL, &SOS},
    /* The plot window from the ODE file (@ xp=, yp=, xlo=, ...). XP and YP
       number the columns: 0 is time, 1 the first variable. */
    {"XLO", &MY_XLO, NULL},     {"XHI", &MY_XHI, NULL},
    {"YLO", &MY_YLO, NULL},     {"YHI", &MY_YHI, NULL},
    {"XP", NULL, &IXPLT},       {"YP", NULL, &IYPLT},
    /* The 3D view (@ zp=, axes=3d, phi=, theta=, xmin=, ..., zmax=).
       AXES is 5 for a 3D plot. */
    {"ZP", NULL, &IZPLT},       {"AXES", NULL, &AXES},
    {"THETA", &THETA0, NULL},   {"PHI", &PHI0, NULL},
    /* the same, never shadowed by a parameter called phi or theta */
    {"VIEW_THETA", &THETA0, NULL}, {"VIEW_PHI", &PHI0, NULL},
    {"XMIN", &x_3d[0], NULL},   {"XMAX", &x_3d[1], NULL},
    {"YMIN", &y_3d[0], NULL},   {"YMAX", &y_3d[1], NULL},
    {"ZMIN", &z_3d[0], NULL},   {"ZMAX", &z_3d[1], NULL},
};

int xppe_get(const char *name, double *value) {
  int i, rc;
  char buf[64], upper[64];
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  for (i = 0; i < NODE + NMarkov; i++)
    if (xppe_same_name(name, uvar_names[i])) {
      *value = last_ic[i];
      return XPPE_OK;
    }
  for (i = 0; i < NUPAR; i++)
    if (xppe_same_name(name, upar_names[i])) {
      snprintf(buf, sizeof buf, "%s", upar_names[i]);
      convert(buf, upper);
      if (get_val(upper, value)) return XPPE_OK;
    }
  for (i = 0; i < (int)(sizeof option_table / sizeof option_table[0]); i++)
    if (xppe_same_name(name, option_table[i].name)) {
      *value = option_table[i].d ? *option_table[i].d
                                 : (double)*option_table[i].i;
      return XPPE_OK;
    }
  xppe_set_error("unknown name %s", name);
  return XPPE_ERR_NAME;
}

int xppe_set(const char *name, const char *value) {
  char n[256], v[256];
  int rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (strlen(name) >= sizeof n || strlen(value) >= sizeof v) {
    xppe_set_error("name or value too long", NULL);
    return XPPE_ERR_RANGE;
  }
  strcpy(n, name);
  strcpy(v, value);
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  /* XPPAUT's POIMAP option knows only section/max/period; let 0, "off" or
     "none" switch Poincare maps off again. */
  if (xppe_same_name(n, "POIMAP") &&
      (v[0] == '0' || v[0] == 'o' || v[0] == 'O' || v[0] == 'n' || v[0] == 'N'))
    POIMAP = 0;
  /* MAXSTOR is the size of the storage already allocated: XPPAUT reads it
     before allocating, and later only changes it together with the
     storage (stor_full). Set here alone, a larger value let the next run
     write past the end of the arrays. Grow the storage with it. */
  else if (xppe_same_name(n, "MAXSTOR")) {
    int rows = atoi(v);
    if (rows > MAXSTOR && reallocstor(NEQ + 1, rows))
      MAXSTOR = rows;
    else if (rows > 0 && rows <= MAXSTOR)
      MAXSTOR = rows; /* within what is allocated: safe */
  } else
    do_intern_set(n, v);
  redo_stuff();
  XPPE_GUARD_END;
  xppe_end_call();
  return XPPE_OK;
}

int xppe_set_number(const char *name, double value) {
  char v[64];
  snprintf(v, sizeof v, "%.17g", value);
  return xppe_set(name, v);
}

int xppe_apply_set(const char *set_name) {
  int i, rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  for (i = 0; i < Nintern_set; i++)
    if (xppe_same_name(set_name, intern_set[i].name)) {
      xppe_begin_call();
      XPPE_GUARD_BEGIN((void)0);
      extract_internset(i);
      chk_delay();
      redo_stuff();
      XPPE_GUARD_END;
      xppe_end_call();
      return XPPE_OK;
    }
  xppe_set_error("no set named %s", set_name);
  return XPPE_ERR_NAME;
}

/* ---- Integration ---------------------------------------------------------- */

int xppe_run(void) {
  int rc, old_suppress;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  xppe_begin_call();
  old_suppress = SuppressOut;
  SuppressOut = 1; /* keep the data in memory; write no output file */
  XPPE_GUARD_BEGIN((void)0);
  batch_integrate_once();
  XPPE_GUARD_END;
  SuppressOut = old_suppress;
  xppe_end_call();
  return XPPE_OK;
}

int xppe_rows(void) {
  if (!loaded) return 0;
  return my_browser.iend - my_browser.istart;
}

int xppe_column(int col, double *out, int n) {
  int j, i, rows;
  if (!loaded || col < -1 || col >= xppe_count(XPPE_COLUMNS)) {
    xppe_set_error("no such column", NULL);
    return XPPE_ERR_RANGE;
  }
  /* -1 is time, even when an "only" statement leaves it out of the output. */
  j = col < 0 ? 0 : N_plist > 0 ? plotlist[col] : col;
  rows = xppe_rows();
  if (n > rows) n = rows;
  for (i = 0; i < n; i++)
    out[i] = (double)my_browser.data[j][my_browser.istart + i];
  return n;
}

/* Initial conds > Last: the end of the last run, in double precision,
   becomes the initial condition. */
int xppe_ic_last(void) {
  extern double MyData[];
  extern int INFLAG;
  void get_ic(int it, double *x);
  int rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (INFLAG == 0) {
    xppe_set_error("No prior solution", NULL);
    return XPPE_ERR_STATE;
  }
  get_ic(0, MyData);
  return XPPE_OK;
}

/* ---- Equilibria ------------------------------------------------------------ */

int xppe_equilibrium(double *x, double *re, double *im) {
  int rc, ierr = 0;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  do_sing_info(x, NEWT_ERR, EVEC_ERR, BOUND, EVEC_ITER, NODE, re, im, &ierr);
  XPPE_GUARD_END;
  xppe_end_call();
  if (ierr != 0) {
    xppe_set_error("Newton's method did not converge", NULL);
    return XPPE_ERR_NUMERIC;
  }
  return XPPE_OK;
}
