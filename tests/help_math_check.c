/*
 * Every formula of the pages the app's Help window shows (docs/manual and
 * docs/reference), $...$ in a line and $$...$$ on lines of their own, must
 * be typeset by the Figure window's mathtext (fig.h), which the Help window
 * uses: a command it does not know would be shown as written. Code (fenced
 * or inline) is left alone, as the Help window leaves it.
 *
 * usage: help_math_check PAGE.md ...
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fig.h"
#include "help_markup.h"

static int failures, formulas;

static void check_formula(const char *page, const char *s, size_t n) {
  char label[600], msg[200];
  if (n + 3 > sizeof label) {
    printf("FAIL: %s: a formula of %u characters is too long\n", page, (unsigned)n);
    failures++;
    return;
  }
  snprintf(label, sizeof label, "$%.*s$", (int)n, s);
  formulas++;
  if (!fig_text_check(label, msg, sizeof msg)) {
    printf("FAIL: %s: %s: %s\n", page, label, msg);
    failures++;
  }
}

static void check_page(const char *path) {
  static char text[1 << 19], prose[1 << 19];
  size_t n, k = 0;
  const char *p, *end, *s;
  int fenced = 0;
  HelpTok t;
  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("FAIL: cannot open %s\n", path);
    failures++;
    return;
  }
  n = fread(text, 1, sizeof text - 1, f);
  fclose(f);
  text[n] = 0;
  /* Line by line: fenced code is skipped, a display formula is checked and
     taken out, the rest is kept for the inline formulas. */
  for (p = text; *p;) {
    const char *e = strchr(p, '\n');
    const char *next;
    if (!e) e = p + strlen(p);
    next = *e ? e + 1 : e;
    if (!strncmp(p, "```", 3)) {
      fenced = !fenced;
    } else if (!fenced && !strncmp(p, "$$", 2)) {
      const char *q = p + 2, *close = strstr(q, "$$");
      if (close && close < e) {
        check_formula(path, q, (size_t)(close - q));
      } else {
        char block[600];
        size_t b = 0;
        for (q = next; *q && strncmp(q, "$$", 2); ) {
          const char *qe = strchr(q, '\n');
          if (!qe) qe = q + strlen(q);
          b += (size_t)snprintf(block + b, sizeof block - b, "%s%.*s", b ? " " : "",
                                (int)(qe - q), q);
          if (b >= sizeof block) b = sizeof block - 1;
          q = *qe ? qe + 1 : qe;
        }
        check_formula(path, block, b);
        if (*q) {
          const char *qe = strchr(q, '\n');
          q = qe ? qe + 1 : q + strlen(q);
        }
        next = q;
      }
    } else if (!fenced) {
      memcpy(prose + k, p, (size_t)(next - p));
      k += (size_t)(next - p);
    }
    p = next;
  }
  prose[k] = 0;
  /* the help browser tokenises a paragraph at a time; a formula does not
     cross a blank line, so the whole text gives the same formulas */
  end = prose + k;
  for (s = prose;;) {
    s = help_markup_next(prose, s, end, &t);
    if (t.kind == HM_END) break;
    if (t.kind == HM_MATH) check_formula(path, t.text, (size_t)(t.text_end - t.text));
  }
}

int main(int argc, char **argv) {
  int i;
  char msg[200];
  if (!fig_fonts_ok()) {
    puts("FAIL: the figure fonts are missing");
    return 1;
  }
  /* the checker itself: a known command passes, an unknown one fails */
  if (!fig_text_check("$\\dot{x} = \\frac{a}{b}$", msg, sizeof msg) ||
      fig_text_check("$\\nosuchcommand{x}$", msg, sizeof msg)) {
    puts("FAIL: fig_text_check does not tell good mathtext from bad");
    return 1;
  }
  for (i = 1; i < argc; i++) check_page(argv[i]);
  if (failures) return 1;
  printf("help mathematics: ok (%d formulas in %d pages)\n", formulas, argc - 1);
  return 0;
}
