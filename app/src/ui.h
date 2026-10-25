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
void ui_dialog_open_ani(App *a);

/* Native file dialogs (results arrive on a later frame). */
void ui_dialog_open(App *a);
void ui_dialog_save_as(App *a);
void ui_dialog_export_csv(App *a);
void ui_dialog_save_script(App *a);
void ui_dialog_auto_file(App *a, bool save); /* an .auto diagram */
void ui_dialog_poll(App *a);

/* An "Export" button with PNG and SVG choices for a figure; and the
   bookkeeping each plot does so its figure can be exported. */
void ui_export_button(App *a, int figure);
void ui_figure_limits(int figure); /* call inside BeginPlot/EndPlot */
void ui_figure_rect(int figure);   /* call right after EndPlot */

/* Show a help page (by file name, without .md). */
void ui_show_help(App *a, const char *page);

void ui_apply_theme(App *a);

#endif
