/* The inline Markdown of the manual, cut into tokens for the help browser
 * (ui_help.c) and for tests/help_markup_check.c.
 *
 *   text          plain text (a run up to the next special character)
 *   [text](to)    a link
 *   ![text](to)   an image; the help browser shows its caption only
 *   `code`        code
 *   *a*, **a**    emphasis; the markers are skipped and the text stays plain
 */
#ifndef HELP_MARKUP_H
#define HELP_MARKUP_H

#include <ctype.h>
#include <stddef.h>
#include <string.h>

typedef enum { HM_END, HM_TEXT, HM_LINK, HM_IMAGE, HM_CODE, HM_SKIP } HelpTokKind;

typedef struct {
  HelpTokKind kind;
  const char *text, *text_end;     /* the words, the caption or the code */
  const char *target, *target_end; /* of a link or an image */
} HelpTok;

/* "[text](target)" at s (s points at the '['); returns the end or NULL. */
static inline const char *help_markup_bracket(const char *s, const char *end,
                                              HelpTok *t) {
  const char *close = memchr(s, ']', (size_t)(end - s));
  const char *rp = close && close + 1 < end && close[1] == '('
                       ? memchr(close, ')', (size_t)(end - close))
                       : NULL;
  if (!rp) return NULL;
  t->text = s + 1;
  t->text_end = close;
  t->target = close + 2;
  t->target_end = rp;
  return rp + 1;
}

/* A '*' that opens or closes emphasis touches a word on one side. */
static inline int help_markup_emphasis(const char *begin, const char *s,
                                       const char *end) {
  int opens = s + 1 < end && !isspace((unsigned char)s[1]) && s[1] != '*';
  int closes = s > begin && !isspace((unsigned char)s[-1]) && s[-1] != '*';
  return opens || closes;
}

/* The token at s; returns where the next one starts. `begin` is the start of
   the whole text. At the end t->kind is HM_END. */
static inline const char *help_markup_next(const char *begin, const char *s,
                                           const char *end, HelpTok *t) {
  const char *p;
  memset(t, 0, sizeof *t);
  if (s >= end) {
    t->kind = HM_END;
    return s;
  }
  if (*s == '!' && s + 1 < end && s[1] == '[' &&
      (p = help_markup_bracket(s + 1, end, t))) {
    t->kind = HM_IMAGE;
    return p;
  }
  if (*s == '[' && (p = help_markup_bracket(s, end, t))) {
    t->kind = HM_LINK;
    return p;
  }
  if (*s == '`' && (p = memchr(s + 1, '`', (size_t)(end - s - 1)))) {
    t->kind = HM_CODE;
    t->text = s + 1;
    t->text_end = p;
    return p + 1;
  }
  if (*s == '*') {
    if (s + 1 < end && s[1] == '*') {
      t->kind = HM_SKIP;
      return s + 2;
    }
    if (help_markup_emphasis(begin, s, end)) {
      t->kind = HM_SKIP;
      return s + 1;
    }
  }
  t->kind = HM_TEXT;
  t->text = s;
  p = s + 1;
  while (p < end && *p != '[' && *p != '`' && *p != '*' &&
         !(*p == '!' && p + 1 < end && p[1] == '['))
    p++;
  t->text_end = p;
  return p;
}

#endif
