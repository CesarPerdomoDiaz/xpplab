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
#include "xpps_embed_internal.h"

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
int get_val(char *name, double *value);
void convert(char *source, char *dest);
void do_sing_info(double *x, double eps, double err, double big, int maxit,
                  int n, double *er, double *em, int *ierr);

/* ---- State and error handling ------------------------------------------ */

static int loaded;      /* a model is loaded and usable */
static int broken;      /* the engine aborted; the image must be replaced */
static char *last_log;  /* output of the last call */
static char last_error[512];

static void set_error(const char *fmt, const char *arg) {
  snprintf(last_error, sizeof last_error, fmt, arg ? arg : "");
}

static void begin_call(void) {
  xpps_capture_reset();
  last_error[0] = '\0';
}

static void end_call(void) {
  free(last_log);
  last_log = xpps_capture_take();
}

/* Run the statements in a guarded region: if the engine calls exit(), the
   region is abandoned and the call fails with XPPE_ERR_FATAL. */
#define GUARD_BEGIN                                                    \
  do {                                                                 \
    jmp_buf guard_jb;                                                  \
    if (setjmp(guard_jb)) {                                            \
      broken = 1;                                                      \
      loaded = 0;                                                      \
      snprintf(last_error, sizeof last_error,                          \
               "the engine stopped (exit code %d); see the log",       \
               xpps_guard_code());                                     \
      end_call();                                                      \
      return XPPE_ERR_FATAL;                                           \
    }                                                                  \
    xpps_guard_set(&guard_jb);

#define GUARD_END                                                      \
    xpps_guard_set(NULL);                                              \
  } while (0)

static int require_model(void) {
  if (broken) {
    set_error("the engine stopped earlier; load the model in a new engine",
              NULL);
    return XPPE_ERR_STATE;
  }
  if (!loaded) {
    set_error("no model loaded", NULL);
    return XPPE_ERR_STATE;
  }
  return XPPE_OK;
}

static int same_name(const char *a, const char *b) {
  while (*a && *b) {
    if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
    a++;
    b++;
  }
  return *a == *b;
}

/* ---- Version ------------------------------------------------------------ */

int xppe_api_version(void) { return XPPE_API_VERSION; }

const char *xppe_version(void) { return "XPPAUT 8.0 engine (XPP Studio)"; }

const char *xppe_log(void) { return last_log ? last_log : ""; }

const char *xppe_error(void) { return last_error; }

/* ---- Loading ------------------------------------------------------------ */

int xppe_load(const char *ode_path, int noptions, const char *const *options) {
  char **argv;
  int argc = 0, i;
  FILE *probe;

  begin_call();
  if (broken || loaded) {
    set_error("this engine already holds a model; use a new engine", NULL);
    end_call();
    return XPPE_ERR_STATE;
  }
  if (noptions < 0 || (noptions > 0 && options == NULL)) {
    set_error("bad option list", NULL);
    end_call();
    return XPPE_ERR_RANGE;
  }
  /* XPPAUT asks interactively for a new model when the file cannot be
     opened, so check first. */
  probe = fopen(ode_path, "r");
  if (!probe) {
    set_error("cannot open %s", ode_path);
    end_call();
    return XPPE_ERR_LOAD;
  }
  fclose(probe);

  argv = calloc((size_t)noptions + 4, sizeof *argv);
  if (!argv) {
    set_error("out of memory", NULL);
    end_call();
    return XPPE_ERR_RANGE;
  }
  argv[argc++] = "xppaut";
  argv[argc++] = (char *)ode_path;
  argv[argc++] = "-silent";
  for (i = 0; i < noptions; i++) argv[argc++] = (char *)options[i];

  xpps_embedded = 1;
  GUARD_BEGIN;
  do_main(argc, argv); /* returns early when embedded; see main.c */
  GUARD_END;
  free(argv);

  if (NODE + NMarkov <= 0) {
    broken = 1;
    set_error("could not load %s; see the log", ode_path);
    end_call();
    return XPPE_ERR_LOAD;
  }
  loaded = 1;
  end_call();
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
};

int xppe_get(const char *name, double *value) {
  int i, rc;
  char buf[64], upper[64];
  if ((rc = require_model()) != XPPE_OK) return rc;
  for (i = 0; i < NODE + NMarkov; i++)
    if (same_name(name, uvar_names[i])) {
      *value = last_ic[i];
      return XPPE_OK;
    }
  for (i = 0; i < NUPAR; i++)
    if (same_name(name, upar_names[i])) {
      snprintf(buf, sizeof buf, "%s", upar_names[i]);
      convert(buf, upper);
      if (get_val(upper, value)) return XPPE_OK;
    }
  for (i = 0; i < (int)(sizeof option_table / sizeof option_table[0]); i++)
    if (same_name(name, option_table[i].name)) {
      *value = option_table[i].d ? *option_table[i].d
                                 : (double)*option_table[i].i;
      return XPPE_OK;
    }
  set_error("unknown name %s", name);
  return XPPE_ERR_NAME;
}

int xppe_set(const char *name, const char *value) {
  char n[256], v[256];
  int rc;
  if ((rc = require_model()) != XPPE_OK) return rc;
  if (strlen(name) >= sizeof n || strlen(value) >= sizeof v) {
    set_error("name or value too long", NULL);
    return XPPE_ERR_RANGE;
  }
  strcpy(n, name);
  strcpy(v, value);
  begin_call();
  GUARD_BEGIN;
  do_intern_set(n, v);
  redo_stuff();
  GUARD_END;
  end_call();
  return XPPE_OK;
}

int xppe_set_number(const char *name, double value) {
  char v[64];
  snprintf(v, sizeof v, "%.17g", value);
  return xppe_set(name, v);
}

int xppe_apply_set(const char *set_name) {
  int i, rc;
  if ((rc = require_model()) != XPPE_OK) return rc;
  for (i = 0; i < Nintern_set; i++)
    if (same_name(set_name, intern_set[i].name)) {
      begin_call();
      GUARD_BEGIN;
      extract_internset(i);
      chk_delay();
      redo_stuff();
      GUARD_END;
      end_call();
      return XPPE_OK;
    }
  set_error("no set named %s", set_name);
  return XPPE_ERR_NAME;
}

/* ---- Integration ---------------------------------------------------------- */

int xppe_run(void) {
  int rc, old_suppress;
  if ((rc = require_model()) != XPPE_OK) return rc;
  begin_call();
  old_suppress = SuppressOut;
  SuppressOut = 1; /* keep the data in memory; write no output file */
  GUARD_BEGIN;
  batch_integrate_once();
  GUARD_END;
  SuppressOut = old_suppress;
  end_call();
  return XPPE_OK;
}

int xppe_rows(void) {
  if (!loaded) return 0;
  return my_browser.iend - my_browser.istart;
}

int xppe_column(int col, double *out, int n) {
  int j, i, rows;
  if (!loaded || col < 0 || col >= xppe_count(XPPE_COLUMNS)) {
    set_error("no such column", NULL);
    return XPPE_ERR_RANGE;
  }
  j = N_plist > 0 ? plotlist[col] : col;
  rows = xppe_rows();
  if (n > rows) n = rows;
  for (i = 0; i < n; i++)
    out[i] = (double)my_browser.data[j][my_browser.istart + i];
  return n;
}

/* ---- Equilibria ------------------------------------------------------------ */

int xppe_equilibrium(double *x, double *re, double *im) {
  int rc, ierr = 0;
  if ((rc = require_model()) != XPPE_OK) return rc;
  begin_call();
  GUARD_BEGIN;
  do_sing_info(x, NEWT_ERR, EVEC_ERR, BOUND, EVEC_ITER, NODE, re, im, &ierr);
  GUARD_END;
  end_call();
  if (ierr != 0) {
    set_error("Newton's method did not converge", NULL);
    return XPPE_ERR_NUMERIC;
  }
  return XPPE_OK;
}
