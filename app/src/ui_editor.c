/* The .ode file editor. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

static int count_lines(const char *s) {
  int n = 1;
  for (; *s; s++)
    if (*s == '\n') n++;
  return n;
}

static int resize_callback(ImGuiInputTextCallbackData *d) {
  App *a = (App *)d->UserData;
  if (d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
    size_t want = (size_t)d->BufTextLen + 65536;
    if (want > a->editor_cap) {
      char *n = realloc(a->editor, want);
      if (n) {
        a->editor = n;
        a->editor_cap = want;
      }
    }
    d->Buf = a->editor;
    d->BufSize = (int)a->editor_cap;
  }
  return 0;
}

/* Append a snippet at the end of the model, before "done" if present. */
static void insert_snippet(App *a, const char *snippet) {
  char *done = strstr(a->editor, "\ndone");
  size_t n = strlen(a->editor), s = strlen(snippet);
  if (n + s + 2 >= a->editor_cap) return;
  if (done) {
    size_t at = (size_t)(done - a->editor) + 1;
    memmove(a->editor + at + s, a->editor + at, n - at + 1);
    memcpy(a->editor + at, snippet, s);
  } else {
    memcpy(a->editor + n, snippet, s + 1);
  }
  a->editor_dirty = true;
}

static void snippets_menu(App *a) {
  static const struct {
    const char *label, *text, *tip;
  } items[] = {
      {"Differential equation", "x' = -x\n", "dx/dt = ...  (or dx/dt = ...)"},
      {"Map (difference equation)", "x(t+1) = r*x*(1-x)\n",
       "Use with method 'discrete'."},
      {"Parameters", "par a=1, b=2\n", "Named constants you can change live."},
      {"Initial values", "init x=0.1\n", "Starting values of the variables."},
      {"Auxiliary output", "aux energy = 0.5*x^2\n",
       "An extra quantity to plot and save."},
      {"Function", "f(u) = u^3 - u\n", "A user function to use in equations."},
      {"Fixed quantity", "s = x + y\n", "An intermediate quantity."},
      {"Integration settings", "@ total=100, dt=0.05, meth=rungekutta\n",
       "Options read when the model loads."},
      {"Phase-plane axes", "@ xp=x, yp=y, xlo=-2, xhi=2, ylo=-2, yhi=2\n",
       "Which variables the phase plane shows."},
      {"AUTO settings", "@ ds=0.01, dsmax=0.05, parmin=0, parmax=2, nmax=500\n",
       "Defaults for the bifurcation window."},
      {"Parameter set", "set fast {a=2, total=50}\n",
       "A named group of values (Workspace > Sets)."},
      {"Noise (Wiener process)", "wiener w\n",
       "Use w in an equation for additive noise; use method euler."},
      {"Delay", "x' = -x(t-tau)\npar tau=1\n@ delay=2\n",
       "delay(x,tau) also works. @ delay sets the maximum delay."},
      {"Table from a file", "table g g.tab\n", "Tabulated function."},
      {"Comment", "# a comment\n", "Lines starting with # are ignored."},
  };
  int i;
  for (i = 0; i < (int)(sizeof items / sizeof items[0]); i++) {
    if (igMenuItem_Bool(items[i].label, NULL, false, true))
      insert_snippet(a, items[i].text);
    Tooltip(items[i].tip);
  }
}

void ui_editor(App *a) {
  int lines, i;
  float lh, gutter;
  char title[300];
  snprintf(title, sizeof title, "%s%s###" WIN_EDITOR,
           a->model_name[0] ? a->model_name : "Editor",
           a->editor_dirty ? " *" : "");
  if (!igBegin(title, &a->show_editor, ImGuiWindowFlags_MenuBar)) {
    igEnd();
    return;
  }
  if (igBeginMenuBar()) {
    if (igMenuItem_Bool("Apply", "Ctrl+Enter", false, a->editor[0] != 0))
      app_apply_editor(a);
    Tooltip("Save the file and reload the model with your changes.");
    if (igMenuItem_Bool("Revert", NULL, false,
                        a->editor_dirty && a->model_path[0])) {
      char p[1024];
      snprintf(p, sizeof p, "%s", a->model_path);
      app_open(a, p);
    }
    if (igBeginMenu("Insert", true)) {
      snippets_menu(a);
      igEndMenu();
    }
    if (igMenuItem_Bool("Syntax help", NULL, false, true))
      ui_show_help(a, "odefile");
    igEndMenuBar();
  }

  if (a->load_error[0]) {
    igPushStyleColor_Vec4(ImGuiCol_ChildBg, a->dark ? HEX(0x4a1f1f) : HEX(0xfdecea));
    if (igBeginChild_Str("err", V2(0, igGetTextLineHeightWithSpacing() * 2.6f),
                         ImGuiChildFlags_Borders, 0)) {
      igTextColored(HEX(0xc62828), "The model could not be loaded");
      if (a->error_line > 0) {
        igSameLine(0, -1);
        igTextColored(HEX(0xc62828), "(near line %d)", a->error_line);
      }
      TextWrapped(a->load_error);
    }
    igEndChild();
    igPopStyleColor(1);
  }

  /* Line numbers and text scroll together inside one child window. */
  lines = count_lines(a->editor);
  lh = igGetTextLineHeight();
  igPushFont(g_fonts.mono, 0.0f);
  gutter = igCalcTextSize("00000", NULL, false, -1).x;
  if (igBeginChild_Str("code", V2(0, 0), ImGuiChildFlags_Borders,
                       ImGuiWindowFlags_HorizontalScrollbar)) {
    float y0 = igGetCursorPosY() + igGetStyle()->FramePadding.y;
    igBeginGroup();
    for (i = 1; i <= lines + 1; i++) {
      igSetCursorPosY(y0 + (i - 1) * lh);
      if (i == a->error_line)
        igTextColored(HEX(0xc62828), "%4d>", i);
      else
        igTextDisabled("%4d", i);
    }
    igEndGroup();
    igSameLine(gutter + 10, 0);
    igSetCursorPosY(y0 - igGetStyle()->FramePadding.y);
    if (igInputTextMultiline("##text", a->editor, a->editor_cap,
                             V2(-1, (lines + 2) * lh + 8),
                             ImGuiInputTextFlags_AllowTabInput |
                                 ImGuiInputTextFlags_CallbackResize,
                             resize_callback, a))
      a->editor_dirty = true;
  }
  igEndChild();
  igPopFont();
  igEnd();
}
