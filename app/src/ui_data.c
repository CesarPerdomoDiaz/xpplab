/* The data browser: the last run as a table (XPPAUT's Data Viewer). */
#include <stdio.h>

#include "ui.h"

void ui_data(App *a) {
  int j;
  ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                       ImGuiTableFlags_ScrollX | ImGuiTableFlags_BordersInnerV |
                       ImGuiTableFlags_Resizable;
  if (!igBegin(WIN_DATA, &a->show_data, 0)) {
    igEnd();
    return;
  }
  if (!a->have_traj) {
    igTextDisabled("Run the model to see its data here.");
    igEnd();
    return;
  }
  igText("%d rows x %d columns", a->traj.nrows, a->traj.ncols);
  igSameLine(0, -1);
  if (Button("Export CSV or MAT...")) ui_dialog_export_csv(a);
  igPushFont(g_fonts.mono, 0.0f);
  if (igBeginTable("data", a->traj.ncols, tf, V2(0, 0), 0)) {
    ImGuiListClipper *clip = ImGuiListClipper_ImGuiListClipper();
    igTableSetupScrollFreeze(1, 1);
    for (j = 0; j < a->traj.ncols; j++)
      igTableSetupColumn(a->traj.names[j], ImGuiTableColumnFlags_WidthFixed, 96, 0);
    igTableHeadersRow();
    ImGuiListClipper_Begin(clip, a->traj.nrows, -1.0f);
    while (ImGuiListClipper_Step(clip)) {
      int i;
      for (i = clip->DisplayStart; i < clip->DisplayEnd; i++) {
        igTableNextRow(0, 0);
        for (j = 0; j < a->traj.ncols; j++) {
          igTableSetColumnIndex(j);
          igText("%.8g", a->traj.cols[j][i]);
        }
      }
    }
    ImGuiListClipper_destroy(clip);
    igEndTable();
  }
  igPopFont();
  igEnd();
}
