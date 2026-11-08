/* Annotations on a plot: the Annotate menu and the drawing of text, arrows
   and markers in data coordinates (app_notes.c keeps them). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "export.h"
#include "ui.h"

static const char *kind_names[] = {"text", "arrow", "marker"};
static const char *marker_items[] = {"circle", "square", "triangle", "diamond",
                                     "x", "plus", "star", "point"};

static FigNote *notes_of(App *a, int where, int *n) {
  if (where == APP_NOTE_FIGURE) {
    FigPanel *p = &a->fg.fig.p[a->fg.panel < a->fg.fig.npanels ? a->fg.panel : 0];
    *n = p->nnote;
    return p->note;
  }
  *n = a->nt.n[where];
  return a->nt.note[where];
}

static void arm(App *a, int where, int kind) {
  a->nt.mode = kind;
  a->nt.plot = where;
  a->nt.have_start = false;
  app_status(a, kind == FIG_NOTE_ARROW ? "Click where the arrow starts."
                                       : "Click where the %s goes.", kind_names[kind]);
}

void ui_notes_menu(App *a, int where) {
  NoteUI *t = &a->nt;
  int n, i, remove = -1;
  FigNote *list;
  if (Button("Annotate...")) igOpenPopup_Str("annotate", 0);
  Tooltip("Add text, an arrow or a marker at a point of the plot, in its own "
          "coordinates (XPPAUT's Text, etc. menu). They stay with the plot "
          "through new runs, and the Figure window and the SVG export draw them.");
  if (t->mode >= 0 && t->plot == where) {
    igSameLine(0, -1);
    igTextDisabled("Click the plot to place the %s (Esc cancels).", kind_names[t->mode]);
    if (igIsKeyPressed_Bool(ImGuiKey_Escape, false)) {
      t->mode = -1;
      t->have_start = false;
    }
  }
  if (!igBeginPopup("annotate", 0)) return;
  igSetNextItemWidth(260);
  igInputText("text", t->text, sizeof t->text, 0, NULL, NULL);
  Tooltip("Mathtext between dollar signs works in the figure: $\\Gamma$, $V_W$.");
  igSetNextItemWidth(120);
  igSliderFloat("size (pt)", &t->pt, 0, 48, t->pt > 0 ? "%.0f" : "default", 0);
  igSetNextItemWidth(120);
  {
    int m = t->marker > 0 ? t->marker - 1 : 0;
    if (igCombo_Str_arr("marker", &m, marker_items, 8, 8)) t->marker = m + 1;
  }
  igColorEdit3("colour", t->rgb, ImGuiColorEditFlags_NoInputs);
  if (Button("Text")) {
    arm(a, where, FIG_NOTE_TEXT);
    igCloseCurrentPopup();
  }
  igSameLine(0, -1);
  if (Button("Arrow")) {
    arm(a, where, FIG_NOTE_ARROW);
    igCloseCurrentPopup();
  }
  igSameLine(0, -1);
  if (Button("Marker")) {
    arm(a, where, FIG_NOTE_MARKER);
    igCloseCurrentPopup();
  }
  list = notes_of(a, where, &n);
  if (n > 0) igSeparator();
  for (i = 0; i < n; i++) {
    char item[300];
    igPushID_Int(i);
    if (list[i].kind == FIG_NOTE_TEXT)
      snprintf(item, sizeof item, "text \"%.40s\"", list[i].text);
    else
      snprintf(item, sizeof item, "%s at %.4g, %.4g", kind_names[list[i].kind], list[i].x,
               list[i].y);
    igTextUnformatted(item, NULL);
    igSameLine(0, -1);
    if (igSmallButton("delete")) remove = i;
    igPopID();
  }
  if (n > 1 && Button("Delete all")) remove = -2;
  if (remove >= 0) app_note_clear(a, where, remove);
  if (remove == -2) app_note_clear(a, where, -1);
  igEndPopup();
}

static ImU32 colour_of(const FigNote *n) {
  if (n->rgb[0] == 0 && n->rgb[1] == 0 && n->rgb[2] == 0)
    return igGetColorU32_Col(ImGuiCol_Text, 1); /* the default follows the theme */
  return igColorConvertFloat4ToU32(V4(n->rgb[0], n->rgb[1], n->rgb[2], 1));
}

static void marker_shape(ImDrawList *dl, int m, ImVec2_c c, float r, ImU32 col) {
  switch (m) {
  case FIG_MARK_SQUARE:
    ImDrawList_AddRectFilled(dl, V2(c.x - r, c.y - r), V2(c.x + r, c.y + r), col, 0, 0);
    break;
  case FIG_MARK_TRIANGLE:
    ImDrawList_AddTriangleFilled(dl, V2(c.x, c.y - r), V2(c.x + r, c.y + r),
                                 V2(c.x - r, c.y + r), col);
    break;
  case FIG_MARK_DIAMOND:
    ImDrawList_AddQuadFilled(dl, V2(c.x, c.y - r), V2(c.x + r, c.y), V2(c.x, c.y + r),
                             V2(c.x - r, c.y), col);
    break;
  case FIG_MARK_X:
    ImDrawList_AddLine(dl, V2(c.x - r, c.y - r), V2(c.x + r, c.y + r), col, 2, 0);
    ImDrawList_AddLine(dl, V2(c.x - r, c.y + r), V2(c.x + r, c.y - r), col, 2, 0);
    break;
  case FIG_MARK_PLUS:
    ImDrawList_AddLine(dl, V2(c.x - r, c.y), V2(c.x + r, c.y), col, 2, 0);
    ImDrawList_AddLine(dl, V2(c.x, c.y - r), V2(c.x, c.y + r), col, 2, 0);
    break;
  case FIG_MARK_STAR:
    ImDrawList_AddLine(dl, V2(c.x - r, c.y), V2(c.x + r, c.y), col, 2, 0);
    ImDrawList_AddLine(dl, V2(c.x, c.y - r), V2(c.x, c.y + r), col, 2, 0);
    ImDrawList_AddLine(dl, V2(c.x - r, c.y - r), V2(c.x + r, c.y + r), col, 1.5f, 0);
    ImDrawList_AddLine(dl, V2(c.x - r, c.y + r), V2(c.x + r, c.y - r), col, 1.5f, 0);
    break;
  default:
    ImDrawList_AddCircleFilled(dl, c, m == FIG_MARK_POINT ? r * 0.5f : r, col, 0);
  }
}

void ui_notes_draw(App *a, int where) {
  ImDrawList *dl = ImPlot_GetPlotDrawList();
  float scale = igGetStyle()->FontScaleDpi > 0 ? igGetStyle()->FontScaleDpi : 1;
  int i;
  ImPlot_PushPlotClipRect(0);
  for (i = 0; i < a->nt.n[where]; i++) {
    const FigNote *n = &a->nt.note[where][i];
    ImPlotPoint_c p1 = {n->x, n->y}, p2 = {n->x2, n->y2};
    ImVec2_c s1 = ImPlot_PlotToPixels_PlotPoint(p1, ImAxis_X1, ImAxis_Y1);
    ImU32 col = colour_of(n);
    if (n->kind == FIG_NOTE_TEXT) {
      /* the baseline is at the point, as in the figure */
      float px = (n->pt > 0 ? n->pt : 12) * (96.0f / 72.0f) * scale;
      ImDrawList_AddText_FontPtr(dl, igGetFont(), px, V2(s1.x, s1.y - px * 0.8f), col, n->text,
                                 NULL, 0, NULL);
    } else if (n->kind == FIG_NOTE_ARROW) {
      ImVec2_c s2 = ImPlot_PlotToPixels_PlotPoint(p2, ImAxis_X1, ImAxis_Y1);
      float dx = s2.x - s1.x, dy = s2.y - s1.y, len = sqrtf(dx * dx + dy * dy);
      if (len > 0.5f) {
        float ux = dx / len, uy = dy / len, head = 9 * scale, half = 4.5f * scale;
        float bx = len > head ? s2.x - ux * head : s1.x, by = len > head ? s2.y - uy * head : s1.y;
        ImDrawList_AddLine(dl, s1, V2(bx, by), col, 1.5f * scale, 0);
        ImDrawList_AddTriangleFilled(dl, s2, V2(s2.x - ux * head - uy * half, s2.y - uy * head + ux * half),
                                     V2(s2.x - ux * head + uy * half, s2.y - uy * head - ux * half),
                                     col);
      }
    } else {
      marker_shape(dl, n->marker, s1, 4.5f * scale, col);
    }
  }
  ImPlot_PopPlotClipRect();
  /* an arrow's tail waits for its head */
  if (a->nt.mode == FIG_NOTE_ARROW && a->nt.plot == where && a->nt.have_start) {
    ImPlotPoint_c p0 = {a->nt.x0, a->nt.y0};
    ImVec2_c s0 = ImPlot_PlotToPixels_PlotPoint(p0, ImAxis_X1, ImAxis_Y1);
    ImDrawList_AddCircleFilled(dl, s0, 3 * scale, igGetColorU32_Col(ImGuiCol_Text, 1), 0);
  }
  /* the armed tool: a click without a drag places it */
  if (a->nt.mode >= 0 && a->nt.plot == where && ImPlot_IsPlotHovered() &&
      igIsMouseReleased_Nil(ImGuiMouseButton_Left)) {
    ImVec2_c drag = igGetMouseDragDelta(ImGuiMouseButton_Left, 0);
    if (fabsf(drag.x) < 3 && fabsf(drag.y) < 3) {
      ImPlotPoint_c m = ImPlot_GetPlotMousePos(ImAxis_X1, ImAxis_Y1);
      app_note_click(a, where, m.x, m.y);
    }
  }
}
