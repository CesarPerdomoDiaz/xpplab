/*
 * Headless AUTO continuation for the engine API (see xppe.h).
 *
 * XPPAUT drives AUTO from its AUTO window: you pick the parameters and the
 * numerics in dialogs, press Run, answer a menu ("Periodic", "Two Param",
 * ...), and grab labelled points with the mouse. This file does the same
 * through function calls. It sets the same state the dialogs set and calls
 * the same upstream routines (auto_run and friends in auto_nox.c), passing
 * the menu answer through the embedding layer's answer queue. The
 * continuation therefore runs exactly as it would in XPPAUT.
 */
#include <ctype.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xppe.h"
#include "xppe_internal.h"
#include "xpps_embed_internal.h"

#include "auto_nox.h"
#include "xpplim.h"

#define PARAM_BOX 1 /* as in auto_nox.c */

extern BIFUR Auto;
extern ADVAUTO aauto;
extern GRABPT grabpt;
extern DIAGRAM *bifd;
extern int AutoPar[8], NAutoPar, Auto_index_to_array[8];
extern int SuppressBP, DiagFlag, NBifs, NODE, NUPAR;
extern double constants[];
extern char upar_names[MAXPAR][11];

void auto_run(void);
int yes_reset_auto(void);
int get_param_index(char *name);
int find_user_name(int type, char *name);
void set_ivar(int i, double value);
void get_ic(int it, double *x);
void evaluate_derived(void);
void redo_all_fun_tables(void);

static char auto_workdir[1024];

/* ---- Numerical settings (the AutoNum dialog) ----------------------------- */

typedef struct {
  const char *name;
  double *d;
  int *i;
} auto_option;

static const auto_option auto_options[] = {
    {"NTST", NULL, &Auto.ntst},   {"NMAX", NULL, &Auto.nmx},
    {"NPR", NULL, &Auto.npr},     {"DS", &Auto.ds, NULL},
    {"DSMIN", &Auto.dsmin, NULL}, {"DSMAX", &Auto.dsmax, NULL},
    {"PARMIN", &Auto.rl0, NULL},  {"PARMAX", &Auto.rl1, NULL},
    {"NORMMIN", &Auto.a0, NULL},  {"NORMMAX", &Auto.a1, NULL},
    {"NCOL", NULL, &Auto.ncol},   {"EPSL", &Auto.epsl, NULL},
    {"EPSU", &Auto.epsu, NULL},   {"EPSS", &Auto.epss, NULL},
    {"IAD", NULL, &aauto.iad},    {"MXBF", NULL, &aauto.mxbf},
    {"IID", NULL, &aauto.iid},    {"ITMX", NULL, &aauto.itmx},
    {"ITNW", NULL, &aauto.itnw},  {"NWTN", NULL, &aauto.nwtn},
    {"IADS", NULL, &aauto.iads},  {"SUPPBP", NULL, &SuppressBP},
};
#define N_AUTO_OPTIONS ((int)(sizeof auto_options / sizeof auto_options[0]))

static const auto_option *find_auto_option(const char *name) {
  int i;
  for (i = 0; i < N_AUTO_OPTIONS; i++)
    if (xppe_same_name(name, auto_options[i].name)) return &auto_options[i];
  return NULL;
}

int xppe_auto_get(const char *name, double *value) {
  const auto_option *o;
  int rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (!(o = find_auto_option(name))) {
    xppe_set_error("unknown AUTO setting %s", name);
    return XPPE_ERR_NAME;
  }
  *value = o->d ? *o->d : (double)*o->i;
  return XPPE_OK;
}

int xppe_auto_set(const char *name, double value) {
  const auto_option *o;
  int rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (!(o = find_auto_option(name))) {
    xppe_set_error("unknown AUTO setting %s", name);
    return XPPE_ERR_NAME;
  }
  if (o->d)
    *o->d = value;
  else
    *o->i = (int)value;
  return XPPE_OK;
}

/* ---- Continuation parameters (the Parameter and Axes dialogs) ----------- */

/* Return the AUTO slot holding XPP parameter `name`, placing it in a free
   slot (one that is neither the main nor the secondary parameter) if it is
   not one of AUTO's parameters yet. */
static int auto_slot_for(const char *name, int keep1, int keep2) {
  char buf[64];
  int idx, i;
  snprintf(buf, sizeof buf, "%s", name);
  idx = find_user_name(PARAM_BOX, buf);
  if (idx < 0) return -1;
  for (i = 0; i < NAutoPar; i++)
    if (AutoPar[i] == idx) return i;
  for (i = NAutoPar - 1; i >= 0; i--)
    if (i != keep1 && i != keep2) {
      AutoPar[i] = idx;
      Auto_index_to_array[i] = get_param_index(upar_names[idx]);
      return i;
    }
  return -1;
}

int xppe_auto_parameters(const char *main_par, const char *second_par) {
  int rc, s1, s2;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (NAutoPar <= 0) {
    xppe_set_error("the model has no parameters", NULL);
    return XPPE_ERR_STATE;
  }
  s1 = auto_slot_for(main_par, -1, second_par ? Auto.icp2 : -1);
  if (s1 < 0) {
    xppe_set_error("no parameter named %s", main_par);
    return XPPE_ERR_NAME;
  }
  Auto.icp1 = s1;
  if (second_par && second_par[0]) {
    s2 = auto_slot_for(second_par, s1, -1);
    if (s2 < 0) {
      xppe_set_error("no parameter named %s", second_par);
      return XPPE_ERR_NAME;
    }
    Auto.icp2 = s2;
  }
  return XPPE_OK;
}

int xppe_auto_set_workdir(const char *dir) {
  if (strlen(dir) >= sizeof auto_workdir) {
    xppe_set_error("directory name too long", NULL);
    return XPPE_ERR_RANGE;
  }
  strcpy(auto_workdir, dir);
  return XPPE_OK;
}

/* XPPAUT keeps AUTO's scratch and result files (fort.*, model.b/.d/.s) in
   $HOME. Point HOME at the work directory for the duration of a run. */
static char saved_home[1024];
static int had_home;

static void set_home(const char *dir) {
#ifdef _WIN32
  _putenv_s("HOME", dir);
#else
  setenv("HOME", dir, 1);
#endif
}

static void push_workdir(void) {
  const char *h;
  if (!auto_workdir[0]) return;
  h = getenv("HOME");
  had_home = h != NULL;
  if (h) snprintf(saved_home, sizeof saved_home, "%s", h);
  set_home(auto_workdir);
}

static void pop_workdir(void) {
  if (!auto_workdir[0]) return;
  if (had_home) {
    set_home(saved_home);
  } else {
#ifdef _WIN32
    _putenv_s("HOME", "");
#else
    unsetenv("HOME");
#endif
  }
}

/* ---- Running ------------------------------------------------------------ */

int xppe_auto_reset(void) {
  int rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  xppe_begin_call();
  push_workdir();
  XPPE_GUARD_BEGIN(pop_workdir());
  yes_reset_auto();
  XPPE_GUARD_END;
  pop_workdir();
  xppe_end_call();
  return XPPE_OK;
}

int xppe_auto_run(const char *choice) {
  int rc, before;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  xppe_begin_call();
  xpps_answers_clear();
  if (choice && choice[0]) xpps_answers_push(choice);
  before = xppe_auto_count();
  push_workdir();
  XPPE_GUARD_BEGIN(pop_workdir());
  auto_run();
  XPPE_GUARD_END;
  pop_workdir();
  xpps_answers_clear();
  xppe_end_call();
  if (xppe_auto_count() == before) {
    xppe_set_error("AUTO computed no points; see the log", NULL);
    return XPPE_ERR_NUMERIC;
  }
  return XPPE_OK;
}

/* ---- The diagram -------------------------------------------------------- */

static DIAGRAM *nth_point(int i) {
  DIAGRAM *d = bifd;
  if (!DiagFlag || i < 0) return NULL;
  while (d && i > 0) {
    d = d->next;
    i--;
  }
  return d;
}

int xppe_auto_count(void) {
  DIAGRAM *d;
  int n = 0;
  if (!xppe_loaded() || !DiagFlag) return 0;
  for (d = bifd; d; d = d->next) n++;
  return n;
}

static const char *type_name(int itp) {
  switch (itp % 10) {
  case 1:
  case 6:
    return "BP";
  case 2:
  case 5:
    return "LP";
  case 3:
    return "HB";
  case -4:
    return "UZ";
  case 7:
    return "PD";
  case 8:
    return "TR";
  case 9:
    return "EP";
  case -9:
    return "MX";
  }
  return "";
}

int xppe_auto_point(int i, xppe_auto_point_info *info) {
  DIAGRAM *d = nth_point(i);
  if (!d) {
    xppe_set_error("no such diagram point", NULL);
    return XPPE_ERR_RANGE;
  }
  memset(info, 0, sizeof *info);
  info->branch = d->ibr;
  info->point = d->ntot;
  info->type = d->itp;
  info->label = d->lab;
  info->stable = d->ntot < 0;   /* AUTO's sign convention */
  info->periodic = d->ibr < 0;  /* XPPAUT's convention */
  info->two_parameter = d->flag2;
  info->par1_slot = d->icp1;
  info->par2_slot = d->icp2;
  info->par1 = d->par[d->icp1];
  info->par2 = d->icp2 < NAutoPar ? d->par[d->icp2] : 0.0;
  info->norm = d->norm;
  info->period = d->per;
  snprintf(info->type_name, sizeof info->type_name, "%s", type_name(d->itp));
  return XPPE_OK;
}

int xppe_auto_point_data(int i, int what, double *out, int n) {
  DIAGRAM *d = nth_point(i);
  const double *src = NULL;
  int k, len = NODE;
  if (!d) {
    xppe_set_error("no such diagram point", NULL);
    return XPPE_ERR_RANGE;
  }
  switch (what) {
  case XPPE_AUTO_U0: src = d->u0; break;
  case XPPE_AUTO_UMAX: src = d->uhi; break;
  case XPPE_AUTO_UMIN: src = d->ulo; break;
  case XPPE_AUTO_UMEAN: src = d->ubar; break;
  case XPPE_AUTO_EIG_RE: src = d->evr; break;
  case XPPE_AUTO_EIG_IM: src = d->evi; break;
  case XPPE_AUTO_PARAMS: src = d->par; len = NAutoPar; break;
  default:
    xppe_set_error("unknown point data kind", NULL);
    return XPPE_ERR_RANGE;
  }
  if (n > len) n = len;
  for (k = 0; k < n; k++) out[k] = src[k];
  return n;
}

const char *xppe_auto_parameter_name(int slot) {
  if (!xppe_loaded() || slot < 0 || slot >= NAutoPar) return NULL;
  return upar_names[AutoPar[slot]];
}

int xppe_auto_parameter_count(void) { return xppe_loaded() ? NAutoPar : 0; }

int xppe_auto_main_slot(int which) {
  if (!xppe_loaded()) return -1;
  return which == 0 ? Auto.icp1 : Auto.icp2;
}

/* Select a labelled point for the next run, like XPPAUT's Grab. The body
   mirrors the end of traverse_diagram() in auto_x11.c. */
int xppe_auto_grab(int label) {
  DIAGRAM *d;
  int i, rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  if (!DiagFlag) {
    xppe_set_error("the diagram is empty", NULL);
    return XPPE_ERR_STATE;
  }
  for (d = bifd; d; d = d->next)
    if (d->lab == label && label != 0) break;
  if (!d) {
    xppe_set_error("no point with that label", NULL);
    return XPPE_ERR_NAME;
  }
  xppe_begin_call();
  XPPE_GUARD_BEGIN((void)0);
  grabpt.ibr = d->ibr;
  grabpt.lab = d->lab;
  for (i = 0; i < 8; i++) grabpt.par[i] = d->par[i];
  grabpt.icp1 = d->icp1;
  grabpt.icp2 = d->icp2;
  grabpt.per = d->per;
  grabpt.torper = d->torper;
  for (i = 0; i < NODE; i++) {
    grabpt.uhi[i] = d->uhi[i];
    grabpt.ulo[i] = d->ulo[i];
    grabpt.u0[i] = d->u0[i];
    grabpt.ubar[i] = d->ubar[i];
    set_ivar(i + 1, grabpt.u0[i]);
  }
  get_ic(0, grabpt.u0);
  grabpt.flag = 1;
  grabpt.itp = d->itp;
  grabpt.ntot = d->ntot;
  grabpt.nfpar = d->nfpar;
  grabpt.index = d->index;
  for (i = 0; i < NAutoPar; i++)
    constants[Auto_index_to_array[i]] = grabpt.par[i];
  evaluate_derived();
  redo_all_fun_tables();
  XPPE_GUARD_END;
  xppe_end_call();
  return XPPE_OK;
}

int xppe_auto_ungrab(void) {
  int rc;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  grabpt.flag = 0;
  return XPPE_OK;
}
