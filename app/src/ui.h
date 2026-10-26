/* Shared declarations for the user-interface panels (ui_*.c). */
#ifndef XPPS_UI_H
#define XPPS_UI_H

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimplot.h"

#include "app.h"

/* Window names double as docking IDs: keep them stable. */
#define WIN_BROWSER "Models"
#define WIN_WORKSPACE "Workspace"
#define WIN_EDITOR "Editor"
#define WIN_TIME "Time Series"
#define WIN_PHASE "Phase Plane"
#define WIN_AUTO "Bifurcation"
#define WIN_CONSOLE "Command Window"
#define WIN_HELP "Help"
#define WIN_DATA "Data"
#define WIN_ARRAY "Array Plot"
#define WIN_ANALYSIS "Analysis"
#define WIN_VIEW3D "3D View"
#define WIN_ANI "Animator"
#define WIN_KINE "Kinescope"

/* The shortcut modifier as shown in menus. ImGui maps its Ctrl shortcuts
   to the Command key on macOS. */
#ifdef __APPLE__
#define MOD "Cmd+"
#else
#define MOD "Ctrl+"
#endif

typedef struct {
  ImFont *ui, *mono;
} UiFonts;
extern UiFonts g_fonts;

/* Larger text for headings; pop with igPopFont(). */
void PushHeading(void);
void PushTitle(void);

static inline ImVec2_c V2(float x, float y) {
  ImVec2_c v = {x, y};
  return v;
}
static inline ImVec4_c V4(float r, float g, float b, float a) {
  ImVec4_c v = {r, g, b, a};
  return v;
}
static inline ImVec4_c HEX(unsigned rgb) {
  return V4(((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f,
            (rgb & 255) / 255.0f, 1.0f);
}

/* XPPAUT's bifurcation-diagram colours. */
#define COL_STABLE_EQ HEX(0xd62728)
#define COL_UNSTABLE_EQ HEX(0x333333)
#define COL_STABLE_PER HEX(0x2ca02c)
#define COL_UNSTABLE_PER HEX(0x1f77b4)
#define COL_SPECIAL HEX(0xff7f0e)
#define COL_XNULL HEX(0xd62728)
#define COL_YNULL HEX(0x2ca02c)

/* Short forms for cimgui calls that take many default arguments. */
bool Button(const char *label);
bool ButtonW(const char *label, float width);
void Tooltip(const char *text);
void HelpMarker(const char *text);
void TextWrapped(const char *text);
ImPlotSpec_c Spec(ImVec4_c color, float weight);
ImPlotSpec_c SpecMarker(ImVec4_c color, int marker, float size);
bool InputDouble(const char *label, double *v, const char *fmt);
bool Combo(const char *label, int *current, char (*items)[APP_NAME_LEN],
           int count);

/* Panels. */
void ui_draw(App *a);
void ui_menu_bar(App *a);
void ui_toolbar(App *a, float height);
void ui_status_bar(App *a, float height);
void ui_browser(App *a);
void ui_workspace(App *a);
void ui_editor(App *a);
void ui_time_plot(App *a);
void ui_phase_plot(App *a);
void ui_array_plot(App *a);
void ui_auto(App *a);
void ui_console(App *a);
void ui_help(App *a);
void ui_data(App *a);
void ui_analysis(App *a);
void ui_view3d(App *a);
void ui_animator(App *a);
void ui_kinescope(App *a);
void ui_dialog_open_ani(App *a);

/* Native file dialogs (results arrive on a later frame). */
void ui_dialog_open(App *a);
void ui_dialog_save_as(App *a);
void ui_dialog_export_csv(App *a);
void ui_dialog_save_script(App *a);
void ui_dialog_auto_file(App *a, bool save); /* an .auto diagram */
void ui_dialog_ics(App *a, bool save);       /* initial values */
void ui_dialog_python(App *a);               /* the console's Python */
void ui_dialog_kinescope(App *a, bool gif);  /* an animated GIF, or PNG frames */
void ui_dialog_poll(App *a);

/* An "Export" button with PNG and SVG choices for a figure; and the
   bookkeeping each plot does so its figure can be exported. */
void ui_export_button(App *a, int figure);
void ui_figure_limits(int figure); /* call inside BeginPlot/EndPlot */
void ui_figure_rect(int figure);   /* call right after EndPlot */

/* A plot that follows its data: its axes fit the data as it changes, until
   the user zooms or pans it (wheel, left drag, right-drag box). Then the
   view stays where the user put it, even as new data arrive, until a
   double-click in the plot, its Fit button, another model, or the plot
   showing other quantities. */
typedef struct {
  bool zoomed;     /* the user has moved the view */
  bool over;       /* the plot or an axis was under the mouse ... */
  int over_frame;  /* ... at the end of this frame */
  bool held[2];    /* the left / right button went down over the plot */
  unsigned shows;  /* the model and quantities the view was set on */
} PlotView;

/* This frame, read before ImPlot handles the mouse. */
typedef struct {
  int frame;           /* ImGui's frame count */
  unsigned shows;      /* what the plot shows: its quantities and model */
  bool wheel;
  bool clicked[2];     /* left, right: pressed this frame */
  bool down[2];
  bool dragging[2];    /* moved past ImGui's drag threshold */
  bool double_click;   /* left */
} PlotInput;

/* The follow / stop / resume decision, kept apart from ImGui so that it
   can be checked on its own (tests/plot_view_check.c). Returns whether
   the axes follow the data. */
static inline bool plot_view_update(PlotView *v, const PlotInput *in) {
  /* Only last frame's hover counts, not one from before a hidden tab. */
  bool over = v->over && v->over_frame == in->frame - 1;
  int b;
  /* Zoomed on other quantities, or on another model's: show all. */
  if (v->shows != in->shows) {
    v->shows = in->shows;
    v->zoomed = false;
  }
  /* A drag pans or zooms the plot only if it began over it. */
  for (b = 0; b < 2; b++) {
    if (in->clicked[b]) v->held[b] = over;
    else if (!in->down[b]) v->held[b] = false;
  }
  if (over && in->double_click)
    v->zoomed = false;
  else if ((over && in->wheel) || (v->held[0] && in->dragging[0]) ||
           (v->held[1] && in->dragging[1]))
    v->zoomed = true;
  return !v->zoomed;
}

/* Call after BeginPlot, before the Setup calls: true while the axes should
   fit the data. It reads this frame's mouse before ImPlot does, so the
   first turn of the wheel already zooms. x and y say what the axes show
   (their labels, or NULL): when they change, the view follows again. */
bool ui_view_follow(App *a, PlotView *v, const char *x, const char *y);
/* Call just before EndPlot. hint: write "Zoomed..." in the plot's corner
   while zoomed, for plots without a toolbar to show it. */
void ui_view_end(PlotView *v, bool hint);
/* On the toolbar's line: the "Zoomed..." hint while zoomed, else
   `otherwise` (when not NULL). */
void ui_view_hint(const PlotView *v, const char *otherwise);

/* Show a help page (by file name, without .md). */
void ui_show_help(App *a, const char *page);

void ui_apply_theme(App *a);

#endif
