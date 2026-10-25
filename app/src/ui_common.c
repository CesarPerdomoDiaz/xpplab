/* The main window: dock layout, keyboard shortcuts, theme, file dialogs. */
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "export.h"
#include "ui.h"

UiFonts g_fonts;

void PushHeading(void) {
  igPushFont(g_fonts.ui, igGetStyle()->FontSizeBase * 1.2f);
}

void PushTitle(void) {
  igPushFont(g_fonts.ui, igGetStyle()->FontSizeBase * 1.55f);
}

/* ---- Helpers ------------------------------------------------------------ */

bool Button(const char *label) { return igButton(label, V2(0, 0)); }

bool ButtonW(const char *label, float width) {
  return igButton(label, V2(width, 0));
}

void Tooltip(const char *text) {
  if (igIsItemHovered(ImGuiHoveredFlags_DelayShort) && igBeginTooltip()) {
    igPushTextWrapPos(igGetFontSize() * 30.0f);
    igTextUnformatted(text, NULL);
    igPopTextWrapPos();
    igEndTooltip();
  }
}

void HelpMarker(const char *text) {
  igSameLine(0, -1);
  igTextDisabled("(?)");
  Tooltip(text);
}

void TextWrapped(const char *text) {
  igPushTextWrapPos(0.0f);
  igTextUnformatted(text, NULL);
  igPopTextWrapPos();
}

ImPlotSpec_c Spec(ImVec4_c color, float weight) {
  ImPlotSpec_c s;
  memset(&s, 0, sizeof s);
  s.LineColor = color;
  s.LineWeight = weight;
  s.FillColor = V4(0, 0, 0, -1);
  s.FillAlpha = 1.0f;
  s.Marker = ImPlotMarker_None;
  s.MarkerSize = 4;
  s.MarkerLineColor = V4(0, 0, 0, -1);
  s.MarkerFillColor = V4(0, 0, 0, -1);
  s.Size = 4;
  s.Offset = 0;
  s.Stride = IMPLOT_AUTO;
  s.Flags = 0;
  return s;
}

ImPlotSpec_c SpecMarker(ImVec4_c color, int marker, float size) {
  ImPlotSpec_c s = Spec(color, 1.5f);
  s.Marker = marker;
  s.MarkerSize = size;
  s.MarkerLineColor = color;
  s.MarkerFillColor = color;
  return s;
}

bool InputDouble(const char *label, double *v, const char *fmt) {
  return igInputDouble(label, v, 0, 0, fmt ? fmt : "%.6g",
                       ImGuiInputTextFlags_EnterReturnsTrue);
}

bool Combo(const char *label, int *current, char (*items)[APP_NAME_LEN],
           int count) {
  bool changed = false;
  int i;
  const char *preview =
      (*current >= 0 && *current < count) ? items[*current] : "(none)";
  if (igBeginCombo(label, preview, 0)) {
    for (i = 0; i < count; i++) {
      bool sel = i == *current;
      if (igSelectable_Bool(items[i], sel, 0, V2(0, 0))) {
        *current = i;
        changed = true;
      }
    }
    igEndCombo();
  }
  return changed;
}

/* ---- Theme ------------------------------------------------------------------ */

void ui_apply_theme(App *a) {
  ImGuiStyle *s = igGetStyle();
  if (a->dark) {
    igStyleColorsDark(s);
    s->Colors[ImGuiCol_WindowBg] = HEX(0x1e1f22);
    s->Colors[ImGuiCol_ChildBg] = HEX(0x1e1f22);
    s->Colors[ImGuiCol_TitleBgActive] = HEX(0x2b2d30);
    s->Colors[ImGuiCol_Tab] = HEX(0x2b2d30);
  } else {
    igStyleColorsLight(s);
    s->Colors[ImGuiCol_WindowBg] = HEX(0xf7f7f8);
    s->Colors[ImGuiCol_TitleBgActive] = HEX(0xdfe6f0);
    s->Colors[ImGuiCol_TitleBg] = HEX(0xeef1f5);
    s->Colors[ImGuiCol_MenuBarBg] = HEX(0xeef1f5);
  }
  s->WindowRounding = 4;
  s->FrameRounding = 4;
  s->GrabRounding = 4;
  s->TabRounding = 4;
  s->PopupRounding = 4;
  s->ScrollbarRounding = 6;
  s->FramePadding = V2(7, 4);
  s->ItemSpacing = V2(8, 6);
  s->WindowPadding = V2(10, 8);
  s->WindowBorderSize = 1;
  s->FrameBorderSize = a->dark ? 0 : 1;
  if (a->dark)
    ImPlot_StyleColorsDark(NULL);
  else
    ImPlot_StyleColorsLight(NULL);
}

/* ---- File dialogs ------------------------------------------------------------- */

enum { DLG_NONE, DLG_OPEN, DLG_SAVE_AS, DLG_EXPORT, DLG_PNG, DLG_SVG, DLG_ANI,
       DLG_SCRIPT, DLG_DATA, DLG_AUTO_SAVE, DLG_AUTO_OPEN, DLG_IC_READ, DLG_IC_WRITE };
static SDL_Mutex *dlg_lock;
static int dlg_kind;
static char dlg_result[1024];
static bool dlg_ready;
static int dlg_figure; /* the figure a PNG/SVG export is for */

static void SDLCALL dialog_done(void *userdata, const char *const *files,
                                int filter) {
  (void)filter;
  SDL_LockMutex(dlg_lock);
  if (files && files[0]) {
    snprintf(dlg_result, sizeof dlg_result, "%s", files[0]);
    dlg_kind = (int)(intptr_t)userdata;
    dlg_ready = true;
  }
  SDL_UnlockMutex(dlg_lock);
}

static const SDL_DialogFileFilter ode_filters[] = {
    {"XPPAUT models (*.ode)", "ode"}, {"All files", "*"}};
static const SDL_DialogFileFilter csv_filters[] = {
    {"CSV files (*.csv)", "csv"}, {"All files", "*"}};

void ui_dialog_open(App *a) {
  if (!dlg_lock) dlg_lock = SDL_CreateMutex();
  SDL_ShowOpenFileDialog(dialog_done, (void *)(intptr_t)DLG_OPEN,
                         (SDL_Window *)a->window, ode_filters, 2,
                         a->examples_dir, false);
}

static const SDL_DialogFileFilter ani_filters[] = {
    {"XPPAUT animations (*.ani)", "ani"}, {"All files", "*"}};

void ui_dialog_open_ani(App *a) {
  char dir[1024];
  const char *slash;
  if (!dlg_lock) dlg_lock = SDL_CreateMutex();
  /* Start in the model's folder, where its animations usually are. */
  slash = strrchr(a->model_path, '/');
  if (!slash || strrchr(a->model_path, '\\') > slash)
    slash = strrchr(a->model_path, '\\');
  snprintf(dir, sizeof dir, "%.*s", slash ? (int)(slash - a->model_path) : 0,
           a->model_path);
  SDL_ShowOpenFileDialog(dialog_done, (void *)(intptr_t)DLG_ANI,
                         (SDL_Window *)a->window, ani_filters, 2,
                         dir[0] ? dir : NULL, false);
}

void ui_dialog_save_as(App *a) {
  char copy[1024];
  const char *start = a->model_path[0] ? a->model_path : NULL;
  if (!dlg_lock) dlg_lock = SDL_CreateMutex();
  /* For an example, start where Save would put its copy, not in the app. */
  if (start && app_is_example(a, start) &&
      app_example_copy_path(a, copy, sizeof copy))
    start = copy;
  SDL_ShowSaveFileDialog(dialog_done, (void *)(intptr_t)DLG_SAVE_AS,
                         (SDL_Window *)a->window, ode_filters, 2, start);
}

void ui_dialog_export_csv(App *a) {
  if (!dlg_lock) dlg_lock = SDL_CreateMutex();
  SDL_ShowSaveFileDialog(dialog_done, (void *)(intptr_t)DLG_EXPORT,
                         (SDL_Window *)a->window, csv_filters, 2, NULL);
}

static const SDL_DialogFileFilter py_filters[] = {
    {"Python scripts (*.py)", "py"}, {"All files", "*"}};

void ui_dialog_save_script(App *a) {
  if (!dlg_lock) dlg_lock = SDL_CreateMutex();
  SDL_ShowSaveFileDialog(dialog_done, (void *)(intptr_t)DLG_SCRIPT,
                         (SDL_Window *)a->window, py_filters, 2, NULL);
}

static const SDL_DialogFileFilter auto_filters[] = {
    {"AUTO diagrams (*.auto)", "auto"}, {"All files", "*"}};

void ui_dialog_auto_file(App *a, bool save) {
  if (!dlg_lock) dlg_lock = SDL_CreateMutex();
  if (save)
    SDL_ShowSaveFileDialog(dialog_done, (void *)(intptr_t)DLG_AUTO_SAVE,
                           (SDL_Window *)a->window, auto_filters, 2, NULL);
  else
    SDL_ShowOpenFileDialog(dialog_done, (void *)(intptr_t)DLG_AUTO_OPEN,
                           (SDL_Window *)a->window, auto_filters, 2, NULL, false);
}

static const SDL_DialogFileFilter ic_filters[] = {
    {"Initial values (*.dat, *.txt)", "dat;txt"}, {"All files", "*"}};

void ui_dialog_ics(App *a, bool save) {
  if (!dlg_lock) dlg_lock = SDL_CreateMutex();
  if (save)
    SDL_ShowSaveFileDialog(dialog_done, (void *)(intptr_t)DLG_IC_WRITE,
                           (SDL_Window *)a->window, ic_filters, 2, NULL);
  else
    SDL_ShowOpenFileDialog(dialog_done, (void *)(intptr_t)DLG_IC_READ,
                           (SDL_Window *)a->window, ic_filters, 2, NULL, false);
}

/* With a dot and an extension, or the one given. */
static void with_extension(const char *path, const char *ext, char *out, size_t len) {
  size_t n = strlen(path), e = strlen(ext);
  snprintf(out, len, "%s%s", path,
           (n < e || SDL_strcasecmp(path + n - e, ext) != 0) ? ext : "");
}

void ui_dialog_poll(App *a) {
  char path[1024];
  int kind;
  if (!dlg_lock) return;
  SDL_LockMutex(dlg_lock);
  kind = dlg_ready ? dlg_kind : DLG_NONE;
  snprintf(path, sizeof path, "%s", dlg_result);
  dlg_ready = false;
  SDL_UnlockMutex(dlg_lock);
  if (kind == DLG_OPEN) app_open(a, path);
  if (kind == DLG_ANI && app_ani_load(a, path)) a->show_ani = true;
  if (kind == DLG_SAVE_AS) {
    char p[1100];
    size_t n = strlen(path);
    snprintf(p, sizeof p, "%s%s", path,
             (n < 4 || SDL_strcasecmp(path + n - 4, ".ode") != 0) ? ".ode" : "");
    if (app_save_editor(a, p)) app_apply_editor(a);
  }
  if (kind == DLG_EXPORT) app_export_csv(a, path);
  if (kind == DLG_SCRIPT) {
    char p[1100];
    with_extension(path, ".py", p, sizeof p);
    echo_save(a, p);
  }
  if (kind == DLG_AUTO_SAVE) {
    char p[1100];
    with_extension(path, ".auto", p, sizeof p);
    app_auto_save(a, p);
  }
  if (kind == DLG_AUTO_OPEN) app_auto_load(a, path);
  if (kind == DLG_IC_READ) app_ic_read(a, path);
  if (kind == DLG_IC_WRITE) {
    /* .dat unless the name has an extension of its own */
    const char *base = strrchr(path, '/'), *b2 = strrchr(path, '\\');
    char p[1100];
    if (b2 > base) base = b2;
    snprintf(p, sizeof p, "%s%s", path, strchr(base ? base : path, '.') ? "" : ".dat");
    app_ic_write(a, p);
  }
  if (kind == DLG_DATA) {
    char p[1100];
    with_extension(path, ".csv", p, sizeof p);
    if (dlg_figure == FIG_AUTO || dlg_figure == FIG_AUTO2)
      app_export_auto_csv(a, p);
    else
      app_export_csv(a, p);
  }
  if (kind == DLG_PNG || kind == DLG_SVG) {
    char p[1100];
    bool ok = true;
    with_extension(path, kind == DLG_PNG ? ".png" : ".svg", p, sizeof p);
    if (kind == DLG_PNG)
      export_png((FigureKind)dlg_figure, p);
    else
      ok = export_svg(a, (FigureKind)dlg_figure, p);
    if (ok) echo_figure(a, dlg_figure, p);
  }
}

static const SDL_DialogFileFilter png_filters[] = {{"PNG images (*.png)", "png"}};
static const SDL_DialogFileFilter svg_filters[] = {{"SVG vector figures (*.svg)", "svg"}};

void ui_export_button(App *a, int figure) {
  bool diagram = figure == FIG_AUTO || figure == FIG_AUTO2;
  if (Button("Export...")) igOpenPopup_Str("export", 0);
  Tooltip("Save this figure: a PNG image exactly as shown, or an SVG vector "
          "figure for papers and posters. Or save its numbers as CSV.");
  if (igBeginPopup("export", 0)) {
    int kind = DLG_NONE;
    if (igMenuItem_Bool("PNG image (as shown)...", NULL, false, true)) kind = DLG_PNG;
    if (igMenuItem_Bool("SVG vector figure...", NULL, false, true)) kind = DLG_SVG;
    if (igMenuItem_Bool(diagram ? "Diagram points as CSV..." : "Run as CSV...", NULL,
                        false, diagram ? a->au.count > 0 : a->have_traj))
      kind = DLG_DATA;
    if (kind != DLG_NONE) {
      if (!dlg_lock) dlg_lock = SDL_CreateMutex();
      dlg_figure = figure;
      SDL_ShowSaveFileDialog(dialog_done, (void *)(intptr_t)kind,
                             (SDL_Window *)a->window,
                             kind == DLG_PNG   ? png_filters
                             : kind == DLG_SVG ? svg_filters
                                               : csv_filters,
                             1, NULL);
    }
    igEndPopup();
  }
}

void ui_figure_limits(int figure) {
  ImPlotRect_c lim = ImPlot_GetPlotLimits(ImAxis_X1, ImAxis_Y1);
  g_figures[figure].xmin = lim.X.Min;
  g_figures[figure].xmax = lim.X.Max;
  g_figures[figure].ymin = lim.Y.Min;
  g_figures[figure].ymax = lim.Y.Max;
}

void ui_figure_rect(int figure) {
  ImVec2_c lo = igGetItemRectMin(), hi = igGetItemRectMax();
  g_figures[figure].x0 = lo.x;
  g_figures[figure].y0 = lo.y;
  g_figures[figure].x1 = hi.x;
  g_figures[figure].y1 = hi.y;
  g_figures[figure].valid = true;
}

/* ---- Keyboard ------------------------------------------------------------------- */

static bool chord(ImGuiKeyChord c) {
  return igShortcut_Nil(c, ImGuiInputFlags_RouteGlobal);
}

/* XPPAUT's two-key commands, for its users' muscle memory. */
static void classic_keys(App *a) {
  static const struct {
    char k1, k2;
    const char *command;
  } map[] = {
      {'i', 'g', "run"},      {'i', 'l', "ic last"},  {'i', 's', "ic shift"},
      {'n', 'n', "nullclines"},
      {'n', 'e', "nullclines off"}, {'d', 'd', "dirfield"},
      {'s', 'g', "eq"},       {'e', 0, "erase"},      {'w', 'f', "fit"},
      {'f', 'q', "quit"},     {'c', 'c', "continue"},
  };
  ImGuiIO *io = igGetIO_Nil();
  int i, k;
  if (!a->classic_keys || io->WantTextInput || io->KeyCtrl || io->KeyAlt ||
      io->KeySuper)
    return;
  for (k = ImGuiKey_A; k <= ImGuiKey_Z; k++) {
    char c;
    if (!igIsKeyPressed_Bool((ImGuiKey)k, false)) continue;
    c = (char)('a' + (k - ImGuiKey_A));
    if (a->classic_prefix == 0) {
      for (i = 0; i < (int)(sizeof map / sizeof map[0]); i++)
        if (map[i].k1 == c && map[i].k2 == 0) {
          app_command(a, map[i].command);
          return;
        }
      for (i = 0; i < (int)(sizeof map / sizeof map[0]); i++)
        if (map[i].k1 == c) {
          a->classic_prefix = c;
          app_status(a, "XPP keys: %c ...", c - 32);
          return;
        }
    } else {
      char p = a->classic_prefix;
      a->classic_prefix = 0;
      for (i = 0; i < (int)(sizeof map / sizeof map[0]); i++)
        if (map[i].k1 == p && map[i].k2 == c) {
          app_status(a, "XPP keys: %c %c", p - 32, c - 32);
          app_command(a, map[i].command);
          return;
        }
      app_status(a, "XPP keys: %c %c is not a command here", p - 32, c - 32);
    }
  }
  if (igIsKeyPressed_Bool(ImGuiKey_Escape, false)) a->classic_prefix = 0;
}

static void shortcuts(App *a) {
  if (chord(ImGuiMod_Ctrl | ImGuiKey_O)) ui_dialog_open(a);
  if (chord(ImGuiMod_Ctrl | ImGuiKey_S)) {
    if (a->model_path[0])
      app_apply_editor(a);
    else
      ui_dialog_save_as(a);
  }
  if (chord(ImGuiMod_Ctrl | ImGuiKey_N)) app_new_model(a);
  if (chord(ImGuiMod_Ctrl | ImGuiKey_Q)) a->quit = true;
  if (chord(ImGuiKey_F5)) app_run(a);
  if (chord(ImGuiKey_F1)) {
    a->show_help = true;
    igSetWindowFocus_Str(WIN_HELP);
  }
  if (chord(ImGuiMod_Ctrl | ImGuiKey_Enter)) app_apply_editor(a);
  classic_keys(a);
}

/* ---- Layout ------------------------------------------------------------------------ */

static void default_layout(App *a, ImGuiID dock, ImVec2_c size) {
  ImGuiID left, rest, left_top, left_bottom, right, center, bottom, beside;
  igDockBuilderRemoveNode(dock);
  igDockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
  igDockBuilderSetNodeSize(dock, size);
  igDockBuilderSplitNode(dock, ImGuiDir_Left, 0.22f, &left, &rest);
  igDockBuilderSplitNode(left, ImGuiDir_Up, 0.38f, &left_top, &left_bottom);
  /* With the manual closed, its column goes to the plots. */
  if (a->show_help)
    igDockBuilderSplitNode(rest, ImGuiDir_Right, 0.27f, &right, &center);
  else
    right = center = rest;
  igDockBuilderSplitNode(center, ImGuiDir_Down, 0.27f, &bottom, &center);
  beside = center;
  if (a->layout == 1)
    igDockBuilderSplitNode(center, ImGuiDir_Right, 0.5f, &beside, &center);
  igDockBuilderDockWindow(WIN_BROWSER, left_top);
  igDockBuilderDockWindow(WIN_WORKSPACE, left_bottom);
  igDockBuilderDockWindow(WIN_TIME, beside);
  igDockBuilderDockWindow(WIN_PHASE, center);
  igDockBuilderDockWindow(WIN_AUTO, center);
  igDockBuilderDockWindow(WIN_ARRAY, center);
  igDockBuilderDockWindow(WIN_ANALYSIS, center);
  igDockBuilderDockWindow(WIN_VIEW3D, center);
  igDockBuilderDockWindow(WIN_ANI, center);
  igDockBuilderDockWindow(WIN_EDITOR, center);
  igDockBuilderDockWindow(WIN_CONSOLE, bottom);
  igDockBuilderDockWindow(WIN_DATA, bottom);
  igDockBuilderDockWindow(WIN_HELP, right);
  igDockBuilderFinish(dock);
}

void ui_draw(App *a) {
  const ImGuiViewport *vp = igGetMainViewport();
  const float toolbar_h = igGetFrameHeight() + 14.0f;
  const float status_h = igGetFrameHeight() + 4.0f;
  ImGuiID dock;
  ImGuiWindowFlags host =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;

  ui_dialog_poll(a);
  shortcuts(a);
  ui_menu_bar(a);

  /* Toolbar, dock area and status bar stacked inside the work area. */
  igSetNextWindowPos(vp->WorkPos, ImGuiCond_Always, V2(0, 0));
  igSetNextWindowSize(V2(vp->WorkSize.x, toolbar_h), ImGuiCond_Always);
  igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, V2(8, 6));
  if (igBegin("##toolbar", NULL, host | ImGuiWindowFlags_NoScrollbar))
    ui_toolbar(a, toolbar_h);
  igEnd();
  igPopStyleVar(1);

  igSetNextWindowPos(V2(vp->WorkPos.x, vp->WorkPos.y + toolbar_h),
                     ImGuiCond_Always, V2(0, 0));
  igSetNextWindowSize(V2(vp->WorkSize.x, vp->WorkSize.y - toolbar_h - status_h),
                      ImGuiCond_Always);
  igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, V2(0, 0));
  igBegin("##dockhost", NULL, host | ImGuiWindowFlags_NoBackground);
  igPopStyleVar(1);
  /* The id has the old name, XPP Studio: saved layouts refer to it. */
  dock = igGetID_Str("XPPStudioDock");
  if (a->reset_layout) {
    default_layout(a, dock, V2(vp->WorkSize.x, vp->WorkSize.y - toolbar_h - status_h));
    a->reset_layout = false;
  }
  igDockSpace(dock, V2(0, 0), 0, NULL);
  igEnd();

  igSetNextWindowPos(V2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - status_h),
                     ImGuiCond_Always, V2(0, 0));
  igSetNextWindowSize(V2(vp->WorkSize.x, status_h), ImGuiCond_Always);
  igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, V2(8, 2));
  if (igBegin("##status", NULL, host | ImGuiWindowFlags_NoScrollbar))
    ui_status_bar(a, status_h);
  igEnd();
  igPopStyleVar(1);

  if (a->show_browser) ui_browser(a);
  if (a->show_workspace) ui_workspace(a);
  if (a->show_editor) ui_editor(a);
  if (a->show_time) ui_time_plot(a);
  if (a->show_phase) ui_phase_plot(a);
  if (a->show_auto) ui_auto(a);
  if (a->show_console) ui_console(a);
  if (a->show_help) ui_help(a);
  if (a->show_data) ui_data(a);
  if (a->show_array) ui_array_plot(a);
  if (a->show_analysis) ui_analysis(a);
  if (a->show_view3d) ui_view3d(a);
  if (a->show_ani) ui_animator(a);
  if (a->show_demo) {
    igShowDemoWindow(&a->show_demo);
    ImPlot_ShowDemoWindow(&a->show_demo);
  }

  if (a->focus[0]) {
    static const struct {
      const char *key, *win;
      bool *shown;
    } map[] = {{"time", WIN_TIME, NULL},      {"phase", WIN_PHASE, NULL},
               {"auto", WIN_AUTO, NULL},      {"editor", WIN_EDITOR, NULL},
               {"data", WIN_DATA, NULL},      {"help", WIN_HELP, NULL},
               {"console", WIN_CONSOLE, NULL}, {"workspace", WIN_WORKSPACE, NULL},
               {"models", WIN_BROWSER, NULL}, {"array", WIN_ARRAY, NULL},
               {"analysis", WIN_ANALYSIS, NULL}, {"3d", WIN_VIEW3D, NULL},
               {"animator", WIN_ANI, NULL}};
    int i;
    for (i = 0; i < (int)(sizeof map / sizeof map[0]); i++)
      if (SDL_strncasecmp(a->focus, map[i].key, strlen(a->focus)) == 0) {
        if (i == 0) a->show_time = true;
        if (i == 1) a->show_phase = true;
        if (i == 2) a->show_auto = true;
        if (i == 3) a->show_editor = true;
        if (i == 4) a->show_data = true;
        if (i == 5) a->show_help = true;
        if (i == 9) a->show_array = true;
        if (i == 10) a->show_analysis = true;
        if (i == 11) a->show_view3d = true;
        if (i == 12) a->show_ani = true;
        igSetWindowFocus_Str(map[i].win);
        break;
      }
    a->focus[0] = '\0';
  }

  /* Live mode: rerun after parameter edits, at most once per frame. */
  if (a->needs_run) app_run_live(a);
  /* The session's Python: a slider or field is written once let go. */
  echo_frame(a, igIsAnyItemActive() || igIsMouseDown_Nil(ImGuiMouseButton_Left));
}
