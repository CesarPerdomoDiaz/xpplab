/* The command window: a log of what happened and a command line. */
#include <stdio.h>
#include <string.h>

#include "ui.h"

static char input[256];
static int history_pos = -1;

static int history_callback(ImGuiInputTextCallbackData *d) {
  App *a = (App *)d->UserData;
  if (d->EventFlag == ImGuiInputTextFlags_CallbackHistory && a->nhistory > 0) {
    if (d->EventKey == ImGuiKey_UpArrow) {
      if (history_pos < 0)
        history_pos = a->nhistory - 1;
      else if (history_pos > 0)
        history_pos--;
    } else if (d->EventKey == ImGuiKey_DownArrow) {
      if (history_pos >= 0 && ++history_pos >= a->nhistory) history_pos = -1;
    }
    ImGuiInputTextCallbackData_DeleteChars(d, 0, d->BufTextLen);
    if (history_pos >= 0)
      ImGuiInputTextCallbackData_InsertChars(d, 0, a->history[history_pos], NULL);
  }
  return 0;
}

static void remember(App *a, const char *line) {
  if (a->nhistory > 0 && strcmp(a->history[a->nhistory - 1], line) == 0) return;
  if (a->nhistory == 32) {
    memmove(a->history[0], a->history[1], sizeof a->history[0] * 31);
    a->nhistory--;
  }
  snprintf(a->history[a->nhistory++], sizeof a->history[0], "%s", line);
}

void ui_console(App *a) {
  float input_h = igGetFrameHeightWithSpacing() + 4;
  bool reclaim = false;
  if (!igBegin(WIN_CONSOLE, &a->show_console, 0)) {
    igEnd();
    return;
  }
  igPushFont(g_fonts.mono, 0.0f);
  if (igBeginChild_Str("log", V2(0, -input_h), ImGuiChildFlags_Borders,
                       ImGuiWindowFlags_HorizontalScrollbar)) {
    const char *p = a->log, *end;
    /* Colour errors and warnings so they stand out. */
    while (*p) {
      end = strchr(p, '\n');
      if (!end) end = p + strlen(p);
      if (strncmp(p, "error", 5) == 0)
        igPushStyleColor_Vec4(ImGuiCol_Text, HEX(0xe53935));
      else if (strncmp(p, "warning", 7) == 0)
        igPushStyleColor_Vec4(ImGuiCol_Text, HEX(0xf9a825));
      else if (strncmp(p, ">>> ", 4) == 0) /* an action as Python (echo.c) */
        igPushStyleColor_Vec4(ImGuiCol_Text, a->dark ? HEX(0x81c784) : HEX(0x2e7d32));
      else if (strncmp(p, ">> ", 3) == 0)
        igPushStyleColor_Vec4(ImGuiCol_Text, a->dark ? HEX(0x64b5f6) : HEX(0x1565c0));
      else
        igPushStyleColor_Vec4(ImGuiCol_Text, igGetStyle()->Colors[ImGuiCol_Text]);
      igTextUnformatted(p, end);
      igPopStyleColor(1);
      p = *end ? end + 1 : end;
    }
    if (a->log_scroll) {
      igSetScrollHereY(1.0f);
      a->log_scroll = false;
    }
  }
  igEndChild();
  igTextColored(a->dark ? HEX(0x64b5f6) : HEX(0x1565c0), ">>");
  igSameLine(0, -1);
  igSetNextItemWidth(-1);
  if (igInputTextWithHint("##cmd", "type a command, e.g.  set I=1   run   eq   help",
                          input, sizeof input,
                          ImGuiInputTextFlags_EnterReturnsTrue |
                              ImGuiInputTextFlags_CallbackHistory,
                          history_callback, a)) {
    if (input[0]) {
      app_log(a, ">> %s", input);
      remember(a, input);
      app_command(a, input);
    }
    input[0] = '\0';
    history_pos = -1;
    reclaim = true;
  }
  igSetItemDefaultFocus();
  if (reclaim) igSetKeyboardFocusHere(-1);
  igPopFont();
  igEnd();
}
