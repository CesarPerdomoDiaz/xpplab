/*
 * The embedding layer behind xpps_embed.h: output capture and the exit()
 * guard. See xpps_embed.h for why this exists.
 */
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xpps_embed.h"
#include "xpps_embed_internal.h"

#undef printf
#undef exit

int xpps_embedded = 0;

static FILE *capture;
static jmp_buf *guard;
static int guard_code;

/* In this file stdout and stderr name the engine's view of the streams
   (see xpps_embed.h); the real ones come from xpps_real_stdout/stderr. */
FILE *xpps_log_stream(void) {
  if (xpps_embedded) {
    if (!capture) capture = tmpfile();
    if (capture) return capture;
  }
  return xpps_real_stdout();
}

FILE *xpps_stdout(void) { return xpps_log_stream(); }

FILE *xpps_stderr(void) {
  return xpps_embedded ? xpps_log_stream() : xpps_real_stderr();
}

int xpps_printf(const char *fmt, ...) {
  va_list ap;
  int n;
  va_start(ap, fmt);
  n = vfprintf(xpps_log_stream(), fmt, ap);
  va_end(ap);
  return n;
}

void xpps_exit(int code) {
  if (guard) {
    jmp_buf *g = guard;
    guard = NULL;
    guard_code = code;
    longjmp(*g, 1);
  }
  fflush(xpps_real_stdout());
  exit(code);
}

void xpps_guard_set(jmp_buf *jb) { guard = jb; }

int xpps_guard_code(void) { return guard_code; }

extern FILE *logfile; /* XPPAUT's console stream (main.c) */

/* tmpfile() streams cannot be truncated portably, so each call gets a new
   one. XPPAUT keeps the stream in its global logfile, which must follow:
   left pointing at the closed stream, every later message wrote into
   freed memory and corrupted the heap. */
void xpps_capture_reset(void) {
  if (capture) {
    int was_log = logfile == capture;
    fclose(capture);
    capture = NULL;
    if (was_log) logfile = xpps_log_stream();
  }
}

char *xpps_capture_take(void) {
  FILE *f = xpps_log_stream();
  long n;
  char *text;
  if (f == xpps_real_stdout()) return NULL;
  fflush(f);
  n = ftell(f);
  if (n < 0) n = 0;
  text = malloc((size_t)n + 1);
  if (!text) return NULL;
  rewind(f);
  n = (long)fread(text, 1, (size_t)n, f);
  text[n] = '\0';
  xpps_capture_reset();
  return text;
}

/* ---- Headless dialog answers ------------------------------------------- */

#define MAX_ANSWERS 32
#define MAX_ANSWER_LEN 512

static char answers[MAX_ANSWERS][MAX_ANSWER_LEN];
static int answer_head, answer_count;

void xpps_answers_push(const char *answer) {
  int slot;
  if (answer_count >= MAX_ANSWERS) return;
  slot = (answer_head + answer_count) % MAX_ANSWERS;
  snprintf(answers[slot], MAX_ANSWER_LEN, "%s", answer);
  answer_count++;
}

void xpps_answers_clear(void) { answer_head = answer_count = 0; }

const char *xpps_answer_next(void) {
  const char *a;
  if (answer_count == 0) return NULL;
  a = answers[answer_head];
  answer_head = (answer_head + 1) % MAX_ANSWERS;
  answer_count--;
  return a;
}

/* Field labels in XPPAUT forms may carry a "*N" prefix that selects the
   kind of input widget; it is not part of the name. */
static const char *field_label(const char *name) {
  if (name[0] == '*' && name[1] != '\0') return name + 2;
  return name;
}

static int same_label(const char *a, const char *b, size_t blen) {
  size_t i;
  for (i = 0; i < blen; i++)
    if (a[i] == '\0' ||
        toupper((unsigned char)a[i]) != toupper((unsigned char)b[i]))
      return 0;
  return a[blen] == '\0';
}

int xpps_answer_fields(int n, char **names, char *values, int stride) {
  const char *a = xpps_answer_next(), *p;
  if (!a) return 0;
  for (p = a; *p;) {
    const char *eq = strchr(p, '='), *end = strchr(p, ';');
    int i;
    if (!end) end = p + strlen(p);
    if (eq && eq < end) {
      for (i = 0; i < n; i++)
        if (same_label(field_label(names[i]), p, (size_t)(eq - p))) {
          size_t len = (size_t)(end - eq - 1);
          if (len >= (size_t)stride) len = (size_t)stride - 1;
          memcpy(values + (size_t)i * (size_t)stride, eq + 1, len);
          values[(size_t)i * (size_t)stride + len] = '\0';
          break;
        }
    }
    p = *end ? end + 1 : end;
  }
  return 1;
}
