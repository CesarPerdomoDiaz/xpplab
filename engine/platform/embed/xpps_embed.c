/*
 * The embedding layer behind xpps_embed.h: output capture and the exit()
 * guard. See xpps_embed.h for why this exists.
 */
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

FILE *xpps_log_stream(void) {
  if (xpps_embedded) {
    if (!capture) capture = tmpfile();
    if (capture) return capture;
  }
  return stdout;
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
  fflush(stdout);
  exit(code);
}

void xpps_guard_set(jmp_buf *jb) { guard = jb; }

int xpps_guard_code(void) { return guard_code; }

void xpps_capture_reset(void) {
  /* tmpfile() streams cannot be truncated portably, so start a new one. */
  if (capture) {
    fclose(capture);
    capture = NULL;
  }
}

char *xpps_capture_take(void) {
  FILE *f = xpps_log_stream();
  long n;
  char *text;
  if (f == stdout) return NULL;
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
