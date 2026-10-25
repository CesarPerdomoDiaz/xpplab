/*
 * Force-included into every XPPAUT engine source file.
 *
 * XPPAUT was written as a program, not a library: it prints with printf()
 * and ends the process with exit() when it meets an error. Routing both
 * through the embedding layer (xpps_embed.c) lets the engine run inside a
 * library or an application. Output is captured for the caller, and fatal
 * errors unwind to the API call that triggered them instead of killing the
 * host. Standalone builds behave exactly as before.
 */
#ifndef XPPS_EMBED_H
#define XPPS_EMBED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Nonzero while the engine runs under the embedding API. */
extern int xpps_embedded;

int xpps_printf(const char *fmt, ...);
void xpps_exit(int code)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((noreturn))
#endif
    ;
/* Where plintf() output goes while embedded. */
FILE *xpps_log_stream(void);

/* Run fn() so that an exit() inside it ends only fn(): returns 1 when fn()
   returned, 0 when it called exit(). Used around one AUTO continuation,
   whose code calls exit() on a singular matrix and a few other errors.
   Outside the embedding API it simply calls fn(). */
int xpps_call_guarded(int (*fn)(void));

/* Headless answers for XPPAUT's dialogs. While embedded, every dialog
   (menus, yes/no questions, text prompts, forms) takes its answer from a
   queue filled by the API instead of waiting for the user, so no code path
   can block. An empty queue means "cancel". */
const char *xpps_answer_next(void);
/* Fill a form: the next answer is "Name=value;Name=value". Field names are
   matched without XPPAUT's "*N" prefixes, ignoring case. Returns 1 if an
   answer was used, 0 if the queue was empty. */
int xpps_answer_fields(int n, char **names, char *values, int stride);

#ifdef __cplusplus
}
#endif

/* The real standard streams, captured before the macros below redefine
   the names. */
static inline FILE *xpps_real_stdout(void) { return stdout; }
static inline FILE *xpps_real_stderr(void) { return stderr; }
/* stdout and stderr as the engine sees them: the capture stream while
   embedded, the real streams otherwise. */
FILE *xpps_stdout(void);
FILE *xpps_stderr(void);

#define printf xpps_printf
#define exit xpps_exit
#undef stdout
#define stdout (xpps_stdout())
#undef stderr
#define stderr (xpps_stderr())

#endif
