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
    "parameters to vary. (stocHastic > fIt data)"};

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
        ImPlot_PlotLine_doublePtrdoublePtr(r->names[n->show_col], x, y, r->nrows,
                                           Spec(HEX(0xd62728), 2.0f));
        if (n->res_tool == AN_PRC || n->res_tool == AN_HFUNCTION ||
            n->res_tool == AN_CORRELATION) {
          double zero = 0;
          ImPlot_PlotInfLines_doublePtr("##zero", &zero, 1,
                                        Spec(V4(0.5f, 0.5f, 0.5f, 0.6f), 1.0f));
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
