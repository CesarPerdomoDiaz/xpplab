/* The .ode file editor. */
#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "ui.h"

static int count_lines(const char *s) {
  int n = 1;
  for (; *s; s++)
    if (*s == '\n') n++;
  return n;
}

static int longest_line(const char *s) {
  int best = 0, cur = 0;
  for (; *s; s++) {
    if (*s == '\n') {
      if (cur > best) best = cur;
      cur = 0;
    } else {
      cur += *s == '\t' ? 4 : 1;
    }
  }
  return cur > best ? cur : best;
}

/* ---- Syntax highlighting ----------------------------------------------------
 *
 * The text box draws its text transparent; the coloured text is painted
 * underneath at the same glyph positions (the editor font is monospaced and
 * the box never scrolls by itself, so positions are exact). */

typedef struct {
  ImU32 text, comment, tutorial, keyword, number, function, param, var, option;
} Palette;

static Palette palette_for(App *a) {
  Palette p;
  if (a->dark) {
    p.text = igColorConvertFloat4ToU32(HEX(0xd4d4d4));
    p.comment = igColorConvertFloat4ToU32(HEX(0x6a9955));
    p.tutorial = igColorConvertFloat4ToU32(HEX(0xce9178));
    p.keyword = igColorConvertFloat4ToU32(HEX(0xc586c0));
    p.number = igColorConvertFloat4ToU32(HEX(0xb5cea8));
    p.function = igColorConvertFloat4ToU32(HEX(0xdcdcaa));
    p.param = igColorConvertFloat4ToU32(HEX(0x4fc1ff));
    p.var = igColorConvertFloat4ToU32(HEX(0xf28b82));
    p.option = igColorConvertFloat4ToU32(HEX(0x9cdcfe));
  } else {
    p.text = igColorConvertFloat4ToU32(HEX(0x1f1f1f));
    p.comment = igColorConvertFloat4ToU32(HEX(0x2e7d32));
    p.tutorial = igColorConvertFloat4ToU32(HEX(0xa31515));
    p.keyword = igColorConvertFloat4ToU32(HEX(0x7b1fa2));
    p.number = igColorConvertFloat4ToU32(HEX(0x098658));
    p.function = igColorConvertFloat4ToU32(HEX(0x795e26));
    p.param = igColorConvertFloat4ToU32(HEX(0x0451a5));
    p.var = igColorConvertFloat4ToU32(HEX(0xb71c1c));
    p.option = igColorConvertFloat4ToU32(HEX(0x001080));
  }
  return p;
}

static bool word_in(const char *w, int n, const char *const *list) {
  for (; *list; list++)
    if ((int)strlen(*list) == n && SDL_strncasecmp(w, *list, (size_t)n) == 0)
      return true;
  return false;
}

static bool name_in(const char *w, int n, char (*names)[APP_NAME_LEN], int count) {
  int i;
  for (i = 0; i < count; i++)
    if ((int)strlen(names[i]) == n && SDL_strncasecmp(w, names[i], (size_t)n) == 0)
      return true;
  return false;
}

static const char *const keywords[] = {
    "par", "param", "params", "p", "init", "i", "aux", "a", "number", "n",
    "table", "t", "wiener", "w", "markov", "m", "global", "g", "bdry", "b",
    "set", "done", "d", "volt", "v", "solv", "special", "export", "only",
    "options", "o", "include", "pi", NULL};
static const char *const functions[] = {
    "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "sinh", "cosh",
    "tanh", "exp", "ln", "log", "log10", "sqrt", "abs", "heav", "sign", "mod",
    "max", "min", "flr", "ran", "normal", "besselj", "bessely", "besseli",
    "erf", "erfc", "if", "then", "else", "delay", "int", "sum", "of",
    "hom_bcs", "shift", "del_shft", "lgamma", NULL};

static void draw_highlighted(App *a, ImDrawList *dl, ImVec2_c origin, float adv,
                             float lh, const char *text) {
  Palette c = palette_for(a);
  const char *p = text;
  int row = 0;
  while (1) {
    const char *e = strchr(p, '\n');
    const char *q;
    float x = origin.x, y = origin.y + row * lh;
    bool first_word = true;
    if (!e) e = p + strlen(p);
    /* Skip leading blanks to find what kind of line this is. */
    q = p;
    while (q < e && (*q == ' ' || *q == '\t')) q++;
    if (row + 1 == a->error_line) {
      ImU32 bg = igColorConvertFloat4ToU32(a->dark ? V4(0.6f, 0.1f, 0.1f, 0.35f)
                                                   : V4(1.0f, 0.3f, 0.3f, 0.18f));
      ImDrawList_AddRectFilled(dl, V2(origin.x - 4, y), V2(origin.x + 4000, y + lh),
                               bg, 0, 0);
    }
    if (q < e && (*q == '#' || *q == '"')) {
      ImU32 col = *q == '#' ? c.comment : c.tutorial;
      if (strncmp(q, "#include", 8) == 0) col = c.keyword;
      ImDrawList_AddText_Vec2(dl, V2(x + (float)(q - p) * adv, y), col, q, e);
    } else {
      bool option_line = q < e && *q == '@';
      const char *s = p;
      while (s < e) {
        ImU32 col = c.text;
        const char *t = s;
        if (*s == '\t') {
          x += 4 * adv;
          s++;
          continue;
        }
        if (isalpha((unsigned char)*s) || *s == '_') {
          int n;
          while (t < e && (isalnum((unsigned char)*t) || *t == '_')) t++;
          n = (int)(t - s);
          if (option_line)
            col = c.option;
          else if (first_word && t < e && *t == ' ' && word_in(s, n, keywords))
            col = c.keyword; /* "par a=1", but not the equation "a' = ..." */
          else if (t < e && *t == '(' && word_in(s, n, functions))
            col = c.function;
          else if (a->model_ok && name_in(s, n, a->pars, a->npars))
            col = c.param;
          else if (a->model_ok && name_in(s, n, a->vars, a->nvars))
            col = c.var;
          else if (word_in(s, n, functions))
            col = c.function;
          first_word = false;
        } else if (isdigit((unsigned char)*s) ||
                   (*s == '.' && s + 1 < e && isdigit((unsigned char)s[1]))) {
          while (t < e && (isalnum((unsigned char)*t) || *t == '.' ||
                           ((*t == '-' || *t == '+') && (t[-1] == 'e' || t[-1] == 'E'))))
            t++;
          col = option_line ? c.option : c.number;
        } else if (*s == '@' || *s == '!') {
          t = s + 1;
          col = c.keyword;
          first_word = *s == '@';
        } else {
          t = s + 1;
          if (*s != ' ') first_word = false;
        }
        ImDrawList_AddText_Vec2(dl, V2(x, y), col, s, t);
        x += (float)(t - s) * adv;
        s = t;
      }
    }
    if (!*e) break;
    p = e + 1;
    row++;
  }
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
  igPushFont(g_fonts.mono, 0.0f);
  lh = igGetTextLineHeight();
  gutter = igCalcTextSize("00000", NULL, false, -1).x;
  if (igBeginChild_Str("code", V2(0, 0), ImGuiChildFlags_Borders,
                       ImGuiWindowFlags_HorizontalScrollbar)) {
    const ImGuiStyle *st = igGetStyle();
    float y0 = igGetCursorPosY() + st->FramePadding.y;
    float adv = ImFont_CalcTextSizeA(igGetFont(), igGetFontSize(), FLT_MAX, 0, "M",
                                     NULL, NULL).x;
    float width = (float)longest_line(a->editor) * adv + 2 * st->FramePadding.x + 40;
    ImVec2_c origin;
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
    igSetCursorPosY(y0 - st->FramePadding.y);
    origin = igGetCursorScreenPos();
    origin.x += st->FramePadding.x;
    origin.y += st->FramePadding.y;
    draw_highlighted(a, igGetWindowDrawList(), origin, adv, lh, a->editor);
    /* The text box keeps its cursor and selection; its own text and
       background are invisible so the colours above show through. */
    igPushStyleColor_Vec4(ImGuiCol_Text, V4(0, 0, 0, 0));
    igPushStyleColor_Vec4(ImGuiCol_FrameBg, V4(0, 0, 0, 0));
    igPushStyleColor_Vec4(ImGuiCol_InputTextCursor, a->dark ? HEX(0xffffff) : HEX(0x000000));
    if (width < igGetContentRegionAvail().x) width = -1;
    if (igInputTextMultiline("##text", a->editor, a->editor_cap,
                             V2(width, (lines + 2) * lh + 8),
                             ImGuiInputTextFlags_AllowTabInput |
                                 ImGuiInputTextFlags_CallbackResize,
                             resize_callback, a))
      a->editor_dirty = true;
    igPopStyleColor(3);
  }
  igEndChild();
  igPopFont();
  igEnd();
}
