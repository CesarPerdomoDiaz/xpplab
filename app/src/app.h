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

/* XPPAUT's Initial conds > Range (the Range tab), and with
   SWEEP_EQUILIBRIA its Sing pts > Range: the equilibrium followed over the
   parameter. */
enum { SWEEP_OVERLAY, SWEEP_MAXMIN, SWEEP_LYAPUNOV, SWEEP_EQUILIBRIA };

typedef struct {
  int par;             /* what is varied: a parameter index, or with   */
  bool over_ic;        /* over_ic, an initial-value (variable) index   */
  double from, to;
  int steps;
  int mode;            /* SWEEP_* */
  bool continue_runs;  /* start each run where the last ended ("Use old
                          ICs" = No) */
  int var;             /* variable measured (max/min mode) */
  double transient;    /* fraction of each run ignored before measuring */
  /* A second value varied with the first (XPPAUT's 2par range): on a grid
     (its Array), or both together along a line (its Crv). */
  bool two, grid;
  int par2;
  bool over_ic2;
  double from2, to2;
  int steps2;          /* grid: values of the second */
  int show;            /* a grid of max/min shows 0 max - min, 1 max, 2 min */
  bool overlay;        /* mode == SWEEP_OVERLAY (kept for the plots) */
  int n;               /* results of the last measuring sweep */
  int n1, n2, result_kind; /* a grid of n2 rows of n1 (kind 2), a line (1),
                              one value (0) */
  double *x, *x2, *ymax, *ymin; /* x2: the second value of each run.
                              Equilibria: the variable, and the number of
                              unstable eigenvalues (NaN: none found) */
  Trajectory eqt;      /* equilibria: the parameter, every variable,
                          unstable, stable */
  int result_mode;     /* the mode the results above come from */
  bool reveal;         /* bring the Range tab forward next frame */
} SweepUI;

/* Every equilibrium found in a box (XPPAUT's Sing pts > Monte Carlo):
   n of them, nvars values each. */
typedef struct {
  int n, guesses;
  double *x, *re, *im;
  int *unstable;
} FoundEquilibria;

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

/* The 3D view: three columns of the run, rotated with the mouse. The
   model's @ zplot=, axes=3d, theta=, phi= and xmin= ... zmax= set it up as
   in XPPAUT. */
typedef struct {
  int x, y, z;          /* columns (indices into App.cols) */
  float theta, phi;     /* view angles in degrees, as XPPAUT's */
  float zoom;
  bool auto_box;        /* the box from the data, else box[] */
  double box[6];        /* xmin xmax ymin ymax zmin zmax */
  int front;            /* bring the view forward this many frames from now */
  float spin;           /* degrees a second about the vertical axis; 0: still */
} View3D;

/* The animator: an XPPAUT .ani file played over the run (XPPAUT's
   Viewer). XPPAUT evaluates the frames; comet trails are kept here. */
#define APP_MAX_COMETS 32
#define APP_COMET_POINTS 512
typedef struct {
  int command, n;
  float x[APP_COMET_POINTS], y[APP_COMET_POINTS];
  unsigned col[APP_COMET_POINTS];
} CometTrail;

typedef struct {
  char path[1024];
  bool loaded;
  int ncommands;
  double xlo, ylo, xhi, yhi;
  int row, last_row;
  bool playing, loop;
  float speed;          /* rows of the run per second */
  double acc;
  CometTrail comet[APP_MAX_COMETS];
  int ncomets;
} AniUI;

/* The session as Python (echo.c): each action that changes the model or
   computes a result, written as the line of Python, with the xpplab
   package, that does the same. */
#define ECHO_NOPTS 21
typedef struct {
  bool console;       /* also show each line in the command window */
  char *text;         /* the lines so far */
  size_t len, cap;
  bool loaded;        /* the lines have loaded the current model */
  int depth;          /* actions in progress; only the outermost is written */
  bool live;          /* a live rerun is in progress */
  bool pending_run;   /* a live rerun not written yet */
  bool dirty;         /* a value changed since the last line */
  bool wrote;         /* the action in progress wrote a line */
  bool nc_written;    /* the nullclines of this view are written */
  bool plt;           /* the lines draw figures */
  /* What the script's model holds after the lines so far. */
  double *par, *ic, opt[ECHO_NOPTS];
  int npar, nic;
  char table[16];     /* the variable holding the last analysis result */
  int sweep_mode;     /* what the variable 'sweep' holds: SWEEP_*, or -1 */
  int sweep_kind;     /* and how: one value (0), a line (1), a grid (2) */
  char sweep_name[APP_NAME_LEN], sweep_name2[APP_NAME_LEN], sweep_var[APP_NAME_LEN];
} Echo;

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
  FoundEquilibria found;

  PhasePlane pp;
  AutoUI au;
  SweepUI sw;
  AnalysisUI an;
  View3D v3;
  AniUI ani;
  int nbcs;          /* the model's boundary conditions (bndry) */
  Echo echo;

  /* console */
  char *log;
  size_t log_len, log_cap;
  bool log_scroll;
  char history[32][256];
  int nhistory;

  /* interface */
  bool show_browser, show_workspace, show_editor, show_time, show_phase,
      show_auto, show_console, show_help, show_data, show_demo, show_array,
      show_analysis, show_view3d, show_ani;
  int array_first, array_last; /* columns shown in the array plot */
  bool reset_layout;
  int layout; /* 0: the plots in tabs; 1: the phase plane beside the time series */
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
/* Whether path is one of the examples that come with the app, which Save
   never overwrites: an edited example goes to a copy in Documents/XPP
   Lab, under the first free name that app_example_copy_path() gives. */
bool app_is_example(const App *a, const char *path);
bool app_example_copy_path(const App *a, char *out, size_t len);
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
/* A rerun after a change in live mode: written to the session once the
   mouse is let go, not once per frame of a drag. */
bool app_run_live(App *a);
bool app_continue(App *a, double more_time); /* 0: the current TOTAL */
/* XPPAUT's Initial conds > Last, from the run as shown. */
bool app_ic_last(App *a);
void app_freeze(App *a);
void app_clear_curves(App *a);
bool app_equilibrium(App *a, const double *guess);
void app_clear_equilibria(App *a);
/* Every equilibrium Newton's method finds from `guesses` random points
   (XPPAUT's Sing pts > Monte Carlo), in the phase plane's window and, for
   the other variables, the range of the last run widened by a quarter.
   They replace the equilibria shown. */
bool app_find_equilibria(App *a, int guesses);
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
/* Boundary-value problem (XPPAUT's Bndryval), the 3D view's setup from the
   model, and the animator. */
bool app_bvp(App *a);
void app_view3d_reset(App *a);
bool app_ani_load(App *a, const char *path);

/* AUTO */
bool app_auto_equilibria(App *a);
bool app_auto_continue(App *a, int point, const char *action);
void app_auto_reset(App *a);
void app_auto_refresh(App *a);
/* The diagram to and from an .auto file (XPPAUT's AUTO > File > Save and
   Load diagram), and the periodic orbit at a point as the run (Import
   orbit). */
bool app_auto_save(App *a, const char *path);
bool app_auto_load(App *a, const char *path);
bool app_auto_orbit(App *a, int point);
bool app_auto_push_settings(App *a);
/* Actions available from a diagram point, as (label, key) pairs. */
int app_auto_actions(App *a, int point, const char **labels, const char **keys,
                     int max);

/* Files. The CSV files hold every number exactly (the shortest text that
   reads back as the same double). */
bool app_export_csv(App *a, const char *path);          /* the last run */
bool app_export_auto_csv(App *a, const char *path);     /* the diagram's points */
bool app_export_analysis_csv(App *a, const char *path); /* the Analysis result */
bool app_export_range_csv(App *a, const char *path);    /* a measuring Range */
bool app_export_equilibria_csv(App *a, const char *path); /* app_find_equilibria */
void app_format_double(char *buf, size_t len, double v);
void app_add_recent(App *a, const char *path);
/* The app's own folder (settings, layout, untitled models), ending in a
   separator, or NULL; free it with SDL_free(). */
char *app_pref_dir(void);
void app_save_settings(App *a);
void app_load_settings(App *a);

/* Commands typed in the command window */
void app_command(App *a, const char *line);
const char *app_method_name(int index);

/* The session as Python (echo.c). An action calls echo_begin() first: it
   writes the values changed since the last line, and says whether this is
   the outermost action, the one whose line to write. echo_end() closes the
   action: the engine's state becomes the script's. */
bool echo_begin(App *a);
void echo_end(App *a);
void echo_line(App *a, const char *fmt, ...);
void echo_model_loaded(App *a); /* m = xs.load(...) */
void echo_values(App *a);       /* write the values that changed */
/* Once a frame: write what waits (a changed value, a live rerun) unless a
   control is still held (busy). */
void echo_frame(App *a, bool busy);
void echo_figure(App *a, int figure, const char *path);
bool echo_save(App *a, const char *path);
/* Whether a name is a parameter, a variable or an option the echo follows. */
bool echo_knows(App *a, const char *name);
/* A number or a string as Python source; buf is returned. */
const char *echo_num(double v, char *buf, size_t len);
const char *echo_str(const char *s, char *buf, size_t len);
/* The absolute form of a path, as the session should name it. */
void echo_abs_path(const char *path, char *out, size_t len);

#endif
