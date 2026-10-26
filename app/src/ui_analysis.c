/* The Analysis window: XPPAUT's data-analysis tools (Numerics > stocHastic
   and Averaging) on the last run, with the result plotted beside them. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "ui.h"

static const char *about[AN_COUNT] = {
    "Power spectral density by Welch's method: the run is cut into windows "
    "that overlap by half, and their FFTs are averaged. Cross spectrum and "
    "coherence compare two columns. (XPPAUT: Numerics > stocHastic > "
    "Spec.dns)",
    "Fourier series of the whole run, treated as one period: frequencies "
    "are mode k divided by the length of the run. (stocHastic > Fourier, "
    "Power)",
    "Histogram of a column over the run, optionally counting only points "
    "where a condition holds, e.g. W>0.2. (stocHastic > Histogram)",
    "Two-dimensional histogram: the fraction of points in each cell of a "
    "grid over two columns. (stocHastic > 2D-hist)",
    "Auto- or cross-correlation of the mean-subtracted columns, directly or "
    "by FFT, with lags in time units. (stocHastic > Xcorrel)",
    "Mean and standard deviation of a column, and the largest Liapunov "
    "exponent of the run: negative at a stable equilibrium, near zero on a "
    "limit cycle, positive for chaos. (stocHastic > Mean, Liapunov)",
    "Phase response curves of a stable limit cycle. XPP Lab settles onto "
    "the cycle, measures its period, integrates exactly one period and "
    "computes XPPAUT's adjoint along it. Each column is how much a small "
    "kick in that variable advances the cycle, in time units. (Averaging > "
    "New adjoint)",
    "The interaction function H(phi) of two weakly coupled copies of the "
    "oscillator, averaged over one cycle with the adjoint. Give one coupling "
    "formula per variable; primed names (V') are the other oscillator's "
    "variables. (Averaging > Make H)",
    "Fit parameters and initial conditions to data with Levenberg-"
    "Marquardt. The file holds rows of numbers, time first; give the model "
    "variables to fit, their columns in the file (from 2) and the "
    "parameters to vary. (stocHastic > fIt data)",
    "Run a noisy model (wiener or ran()) many times from the same initial "
    "values, the random numbers going on from run to run, and average: the "
    "mean of every column at each time, with a band of one standard "
    "deviation. Choose a column's mean or variance under Show. The run "
    "left in the other windows is the last one. (stocHastic > Compute, Mean, "
    "Variance)",
    "The linear model dx' = A dx + B du near a point: A is the Jacobian of "
    "the right-hand side, B its derivative with respect to each input (the "
    "parameters that act as controls). The eigenvalues of A are plotted in "
    "the complex plane, and the rank of [B, AB, ..., A^(n-1) B] says whether "
    "the inputs can steer every state. The derivatives are extrapolated to "
    "zero step, so no step has to be chosen. Not in XPPAUT; in Python, "
    "m.linearize()."};

/* The result's view stays where the user zoomed it when the same result is
   computed again; another one, or another scale, is shown whole. */
static PlotView result_view;

static const char *spectra[] = {"Power (PSD)", "Cross spectrum", "Coherence"};

/* The part of a spectrum worth looking at: up to the last frequency with
   at least 1e-4 of the peak. */
static double spectrum_extent(const double *f, const double *p, int n) {
  double peak = 0;
  int i, last = n - 1;
  for (i = 1; i < n; i++)
    if (fabs(p[i]) > peak) peak = fabs(p[i]);
  for (i = n - 1; i > 0; i--)
    if (fabs(p[i]) >= 1e-4 * peak) {
      last = i;
      break;
    }
  last = last + last / 5 + 1;
  return f[last < n ? last : n - 1];
}

/* Frame x and y on the data (x up to xmax); for log axes only y > 0. */
static void frame_result(const double *x, const double *y, int n, double xmax,
                         bool log_y) {
  double ylo = 1e300, yhi = -1e300, xlo = x[0];
  int i;
  for (i = 0; i < n && x[i] <= xmax; i++) {
    if (log_y && y[i] <= 0) continue;
    if (y[i] < ylo) ylo = y[i];
    if (y[i] > yhi) yhi = y[i];
  }
  if (!(yhi > ylo)) {
    ylo = log_y ? 1e-6 : -1;
    yhi = 1;
  }
  if (log_y) {
    ylo *= 0.5;
    yhi *= 2;
  } else {
    double pad = 0.06 * (yhi - ylo);
    ylo -= pad;
    yhi += pad;
  }
  if (!(xmax > xlo)) xmax = xlo + 1;
  ImPlot_SetupAxesLimits(xlo, xmax, ylo, yhi, ImPlotCond_Always);
}
static const char *windows[] = {"Square", "Parzen", "Hamming", "Bartlett",
                                "Hann"};

static bool needs_two(const AnalysisUI *n) {
  return (n->tool == AN_SPECTRUM && n->spec_kind > 0) ||
         n->tool == AN_HISTOGRAM2D || n->tool == AN_CORRELATION;
}

/* ---- Linearise ----------------------------------------------------------- */

static PlotView plane_view;
static ImVec2_c plane_size; /* the plot's area in the last frame */

/* Whether name is in the list of inputs (names separated by commas or
   spaces), ignoring case. */
static bool has_input(const char *list, const char *name) {
  size_t len = strlen(name);
  const char *p = list;
  while (*p) {
    size_t w;
    p += strspn(p, " ,;");
    w = strcspn(p, " ,;");
    if (w == len && SDL_strncasecmp(p, name, len) == 0) return true;
    p += w;
  }
  return false;
}

/* The list with name added (on) or taken out, in the model's order. */
static void set_input(App *a, bool on, const char *name) {
  AnalysisUI *n = &a->an;
  char list[sizeof n->lin_inputs];
  size_t k = 0;
  int i;
  list[0] = '\0';
  for (i = 0; i < a->npars; i++) {
    bool in = SDL_strcasecmp(a->pars[i], name) == 0 ? on : has_input(n->lin_inputs, a->pars[i]);
    if (in && k + strlen(a->pars[i]) + 3 < sizeof list)
      k += (size_t)snprintf(list + k, sizeof list - k, "%s%s", k ? ", " : "", a->pars[i]);
  }
  snprintf(n->lin_inputs, sizeof n->lin_inputs, "%s", list);
}

static void linear_controls(App *a) {
  AnalysisUI *n = &a->an;
  float line = igGetFrameHeightWithSpacing();
  int i, rows = a->npars < 5 ? a->npars : 5;
  igTextDisabled("Linearise at");
  igRadioButton_IntPtr("the equilibrium", &n->lin_at, 0);
  Tooltip("Newton's method from the initial values finds it, as Run > Find "
          "equilibrium does.");
  igRadioButton_IntPtr("the initial values", &n->lin_at, 1);
  Tooltip("Wherever the initial values are, equilibrium or not.");
  igSpacing();
  igTextDisabled("Inputs");
  HelpMarker("The parameters that act as controls: B holds the derivatives "
             "with respect to them, and the controllability test asks whether "
             "they can steer every state.");
  if (a->npars == 0) {
    igTextDisabled("(the model has no parameters)");
    return;
  }
  if (igBeginChild_Str("##inputs", V2(-1, line * (float)rows + 8), ImGuiChildFlags_Borders, 0)) {
    for (i = 0; i < a->npars; i++) {
      bool on = has_input(n->lin_inputs, a->pars[i]);
      if (igCheckbox(a->pars[i], &on)) set_input(a, on, a->pars[i]);
    }
  }
  igEndChild();
}

static void count_unstable(const LinearSystem *L, int *unstable, int *edge) {
  int i;
  *unstable = *edge = 0;
  for (i = 0; i < L->n; i++) {
    double r = L->discrete ? hypot(L->re[i], L->im[i]) - 1.0 : L->re[i];
    if (r > 1e-9) (*unstable)++;
    else if (r >= -1e-9) (*edge)++;
  }
}

enum { GOOD, BAD, DOUBT };

/* Lighter on the dark theme, as the command window's colours. */
static ImVec4_c verdict_color(const App *a, int kind) {
  if (kind == GOOD) return a->dark ? HEX(0x81c784) : HEX(0x2e7d32);
  if (kind == BAD) return a->dark ? HEX(0xe57373) : HEX(0xd62728);
  return a->dark ? HEX(0xffb74d) : HEX(0xe65100);
}

/* What the numbers say, in words. */
static void linear_verdict(App *a) {
  const LinearSystem *L = &a->an.lin;
  char buf[1024];
  size_t k = 0;
  int i, unstable, edge;
  bool equilibrium = L->at_eq || L->residual <= 1e-8;
  k += (size_t)snprintf(buf, sizeof buf, "At %s",
                        L->at_eq ? "the equilibrium" : "the initial values");
  for (i = 0; i < L->n && i < 6; i++)
    k += (size_t)snprintf(buf + k, sizeof buf - k, "%s %s = %.6g", i ? "," : "",
                          a->vars[i], L->x[i]);
  if (L->n > 6) k += (size_t)snprintf(buf + k, sizeof buf - k, ", ...");
  if (!equilibrium)
    snprintf(buf + k, sizeof buf - k, ". This point is not an equilibrium "
             "(%s = %.3g there): A and B describe the motion near it, and the "
             "eigenvalues do not decide its stability.",
             L->discrete ? "|F(x) - x|" : "|dx/dt|", L->residual);
  else
    snprintf(buf + k, sizeof buf - k, ".");
  TextWrapped(buf);
  if (equilibrium) {
    count_unstable(L, &unstable, &edge);
    if (unstable > 0)
      snprintf(buf, sizeof buf, "Unstable: %d of %d eigenvalue%s %s.", unstable,
               L->n, L->n == 1 ? "" : "s",
               L->discrete ? "lie outside the unit circle"
                           : (unstable == 1 ? "has a positive real part"
                                            : "have positive real parts"));
    else if (edge > 0)
      snprintf(buf, sizeof buf, "On the edge of stability: %d eigenvalue%s %s, "
               "so the linear model cannot decide; the nonlinear terms do.",
               edge, edge == 1 ? "" : "s",
               L->discrete ? "on the unit circle" : "on the imaginary axis");
    else
      snprintf(buf, sizeof buf, "Stable: every eigenvalue %s.",
               L->discrete ? "lies inside the unit circle"
                           : "has a negative real part");
    igPushStyleColor_Vec4(ImGuiCol_Text, verdict_color(a, unstable > 0 ? BAD
                                                       : edge > 0  ? DOUBT
                                                                   : GOOD));
    TextWrapped(buf);
    igPopStyleColor(1);
  }
  if (L->m == 0) {
    igTextDisabled("Tick inputs on the left to test whether they can steer the "
                   "state.");
  } else if (L->rank < 0) {
    snprintf(buf, sizeof buf, "The controllability rank could not be computed: %s",
             L->rank_error);
    TextWrapped(buf);
  } else {
    char names[400];
    size_t q = 0;
    names[0] = '\0';
    for (i = 0; i < L->m && q < sizeof names - 80; i++)
      q += (size_t)snprintf(names + q, sizeof names - q, "%s%s",
                            i == 0 ? "" : i + 1 == L->m ? " and " : ", ",
                            L->inputs[i]);
    if (L->rank == L->n)
      snprintf(buf, sizeof buf, "Controllable: %s can steer every state (the "
               "controllability matrix has full rank, %d). The smallest of its "
               "singular values is %.2g of the largest; the smaller it is, the "
               "harder some direction is to move.",
               names, L->n, L->sv[0] > 0 ? L->sv[L->n - 1] / L->sv[0] : 0.0);
    else
      snprintf(buf, sizeof buf, "Not controllable: %s %s only %d of the %d "
               "directions of the state (the controllability matrix has rank "
               "%d). Add an input, or choose another one.",
               names, L->m == 1 ? "reaches" : "reach", L->rank, L->n, L->rank);
    igPushStyleColor_Vec4(ImGuiCol_Text, verdict_color(a, L->rank == L->n ? GOOD : BAD));
    TextWrapped(buf);
    igPopStyleColor(1);
  }
  {
    double scale = 1;
    for (i = 0; i < L->n * L->n; i++)
      if (fabs(L->A[i]) > scale) scale = fabs(L->A[i]);
    for (i = 0; i < L->n * L->m; i++)
      if (fabs(L->B[i]) > scale) scale = fabs(L->B[i]);
    if (L->worst > 1e-5 * scale) {
      snprintf(buf, sizeof buf, "The derivatives are only good to about %.1e: "
               "the right-hand side may not be smooth here (hover over an entry "
               "for its error).", L->worst);
      igPushStyleColor_Vec4(ImGuiCol_Text, verdict_color(a, DOUBT));
      TextWrapped(buf);
      igPopStyleColor(1);
    }
  }
}

/* A matrix as a table: rows named by the variables, columns by names. */
static void matrix_table(App *a, const char *id, const double *m,
                         const double *err, int rows, int cols,
                         char (*names)[APP_NAME_LEN]) {
  ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                       ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX |
                       ImGuiTableFlags_ScrollY;
  float h = igGetTextLineHeightWithSpacing() * (float)((rows < 8 ? rows : 8) + 1) + 6;
  int i, j;
  if (!igBeginTable(id, cols + 1, tf, V2(0, h), 0)) return;
  igTableSetupScrollFreeze(1, 1);
  igTableSetupColumn("", 0, 0, 0);
  for (j = 0; j < cols; j++) igTableSetupColumn(names[j], 0, 0, 0);
  igTableHeadersRow();
  for (i = 0; i < rows; i++) {
    igTableNextRow(0, 0);
    igTableSetColumnIndex(0);
    igTextDisabled("%s", a->vars[i]);
    for (j = 0; j < cols; j++) {
      double v = m[i * cols + j];
      igTableSetColumnIndex(j + 1);
      igText("%.6g", v);
      if (igIsItemHovered(0))
        igSetTooltip("d %s' / d %s = %.17g\nerror estimate %.1e", a->vars[i],
                     names[j], v, err[i * cols + j]);
    }
  }
  igEndTable();
}

/* Axis limits that show the eigenvalues and the edge of the stable region
   (Linearization.plot() in Python frames its figure the same way). */
static void plane_window(const LinearSystem *L, double w[4]) {
  double edge = L->discrete ? 1.0 : 0.0, lo = -edge, hi = edge, h = edge, span, pad;
  int i;
  for (i = 0; i < L->n; i++) {
    if (!isfinite(L->re[i]) || !isfinite(L->im[i])) continue;
    if (L->re[i] < lo) lo = L->re[i];
    if (L->re[i] > hi) hi = L->re[i];
    if (fabs(L->im[i]) > h) h = fabs(L->im[i]);
  }
  span = hi - lo > 2 * h ? hi - lo : 2 * h;
  if (!(span > 0)) span = 1;
  pad = 0.15 * span;
  lo -= pad;
  hi += pad;
  h = h + pad > 0.3 * (hi - lo) ? h + pad : 0.3 * (hi - lo);
  w[0] = lo;
  w[1] = hi;
  w[2] = -h;
  w[3] = h;
}

static void eigen_plane(App *a) {
  const LinearSystem *L = &a->an.lin;
  if (!ImPlot_BeginPlot("##plane", V2(-1, -1), ImPlotFlags_NoTitle)) return;
  /* Framed on the eigenvalues every frame until the user zooms or pans. */
  bool follow = ui_view_follow(a, &plane_view, "real part", "imaginary part");
  ImPlot_SetupAxes("real part", "imaginary part", 0, 0);
  /* outside, where it covers no eigenvalue */
  ImPlot_SetupLegend(ImPlotLocation_North,
                     ImPlotLegendFlags_Outside | ImPlotLegendFlags_Horizontal);
  if (follow) {
    double w[4];
    plane_window(L, w);
    if (L->discrete && plane_size.x > 0 && plane_size.y > 0) {
      /* one scale on both axes, so that the unit circle is round */
      double sx = (w[1] - w[0]) / plane_size.x, sy = (w[3] - w[2]) / plane_size.y;
      int k = sx > sy ? 2 : 0;
      double c = 0.5 * (w[k] + w[k + 1]);
      double half = 0.5 * (sx > sy ? sx * plane_size.y : sy * plane_size.x);
      w[k] = c - half;
      w[k + 1] = c + half;
    }
    ImPlot_SetupAxesLimits(w[0], w[1], w[2], w[3], ImPlotCond_Always);
  }
  {
    ImPlotRect_c lim = ImPlot_GetPlotLimits(IMPLOT_AUTO, IMPLOT_AUTO);
    ImPlotSpec_c fill = Spec(HEX(0x2ca02c), 0.0f), edge = Spec(V4(0.4f, 0.4f, 0.4f, 0.9f), 1.5f);
    ImPlotSpec_c axis = Spec(V4(0.5f, 0.5f, 0.5f, 0.5f), 1.0f);
    double zero = 0;
    fill.FillColor = HEX(0x2ca02c);
    fill.FillAlpha = 0.3f;
    if (L->discrete) {
      /* the unit circle, its inside shaded */
      double cx[129], up[129], down[129];
      int i;
      for (i = 0; i < 129; i++) {
        double t = 3.14159265358979323846 * i / 128;
        cx[i] = cos(t);
        up[i] = sin(t);
        down[i] = -up[i];
      }
      ImPlot_PlotShaded_doublePtrdoublePtrdoublePtr("stable", cx, up, down, 129, fill);
      ImPlot_PlotLine_doublePtrdoublePtr("##circle", cx, up, 129, edge);
      ImPlot_PlotLine_doublePtrdoublePtr("##circle2", cx, down, 129, edge);
    } else if (lim.X.Min < 0) {
      /* the left half-plane shaded, the imaginary axis drawn */
      double xs[2] = {lim.X.Min, 0}, lo[2] = {lim.Y.Min, lim.Y.Min},
             hi[2] = {lim.Y.Max, lim.Y.Max};
      ImPlot_PlotShaded_doublePtrdoublePtrdoublePtr("stable", xs, lo, hi, 2, fill);
    }
    if (!L->discrete) ImPlot_PlotInfLines_doublePtr("##imaginary axis", &zero, 1, edge);
    axis.Flags = ImPlotInfLinesFlags_Horizontal;
    ImPlot_PlotInfLines_doublePtr("##real axis", &zero, 1, axis);
  }
  {
    ImPlotSpec_c s = SpecMarker(HEX(0xd62728), ImPlotMarker_Cross, 7.0f);
    s.MarkerLineColor = HEX(0xd62728);
    s.LineWeight = 2.0f;
    ImPlot_PlotScatter_doublePtrdoublePtr("eigenvalues", L->re, L->im, L->n, s);
  }
  plane_size = ImPlot_GetPlotSize();
  ui_view_end(&plane_view, true);
  ImPlot_EndPlot();
}

static void heading(const char *title, const char *help) {
  igTextDisabled("%s", title);
  HelpMarker(help);
}

static void linear_result(App *a) {
  const LinearSystem *L = &a->an.lin;
  ImVec2_c avail;
  float left;
  int i;
  linear_verdict(a);
  igSpacing();
  avail = igGetContentRegionAvail();
  left = avail.x * 0.45f < 240 ? (avail.x < 240 ? avail.x : 240) : avail.x * 0.45f;
  if (igBeginChild_Str("##matrices", V2(left, 0), 0, 0)) {
    heading("A", "The Jacobian: row i, column j is d xi' / d xj, how the rate "
                 "of state i changes with state j. Hover over an entry for "
                 "all its digits and its error estimate.");
    matrix_table(a, "##A", L->A, L->A_err, L->n, L->n, a->vars);
    if (L->m > 0) {
      igSpacing();
      heading("B", "Row i, column k is d xi' / d uk, how the rate of state i "
                   "changes with input k.");
      matrix_table(a, "##B", L->B, L->B_err, L->n, L->m, L->inputs);
    }
    igSpacing();
    heading("Eigenvalues",
            L->at_eq ? "Of the Jacobian at the equilibrium, as Run > Find "
                       "equilibrium (XPPAUT's Sing pts) reports them."
                     : "Of A, by XPPAUT's own eigenvalue routine.");
    for (i = 0; i < L->n && i < 200; i++) {
      if (L->im[i] == 0)
        igText("%.6g", L->re[i]);
      else
        igText("%.6g %c %.6gi", L->re[i], L->im[i] < 0 ? '-' : '+', fabs(L->im[i]));
    }
    if (L->m > 0 && L->rank >= 0) {
      igSpacing();
      heading("Singular values", "Of the controllability matrix [B, AB, ..., "
                                 "A^(n-1) B]. The rank counts those above "
                                 "1e-8 times the largest (times the matrix's "
                                 "larger side).");
      for (i = 0; i < L->n && i < 200; i++) igText("%.6g", L->sv[i]);
    }
  }
  igEndChild();
  igSameLine(0, -1);
  if (igBeginChild_Str("##planebox", V2(0, 0), 0, 0)) eigen_plane(a);
  igEndChild();
}

static void column_combo(App *a, const char *label, int *col) {
  igSetNextItemWidth(-1);
  if (igBeginCombo(label, *col >= 0 && *col < a->ncols ? a->cols[*col] : "", 0)) {
    int j;
    for (j = 0; j < a->ncols; j++)
      if (igSelectable_Bool(a->cols[j], j == *col, 0, V2(0, 0))) *col = j;
    igEndCombo();
  }
}

static void controls(App *a) {
  AnalysisUI *n = &a->an;
  int i;
  igTextDisabled("Tool");
  igSetNextItemWidth(-1);
  if (igBeginCombo("##tool", app_analysis_name(n->tool), 0)) {
    for (i = 0; i < AN_COUNT; i++)
      if (igSelectable_Bool(app_analysis_name(i), i == n->tool, 0, V2(0, 0)))
        n->tool = i;
    igEndCombo();
  }
  igSpacing();
  if (n->tool <= AN_STATISTICS) {
    igTextDisabled(needs_two(n) ? "Columns" : "Column");
    column_combo(a, "##col", &n->col);
    if (needs_two(n)) column_combo(a, "##col2", &n->col2);
  }
  igPushItemWidth(-90);
  switch (n->tool) {
  case AN_SPECTRUM:
    if (igBeginCombo("Kind", spectra[n->spec_kind], 0)) {
      for (i = 0; i < 3; i++)
        if (igSelectable_Bool(spectra[i], i == n->spec_kind, 0, V2(0, 0)))
          n->spec_kind = i;
      igEndCombo();
    }
    igInputInt("Window", &n->spec_len, 64, 256, 0);
    Tooltip("Points per window. Longer windows resolve frequencies more "
            "finely but average fewer windows.");
    if (n->spec_len < 4) n->spec_len = 4;
    if (igBeginCombo("Taper", windows[n->spec_window], 0)) {
      for (i = 0; i < 5; i++)
        if (igSelectable_Bool(windows[i], i == n->spec_window, 0, V2(0, 0)))
          n->spec_window = i;
      igEndCombo();
    }
    if (igCheckbox("Log scale", &n->log_y)) {
      n->refit = true;
      result_view.zoomed = false;
    }
    break;
  case AN_FOURIER:
    igCheckbox("Amplitude and phase", &n->polar);
    Tooltip("Show sqrt(cos^2+sin^2) and the phase instead of the cos and sin "
            "coefficients.");
    break;
  case AN_HISTOGRAM:
  case AN_HISTOGRAM2D:
    igInputInt(n->tool == AN_HISTOGRAM ? "Bins" : "Bins (x)", &n->bins, 1, 10, 0);
    if (n->bins < 2) n->bins = 2;
    if (n->tool == AN_HISTOGRAM2D) {
      igInputInt("Bins (y)", &n->bins2, 1, 10, 0);
      if (n->bins2 < 2) n->bins2 = 2;
    }
    igCheckbox("Range from the data", &n->auto_range);
    if (!n->auto_range) {
      InputDouble("Low", &n->lo, "%.6g");
      InputDouble("High", &n->hi, "%.6g");
      if (n->tool == AN_HISTOGRAM2D) {
        InputDouble("Low (y)", &n->lo2, "%.6g");
        InputDouble("High (y)", &n->hi2, "%.6g");
      }
    }
    if (n->tool == AN_HISTOGRAM) {
      igInputText("Condition", n->condition, sizeof n->condition, 0, NULL, NULL);
      Tooltip("Count only the points where this XPPAUT expression is nonzero, "
              "e.g. W>0.2. Leave empty to count all.");
    }
    break;
  case AN_CORRELATION:
    igInputInt("Lags", &n->lags, 10, 50, 0);
    if (n->lags < 2) n->lags = 2;
    igCheckbox("By FFT", &n->use_fft);
    Tooltip("Faster for long runs; the direct sum is XPPAUT's default.");
    break;
  case AN_PRC:
  case AN_HFUNCTION:
    InputDouble("Transient", &n->transient, "%.6g");
    Tooltip("Time to integrate before measuring the period, so the run "
            "settles onto the cycle. 0 uses the total time.");
    if (n->tool == AN_HFUNCTION && n->coupling) {
      igPopItemWidth();
      igSpacing();
      igTextDisabled("Coupling, one formula per variable");
      igPushItemWidth(-60);
      for (i = 0; i < a->nvars; i++) {
        igPushID_Int(i);
        igInputText(a->vars[i], n->coupling[i], sizeof n->coupling[i], 0, NULL,
                    NULL);
        igPopID();
      }
    }
    break;
  case AN_LINEARIZE:
    igPopItemWidth();
    linear_controls(a);
    igPushItemWidth(-90);
    break;
  case AN_ENSEMBLE:
    igInputInt("Runs", &n->trials, 10, 100, 0);
    if (n->trials < 2) n->trials = 2;
    if (n->trials > 100000) n->trials = 100000;
    Tooltip("How many runs to average.");
    break;
  case AN_FIT:
    igPopItemWidth();
    igTextDisabled("Data file (time, then columns)");
    igSetNextItemWidth(-1);
    igInputText("##file", n->fit_file, sizeof n->fit_file, 0, NULL, NULL);
    igPushItemWidth(-90);
    igInputText("Variables", n->fit_vars, sizeof n->fit_vars, 0, NULL, NULL);
    Tooltip("Model variables to fit, separated by commas.");
    igInputText("Columns", n->fit_cols, sizeof n->fit_cols, 0, NULL, NULL);
    Tooltip("Their columns in the file, counting time as column 1.");
    igInputText("Vary", n->fit_pars, sizeof n->fit_pars, 0, NULL, NULL);
    Tooltip("Parameters and initial conditions to fit, separated by commas. "
            "They start from their current values.");
    break;
  default:
    break;
  }
  igPopItemWidth();
  igSpacing();
  igPushStyleColor_Vec4(ImGuiCol_Button, HEX(0x2e7d32));
  igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, HEX(0x388e3c));
  igPushStyleColor_Vec4(ImGuiCol_Text, HEX(0xffffff));
  if (ButtonW("Compute", -1)) app_analysis_run(a);
  igPopStyleColor(3);
  igSpacing();
  igPushTextWrapPos(0);
  igTextDisabled("%s", about[n->tool]);
  igPopTextWrapPos();
}

static void plot_result(App *a) {
  AnalysisUI *n = &a->an;
  Trajectory *r = &n->res;
  const double *x;
  int j;
  if (n->have && n->res_tool == AN_LINEARIZE && n->lin.A) {
    linear_result(a);
    return;
  }
  if (n->summary[0]) {
    igPushTextWrapPos(0);
    igTextUnformatted(n->summary, NULL);
    igPopTextWrapPos();
  }
  if (!n->have || r->ncols < 2 || r->nrows < 1) {
    if (!n->summary[0])
      igTextDisabled("Choose a tool and press Compute. The tools analyse the "
                     "last run.");
    return;
  }
  /* Tables with several result columns: choose what to plot. */
  if (n->res_tool != AN_HISTOGRAM2D && r->ncols > 2) {
    igSetNextItemWidth(160);
    if (igBeginCombo("Show", r->names[n->show_col], 0)) {
      for (j = 1; j < r->ncols; j++)
        if (igSelectable_Bool(r->names[j], j == n->show_col, 0, V2(0, 0))) {
          n->show_col = j;
          n->refit = true;
        }
      igEndCombo();
    }
    igSameLine(0, -1);
  }
  /* No Fit button: it is taller than this line of text and would move the
     plot down. */
  igTextDisabled("%d rows. %s", r->nrows,
                 result_view.zoomed ? "Zoomed: double-click to see all."
                                    : "Right-click the plot for options.");
  x = r->cols[0];
  if (ImPlot_BeginPlot("##analysis", V2(-1, -1), ImPlotFlags_NoTitle)) {
    const bool map = n->res_tool == AN_HISTOGRAM2D && r->ncols >= 3;
    const int show = n->show_col < r->ncols ? n->show_col : 1;
    const char *xl = map ? a->cols[n->col] : r->names[0];
    const char *yl = map ? a->cols[n->col2] : r->names[show];
    bool was_zoomed = result_view.zoomed;
    bool follow = ui_view_follow(a, &result_view, xl, yl);
    if (follow && was_zoomed) n->refit = true; /* back to the framed view */
    if (map) {
      /* cells are x-major: index i + j * nx */
      int nx = n->bins, ny = n->bins2, i, k;
      double *grid = malloc(sizeof(double) * (size_t)(nx * ny));
      double hi = 0;
      ImPlotPoint_c bmin = {n->lo, n->lo2}, bmax = {n->hi, n->hi2};
      for (k = 0; k < nx * ny && k < r->nrows; k++)
        if (r->cols[2][k] > hi) hi = r->cols[2][k];
      for (j = 0; j < ny; j++)
        for (i = 0; i < nx; i++) {
          int src = i + j * nx;
          grid[(ny - 1 - j) * nx + i] = src < r->nrows ? r->cols[2][src] : 0;
        }
      ImPlotAxisFlags fit = follow ? ImPlotAxisFlags_AutoFit : 0;
      ImPlot_SetupAxes(xl, yl, fit, fit);
      ImPlot_PushColormap_PlotColormap(ImPlotColormap_Viridis);
      ImPlot_PlotHeatmap_doublePtr("##h2", grid, ny, nx, 0, hi > 0 ? hi : 1, NULL,
                                   bmin, bmax, Spec(V4(0, 0, 0, -1), 1.0f));
      ImPlot_PopColormap(1);
      free(grid);
    } else {
      const double *y = r->cols[show];
      bool log_y = n->res_tool == AN_SPECTRUM && n->log_y;
      ImPlot_SetupAxes(xl, yl, 0, 0);
      if (log_y) ImPlot_SetupAxisScale_PlotScale(ImAxis_Y1, ImPlotScale_Log10);
      if (n->refit && follow) {
        double xmax = n->res_tool == AN_SPECTRUM || n->res_tool == AN_FOURIER
                          ? spectrum_extent(x, y, r->nrows)
                          : x[r->nrows - 1];
        frame_result(x, y, r->nrows, xmax, log_y);
        n->refit = false;
      }
      if (n->res_tool == AN_HISTOGRAM) {
        double w = r->nrows > 1 ? x[1] - x[0] : 1;
        ImPlotSpec_c s = Spec(HEX(0x1f77b4), 1.0f);
        s.FillColor = HEX(0x1f77b4);
        ImPlot_PlotBars_doublePtrdoublePtr("count", x, y, r->nrows, w * 0.9, s);
      } else {
        /* A mean with its band of one standard deviation. */
        if (n->res_tool == AN_ENSEMBLE && n->show_col % 2 == 1 && n->show_col + 1 < r->ncols) {
          double *lo = malloc(sizeof(double) * (size_t)r->nrows);
          double *hi = malloc(sizeof(double) * (size_t)r->nrows);
          int i;
          if (lo && hi) {
            ImPlotSpec_c band = Spec(HEX(0xd62728), 0.0f);
            for (i = 0; i < r->nrows; i++) {
              double v = r->cols[n->show_col + 1][i], sd = v > 0 ? sqrt(v) : 0;
              lo[i] = y[i] - sd;
              hi[i] = y[i] + sd;
            }
            band.FillColor = HEX(0xd62728);
            band.FillAlpha = 0.25f;
            ImPlot_PlotShaded_doublePtrdoublePtrdoublePtr("one standard deviation", x, lo,
                                                         hi, r->nrows, band);
          }
          free(lo);
          free(hi);
        }
        ImPlot_PlotLine_doublePtrdoublePtr(r->names[n->show_col], x, y, r->nrows,
                                           Spec(HEX(0xd62728), 2.0f));
        if (n->res_tool == AN_PRC || n->res_tool == AN_HFUNCTION ||
            n->res_tool == AN_CORRELATION) {
          double zero = 0;
          ImPlotSpec_c line = Spec(V4(0.5f, 0.5f, 0.5f, 0.6f), 1.0f);
          line.Flags = ImPlotInfLinesFlags_Horizontal;
          ImPlot_PlotInfLines_doublePtr("##zero", &zero, 1, line);
        }
      }
    }
    ui_view_end(&result_view, false);
    ImPlot_EndPlot();
  }
}

void ui_analysis(App *a) {
  if (!igBegin(WIN_ANALYSIS, &a->show_analysis, 0)) {
    igEnd();
    return;
  }
  if (!a->model_ok) {
    igTextDisabled("Open a model, run it, then analyse the run here.");
    igEnd();
    return;
  }
  if (igBeginChild_Str("##controls", V2(290, 0), ImGuiChildFlags_Borders, 0))
    controls(a);
  igEndChild();
  igSameLine(0, -1);
  if (igBeginChild_Str("##result", V2(0, 0), 0, 0)) plot_result(a);
  igEndChild();
  igEnd();
}
