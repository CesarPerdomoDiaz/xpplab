/* The command window: a log of what happened and a command line, which
   takes XPP commands or, in Python mode, Python (pyconsole.c). */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "pyconsole.h"
#include "pyfetch.h"
#include "ui.h"

static char input[1024];
static int history_pos = -1;
static int comp_start = -1, comp_len; /* the word waiting for its completion */

/* A line of a Python traceback's end: "NameError: ...", "Traceback ...". */
static bool python_error(const char *p, const char *end) {
  const char *q = p;
  if (end - p >= 9 && strncmp(p, "Traceback", 9) == 0) return true;
  while (q < end && (isalnum((unsigned char)*q) || *q == '_' || *q == '.')) q++;
  if (q >= end || *q != ':' || q - p < 5) return false;
  return (q - p >= 5 && strncmp(q - 5, "Error", 5) == 0) ||
         (q - p >= 9 && strncmp(q - 9, "Exception", 9) == 0) ||
         (q - p >= 9 && strncmp(q - 9, "Interrupt", 9) == 0);
}

static int history_callback(ImGuiInputTextCallbackData *d) {
  App *a = (App *)d->UserData;
  /* Tab in Python mode: ask for the word before the cursor ... */
  if (d->EventFlag == ImGuiInputTextFlags_CallbackCompletion && a->py_mode) {
    int s = d->CursorPos;
    char word[256];
    while (s > 0 && (isalnum((unsigned char)d->Buf[s - 1]) || d->Buf[s - 1] == '_' ||
                     d->Buf[s - 1] == '.'))
      s--;
    if (d->CursorPos - s > 0 && d->CursorPos - s < (int)sizeof word) {
      snprintf(word, sizeof word, "%.*s", d->CursorPos - s, d->Buf + s);
      comp_start = s;
      comp_len = d->CursorPos - s;
      pyconsole_complete(a, word);
    }
    return 0;
  }
  /* ... and put the answer in when it comes. */
  if (d->EventFlag == ImGuiInputTextFlags_CallbackAlways && comp_start >= 0) {
    char done[256];
    if (pyconsole_completion(done, sizeof done)) {
      if (comp_start + comp_len <= d->BufTextLen) {
        ImGuiInputTextCallbackData_DeleteChars(d, comp_start, comp_len);
        ImGuiInputTextCallbackData_InsertChars(d, comp_start, done, NULL);
      }
      comp_start = -1;
    }
    return 0;
  }
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
  /* a row more for XPP Lab's Python: on its way, or offered */
  if (pyfetch_running() || pyconsole_needs_python()) input_h += igGetFrameHeightWithSpacing();
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
      if (strncmp(p, "error", 5) == 0 || python_error(p, end))
        igPushStyleColor_Vec4(ImGuiCol_Text, HEX(0xe53935));
      else if (strncmp(p, "warning", 7) == 0)
        igPushStyleColor_Vec4(ImGuiCol_Text, HEX(0xf9a825));
      else if (strncmp(p, ">>> ", 4) == 0) /* an action as Python (echo.c) */
        igPushStyleColor_Vec4(ImGuiCol_Text, a->dark ? HEX(0x81c784) : HEX(0x2e7d32));
      else if (strncmp(p, ">> ", 3) == 0 || strncmp(p, "py> ", 4) == 0 ||
               strncmp(p, "... ", 4) == 0)
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
  /* XPP Lab's own Python on its way, or offered when there is none. */
  if (pyfetch_running()) {
    float frac;
    const char *what = pyfetch_status(&frac);
    char label[200];
    if (frac >= 0)
      snprintf(label, sizeof label, "%s  %.0f%%", what, 100 * frac);
    else
      snprintf(label, sizeof label, "%s ...", what);
    igProgressBar(frac >= 0 ? frac : -1.0f * (float)igGetTime(),
                  V2(-igGetFrameHeight() * 3.5f, 0), label);
    igSameLine(0, -1);
    if (ButtonW("Stop##fetch", -1)) app_command(a, "python fetch stop");
    Tooltip("Stop installing; nothing that was downloaded is kept.");
  } else if (pyconsole_needs_python()) {
    char tip[300];
    int mb = pyfetch_megabytes();
    if (Button("Install Python")) app_command(a, "python fetch");
    if (mb > 0)
      snprintf(tip, sizeof tip,
               "Download %s for XPP Lab's console (about %d MB, once). It goes into a "
               "folder of its own; nothing else on this computer changes.",
               pyfetch_about(), mb);
    else
      snprintf(tip, sizeof tip, "Download %s for XPP Lab's console.", pyfetch_about());
    Tooltip(tip);
    igSameLine(0, -1);
    if (Button("Choose a Python...")) ui_dialog_python(a);
    Tooltip("A Python 3 of yours that has NumPy (remembered).");
  }
  /* The prompt says which language the line takes; a click switches. */
  igPushStyleColor_Vec4(ImGuiCol_Text, a->dark ? HEX(0x64b5f6) : HEX(0x1565c0));
  igPushStyleColor_Vec4(ImGuiCol_Button, V4(0, 0, 0, 0));
  if (igSmallButton(a->py_mode ? pyconsole_prompt() : ">>")) {
    a->py_mode = !a->py_mode;
    reclaim = true;
  }
  igPopStyleColor(2);
  Tooltip(a->py_mode ? "Python, run in its own process: xs, np, app and m (the model "
                       "here) are defined. Click for XPP commands."
                     : "XPP commands (help lists them). Click for Python, or type py "
                       "followed by a line of Python.");
  igSameLine(0, -1);
  if (a->py_mode && pyconsole_busy()) {
    if (Button("Stop")) app_command(a, "python stop");
    Tooltip("End the Python process (its variables are lost); the next line "
            "starts a fresh one.");
    igSameLine(0, -1);
  }
  igSetNextItemWidth(-1);
  if (igInputTextWithHint("##cmd",
                          a->py_mode ? "Python, e.g.  sol = m.run(total=200)   "
                                       "np.max(sol['V'])   (Tab completes)"
                                     : "type a command, e.g.  set I=1   run   eq   help",
                          input, sizeof input,
                          ImGuiInputTextFlags_EnterReturnsTrue |
                              ImGuiInputTextFlags_CallbackHistory |
                              (a->py_mode ? ImGuiInputTextFlags_CallbackCompletion |
                                                ImGuiInputTextFlags_CallbackAlways
                                          : 0),
                          history_callback, a)) {
    if (a->py_mode && strcmp(input, ":cmd") == 0) {
      a->py_mode = false;
    } else if (a->py_mode) {
      /* An empty line ends a block (if, for, def). */
      if (input[0]) remember(a, input);
      if (input[0] || strcmp(pyconsole_prompt(), "...") == 0) pyconsole_send(a, input);
    } else if (input[0]) {
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
