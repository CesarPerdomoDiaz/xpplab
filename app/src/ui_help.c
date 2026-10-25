/* The help browser: renders the embedded Markdown manual.
 *
 * Besides ordinary Markdown it understands two kinds of links that make
 * the manual live: [text](example:lecar.ode) opens an example model and
 * [text](cmd:run) runs a command. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "docs.h"
#include "ui.h"

static char search[64];

void ui_show_help(App *a, const char *page) {
  const doc_page *p = docs_find(page);
  if (!p) {
    app_log(a, "No help page '%s'. Pages: welcome, tutorial, "
               "neuron-tutorial, auto, odefile, numerics, commands, xppaut, "
               "about.", page);
    return;
  }
  snprintf(a->help_page, sizeof a->help_page, "%s", p->name);
  a->show_help = true;
  igSetWindowFocus_Str(WIN_HELP);
}

static void follow_link(App *a, const char *target) {
  if (strncmp(target, "example:", 8) == 0) {
    char path[1100];
    snprintf(path, sizeof path, "%s/%s", a->examples_dir, target + 8);
    app_open(a, path);
  } else if (strncmp(target, "cmd:", 4) == 0) {
    app_log(a, ">> %s", target + 4);
    app_command(a, target + 4);
  } else if (strncmp(target, "http", 4) == 0) {
    SDL_OpenURL(target);
  } else {
    char name[64];
    const char *dot = strstr(target, ".md");
    snprintf(name, sizeof name, "%.*s",
             dot ? (int)(dot - target) : (int)strlen(target), target);
    ui_show_help(a, name);
  }
}

/* Inline layout: words are placed one by one and wrap at the right edge,
   so links and code can sit in the middle of a wrapped paragraph. */
typedef struct {
  float x0, right; /* left margin and right edge, in window coordinates */
  bool line_start;
} Flow;

static void flow_begin(Flow *f) {
  f->x0 = igGetCursorPosX();
  f->right = igGetCursorPosX() + igGetContentRegionAvail().x;
  f->line_start = true;
}

/* Place one word (with its trailing space) and say whether it was clicked. */
static bool flow_word(Flow *f, const char *w, const char *we, ImVec4_c *color,
                      bool mono, const char *tip) {
  ImVec2_c sz;
  bool clicked = false;
  if (mono) igPushFont(g_fonts.mono, 0.0f);
  sz = igCalcTextSize(w, we, false, -1);
  if (!f->line_start) {
    igSameLine(0, 0);
    if (igGetCursorPosX() + sz.x > f->right) {
      igNewLine();
      igSetCursorPosX(f->x0);
    }
  } else {
    igSetCursorPosX(f->x0);
  }
  if (color) igPushStyleColor_Vec4(ImGuiCol_Text, *color);
  igTextUnformatted(w, we);
  if (color) igPopStyleColor(1);
  if (mono) igPopFont();
  if (tip && igIsItemHovered(0)) {
    igSetMouseCursor(ImGuiMouseCursor_Hand);
    igSetTooltip("%s", tip);
  }
  if (tip && igIsItemClicked(ImGuiMouseButton_Left)) clicked = true;
  f->line_start = false;
  return clicked;
}

/* Lay out words from s..end; spaces separate words. */
static bool flow_run(Flow *f, const char *s, const char *end, ImVec4_c *color,
                     bool mono, const char *tip) {
  bool clicked = false;
  while (s < end) {
    const char *w = s;
    while (s < end && *s != ' ') s++;
    while (s < end && *s == ' ') s++; /* keep the trailing space */
    if (s > w && flow_word(f, w, s, color, mono, tip)) clicked = true;
  }
  return clicked;
}

/* Render Markdown inline text: [links](target), `code` and **bold**
   (shown plain). */
static void inline_text(App *a, const char *s, const char *end) {
  Flow f;
  ImVec4_c link = a->dark ? HEX(0x64b5f6) : HEX(0x1565c0);
  ImVec4_c code = a->dark ? HEX(0xffb74d) : HEX(0x6a1b9a);
  char plain[2048];
  int k = 0;
  flow_begin(&f);
#define FLUSH()                                       \
  if (k > 0) {                                        \
    flow_run(&f, plain, plain + k, NULL, false, NULL); \
    k = 0;                                            \
  }
  while (s < end) {
    if (*s == '[') {
      const char *close = memchr(s, ']', (size_t)(end - s));
      const char *rp = close && close + 1 < end && close[1] == '('
                           ? memchr(close, ')', (size_t)(end - close))
                           : NULL;
      if (rp) {
        char target[256], tip[300];
        FLUSH();
        snprintf(target, sizeof target, "%.*s", (int)(rp - close - 2), close + 2);
        snprintf(tip, sizeof tip, "%s",
                 strncmp(target, "example:", 8) == 0 ? "Open this example"
                 : strncmp(target, "cmd:", 4) == 0   ? target + 4
                                                     : target);
        if (flow_run(&f, s + 1, close, &link, false, tip)) follow_link(a, target);
        s = rp + 1;
        continue;
      }
    }
    if (*s == '`') {
      const char *close = memchr(s + 1, '`', (size_t)(end - s - 1));
      if (close) {
        FLUSH();
        flow_run(&f, s + 1, close, &code, true, NULL);
        s = close + 1;
        continue;
      }
    }
    if (*s == '*' && s + 1 < end && s[1] == '*') {
      s += 2;
      continue;
    }
    if (k < (int)sizeof plain - 1) plain[k++] = *s;
    s++;
  }
  FLUSH();
#undef FLUSH
  if (f.line_start) igNewLine();
}

/* Join the lines of a paragraph (until a blank or a block line). */
static bool is_block_line(const char *p) {
  return *p == '\n' || *p == '\0' || *p == '#' || *p == '|' || *p == '>' ||
         strncmp(p, "```", 3) == 0 || ((p[0] == '-' || p[0] == '*') && p[1] == ' ') ||
         strncmp(p, "  ", 2) == 0 || (isdigit((unsigned char)p[0]) && p[1] == '.');
}

static void table_row(App *a, const char *s, const char *e, int ncols, bool header) {
  int c = 0;
  const char *p = s;
  (void)a;
  if (*p == '|') p++;
  igTableNextRow(header ? ImGuiTableRowFlags_Headers : 0, 0);
  while (p < e && c < ncols) {
    const char *bar = memchr(p, '|', (size_t)(e - p));
    const char *cell_end = bar ? bar : e;
    igTableSetColumnIndex(c++);
    while (p < cell_end && *p == ' ') p++;
    while (cell_end > p && cell_end[-1] == ' ') cell_end--;
    if (header) PushHeading();
    inline_text(a, p, cell_end);
    if (header) igPopFont();
    if (!bar) break;
    p = bar + 1;
  }
}

static void render_markdown(App *a, const char *text) {
  const char *p = text;
  bool in_code = false;
  while (*p) {
    const char *e = strchr(p, '\n');
    const char *next;
    if (!e) e = p + strlen(p);
    next = *e ? e + 1 : e;
    if (strncmp(p, "```", 3) == 0) {
      in_code = !in_code;
      if (in_code) {
        /* Collect the block and show it on a tinted background. */
        const char *q = next, *block_end = next;
        int lines = 0;
        while (*q) {
          const char *qe = strchr(q, '\n');
          if (!qe) qe = q + strlen(q);
          if (strncmp(q, "```", 3) == 0) break;
          lines++;
          block_end = *qe ? qe + 1 : qe;
          q = *qe ? qe + 1 : qe;
        }
        char id[32];
        snprintf(id, sizeof id, "code%p", (const void *)next);
        igPushStyleColor_Vec4(ImGuiCol_ChildBg, a->dark ? HEX(0x2b2d30) : HEX(0xeef1f5));
        igPushFont(g_fonts.mono, 0.0f);
        if (igBeginChild_Str(id, V2(0, lines * igGetTextLineHeight() + 12),
                             ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar))
          igTextUnformatted(next, block_end > next ? block_end - 1 : next);
        igEndChild();
        igPopFont();
        igPopStyleColor(1);
        p = q;
        in_code = true;
        continue;
      }
      p = next;
      continue;
    }
    if (p == e) {
      igSpacing();
    } else if (strncmp(p, "# ", 2) == 0) {
      PushTitle();
      inline_text(a, p + 2, e);
      igPopFont();
      igSeparator();
    } else if (strncmp(p, "## ", 3) == 0) {
      igSpacing();
      PushHeading();
      inline_text(a, p + 3, e);
      igPopFont();
    } else if (strncmp(p, "### ", 4) == 0) {
      PushHeading();
      inline_text(a, p + 4, e);
      igPopFont();
    } else if (((p[0] == '-' || p[0] == '*') && p[1] == ' ') ||
               strncmp(p, "  - ", 4) == 0 ||
               (isdigit((unsigned char)p[0]) && p[1] == '.' && p[2] == ' ')) {
      /* A list item, with any indented continuation lines. */
      bool nested = p[0] == ' ';
      bool numbered = isdigit((unsigned char)p[0]) != 0;
      const char *body = p + (nested ? 4 : numbered ? 3 : 2);
      char num[4] = {p[0], '.', 0, 0};
      char text[2048];
      int k = snprintf(text, sizeof text, "%.*s", (int)(e - body), body);
      while (*next == ' ' && next[1] == ' ' && strncmp(next, "  - ", 4) != 0 &&
             next[2] != '\n') {
        const char *ne = strchr(next, '\n'), *t = next;
        if (!ne) ne = next + strlen(next);
        while (*t == ' ') t++;
        k += snprintf(text + k, sizeof text - (size_t)k, " %.*s", (int)(ne - t), t);
        if (k >= (int)sizeof text) k = (int)sizeof text - 1;
        next = *ne ? ne + 1 : ne;
      }
      if (nested) igIndent(18);
      if (numbered)
        igTextUnformatted(num, NULL);
      else
        igBullet();
      igSameLine(0, -1);
      inline_text(a, text, text + k);
      if (nested) igUnindent(18);
    } else if (p[0] == '>' ) {
      /* A tip: join the quoted lines and show them in a tinted box. */
      char text[2048], id[32];
      int k = 0;
      const char *q = p;
      while (*q == '>') {
        const char *qe = strchr(q, '\n'), *t = q + 1;
        if (!qe) qe = q + strlen(q);
        while (*t == ' ') t++;
        k += snprintf(text + k, sizeof text - (size_t)k, "%s%.*s", k ? " " : "",
                      (int)(qe - t), t);
        if (k >= (int)sizeof text) k = (int)sizeof text - 1;
        q = *qe ? qe + 1 : qe;
      }
      next = q;
      snprintf(id, sizeof id, "tip%p", (const void *)p);
      igPushStyleColor_Vec4(ImGuiCol_ChildBg, a->dark ? HEX(0x3a3020) : HEX(0xfff6e0));
      if (igBeginChild_Str(id, V2(0, 0),
                           ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, 0))
        inline_text(a, text, text + k);
      igEndChild();
      igPopStyleColor(1);
    } else if (p[0] == '|') {
      /* A table: count columns from the header row, skip the --- row. */
      int ncols = 0;
      const char *q;
      char id[32];
      snprintf(id, sizeof id, "tbl%p", (const void *)p);
      for (q = p + 1; q < e; q++)
        if (*q == '|') ncols++;
      if (ncols > 0 &&
          igBeginTable(id, ncols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg,
                       V2(0, 0), 0)) {
        bool header = true;
        while (*p == '|') {
          const char *le = strchr(p, '\n');
          if (!le) le = p + strlen(p);
          if (strstr(p, "---") && strstr(p, "---") < le) {
            p = *le ? le + 1 : le;
            continue;
          }
          table_row(a, p, le, ncols, header);
          header = false;
          p = *le ? le + 1 : le;
        }
        igEndTable();
        continue;
      }
    } else {
      /* A paragraph: join lines until a blank line or a block. */
      char text[4096];
      int k = snprintf(text, sizeof text, "%.*s", (int)(e - p), p);
      while (*next && !is_block_line(next)) {
        const char *ne = strchr(next, '\n');
        if (!ne) ne = next + strlen(next);
        k += snprintf(text + k, sizeof text - (size_t)k, " %.*s", (int)(ne - next), next);
        if (k >= (int)sizeof text) k = (int)sizeof text - 1;
        next = *ne ? ne + 1 : ne;
      }
      inline_text(a, text, text + k);
    }
    p = next;
  }
}

void ui_help(App *a) {
  const doc_page *page;
  int i;
  if (!igBegin(WIN_HELP, &a->show_help, 0)) {
    igEnd();
    return;
  }
  page = docs_find(a->help_page);
  if (igBeginChild_Str("helpnav", V2(0, igGetFrameHeightWithSpacing() * 1.2f), 0,
                       ImGuiWindowFlags_NoScrollbar)) {
    char title[128];
    igSetNextItemWidth(170);
    if (igBeginCombo("##page", page ? docs_title(page, title, sizeof title) : "Contents",
                     0)) {
      for (i = 0; doc_pages[i].name; i++) {
        char t[128];
        if (search[0] && !SDL_strcasestr(doc_pages[i].text, search)) continue;
        if (igSelectable_Bool(docs_title(&doc_pages[i], t, sizeof t),
                              page == &doc_pages[i], 0, V2(0, 0)))
          snprintf(a->help_page, sizeof a->help_page, "%s", doc_pages[i].name);
      }
      igEndCombo();
    }
    igSameLine(0, -1);
    igSetNextItemWidth(-1);
    igInputTextWithHint("##search", "Search the manual...", search, sizeof search, 0,
                        NULL, NULL);
  }
  igEndChild();
  if (search[0]) {
    igTextDisabled("Pages mentioning \"%s\":", search);
    for (i = 0; doc_pages[i].name; i++) {
      char t[128];
      if (!SDL_strcasestr(doc_pages[i].text, search)) continue;
      if (igSelectable_Bool(docs_title(&doc_pages[i], t, sizeof t), false, 0, V2(0, 0))) {
        snprintf(a->help_page, sizeof a->help_page, "%s", doc_pages[i].name);
        search[0] = '\0';
      }
    }
    igSeparator();
  }
  if (igBeginChild_Str("helpbody", V2(0, 0), 0, 0)) {
    if (page)
      render_markdown(a, page->text);
    else
      igTextDisabled("No manual pages were built into this program.");
  }
  igEndChild();
  igEnd();
}
