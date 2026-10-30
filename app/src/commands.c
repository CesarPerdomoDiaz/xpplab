/* Commands typed in the command window. Toolbar buttons and XPPAUT's
   keyboard commands run through here too, so both always behave alike. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "actions.h"
#include "app.h"
#include "channel.h"
#include "export.h"
#include "pyconsole.h"
#include "pyfetch.h"

void ui_show_help(App *a, const char *page);

static const char *usage =
    "Commands (XPPAUT keyboard equivalents in brackets):\n"
    "  run                      integrate from the initial values [I G]\n"
    "  continue [TIME]          integrate further from the end of the last\n"
    "                           run and append to it [C C]\n"
    "  set NAME=VALUE ...       set parameters, initial values or options\n"
    "                           e.g.  set I=0.5 total=200 meth=stiff\n"
    "  get NAME                 show a value\n"
    "  whos                     list variables and parameters\n"
    "  ic last                  start the next run where the last ended [I L]\n"
    "  ic shift                 the same, and the time goes on from there [I S]\n"
    "  ic read FILE             initial values from a file, one number per\n"
    "                           variable [I F]; ic write FILE saves them\n"
    "  ic NAME = FORMULA        a variable, or a range u[1..20] with [j] for\n"
    "                           the index, from a formula [I U]\n"
    "  click X Y                start a run at a point of the phase plane, as a\n"
    "                           click does [I M]\n"
    "  eq                       find an equilibrium and its stability [S G]\n"
    "  eq all [GUESSES]         every equilibrium in the phase plane's window,\n"
    "                           from random guesses (Sing pts > Monte Carlo)\n"
    "  nullclines [off]         draw nullclines in the phase plane [N N]\n"
    "  nullclines freeze        keep the nullclines drawn [N F]; nullclines\n"
    "                           freeze PAR FROM TO N: for N values of PAR;\n"
    "                           nullclines unfreeze deletes them; nullclines\n"
    "                           show K shows the K-th alone (0: all), and\n"
    "                           nullclines play steps through them\n"
    "  dirfield [off]           draw the direction field [D D]\n"
    "  erase                    remove kept curves [E]\n"
    "  use SET                  apply a named parameter set\n"
    "  auto steady [PAR]        follow steady states as PAR varies\n"
    "  auto from LABEL ACTION [PAR2]\n"
    "                           continue from a labelled point; ACTION is\n"
    "                           periodic | two | extend | switch | doubling\n"
    "  auto set DS=0.01 ...     AUTO numerics (ds, dsmin, dsmax, nmax, npr,\n"
    "                           parmin, parmax, ntst)\n"
    "  auto reset               clear the bifurcation diagram\n"
    "  auto save FILE.auto      save the diagram, AUTO's settings and restart\n"
    "                           data; auto load FILE.auto brings it back\n"
    "  auto orbit LABEL         the periodic orbit at a point as the run\n"
    "  range NAME FROM TO N [NAME2 FROM2 TO2 [N2]] [VAR|lyap|eq] [continue]\n"
    "                           XPPAUT's Range: N runs over a parameter or\n"
    "                           initial value; with VAR plot its max and min\n"
    "                           (a brute-force diagram), with lyap the\n"
    "                           Liapunov exponent; continue starts each run\n"
    "                           where the last ended (also: sweep); eq\n"
    "                           follows the equilibrium (Sing pts > Range).\n"
    "                           NAME2: a second value too, on a grid of\n"
    "                           N2 steps, or without N2 along with the first\n"
    "                           (XPPAUT's 2par range). film [FIGURE]: each\n"
    "                           run drawn alone and captured by the Kinescope\n"
    "                           (Range's Movie)\n"
    "  kinescope capture [FIG]  a picture of a figure (time, phase, auto,\n"
    "                           auto2) for the Kinescope [K C]; kinescope\n"
    "                           play [MS], stop, show K, clear, save F.gif\n"
    "                           [MS], frames BASE (PNG files), film [FIG]\n"
    "  py CODE                  run a line of Python in the console (xs, np,\n"
    "                           app and m, the model here, are defined);\n"
    "                           py alone: the command window takes Python\n"
    "  python [use PATH | stop] the console's Python: choose one, or stop it;\n"
    "                           python fetch downloads XPP Lab's own (once);\n"
    "                           python install PKG... runs pip for it\n"
    "  serve [on|off]           let Python drive the app: in Python,\n"
    "                           app = xpplab.app.connect()\n"
    "  many N [seed S] VAR LO HI ... [keep COL LO HI]\n"
    "                           N runs from initial values drawn in a box,\n"
    "                           kept where COL (e.g. an aux) lies in LO HI;\n"
    "                           each keeps the global flag that stopped it\n"
    "  shoot [eps E] [time T] [unstable|stable|both]\n"
    "                           runs off the equilibrium along its real\n"
    "                           eigenvectors, both ways (XPPAUT's sHoot)\n"
    "  figure [new W H [ROWS COLS] | panel K | grid R C | inset]\n"
    "                           the Figure window: a figure for papers\n"
    "  figure run X Y | many X Y [split] | diagram Y [x X] [min]\n"
    "         [stable|unstable] [steady|orbits] [branch B] [between L1 L2]\n"
    "         | range [max|min] | function F LO HI | curve Y X LO HI | eq\n"
    "         | from time|phase|auto|auto2\n"
    "                           add series to the selected panel\n"
    "  figure title|xlabel|ylabel TEXT, xlim|ylim LO HI|auto,\n"
    "         style K [color #rrggbb] [line --] [width W] [marker o]\n"
    "         [scale SX SY] [label TEXT], export FILE.pdf|svg|eps|png [DPI]\n"
    "  bvp                      solve the boundary-value problem (bndry)\n"
    "  ani FILE.ani             open an XPPAUT animation of the run;\n"
    "                           ani play, ani stop, ani row N\n"
    "  view3d X Y Z             show three columns in the 3-D view;\n"
    "                           view3d spin [DEG/S | off], view3d angles TH PHI\n"
    "Analysis of the last run (XPPAUT's Numerics > stocHastic, Averaging):\n"
    "  spectrum COL [COL2] [psd|cross|coherence] [N]\n"
    "                           spectral density, windows of N points\n"
    "  fourier COL | power COL  Fourier series: cos/sin, amplitude/phase\n"
    "  hist COL [BINS] [LO HI]  histogram of a column\n"
    "  hist2d X Y [BINS]        two-dimensional histogram\n"
    "  xcorr COL [COL2] [LAGS]  auto- or cross-correlation\n"
    "  mean COL                 mean, deviation, Liapunov exponent\n"
    "  ensemble [N]             N runs of a noisy model averaged: the mean\n"
    "                           and variance at each time (stocHastic >\n"
    "                           Compute; export F.csv analysis saves them)\n"
    "  prc                      phase response curves of the limit cycle\n"
    "  hfun F1; F2; ...         H function: one coupling per variable,\n"
    "                           primed names are the other cell (V'-V)\n"
    "  fitdata FILE VARS COLS PARS\n"
    "                           fit parameters to data (comma lists)\n"
    "Control:\n"
    "  linearize [at eq|ic] [inputs] P1 P2 ...\n"
    "                           A and B at the equilibrium (or the initial\n"
    "                           values), the eigenvalues, and whether the\n"
    "                           parameters P1 ... can steer every state\n"
    "                           (export F.csv analysis saves the table)\n"
    "  poles P1 P2 ...          the feedback gain K that gives A - B K these\n"
    "                           poles (one per state: -2, -1+2i, -1-2i)\n"
    "  lqr [Q q1 ...] [R r1 ...] the optimal gain K for state weights Q and\n"
    "                           input weights R (diagonal; one number for\n"
    "                           all alike), as poles gives one\n"
    "  closedloop [FILE]        write the closed loop u = u0 - K (x - x*) as\n"
    "                           a model next to this one, open it and run it\n"
    "  locus gain LO HI [N]     root locus: the poles of A - g B K for N gains\n"
    "                           g from LO to HI (K from poles); locus feedback\n"
    "                           INPUT STATE LO HI [N] feeds one state back;\n"
    "                           locus par P LO HI [N] follows the equilibrium\n"
    "                           as P changes (export F.csv locus saves it)\n"
    "  open FILE | new | save   models\n"
    "  export FILE.csv [WHAT]   save data as CSV: the last run, or auto (the\n"
    "                           bifurcation diagram), range, analysis or\n"
    "                           equilibria (the last eq all) or nullclines\n"
    "                           (those shown and frozen). FILE.mat writes a\n"
    "                           MATLAB file instead (not for linearize)\n"
    "  export FILE.svg [WHICH]  save a figure (time, phase, auto, auto2);\n"
    "                           .png saves it as it looks on screen\n"
    "  script FILE.py           save the session as a Python script that\n"
    "                           repeats it with the xpplab package\n"
    "  echo on | off            show each action as Python here, or not\n"
    "  show WINDOW              bring a window forward: time, phase, auto,\n"
    "                           editor, data, analysis, 3d, animator, help,\n"
    "                           console, workspace; hide WINDOW closes one\n"
    "  layout side | tabs       the phase plane beside the time series, or\n"
    "                           all plots in tabs\n"
    "  palette [TEXT]           the command palette: find and run any menu action\n"
    "                           or command by name [Ctrl+Shift+P]; palette run\n"
    "                           runs the first match of the text, palette list\n"
    "                           lists everything it finds\n"
    "  help [TOPIC]             open the manual (topics: tutorial,\n"
    "                           neuron-tutorial, auto, analysis, odefile, numerics, commands, xppaut,\n"
    "                           matlab, auto07p)\n";

const char *app_command_usage(void) { return usage; }

/* Every command of the command window, for the palette. `help` (the usage
   text above) describes them in full; `palette check` makes sure each
   command it lists is here. A command with args needs more than its name. */
const CommandInfo app_command_table[] = {
    {"run", NULL, "Integrate from the initial values.", "I G", NEED_MODEL},
    {"continue", NULL, "Integrate further from the end of the last run and append to it.", "C C", NEED_RUN},
    {"set", "NAME=VALUE ...", "Set parameters, initial values or options (total, meth, ...).", NULL, NEED_MODEL},
    {"get", "NAME", "Show the value of a variable, parameter or option.", NULL, NEED_MODEL},
    {"whos", NULL, "List the variables and parameters.", NULL, NEED_MODEL},
    {"ic last", NULL, "Start the next run where the last one ended.", "I L", NEED_RUN},
    {"ic shift", NULL, "Start the next run where the last ended, and let time go on from there.", "I S", NEED_RUN},
    {"ic read", "FILE", "Read initial values from a file, one number per variable.", "I F", NEED_MODEL},
    {"ic write", "FILE", "Write the initial values to a file.", NULL, NEED_MODEL},
    {"click", "X Y", "Start a run at a point of the phase plane, as a click does.", "I M", NEED_MODEL},
    {"eq", NULL, "Find an equilibrium near the initial values and its stability.", "S G", NEED_MODEL},
    {"eq all", NULL, "Find every equilibrium in the phase plane's window from random guesses.", NULL, NEED_MODEL},
    {"nullclines", NULL, "Draw the nullclines in the phase plane.", "N N", NEED_MODEL},
    {"nullclines freeze", NULL, "Keep the nullclines drawn.", "N F", NEED_MODEL},
    {"nullclines unfreeze", NULL, "Delete the frozen nullclines.", NULL, NEED_MODEL},
    {"nullclines play", NULL, "Step through the frozen nullclines.", NULL, NEED_MODEL},
    {"dirfield", NULL, "Draw the direction field in the phase plane.", "D D", NEED_MODEL},
    {"erase", NULL, "Remove the kept curves.", "E", NEED_NONE},
    {"freeze", NULL, "Keep the current curve on the plots.", NULL, NEED_RUN},
    {"use", "SET", "Apply a named parameter set of the model.", NULL, NEED_MODEL},
    {"sets", NULL, "List the model's named parameter sets.", NULL, NEED_MODEL},
    {"auto steady", NULL, "Follow steady states as a parameter varies (AUTO).", NULL, NEED_MODEL},
    {"auto from", "LABEL ACTION [PAR2]", "Continue from a labelled point of the diagram: periodic, two, extend, switch, doubling.", NULL, NEED_BRANCH},
    {"auto set", "DS=0.01 ...", "Set AUTO's numerics (ds, dsmin, dsmax, nmax, npr, parmin, parmax, ntst).", NULL, NEED_MODEL},
    {"auto reset", NULL, "Clear the bifurcation diagram.", NULL, NEED_NONE},
    {"auto save", "FILE.auto", "Save the diagram, AUTO's settings and restart data.", NULL, NEED_BRANCH},
    {"auto load", "FILE.auto", "Bring a saved diagram back.", NULL, NEED_MODEL},
    {"auto orbit", "LABEL", "Show the periodic orbit at a point of the diagram as the run.", NULL, NEED_BRANCH},
    {"range", "NAME FROM TO N ...", "Run over a range of a parameter or initial value (also sweep).", NULL, NEED_MODEL},
    {"kinescope capture", NULL, "Take a picture of a figure for the Kinescope.", "K C", NEED_MODEL},
    {"kinescope play", NULL, "Play the Kinescope's pictures.", "K A", NEED_NONE},
    {"kinescope clear", NULL, "Delete the Kinescope's pictures.", "K R", NEED_NONE},
    {"py", "CODE", "Run a line of Python in the console.", NULL, NEED_NONE},
    {"python", "use PATH | stop | fetch | install PKG", "Choose, stop or download the console's Python.", NULL, NEED_NONE},
    {"serve", NULL, "Let Python drive the app (xpplab.app.connect()).", NULL, NEED_NONE},
    {"many", "N VAR LO HI ...", "Many runs from initial values drawn in a box.", NULL, NEED_MODEL},
    {"shoot", NULL, "Runs off the equilibrium along its real eigenvectors, both ways.", NULL, NEED_MODEL},
    {"figure", "new W H | panel K | run X Y | ...", "The Figure window: build a figure for papers.", NULL, NEED_MODEL},
    {"bvp", NULL, "Solve the boundary-value problem (bndry conditions).", NULL, NEED_MODEL},
    {"ani", "FILE.ani", "Open an XPPAUT animation of the run.", NULL, NEED_MODEL},
    {"view3d", "X Y Z", "Show three columns in the 3-D view.", NULL, NEED_RUN},
    {"spectrum", "COL [COL2] [N]", "Power spectral density, cross spectrum or coherence of the last run.", NULL, NEED_RUN},
    {"fourier", "COL", "Fourier series of a column: cos/sin and amplitude/phase.", NULL, NEED_RUN},
    {"power", "COL", "Fourier series of a column as amplitude and phase.", NULL, NEED_RUN},
    {"hist", "COL [BINS]", "Histogram of a column of the last run.", NULL, NEED_RUN},
    {"hist2d", "X Y [BINS]", "Two-dimensional histogram of two columns.", NULL, NEED_RUN},
    {"xcorr", "COL [COL2] [LAGS]", "Auto- or cross-correlation of the last run.", NULL, NEED_RUN},
    {"mean", "COL", "Mean, deviation and Liapunov exponent of a column.", NULL, NEED_RUN},
    {"ensemble", NULL, "Average many runs of a noisy model: mean and variance at each time.", NULL, NEED_MODEL},
    {"prc", NULL, "Phase response curves of the limit cycle.", NULL, NEED_MODEL},
    {"hfun", "F1; F2; ...", "H function: the weak-coupling interaction, one per variable.", NULL, NEED_MODEL},
    {"fitdata", "FILE VARS COLS PARS", "Fit parameters to data.", NULL, NEED_MODEL},
    {"linearize", NULL, "A and B at the equilibrium, its eigenvalues and controllability.", NULL, NEED_MODEL},
    {"poles", "P1 P2 ...", "The feedback gain K that gives A - B K these poles.", NULL, NEED_MODEL},
    {"lqr", NULL, "The optimal feedback gain for state and input weights.", NULL, NEED_MODEL},
    {"closedloop", NULL, "Write the closed loop as a model next to this one and open it.", NULL, NEED_MODEL},
    {"locus", "gain LO HI [N]", "Root locus of the closed loop over a gain, a state or a parameter.", NULL, NEED_MODEL},
    {"open", "FILE", "Open a model file.", NULL, NEED_NONE},
    {"new", NULL, "Start a new model from a template.", NULL, NEED_NONE},
    {"save", NULL, "Save the model in the editor and reload it.", NULL, NEED_NONE},
    {"export", "FILE.csv [WHAT]", "Save data as CSV or MATLAB .mat, or a figure as SVG or PNG.", NULL, NEED_NONE},
    {"script", "FILE.py", "Save the session as a Python script that repeats it.", NULL, NEED_NONE},
    {"echo", "on | off", "Show each action as Python in the command window, or not.", NULL, NEED_NONE},
    {"show", "WINDOW", "Bring a window forward (time, phase, auto, editor, data, ...).", NULL, NEED_NONE},
    {"hide", "WINDOW", "Close a window.", NULL, NEED_NONE},
    {"layout", "side | tabs", "The phase plane beside the time series, or all plots in tabs.", NULL, NEED_NONE},
    {"theme", "dark | light", "Switch the colour theme.", NULL, NEED_NONE},
    {"help", NULL, "List the commands, or open a manual page (help TOPIC).", NULL, NEED_NONE},
    {"palette", NULL, "Open the command palette.", MOD "Shift+P", NEED_NONE},
    {"clear", NULL, "Clear the command window.", NULL, NEED_NONE},
    {"quit", NULL, "Leave the app.", "F Q", NEED_NONE},
};
const int app_ncommands = (int)(sizeof app_command_table / sizeof app_command_table[0]);

static char *skip_space(char *s) {
  while (*s && isspace((unsigned char)*s)) s++;
  return s;
}

static char *next_word(char **s) {
  char *w = skip_space(*s), *e;
  if (!*w) return NULL;
  e = w;
  while (*e && !isspace((unsigned char)*e)) e++;
  if (*e) *e++ = '\0';
  *s = e;
  return w;
}

static int col_index(App *a, const char *name) {
  int j;
  for (j = 0; j < a->ncols; j++)
    if (SDL_strcasecmp(a->cols[j], name) == 0) return j;
  app_log(a, "error: no column '%s'; type 'whos' for the names", name);
  return -1;
}

static bool is_number(const char *s) {
  char *end;
  if (!s) return false;
  strtod(s, &end);
  return end != s && *end == '\0';
}

/* time, phase, auto or auto2: a figure; -1 for any other word. */
static int figure_word(const char *w) {
  static const char *names[] = {"time", "phase", "auto", "auto2"};
  int i;
  for (i = 0; w && i < FIG_COUNT; i++)
    if (!strcmp(w, names[i])) return i;
  return -1;
}

/* kinescope capture [FIGURE] | clear | play [MS] | stop | show K |
   save FILE.gif [MS] | frames BASE | film [FIGURE]: XPPAUT's Kinescope. */
static void cmd_kinescope(App *a, char *rest) {
  Kinescope *k = &a->ks;
  char *w = next_word(&rest), *x = next_word(&rest);
  int f = figure_word(x);
  if (a->batch) {
    app_log(a, "error: the Kinescope takes pictures of the window: it needs the "
               "desktop app");
    return;
  }
  if (w && !strcmp(w, "capture")) {
    app_kine_capture(a, f >= 0 ? f : k->source);
  } else if (w && (!strcmp(w, "clear") || !strcmp(w, "reset"))) {
    app_kine_clear(a);
    app_log(a, "Kinescope emptied.");
  } else if (w && !strcmp(w, "play")) {
    if (x && is_number(x)) k->ms = atoi(x);
    k->playing = k->n > 1;
    k->acc = 0;
  } else if (w && !strcmp(w, "stop")) {
    k->playing = false;
  } else if (w && !strcmp(w, "show") && x && is_number(x)) {
    int i = atoi(x);
    if (i >= 1 && i <= k->n) k->cur = i - 1;
    k->playing = false;
  } else if (w && !strcmp(w, "save") && x) {
    char *ms = next_word(&rest);
    app_kine_save_gif(a, x, ms && is_number(ms) ? atoi(ms) : k->ms);
  } else if (w && !strcmp(w, "frames") && x) {
    app_kine_save_frames(a, x);
  } else if (w && !strcmp(w, "film")) {
    app_film_range(a, f >= 0 ? f : k->source);
  } else if (w) {
    app_log(a, "Write: kinescope capture [time|phase|auto|auto2], clear, play "
               "[MS], stop, show K, save FILE.gif [MS], frames BASE, or film "
               "[FIGURE] (the Range tab's runs)");
  }
  a->show_kine = true;
}

/* spectrum, fourier, power, hist, hist2d, xcorr, mean, prc, hfun, fitdata,
   ensemble, linearize */
static int name_index(char (*names)[APP_NAME_LEN], int n, const char *name) {
  int i;
  for (i = 0; i < n; i++)
    if (SDL_strcasecmp(names[i], name) == 0) return i;
  return -1;
}

/* locus gain FROM TO [STEPS]             the gain K from pole placement
   locus feedback INPUT STATE FROM TO [STEPS]
   locus par NAME FROM TO [STEPS]         a parameter, the equilibrium followed */
static void cmd_locus(App *a, char *rest) {
  const LinearSystem *L = &a->an.lin;
  RootLocusUI *R = &a->an.locus;
  char *kind = next_word(&rest), *w;
  int input = -1, state = -1, par = -1, steps = 0;
  double lo, hi;
  if (!a->model_ok) {
    app_log(a, "error: open a model first");
    return;
  }
  if (kind && !strcmp(kind, "feedback")) {
    char *u = next_word(&rest), *v = next_word(&rest);
    if (u && L->inputs) input = name_index(L->inputs, L->m, u);
    if (v) state = name_index(a->vars, a->nvars, v);
    if (input < 0 || state < 0) {
      app_log(a, "error: write locus feedback INPUT STATE FROM TO, an input of "
                 "the linearisation and a state");
      return;
    }
  } else if (kind && (!strcmp(kind, "par") || !strcmp(kind, "parameter"))) {
    char *p = next_word(&rest);
    if (!p || (par = name_index(a->pars, a->npars, p)) < 0) {
      app_log(a, "error: write locus par NAME FROM TO with a parameter's name");
      return;
    }
  } else if (!kind || strcmp(kind, "gain") != 0) {
    app_log(a, "Write: locus gain FROM TO [STEPS] (the gain from poles), "
               "locus feedback INPUT STATE FROM TO [STEPS], or locus par NAME "
               "FROM TO [STEPS]");
    return;
  }
  if (!(w = next_word(&rest)) || !is_number(w)) {
    app_log(a, "error: give the range, FROM TO");
    return;
  }
  lo = atof(w);
  if (!(w = next_word(&rest)) || !is_number(w)) {
    app_log(a, "error: give the range, FROM TO");
    return;
  }
  hi = atof(w);
  if ((w = next_word(&rest)) && is_number(w)) steps = atoi(w);
  a->show_analysis = true;
  if (par >= 0) {
    R->mode = 2;
    R->par = par;
    R->p_lo = lo;
    R->p_hi = hi;
    R->p_steps = steps ? steps : 101;
    app_parameter_locus(a, par, lo, hi, R->p_steps);
  } else {
    R->mode = input >= 0 ? 1 : 0;
    if (input >= 0) {
      R->input = input;
      R->state = state;
    }
    R->g_lo = lo;
    R->g_hi = hi;
    R->g_steps = steps ? steps : 201;
    app_gain_locus(a, R->mode, input, state, lo, hi, R->g_steps);
  }
  R->form_set = true;
}

static void cmd_analysis(App *a, const char *cmd, char *rest) {
  AnalysisUI *n = &a->an;
  char *w = NULL;
  if (!a->model_ok) {
    app_log(a, "error: open a model first");
    return;
  }
  if (!strcmp(cmd, "ensemble")) {
    n->tool = AN_ENSEMBLE;
    if ((w = next_word(&rest)) && is_number(w)) n->trials = atoi(w);
  } else if (!strcmp(cmd, "linearize") || !strcmp(cmd, "linearise")) {
    /* linearize [at eq|ic] [inputs] [P1 P2 ...] */
    char list[512];
    size_t k = 0;
    list[0] = '\0';
    n->tool = AN_LINEARIZE;
    n->lin_at = 0;
    while ((w = next_word(&rest))) {
      if (!strcmp(w, "at")) {
        char *where = next_word(&rest);
        if (where && (!strcmp(where, "eq") || !strcmp(where, "equilibrium")))
          n->lin_at = 0;
        else if (where && (!strcmp(where, "ic") || !strcmp(where, "initial")))
          n->lin_at = 1;
        else {
          app_log(a, "Write: linearize [at eq | at ic] [inputs] P1 P2 ..., "
                     "e.g. linearize at eq inputs iapp");
          return;
        }
      } else if (strcmp(w, "inputs") != 0 && k < sizeof list - 1) {
        k += (size_t)snprintf(list + k, sizeof list - k, "%s%s", k ? ", " : "", w);
      }
    }
    snprintf(n->lin_inputs, sizeof n->lin_inputs, "%s", list);
  } else if (!strcmp(cmd, "prc")) {
    n->tool = AN_PRC;
    if ((w = next_word(&rest)) && is_number(w)) n->transient = atof(w);
  } else if (!strcmp(cmd, "hfun")) {
    int i = 0;
    char *f;
    n->tool = AN_HFUNCTION;
    for (f = strtok(rest, ";"); f && i < a->nvars; f = strtok(NULL, ";"), i++)
      snprintf(n->coupling[i], sizeof n->coupling[i], "%s", skip_space(f));
    if (i > 0 && i < a->nvars)
      app_log(a, "note: %d of %d coupling formulas given; the rest are kept",
              i, a->nvars);
  } else if (!strcmp(cmd, "fitdata")) {
    char *file = next_word(&rest), *vars = next_word(&rest),
         *cols = next_word(&rest), *pars = next_word(&rest);
    if (!pars) {
      app_log(a, "Write: fitdata FILE VARS COLS PARS, e.g. fitdata v.dat V 2 "
                 "gca,gk");
      return;
    }
    n->tool = AN_FIT;
    snprintf(n->fit_file, sizeof n->fit_file, "%s", file);
    snprintf(n->fit_vars, sizeof n->fit_vars, "%s", vars);
    snprintf(n->fit_cols, sizeof n->fit_cols, "%s", cols);
    snprintf(n->fit_pars, sizeof n->fit_pars, "%s", pars);
  } else {
    int c;
    if (!(w = next_word(&rest))) {
      app_log(a, "error: give a column, e.g. %s V", cmd);
      return;
    }
    if ((c = col_index(a, w)) < 0) return;
    n->col = c;
    n->col2 = c;
    if (!strcmp(cmd, "spectrum") || !strcmp(cmd, "psd")) {
      n->tool = AN_SPECTRUM;
      n->spec_kind = 0;
      while ((w = next_word(&rest))) {
        if (!strcmp(w, "psd")) n->spec_kind = 0;
        else if (!strcmp(w, "cross")) n->spec_kind = 1;
        else if (!strcmp(w, "coherence")) n->spec_kind = 2;
        else if (is_number(w)) n->spec_len = atoi(w);
        else if ((c = col_index(a, w)) >= 0) {
          n->col2 = c;
          if (n->spec_kind == 0) n->spec_kind = 1;
        } else
          return;
      }
    } else if (!strcmp(cmd, "fourier") || !strcmp(cmd, "fft") ||
               !strcmp(cmd, "power")) {
      n->tool = AN_FOURIER;
      n->polar = !strcmp(cmd, "power");
    } else if (!strcmp(cmd, "hist")) {
      char *lo, *hi;
      n->tool = AN_HISTOGRAM;
      if ((w = next_word(&rest))) n->bins = atoi(w);
      lo = next_word(&rest);
      hi = next_word(&rest);
      n->auto_range = !(lo && hi);
      if (lo && hi) {
        n->lo = atof(lo);
        n->hi = atof(hi);
      }
    } else if (!strcmp(cmd, "hist2d")) {
      n->tool = AN_HISTOGRAM2D;
      if (!(w = next_word(&rest)) || (c = col_index(a, w)) < 0) {
        app_log(a, "error: write hist2d X Y [BINS]");
        return;
      }
      n->col2 = c;
      if ((w = next_word(&rest))) n->bins = n->bins2 = atoi(w);
      n->auto_range = true;
    } else if (!strcmp(cmd, "xcorr")) {
      n->tool = AN_CORRELATION;
      while ((w = next_word(&rest))) {
        if (is_number(w)) n->lags = atoi(w);
        else if ((c = col_index(a, w)) >= 0) n->col2 = c;
        else return;
      }
    } else {
      n->tool = AN_STATISTICS;
    }
  }
  a->show_analysis = true;
  snprintf(a->focus, sizeof a->focus, "analysis");
  app_analysis_run(a);
}

static void cmd_set(App *a, char *rest) {
  char *tok = strtok(rest, " ,;");
  while (tok) {
    char *eq = strchr(tok, '=');
    if (!eq) {
      app_log(a, "error: write set NAME=VALUE (got '%s')", tok);
    } else {
      int i;
      *eq = '\0';
      i = app_find_par(a, tok);
      if (i >= 0)
        app_set_param(a, i, atof(eq + 1));
      else if ((i = app_find_var(a, tok)) >= 0)
        app_set_ic(a, i, atof(eq + 1));
      else {
        double v;
        /* XPPAUT takes options this app does not list, and ignores names
           it does not know: say so, in case of a typo. */
        if (a->model_ok && a->eng->api.xppe_get(tok, &v) != XPPE_OK)
          app_log(a, "note: %s is not a parameter, variable or option the app "
                     "knows; XPPAUT may ignore it", tok);
        app_set_option(a, tok, eq + 1);
      }
    }
    tok = strtok(NULL, " ,;");
  }
}

static void cmd_whos(App *a) {
  int i;
  char line[4096];
  int k = 0;
  if (!a->model_ok) {
    app_log(a, "No model loaded.");
    return;
  }
  k = snprintf(line, sizeof line, "Variables (initial value):");
  for (i = 0; i < a->nvars && k < (int)sizeof line - 64; i++)
    k += snprintf(line + k, sizeof line - k, " %s=%g", a->vars[i], a->ic[i]);
  app_log(a, "%s", line);
  k = snprintf(line, sizeof line, "Parameters:");
  for (i = 0; i < a->npars && k < (int)sizeof line - 64; i++)
    k += snprintf(line + k, sizeof line - k, " %s=%g", a->pars[i],
                  a->par[i].value);
  app_log(a, "%s", line);
  app_log(a, "Output columns: %d. Method %s, total %g, dt %g.", a->ncols,
          app_method_name(a->meth), a->total, a->dt);
}

static const char *action_key(App *a, int point, const char *word) {
  const xppe_auto_point_info *p = &a->au.info[point];
  /* From a fold XPPAUT's AUTO always follows the fold in two parameters
     (auto_run() in auto_nox.c), whatever else is asked. */
  if (strcmp(p->type_name, "LP") == 0)
    return !strcmp(word, "two") || !strcmp(word, "two-parameter") ? "" : NULL;
  if (!strcmp(word, "periodic")) return "p";
  if (!strcmp(word, "two") || !strcmp(word, "two-parameter"))
    return strcmp(p->type_name, "LP") == 0 ? "" : "t";
  if (!strcmp(word, "extend")) return (p->periodic || !strcmp(p->type_name, "HB") ||
                                       !strcmp(p->type_name, "BP") ||
                                       !strcmp(p->type_name, "PD") ||
                                       !strcmp(p->type_name, "TR"))
                                          ? "e"
                                          : "";
  if (!strcmp(word, "switch")) return "s";
  if (!strcmp(word, "doubling")) return "d";
  if (!strcmp(word, "fixed")) return "f";
  return NULL;
}

/* The show_ flag of a window named as in 'show NAME'. */
static bool *window_flag(App *a, const char *w) {
  static const char *names[] = {"time",      "phase",   "auto",   "editor",   "data",
                                "help",      "console", "workspace", "models", "array",
                                "analysis",  "3d",      "animator"};
  bool *flags[] = {&a->show_time,     &a->show_phase,    &a->show_auto,
                   &a->show_editor,   &a->show_data,     &a->show_help,
                   &a->show_console,  &a->show_workspace, &a->show_browser,
                   &a->show_array,    &a->show_analysis, &a->show_view3d,
                   &a->show_ani};
  int i;
  for (i = 0; i < (int)(sizeof names / sizeof names[0]); i++)
    if (SDL_strncasecmp(w, names[i], strlen(w)) == 0) return flags[i];
  return NULL;
}

static void cmd_auto(App *a, char *rest) {
  char *w = next_word(&rest);
  if (!a->model_ok) {
    app_log(a, "No model loaded.");
    return;
  }
  if (!w || !strcmp(w, "steady") || !strcmp(w, "eq")) {
    char *par = next_word(&rest);
    if (par) {
      int i = app_find_par(a, par);
      if (i < 0) {
        app_log(a, "error: no parameter %s", par);
        return;
      }
      a->au.main_par = i;
    }
    a->show_auto = true;
    app_auto_equilibria(a);
  } else if (!strcmp(w, "from")) {
    char *lab = next_word(&rest), *act = next_word(&rest), *second = next_word(&rest);
    int i, label = lab ? atoi(lab) : 0, point = -1;
    const char *key;
    if (second) {
      /* auto from LABEL two PAR2: the second parameter of the curve */
      int j = app_find_par(a, second);
      if (j < 0) {
        app_log(a, "error: no parameter %s", second);
        return;
      }
      a->au.second_par = j;
    }
    for (i = 0; i < a->au.count; i++)
      if (a->au.info[i].label == label) point = i;
    if (point < 0) {
      app_log(a, "error: no point labelled %s", lab ? lab : "?");
      return;
    }
    key = action_key(a, point, act ? act : "extend");
    if (!key && strcmp(a->au.info[point].type_name, "LP") == 0) {
      app_log(a, "error: from a fold (LP) AUTO can only follow the fold in two "
                 "parameters: write auto from %d two", label);
      return;
    }
    if (!key) {
      app_log(a, "error: unknown action %s", act);
      return;
    }
    a->au.selected = point;
    app_auto_continue(a, point, key);
  } else if (!strcmp(w, "reset")) {
    app_auto_reset(a);
  } else if (!strcmp(w, "save") || !strcmp(w, "load")) {
    char *p = skip_space(rest);
    if (!*p)
      app_log(a, "Write: auto %s FILE.auto", w);
    else if (w[0] == 's')
      app_auto_save(a, p);
    else
      app_auto_load(a, p);
  } else if (!strcmp(w, "orbit")) {
    char *lab = next_word(&rest);
    int i, label = lab ? atoi(lab) : 0, point = -1;
    for (i = 0; i < a->au.count; i++)
      if (a->au.info[i].label == label && label) point = i;
    if (point < 0)
      app_log(a, "error: no point labelled %s", lab ? lab : "?");
    else {
      a->au.selected = point;
      app_auto_orbit(a, point);
    }
  } else if (!strcmp(w, "set")) {
    char *tok = strtok(rest, " ,;");
    while (tok) {
      char *eq = strchr(tok, '=');
      if (eq) {
        double v = atof(eq + 1);
        *eq = '\0';
        if (!SDL_strcasecmp(tok, "ds")) a->au.ds = v;
        else if (!SDL_strcasecmp(tok, "dsmin")) a->au.dsmin = v;
        else if (!SDL_strcasecmp(tok, "dsmax")) a->au.dsmax = v;
        else if (!SDL_strcasecmp(tok, "nmax")) a->au.nmax = (int)v;
        else if (!SDL_strcasecmp(tok, "npr")) a->au.npr = (int)v;
        else if (!SDL_strcasecmp(tok, "ntst")) a->au.ntst = (int)v;
        else if (!SDL_strcasecmp(tok, "parmin")) a->au.parmin = v;
        else if (!SDL_strcasecmp(tok, "parmax")) a->au.parmax = v;
        else app_log(a, "error: unknown AUTO setting %s", tok);
      }
      tok = strtok(NULL, " ,;");
    }
  } else {
    app_log(a, "error: unknown auto command '%s'; try 'help'", w);
  }
}

/* many N [seed S] [VAR LO HI]... [keep COL LO HI]: N runs from random
   initial values (Workspace > Many runs). */
static void cmd_many(App *a, char *rest) {
  ManyUI *m = &a->mu;
  char *w = next_word(&rest);
  int i;
  if (!m->use || !w) {
    app_log(a, "Write: many N [seed S] VAR LO HI ... [keep COLUMN LO HI]");
    return;
  }
  m->n = atoi(w);
  for (i = 0; i < a->nvars; i++) m->use[i] = false;
  m->req_col = -1;
  while ((w = next_word(&rest)) != NULL) {
    if (!strcmp(w, "seed")) {
      char *s = next_word(&rest);
      if (s) m->seed = atoi(s);
    } else if (!strcmp(w, "keep")) {
      char *c = next_word(&rest), *lo = next_word(&rest), *hi = next_word(&rest);
      int j = c ? col_index(a, c) : -1;
      if (j <= 0 || !lo || !hi) {
        app_log(a, "error: keep COLUMN LO HI");
        return;
      }
      m->req_col = j;
      m->req_lo = strtod(lo, NULL);
      m->req_hi = strtod(hi, NULL);
    } else {
      char *lo = next_word(&rest), *hi = next_word(&rest);
      int v = app_find_var(a, w);
      if (v < 0 || !lo || !hi) {
        app_log(a, "error: %s is not a variable, or its LO HI are missing", w);
        return;
      }
      m->use[v] = true;
      m->lo[v] = strtod(lo, NULL);
      m->hi[v] = strtod(hi, NULL);
    }
  }
  app_many_runs(a);
}

/* shoot [eps E] [time T] [unstable|stable|both] */
static void cmd_shoot(App *a, char *rest) {
  ManyUI *m = &a->mu;
  char *w;
  m->dirs = 0;
  m->shoot_total = 0;
  while ((w = next_word(&rest)) != NULL) {
    if (!strcmp(w, "eps")) {
      char *v = next_word(&rest);
      if (v) m->eps = strtod(v, NULL);
    } else if (!strcmp(w, "time")) {
      char *v = next_word(&rest);
      if (v) m->shoot_total = strtod(v, NULL);
    } else if (!strcmp(w, "stable")) {
      m->dirs = 1;
    } else if (!strcmp(w, "both") || !strcmp(w, "all")) {
      m->dirs = 2;
    } else if (strcmp(w, "unstable") != 0) {
      app_log(a, "error: shoot [eps E] [time T] [unstable|stable|both]");
      return;
    }
  }
  app_shoot(a);
}

/* figure ...: the Figure window from the command window and scripts. */
static void cmd_figure(App *a, char *rest) {
  FigUI *g = &a->fg;
  char *w = next_word(&rest);
  int panel = g->panel;
  if (!w) {
    a->show_figure = true;
    snprintf(a->focus, sizeof a->focus, "figure");
    return;
  }
  if (!strcmp(w, "new")) {
    char *ws = next_word(&rest), *hs = next_word(&rest), *rs = next_word(&rest),
         *cs = next_word(&rest);
    app_fig_new(a, ws ? (float)atof(ws) : 86, hs ? (float)atof(hs) : 65, rs ? atoi(rs) : 1,
                cs ? atoi(cs) : 1);
  } else if (!strcmp(w, "panel")) {
    char *p = next_word(&rest);
    int k = p ? atoi(p) - 1 : -1;
    if (k < 0 || k >= g->fig.npanels) app_log(a, "error: figure panel K, from 1 to %d", g->fig.npanels);
    else g->panel = k;
  } else if (!strcmp(w, "grid")) {
    char *r = next_word(&rest), *c = next_word(&rest);
    if (!r || !c || !app_fig_grid(a, atoi(r), atoi(c))) app_log(a, "error: figure grid ROWS COLS");
  } else if (!strcmp(w, "inset")) {
    int k = app_fig_inset(a, panel < g->fig.rows * g->fig.cols ? panel : 0);
    if (k < 0) app_log(a, "error: the figure has no room for another panel");
  } else if (!strcmp(w, "run") || !strcmp(w, "many")) {
    char *x = next_word(&rest), *y = next_word(&rest), *opt = next_word(&rest);
    int cx = x ? col_index(a, x) : -1, cy = y ? col_index(a, y) : -1;
    if (cx < 0 || cy < 0) {
      app_log(a, "error: figure %s X Y: two columns", w);
      return;
    }
    if (!strcmp(w, "run")) {
      app_fig_add_run(a, panel, cx, cy);
    } else if (opt && !strcmp(opt, "split")) {
      bool seen[64] = {0};
      int i, e;
      for (i = 0; i < a->many.n; i++)
        if (a->many.event[i] >= 0 && a->many.event[i] < 64) seen[a->many.event[i]] = true;
      for (e = 0; e < 64; e++)
        if (seen[e]) app_fig_add_runs(a, panel, cx, cy, e);
    } else {
      app_fig_add_runs(a, panel, cx, cy, -1);
    }
  } else if (!strcmp(w, "diagram")) {
    FigDiagram d;
    char *y = next_word(&rest), *o;
    memset(&d, 0, sizeof d);
    d.stable = d.periodic = -1;
    d.any_branch = true;
    if (!y) {
      app_log(a, "error: figure diagram Y [x X] [min] [stable|unstable] [steady|orbits] "
                 "[branch B] [between L1 L2]");
      return;
    }
    snprintf(d.y, sizeof d.y, "%s", y);
    while ((o = next_word(&rest)) != NULL) {
      if (!strcmp(o, "x")) {
        char *x = next_word(&rest);
        if (x) snprintf(d.x, sizeof d.x, "%s", x);
      } else if (!strcmp(o, "min")) d.which = 1;
      else if (!strcmp(o, "max")) d.which = 0;
      else if (!strcmp(o, "stable")) d.stable = 1;
      else if (!strcmp(o, "unstable")) d.stable = 0;
      else if (!strcmp(o, "steady")) d.periodic = 0;
      else if (!strcmp(o, "orbits")) d.periodic = 1;
      else if (!strcmp(o, "branch")) {
        char *b = next_word(&rest);
        d.any_branch = false;
        d.branch = b ? atoi(b) : 0;
      } else if (!strcmp(o, "where")) {
        char *c = next_word(&rest), *w = next_word(&rest), *lo = next_word(&rest),
             *hi = next_word(&rest);
        if (c && w && lo && hi) {
          snprintf(d.where, sizeof d.where, "%s", c);
          d.where_which = !strcmp(w, "min");
          d.where_lo = strtod(lo, NULL);
          d.where_hi = strtod(hi, NULL);
        }
      } else if (!strcmp(o, "between")) {
        char *l1 = next_word(&rest), *l2 = next_word(&rest);
        d.lab1 = l1 ? atoi(l1) : 0;
        d.lab2 = l2 ? atoi(l2) : 0;
      }
    }
    app_fig_add_diagram(a, panel, &d);
  } else if (!strcmp(w, "range")) {
    char *which = next_word(&rest);
    app_fig_add_sweep(a, panel, which && !strcmp(which, "min"));
  } else if (!strcmp(w, "function")) {
    char *e = next_word(&rest), *lo = next_word(&rest), *hi = next_word(&rest);
    if (!e || !lo || !hi) app_log(a, "error: figure function FORMULA LO HI (no spaces in it)");
    else app_fig_add_function(a, panel, e, strtod(lo, NULL), strtod(hi, NULL), 400);
  } else if (!strcmp(w, "curve")) {
    char *y = next_word(&rest), *x = next_word(&rest), *lo = next_word(&rest),
         *hi = next_word(&rest);
    int v = x ? app_find_var(a, x) : -1;
    if (!y || v < 0 || !lo || !hi) app_log(a, "error: figure curve Y X LO HI");
    else app_fig_add_curve(a, panel, y, v, strtod(lo, NULL), strtod(hi, NULL), 400);
  } else if (!strcmp(w, "eq")) {
    app_fig_add_equilibria(a, panel, a->pp.xvar, a->pp.yvar);
  } else if (!strcmp(w, "from")) {
    char *which = next_word(&rest);
    FigureKind k = FIG_PHASE;
    if (which && !strcmp(which, "time")) k = FIG_TIME;
    if (which && !strcmp(which, "auto")) k = FIG_AUTO;
    if (which && !strcmp(which, "auto2")) k = FIG_AUTO2;
    app_fig_from_plot(a, k);
  } else if (!strcmp(w, "title") || !strcmp(w, "xlabel") || !strcmp(w, "ylabel")) {
    FigPanel *p = &g->fig.p[panel];
    char *t = skip_space(rest);
    snprintf(!strcmp(w, "title") ? p->title : w[0] == 'x' ? p->xlabel : p->ylabel, FIG_TEXT, "%s", t);
    g->dirty = true;
    app_fig_sync_echo(a);
  } else if (!strcmp(w, "xlim") || !strcmp(w, "ylim")) {
    FigPanel *p = &g->fig.p[panel];
    char *lo = next_word(&rest), *hi = next_word(&rest);
    bool x = w[0] == 'x';
    if (lo && !strcmp(lo, "auto")) {
      if (x) p->autox = true;
      else p->autoy = true;
    } else if (lo && hi) {
      if (x) {
        p->autox = false;
        p->xmin = strtod(lo, NULL);
        p->xmax = strtod(hi, NULL);
      } else {
        p->autoy = false;
        p->ymin = strtod(lo, NULL);
        p->ymax = strtod(hi, NULL);
      }
    }
    g->dirty = true;
    app_fig_sync_echo(a);
  } else if (!strcmp(w, "style")) {
    /* figure style K [color #rrggbb] [line -|--|:|-.|none] [width W]
       [marker o|s|^|D|x|+|*|.|none] [scale SX SY] [label TEXT] */
    char *ks = next_word(&rest), *o;
    FigPanel *p = &g->fig.p[panel];
    int k = ks ? atoi(ks) - 1 : -1;
    if (k < 0 || k >= p->nseries) {
      app_log(a, "error: figure style K ...: K from 1 to %d", p->nseries);
      return;
    }
    while ((o = next_word(&rest)) != NULL) {
      FigSeries *s = &p->s[k];
      if (!strcmp(o, "color")) {
        char *c = next_word(&rest);
        unsigned v = c ? (unsigned)strtoul(c[0] == '#' ? c + 1 : c, NULL, 16) : 0;
        s->color[0] = ((v >> 16) & 255) / 255.0f;
        s->color[1] = ((v >> 8) & 255) / 255.0f;
        s->color[2] = (v & 255) / 255.0f;
        memcpy(s->mcolor, s->color, sizeof s->color);
      } else if (!strcmp(o, "line")) {
        static const char *names[] = {"none", "-", "--", ":", "-."};
        char *l = next_word(&rest);
        int i;
        for (i = 0; l && i < FIG_LINE_COUNT; i++)
          if (!strcmp(l, names[i])) s->line = i;
      } else if (!strcmp(o, "width")) {
        char *v = next_word(&rest);
        if (v) s->width = (float)atof(v);
      } else if (!strcmp(o, "marker")) {
        static const char *names[] = {"none", "o", "s", "^", "D", "x", "+", "*", "."};
        char *l = next_word(&rest);
        int i;
        for (i = 0; l && i < FIG_MARK_COUNT; i++)
          if (!strcmp(l, names[i])) s->marker = i;
      } else if (!strcmp(o, "scale")) {
        char *sx = next_word(&rest), *sy = next_word(&rest);
        if (sx && sy) {
          s->sx = strtod(sx, NULL);
          s->sy = strtod(sy, NULL);
        }
      } else if (!strcmp(o, "label")) {
        snprintf(s->label, FIG_TEXT, "%s", skip_space(rest));
        break;
      }
    }
    g->dirty = true;
    app_fig_sync_echo(a);
  } else if (!strcmp(w, "export")) {
    char *f = next_word(&rest), *dpi = next_word(&rest);
    if (dpi) g->fig.dpi = atoi(dpi);
    if (!f) app_log(a, "error: figure export FILE.pdf|svg|eps|png [DPI]");
    else app_fig_export(a, f);
  } else {
    app_log(a, "error: unknown figure command '%s'; try 'help'", w);
  }
}

void app_command(App *a, const char *line_in) {
  char line[1024], *rest = line, *cmd;
  snprintf(line, sizeof line, "%s", line_in);
  cmd = next_word(&rest);
  if (!cmd) return;
  for (char *c = cmd; *c; c++) *c = (char)tolower((unsigned char)*c);

  if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
    char *topic = next_word(&rest);
    if (topic)
      ui_show_help(a, topic);
    else
      app_log(a, "%s", usage);
  } else if (!strcmp(cmd, "run") || !strcmp(cmd, "go") || !strcmp(cmd, "ig")) {
    app_run(a);
  } else if (!strcmp(cmd, "continue") || !strcmp(cmd, "cc")) {
    char *w = next_word(&rest);
    app_continue(a, w ? atof(w) : 0.0);
  } else if (!strcmp(cmd, "set")) {
    cmd_set(a, rest);
    if (a->needs_run) app_run_live(a);
  } else if (!strcmp(cmd, "get")) {
    char *n = next_word(&rest);
    double v = 0;
    if (n && a->model_ok && a->eng->api.xppe_get(n, &v) == XPPE_OK)
      app_log(a, "%s = %.10g", n, v);
    else
      app_log(a, "error: no value named %s", n ? n : "?");
  } else if (!strcmp(cmd, "whos") || !strcmp(cmd, "who") || !strcmp(cmd, "list")) {
    cmd_whos(a);
  } else if (!strcmp(cmd, "ic")) {
    /* ic last | ic shift | ic read FILE | ic write FILE | ic NAME = FORMULA,
       NAME a variable or a range u[1..20] with [j] in the formula. */
    char line[2100], *w, *eq;
    bool file;
    snprintf(line, sizeof line, "%s", skip_space(rest));
    w = next_word(&rest);
    file = w && (!strcmp(w, "read") || !strcmp(w, "write"));
    if (file && *skip_space(rest)) {
      char *p = skip_space(rest);
      if (!strcmp(w, "read"))
        app_ic_read(a, p);
      else
        app_ic_write(a, p);
    } else if (w && !file && !strcmp(w, "last") && !*skip_space(rest)) {
      app_ic_last(a, false);
    } else if (w && !file && !strcmp(w, "shift") && !*skip_space(rest)) {
      app_ic_last(a, true);
    } else if (w && !file && (eq = strchr(line, '=')) != NULL) {
      char *f = skip_space(eq + 1), *end = f + strlen(f);
      while (end > f && isspace((unsigned char)end[-1])) *--end = '\0';
      for (end = eq; end > line && isspace((unsigned char)end[-1]); end--) {}
      *end = '\0';
      app_ic_formula(a, line, f);
    } else {
      app_log(a, "Write: ic last (from the end of the last run), ic shift (and "
                 "the time goes on), ic read FILE, ic write FILE, or ic NAME = "
                 "FORMULA (NAME a variable or u[1..20], [j] the index)");
    }
  } else if (!strcmp(cmd, "py")) {
    /* py CODE: a line of Python (indented as typed); py alone ends a block
       when one is open, else the command window takes Python. */
    char *code = rest;
    if (*code == ' ') code++; /* the space after py, not the indentation */
    if (*skip_space(code)) {
      pyconsole_send(a, code);
    } else if (pyconsole_waiting()) {
      pyconsole_send(a, ""); /* ends a block (and does nothing otherwise) */
    } else {
      a->py_mode = true;
      app_log(a, "The command window now takes Python (click py> or type :cmd to "
                 "go back to commands).");
      pyconsole_start(a);
    }
  } else if (!strcmp(cmd, "python")) {
    /* python [use PATH | stop]: the console's interpreter */
    char *w = next_word(&rest);
    if (w && !strcmp(w, "use")) {
      char *p = skip_space(rest);
      snprintf(a->python, sizeof a->python, "%s", p);
      pyconsole_stop(a);
      app_log(a, *p ? "The console will use %s." : "The console will look for Python "
                                                   "on the PATH.", p);
    } else if (w && (!strcmp(w, "stop") || !strcmp(w, "restart"))) {
      pyconsole_stop(a);
      app_log(a, "Python stopped; its variables are gone. The next Python line "
                 "starts a fresh one.");
    } else if (w && !strcmp(w, "fetch")) {
      /* python fetch [stop]: XPP Lab's own Python, downloaded once */
      char *x = next_word(&rest);
      if (x && (!strcmp(x, "stop") || !strcmp(x, "cancel")))
        pyfetch_cancel(a);
      else
        pyfetch_start(a);
    } else if (w && !strcmp(w, "install")) {
      /* python install PKG...: pip, with the console's Python */
      char *pkgs = skip_space(rest), py[1100];
      if (!*pkgs)
        app_log(a, "Write: python install PACKAGE ... (for example python install pandas)");
      else if (pyconsole_running())
        pyfetch_pip(a, pyconsole_python(), pkgs);
      else if (a->python[0])
        pyfetch_pip(a, a->python, pkgs);
      else if (pyfetch_python(py, sizeof py))
        pyfetch_pip(a, py, pkgs);
      else
        app_log(a, "error: start the console first (py), so that pip runs with its Python");
    } else {
      char own[1100];
      app_log(a, pyconsole_running() ? "Python is running: %s" : "Python is not running%s",
              pyconsole_running() ? pyconsole_python() : " (py CODE starts it).");
      if (pyfetch_python(own, sizeof own))
        app_log(a, "XPP Lab's own Python: %s", own);
      else
        app_log(a, "XPP Lab's own Python is not installed (python fetch installs it).");
    }
  } else if (!strcmp(cmd, "serve")) {
    /* serve [on | off]: the channel through which Python drives the app */
    char *w = next_word(&rest);
    if (w && !strcmp(w, "off")) {
      channel_stop(a);
      app_log(a, "Python can no longer connect (serve on lets it again).");
    } else if (w && strcmp(w, "on")) {
      app_log(a, "Write: serve, serve on or serve off");
    } else if (a->batch) {
      app_log(a, "error: xpplab-batch has no channel; drive the desktop app instead");
    } else if (channel_start(a)) {
      app_log(a, "Python can connect: import xpplab as xs; app = xs.app.connect() "
                 "(%d connected now).", channel_clients());
    } else {
      app_log(a, "error: the channel for Python could not be opened");
    }
  } else if (!strcmp(cmd, "kinescope") || !strcmp(cmd, "ks")) {
    cmd_kinescope(a, rest);
  } else if (!strcmp(cmd, "click")) {
    /* click X Y: a run from that point of the phase plane; click keep [off]
       keeps each such run (Initial conds > mIce). */
    char *x = next_word(&rest), *y = next_word(&rest);
    if (x && !strcmp(x, "keep")) {
      a->pp.click_keep = !(y && !strcmp(y, "off"));
      a->pp.click_to_start = a->pp.click_to_start || a->pp.click_keep;
    } else if (x && y && is_number(x) && is_number(y)) {
      app_click_start(a, atof(x), atof(y));
    } else {
      app_log(a, "Write: click X Y (a run from that point of the phase plane), "
                 "or click keep [off]");
    }
  } else if (!strcmp(cmd, "eq") || !strcmp(cmd, "equilibrium") ||
             !strcmp(cmd, "sg")) {
    char *w = next_word(&rest);
    if (w && !strcmp(w, "all")) {
      char *g = next_word(&rest);
      app_find_equilibria(a, g ? atoi(g) : 100);
    } else {
      app_equilibrium(a, NULL);
    }
  } else if (!strcmp(cmd, "nullclines") || !strcmp(cmd, "nn")) {
    /* nullclines [off] | freeze [PAR FROM TO N] | unfreeze | show K | play
       [off]: XPPAUT's Nullcline > New and Freeze. */
    char *w = next_word(&rest);
    if (w && !strcmp(w, "freeze")) {
      char *par = next_word(&rest), *from = next_word(&rest), *to = next_word(&rest),
           *n = next_word(&rest);
      int p = par ? app_find_par(a, par) : -1;
      if (!par)
        app_freeze_nullclines(a);
      else if (p >= 0 && from && to && n && is_number(from) && is_number(to) &&
               is_number(n))
        app_freeze_nullcline_range(a, p, atof(from), atof(to), atoi(n));
      else
        app_log(a, "Write: nullclines freeze, or nullclines freeze PAR FROM TO N "
                   "(N values of the parameter PAR)");
    } else if (w && !strcmp(w, "unfreeze")) {
      app_unfreeze_nullclines(a);
    } else if (w && !strcmp(w, "show")) {
      char *k = next_word(&rest);
      a->pp.show_frozen = k ? atoi(k) : 0;
      if (a->pp.show_frozen < 0 || a->pp.show_frozen > a->pp.nfrozen) a->pp.show_frozen = 0;
      a->pp.play = false;
    } else if (w && !strcmp(w, "play")) {
      char *k = next_word(&rest);
      a->pp.play = a->pp.nfrozen > 1 && !(k && !strcmp(k, "off"));
    } else {
      a->pp.nullclines = !(w && !strcmp(w, "off"));
      a->pp.nc_dirty = true;
      a->echo.nc_written = false;
    }
    a->show_phase = true;
  } else if (!strcmp(cmd, "dirfield") || !strcmp(cmd, "dd")) {
    char *w = next_word(&rest);
    a->pp.dirfield = w ? strcmp(w, "off") != 0 : !a->pp.dirfield;
    a->show_phase = true;
  } else if (!strcmp(cmd, "erase") || !strcmp(cmd, "e")) {
    app_clear_curves(a);
    app_clear_equilibria(a);
    app_status(a, "Erased kept curves and equilibria.");
  } else if (!strcmp(cmd, "freeze")) {
    app_freeze(a);
  } else if (!strcmp(cmd, "fit") || !strcmp(cmd, "wf")) {
    app_status(a, "Double-click a plot to fit it to the data.");
  } else if (!strcmp(cmd, "use")) {
    char *s = next_word(&rest);
    if (s) app_apply_set(a, s);
    if (a->needs_run) app_run_live(a);
  } else if (!strcmp(cmd, "sets")) {
    int i;
    for (i = 0; i < a->nsets; i++) app_log(a, "  %s", a->sets[i]);
    if (!a->nsets) app_log(a, "No parameter sets in this model.");
  } else if (!strcmp(cmd, "auto")) {
    cmd_auto(a, rest);
  } else if (!strcmp(cmd, "spectrum") || !strcmp(cmd, "psd") ||
             !strcmp(cmd, "fourier") || !strcmp(cmd, "fft") ||
             !strcmp(cmd, "power") || !strcmp(cmd, "hist") ||
             !strcmp(cmd, "hist2d") || !strcmp(cmd, "xcorr") ||
             !strcmp(cmd, "mean") || !strcmp(cmd, "prc") ||
             !strcmp(cmd, "hfun") || !strcmp(cmd, "fitdata") ||
             !strcmp(cmd, "ensemble") || !strcmp(cmd, "linearize") ||
             !strcmp(cmd, "linearise")) {
    cmd_analysis(a, cmd, rest);
  } else if (!strcmp(cmd, "poles")) {
    app_place_poles_text(a, rest);
  } else if (!strcmp(cmd, "locus")) {
    cmd_locus(a, rest);
  } else if (!strcmp(cmd, "lqr")) {
    app_lqr_text(a, rest);
  } else if (!strcmp(cmd, "closedloop") || !strcmp(cmd, "closed")) {
    char *w = next_word(&rest);
    if (w && !strcmp(w, "loop")) w = next_word(&rest);
    app_closed_loop(a, w);
  } else if (!strcmp(cmd, "many")) {
    cmd_many(a, rest);
  } else if (!strcmp(cmd, "shoot")) {
    cmd_shoot(a, rest);
  } else if (!strcmp(cmd, "figure") || !strcmp(cmd, "fig")) {
    cmd_figure(a, rest);
  } else if (!strcmp(cmd, "bvp")) {
    app_bvp(a);
  } else if (!strcmp(cmd, "ani")) {
    char *p = skip_space(rest);
    if (!strncmp(p, "play", 4) || !strncmp(p, "row ", 4) ||
        !strncmp(p, "stop", 4)) {
      /* ani play | ani stop | ani row N: drive the animator from scripts */
      a->ani.playing = p[0] == 'p';
      if (p[0] == 'r') a->ani.row = atoi(p + 4);
      a->show_ani = true;
      snprintf(a->focus, sizeof a->focus, "animator");
    } else if (!*p && a->ani.loaded) {
      a->show_ani = true;
      snprintf(a->focus, sizeof a->focus, "animator");
    } else if (!*p) {
      app_log(a, "Write: ani FILE.ani (the file's path, or its name next to "
                 "the model)");
    } else if (app_ani_load(a, p)) {
      a->show_ani = true;
      snprintf(a->focus, sizeof a->focus, "animator");
    }
  } else if (!strcmp(cmd, "view3d")) {
    /* view3d [X Y Z] | view3d spin [SPEED | off] | view3d angles THETA PHI */
    char *w[3];
    int k, c;
    for (k = 0; k < 3; k++) w[k] = next_word(&rest);
    if (w[0] && !strcmp(w[0], "spin")) {
      /* view3d spin [DEGREES_PER_SECOND | off] */
      a->v3.spin = !w[1] ? 24.0f : !strcmp(w[1], "off") ? 0.0f : (float)atof(w[1]);
      w[0] = NULL;
    } else if (w[0] && !strcmp(w[0], "angles")) {
      if (w[1]) a->v3.theta = (float)atof(w[1]);
      if (w[2]) a->v3.phi = (float)atof(w[2]);
      w[0] = NULL;
    }
    for (k = 0; k < 3 && w[k]; k++) {
      if ((c = col_index(a, w[k])) < 0) break;
      if (k == 0) a->v3.x = c;
      if (k == 1) a->v3.y = c;
      if (k == 2) a->v3.z = c;
    }
    a->show_view3d = true;
    snprintf(a->focus, sizeof a->focus, "3d");
  } else if (!strcmp(cmd, "sweep") || !strcmp(cmd, "range")) {
    /* range NAME FROM TO [STEPS] [NAME2 FROM2 TO2 [STEPS2]] [VAR | lyap |
       eq] [continue]: NAME is a parameter or a variable (its initial
       value); with VAR the max/min of VAR, with lyap the Liapunov exponent,
       otherwise keep the runs. A second NAME varies a second value with the
       first: on a grid when STEPS2 is given, else both together. */
    char *par = next_word(&rest), *from = next_word(&rest), *to = next_word(&rest);
    char *w, *wd[24];
    int p = par ? app_find_par(a, par) : -1, ic = -1, nw = 0, k, film_fig = -1;
    bool bad = false, film = false;
    if (p < 0 && par) ic = app_find_var(a, par);
    while (nw < 24 && (wd[nw] = next_word(&rest)) != NULL) nw++;
    if ((p < 0 && ic < 0) || !from || !to) {
      app_log(a, "Write: range NAME FROM TO [STEPS] [NAME2 FROM2 TO2 [STEPS2]] "
                 "[VAR | lyap | eq] [continue]   (NAME: a parameter or initial "
                 "value; no VAR keeps the runs; STEPS2 makes a grid)");
    } else {
      a->sw.over_ic = p < 0;
      a->sw.par = p >= 0 ? p : ic;
      a->sw.from = atof(from);
      a->sw.to = atof(to);
      a->sw.steps = 20;
      a->sw.mode = SWEEP_OVERLAY;
      a->sw.continue_runs = false;
      a->sw.two = false;
      for (k = 0; k < nw; k++) {
        int p2, v2;
        w = wd[k];
        p2 = app_find_par(a, w);
        v2 = p2 < 0 ? app_find_var(a, w) : -1;
        if ((p2 >= 0 || v2 >= 0) && k + 2 < nw && is_number(wd[k + 1]) &&
            is_number(wd[k + 2])) {
          a->sw.two = true;
          a->sw.over_ic2 = p2 < 0;
          a->sw.par2 = p2 >= 0 ? p2 : v2;
          a->sw.from2 = atof(wd[k + 1]);
          a->sw.to2 = atof(wd[k + 2]);
          a->sw.grid = false;
          k += 2;
          if (k + 1 < nw && is_number(wd[k + 1])) {
            a->sw.steps2 = atoi(wd[++k]);
            a->sw.grid = true;
          }
        } else if (is_number(w))
          a->sw.steps = atoi(w);
        else if (!strcmp(w, "continue"))
          a->sw.continue_runs = true;
        else if (!strcmp(w, "film"))
          film = true; /* each run captured by the Kinescope */
        else if (film && figure_word(w) >= 0)
          film_fig = figure_word(w);
        else if (!strcmp(w, "lyap") || !strcmp(w, "liapunov"))
          a->sw.mode = SWEEP_LYAPUNOV;
        else if (!strcmp(w, "eq") || !strcmp(w, "equilibria"))
          a->sw.mode = SWEEP_EQUILIBRIA;
        else if (app_find_var(a, w) >= 0) {
          if (a->sw.mode != SWEEP_EQUILIBRIA) a->sw.mode = SWEEP_MAXMIN;
          a->sw.var = app_find_var(a, w);
        } else if (k + 2 < nw && is_number(wd[k + 1]) && is_number(wd[k + 2])) {
          /* A second range under a name the model lacks: do not run the
             first alone with its numbers taken as steps. */
          app_log(a, "error: no parameter or variable called %s", w);
          bad = true;
          break;
        } else
          app_log(a, "note: ignored '%s'", w);
      }
      if (bad)
        ;
      else if (film && a->batch)
        app_log(a, "error: a film is pictures of the window: it needs the desktop app");
      else if (film)
        app_film_range(a, film_fig >= 0 ? film_fig : a->ks.source);
      else
        app_sweep(a);
    }
  } else if (!strcmp(cmd, "open")) {
    char *p = skip_space(rest);
    if (*p) app_open(a, p);
  } else if (!strcmp(cmd, "new")) {
    app_new_model(a);
  } else if (!strcmp(cmd, "save")) {
    char *p = skip_space(rest);
    if (*p)
      app_save_editor(a, p);
    else
      app_apply_editor(a);
  } else if (!strcmp(cmd, "export")) {
    /* export FILE.csv|FILE.mat [run|auto|range|analysis|equilibria|nullclines] |
       export FILE.svg|FILE.png [time|phase|auto|auto2] */
    char *file = next_word(&rest), *which = next_word(&rest);
    size_t n = file ? strlen(file) : 0;
    if (!file) {
      app_log(a, "Write export FILE.csv, or export FILE.svg phase.");
    } else if (n > 4 && (!SDL_strcasecmp(file + n - 4, ".svg") ||
                         !SDL_strcasecmp(file + n - 4, ".png"))) {
      FigureKind k = FIG_PHASE;
      bool ok = true;
      if (which && !strcmp(which, "time")) k = FIG_TIME;
      if (which && !strcmp(which, "auto")) k = FIG_AUTO;
      if (which && !strcmp(which, "auto2")) k = FIG_AUTO2;
      if (!SDL_strcasecmp(file + n - 4, ".svg")) {
        ok = export_svg(a, k, file);
      } else if (a->batch) {
        app_log(a, "error: a PNG is a picture of the window; without one, export "
                   "the figure as SVG");
        ok = false;
      } else {
        export_png(k, file);
      }
      if (ok) echo_figure(a, k, file);
    } else if (which && !strcmp(which, "auto")) {
      app_export_auto_csv(a, file);
    } else if (which && !strcmp(which, "range")) {
      app_export_range_csv(a, file);
    } else if (which && (!strcmp(which, "nullclines") || !strcmp(which, "nc"))) {
      app_export_nullclines_csv(a, file);
    } else if (which && !strcmp(which, "analysis")) {
      app_export_analysis_csv(a, file);
    } else if (which && !strcmp(which, "equilibria")) {
      app_export_equilibria_csv(a, file);
    } else if (which && !strcmp(which, "many")) {
      app_export_many_csv(a, file);
    } else if (which && !strcmp(which, "locus")) {
      app_export_locus_csv(a, file);
    } else if (which && strcmp(which, "run") != 0) {
      app_log(a, "error: export FILE.csv or FILE.mat takes run, auto, range, analysis, "
                 "equilibria, many or locus, not '%s'", which);
    } else {
      app_export_csv(a, file);
    }
  } else if (!strcmp(cmd, "script")) {
    char *p = skip_space(rest);
    if (*p)
      echo_save(a, p);
    else
      app_log(a, "Write: script FILE.py (saves the session so far as a Python "
                 "script that repeats it)");
  } else if (!strcmp(cmd, "echo")) {
    char *w = next_word(&rest);
    if (w) a->echo.console = strcmp(w, "off") != 0;
    app_log(a, "Actions are %sshown as Python in the command window.",
            a->echo.console ? "" : "no longer ");
  } else if (!strcmp(cmd, "show")) {
    char *w = next_word(&rest);
    if (w && !strcmp(w, "python")) { /* the Workspace's Python tab */
      a->reveal_python = true;
      a->show_workspace = true;
      snprintf(a->focus, sizeof a->focus, "workspace");
    } else if (w) {
      snprintf(a->focus, sizeof a->focus, "%s", w);
    }
  } else if (!strcmp(cmd, "hide")) {
    char *w = next_word(&rest);
    bool *shown = w ? window_flag(a, w) : NULL;
    if (shown)
      *shown = false;
    else
      app_log(a, "error: no window called %s", w ? w : "?");
  } else if (!strcmp(cmd, "layout")) {
    char *w = next_word(&rest);
    a->layout = w && !strcmp(w, "side");
    a->reset_layout = true;
  } else if (!strcmp(cmd, "theme")) {
    char *w = next_word(&rest);
    a->dark = w && !strcmp(w, "dark");
  } else if (!strcmp(cmd, "clc") || !strcmp(cmd, "clear")) {
    a->log_len = 0;
    a->log[0] = '\0';
  } else if (!strcmp(cmd, "palette")) {
    palette_command(a, rest);
  } else if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit") || !strcmp(cmd, "fq")) {
    a->quit = true;
  } else {
    app_log(a, "error: unknown command '%s'. Type 'help' for the list.", cmd);
  }
}
