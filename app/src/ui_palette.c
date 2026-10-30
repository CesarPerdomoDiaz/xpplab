/* The command palette: a search box over the main window that finds any
   menu action or command by words of its name or description and runs it
   (the table and the search are in actions.c). */
#include <stdio.h>
#include <string.h>

#include "ui.h"

#define VISIBLE_ROWS 9

void ui_palette(App *a) {
  static int hits[PALETTE_MAX_HITS];
  CmdPalette *p = &a->pal;
  const ImGuiViewport *vp = igGetMainViewport();
  const float lh = igGetTextLineHeight();
  const float gap = igGetStyle()->ItemSpacing.y;
  const float row_h = 2 * lh + 8 + gap; /* a row and the space below it */
  const float width = lh * 38 < vp->WorkSize.x - 40 ? lh * 38 : vp->WorkSize.x - 40;
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize;
  bool first, enter = false, clicked_row = false;
  int n, k, move = 0, run = -1;

  if (!p->open) return;
  first = p->focus; /* the key that opened it is not meant for it */
  n = palette_search(a, p->query, hits, PALETTE_MAX_HITS);
  if (p->sel >= n) p->sel = n - 1;
  if (p->sel < 0) p->sel = 0;

  if (!first) {
    if (igIsKeyPressed_Bool(ImGuiKey_Escape, false)) p->open = false;
    if (igIsKeyPressed_Bool(ImGuiKey_DownArrow, true) && p->sel + 1 < n) move = 1;
    if (igIsKeyPressed_Bool(ImGuiKey_UpArrow, true) && p->sel > 0) move = -1;
    p->sel += move;
    enter = igIsKeyPressed_Bool(ImGuiKey_Enter, false) ||
            igIsKeyPressed_Bool(ImGuiKey_KeypadEnter, false);
  }

  igSetNextWindowPos(V2(vp->WorkPos.x + vp->WorkSize.x / 2, vp->WorkPos.y + 70),
                     ImGuiCond_Always, V2(0.5f, 0));
  if (first) igSetNextWindowFocus();
  igPushStyleVar_Float(ImGuiStyleVar_WindowBorderSize, 1.0f);
  igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, V2(8, 8));
  if (igBegin("Command palette##palette", NULL, flags)) {
    if (p->focus) {
      igSetKeyboardFocusHere(0);
      p->focus = false;
    }
    igSetNextItemWidth(width);
    if (igInputTextWithHint("##query", "Type part of the name of an action or a command",
                            p->query, sizeof p->query, 0, NULL, NULL))
      p->sel = 0;
    if (igBeginChild_Str("##hits", V2(width, row_h * (n < VISIBLE_ROWS ? (n ? n : 1) : VISIBLE_ROWS)),
                         0, 0)) {
      if (!n) igTextDisabled("Nothing matches.");
      for (k = 0; k < n; k++) {
        int e = hits[k];
        const char *why, *sc = palette_shortcut(a, e);
        bool ok = palette_available(a, e, &why);
        char label[256], second[512];
        ImVec2_c pos, size;
        ImU32 col = igGetColorU32_Col(ok ? ImGuiCol_Text : ImGuiCol_TextDisabled, 1.0f);
        ImDrawList *dl = igGetWindowDrawList();
        float avail = igGetContentRegionAvail().x;
        igPushID_Int(e);
        pos = igGetCursorScreenPos();
        if (igSelectable_Bool("##row", k == p->sel, 0, V2(0, row_h - gap))) {
          p->sel = k;
          clicked_row = true;
        }
        if (move && k == p->sel) igSetScrollHereY(0.5f);
        palette_label(a, e, label, sizeof label);
        ImDrawList_AddText_Vec2(dl, V2(pos.x + 6, pos.y + 2), col, label, NULL);
        if (sc[0]) {
          size = igCalcTextSize(sc, NULL, false, -1);
          ImDrawList_AddText_Vec2(dl, V2(pos.x + avail - size.x - 6, pos.y + 2),
                                  igGetColorU32_Col(ImGuiCol_TextDisabled, 1.0f), sc, NULL);
        }
        if (ok)
          snprintf(second, sizeof second, "%s", palette_description(a, e));
        else
          snprintf(second, sizeof second, "Not available: %s", why);
        ImDrawList_AddText_Vec2(dl, V2(pos.x + 6, pos.y + lh + 4),
                                igGetColorU32_Col(ImGuiCol_TextDisabled, 1.0f), second, NULL);
        igPopID();
      }
    }
    igEndChild();
    if (!first && igIsMouseClicked_Bool(ImGuiMouseButton_Left, false) &&
        !igIsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
      p->open = false;
  }
  igEnd();
  igPopStyleVar(2);

  if (n && (enter || clicked_row)) run = hits[p->sel];
  if (run >= 0) {
    const char *why;
    if (palette_available(a, run, &why)) {
      palette_run(a, run);
    } else {
      char label[256];
      palette_label(a, run, label, sizeof label);
      app_status(a, "%s is not available: %s", label, why);
    }
  }
}
