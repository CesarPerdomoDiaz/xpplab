/*
 * The help browser's inline Markdown (app/src/help_markup.h): an image is
 * not a link, and emphasis loses its asterisks. Tokens are checked as the
 * browser would receive them; no window.
 */
#include <stdio.h>
#include <string.h>

#include "help_markup.h"

static int failures;

/* The tokens of text as "kind:words" joined by '|' (skips left out). */
static void tokens(const char *text, char *out, size_t size) {
  const char *s = text, *end = text + strlen(text);
  HelpTok t;
  out[0] = 0;
  for (;;) {
    char piece[256];
    const char *kind;
    s = help_markup_next(text, s, end, &t);
    if (t.kind == HM_END) break;
    if (t.kind == HM_SKIP) continue;
    kind = t.kind == HM_TEXT ? "text" : t.kind == HM_LINK ? "link"
         : t.kind == HM_IMAGE ? "image" : "code";
    snprintf(piece, sizeof piece, "%s%s:%.*s", out[0] ? "|" : "", kind,
             (int)(t.text_end - t.text), t.text);
    if (t.kind == HM_LINK || t.kind == HM_IMAGE)
      snprintf(piece + strlen(piece), sizeof piece - strlen(piece), "=>%.*s",
               (int)(t.target_end - t.target), t.target);
    strncat(out, piece, size - strlen(out) - 1);
  }
}

static void check(const char *text, const char *want) {
  char got[1024];
  tokens(text, got, sizeof got);
  if (strcmp(got, want)) {
    printf("FAIL: %s\n  got  %s\n  want %s\n", text, got, want);
    failures++;
  }
}

/* A manual page given on the command line: every image is drawn as an
   image, and no link of the page points at a picture file. */
static void check_page(const char *path) {
  char line[4096];
  int images = 0;
  FILE *f = fopen(path, "r");
  if (!f) {
    printf("FAIL: cannot open %s\n", path);
    failures++;
    return;
  }
  while (fgets(line, sizeof line, f)) {
    const char *s = line, *end = line + strlen(line);
    HelpTok t;
    for (;;) {
      s = help_markup_next(line, s, end, &t);
      if (t.kind == HM_END) break;
      if (t.kind == HM_IMAGE) images++;
      if (t.kind == HM_LINK && (t.target_end - t.target) > 4 &&
          !strncmp(t.target_end - 4, ".png", 4)) {
        printf("FAIL: a link to a picture: %s", line);
        failures++;
      }
    }
  }
  fclose(f);
  if (images < 1) {
    printf("FAIL: no image found in %s\n", path);
    failures++;
  }
}

int main(int argc, char **argv) {
  check("see ![a caption](../images/x.png) here",
        "text:see |image:a caption=>../images/x.png|text: here");
  check("see [the page](auto.md) here",
        "text:see |link:the page=>auto.md|text: here");
  check("Kuznetsov, *Elements of Theory*, ch. 8",
        "text:Kuznetsov, |text:Elements of Theory|text:, ch. 8");
  check("**bold** and *it*", "text:bold|text: and |text:it");
  check("2 * 3 * 4 and a `x*y` code",
        "text:2 |text:* 3 |text:* 4 and a |code:x*y|text: code");
  check("not a link [x] or ! mark", "text:not a link |text:[x] or ! mark");
  if (argc > 1) check_page(argv[1]);
  if (failures) return 1;
  puts("help markup: ok");
  return 0;
}
