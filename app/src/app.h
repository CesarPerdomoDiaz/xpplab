/*
 * The application state and the actions the user interface can take.
 *
 * Everything that touches the engine goes through the functions declared
 * here, so the panels (ui_*.c) stay plain views, and every action can also
 * be typed in the command window (commands.c).
 */
#ifndef XPPS_APP_H
#define XPPS_APP_H

#include <stdbool.h>
#include <stddef.h>

#include "xpp_instance.h"

#define APP_MAX_FROZEN 16    /* curves kept on the plots */
#define APP_MAX_EQUILIBRIA 32
#define APP_MAX_RECENT 10
#define APP_NAME_LEN 64

typedef struct {
  int nrows, ncols;
  char (*names)[APP_NAME_LEN];
  double **cols; /* cols[j][i]: column j at output time i */
  char label[96];
} Trajectory;

typedef struct {
  double value;
  double lo, hi; /* slider range */
} ParamUI;

typedef struct {
  double *x;     /* one value per state variable */
  double *re, *im;
  int unstable;
  char kind[32]; /* "stable node", "saddle", ... */
  /* For saddles: the stable and unstable manifold branches. */
  Trajectory manifold[8];
  int manifold_type[8]; /* +1 unstable, -1 stable */
  int nmanifolds;
} Equilibrium;

typedef struct {
  int xvar, yvar; /* indices of state variables */
  bool nullclines, dirfield, click_to_start, show_equilibria;
  double xlo, xhi, ylo, yhi; /* the window the nullclines were computed for */
  float *ncx, *ncy;          /* segments (x1,y1,x2,y2) */
  int ncx_n, ncy_n;
  bool nc_dirty;
  int grid;
  bool set_window; /* apply win_* to the plot on the next frame */
  double win_xlo, win_xhi, win_ylo, win_yhi;
} PhasePlane;

typedef struct {
  int count;
  xppe_auto_point_info *info;
  double *umax, *umin; /* count x nvars */
  int selected;        /* index into the points, or -1 */
  int yvar;            /* variable on the vertical axis, -1 = norm */
  int main_par, second_par;
  double ds, dsmin, dsmax, parmin, parmax, normmin, normmax;
  int nmax, npr, ntst;
} AutoUI;

typedef struct {
  int par;             /* parameter swept */
  double from, to;
  int steps;
  int var;             /* variable measured (max/min mode) */
  double transient;    /* fraction of each run ignored before measuring */
  bool overlay;        /* keep each run's trajectory instead of measuring */
  int n;               /* results of the last max/min sweep */
  double *x, *ymax, *ymin;
  bool reveal;         /* bring the Sweep tab forward next frame */
} SweepUI;

/* The Analysis window: XPPAUT's Numerics > stocHastic and Averaging tools,
   applied to the last run (app_analysis.c). */
enum {
  AN_SPECTRUM,    /* power spectral density, cross spectrum, coherence */
  AN_FOURIER,     /* Fourier series, as cos/sin or amplitude/phase */
  AN_HISTOGRAM,
  AN_HISTOGRAM2D,
  AN_CORRELATION, /* auto- and cross-correlation */
  AN_STATISTICS,  /* mean, standard deviation, Liapunov exponent */
  AN_PRC,         /* one period, adjoint: phase response curves */
  AN_HFUNCTION,   /* weak-coupling interaction function */
  AN_FIT,         /* fit parameters to data */
  AN_COUNT
};

typedef struct {
  int tool;
  int col, col2;       /* columns analysed (indices into App.cols) */
  int spec_kind;       /* 0 PSD, 1 cross spectrum, 2 coherence */
  int spec_len, spec_window;
  bool polar;          /* Fourier as amplitude and phase */
  int bins, bins2;
  bool auto_range;     /* histogram ranges from the data */
  double lo, hi, lo2, hi2;
  char condition[128]; /* count only where this is nonzero */
  int lags;
  bool use_fft;
  double transient;    /* phase response: time to settle onto the cycle */
  char (*coupling)[128]; /* H function: one formula per variable */
  char fit_file[1024];
  char fit_vars[128], fit_cols[64], fit_pars[256];
  bool log_y;
  int show_col;        /* result column on the vertical axis */
  bool refit;          /* frame the next drawn result */
  /* the last result */
  Trajectory res;
  bool have;
  int res_tool;
  char summary[256];   /* scalar results, shown above the plot */
  double period;
} AnalysisUI;

typedef struct App {
  /* engine */
  char engine_path[1024];
  char base_dir[1024];
  char examples_dir[1024];
  xpp_instance *eng;
  bool model_ok;

  /* the model */
  char model_path[1024];
  char model_name[256];
  char *editor; /* the ODE file text being edited */
  size_t editor_cap;
  bool editor_dirty;
  char load_error[1024];
  int error_line; /* 1-based, or 0 when unknown */

  int nvars, npars, ncols, nsets;
  char (*vars)[APP_NAME_LEN];
  char (*pars)[APP_NAME_LEN];
  char (*cols)[APP_NAME_LEN];
  char (*sets)[APP_NAME_LEN];
  ParamUI *par;
  double *ic;
  bool *plot_col;    /* which columns the time plot shows */
  bool time_missing; /* the model's "only" statement leaves out time */

  /* numerics */
  double total, dt, t0, trans, bound, tol, atol;
  int nout, meth;
  int poimap, poivar, poisgn, poistop; /* Poincare section (POIVAR: 0 = t) */
  double poipln;

  /* results */
  Trajectory traj;
  Trajectory frozen[APP_MAX_FROZEN];
  int nfrozen;
  bool have_traj;
  bool live;      /* rerun after every parameter change */
  bool keep_curves;
  bool needs_run;
  Equilibrium eq[APP_MAX_EQUILIBRIA];
  int neq;

  PhasePlane pp;
  AutoUI au;
  SweepUI sw;
  AnalysisUI an;

  /* console */
  char *log;
  size_t log_len, log_cap;
  bool log_scroll;
  char history[32][256];
  int nhistory;

  /* interface */
  bool show_browser, show_workspace, show_editor, show_time, show_phase,
      show_auto, show_console, show_help, show_data, show_demo, show_array,
      show_analysis;
  int array_first, array_last; /* columns shown in the array plot */
  bool reset_layout;
  bool dark;
  float ui_scale;
  bool classic_keys; /* XPPAUT two-key shortcuts (I G, N N, ...) */
  char classic_prefix;
  char status[256];
  char focus[32]; /* bring this window forward next frame ("time", ...) */
  char help_page[64];
  char help_anchor[64];
  char recent[APP_MAX_RECENT][1024];
  int nrecent;
  bool quit;
  bool ephemeral; /* do not touch saved settings */
  void *window; /* SDL_Window */
} App;

extern App *g_app;

/* ephemeral: neither read nor write the saved settings (scripted runs). */
void app_init(App *a, const char *base_dir, bool ephemeral);
void app_shutdown(App *a);

void app_log(App *a, const char *fmt, ...);
void app_status(App *a, const char *fmt, ...);

/* Models */
bool app_open(App *a, const char *path);
bool app_open_text(App *a, const char *text, const char *name);
bool app_apply_editor(App *a);  /* save the editor text and reload */
bool app_save_editor(App *a, const char *path);
void app_new_model(App *a);
void app_refresh_values(App *a);

/* Values */
bool app_set_param(App *a, int i, double v);
bool app_set_ic(App *a, int i, double v);
bool app_set_option(App *a, const char *name, const char *value);
bool app_apply_set(App *a, const char *name);
int app_find_var(App *a, const char *name);
int app_find_par(App *a, const char *name);

/* Computation */
bool app_run(App *a);
bool app_continue(App *a, double more_time); /* 0: the current TOTAL */
void app_freeze(App *a);
void app_clear_curves(App *a);
bool app_equilibrium(App *a, const double *guess);
void app_clear_equilibria(App *a);
bool app_nullclines(App *a, double xlo, double xhi, double ylo, double yhi);
/* XPPAUT's Initial conds > Range: run once per value of a parameter, and
   either keep each trajectory or record the max and min of a variable. */
bool app_sweep(App *a);

/* The engine's current run replaces a->traj (after an analysis changed it). */
void app_fetch_current(App *a);
void app_free_trajectory(Trajectory *t);

/* Analysis (app_analysis.c) */
void app_analysis_reset(App *a); /* a model was loaded: defaults, no result */
void app_analysis_free(App *a);
bool app_analysis_run(App *a);   /* run a->an.tool on the last run */
const char *app_analysis_name(int tool);

/* AUTO */
bool app_auto_equilibria(App *a);
bool app_auto_continue(App *a, int point, const char *action);
void app_auto_reset(App *a);
void app_auto_refresh(App *a);
bool app_auto_push_settings(App *a);
/* Actions available from a diagram point, as (label, key) pairs. */
int app_auto_actions(App *a, int point, const char **labels, const char **keys,
                     int max);

/* Files */
bool app_export_csv(App *a, const char *path);
void app_add_recent(App *a, const char *path);
void app_save_settings(App *a);
void app_load_settings(App *a);

/* Commands typed in the command window */
void app_command(App *a, const char *line);
const char *app_method_name(int index);

#endif
