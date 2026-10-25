/* Helpers shared by the engine API's source files (not part of the API). */
#ifndef XPPE_INTERNAL_H
#define XPPE_INTERNAL_H

#include <setjmp.h>
#include <stdio.h>

#include "xpps_embed_internal.h"

int xppe_loaded(void);
int xppe_require_model(void);
void xppe_set_error(const char *fmt, const char *arg);
int xppe_same_name(const char *a, const char *b);

/* Start and finish a call that may print: resets, then collects, the log. */
void xppe_begin_call(void);
void xppe_end_call(void);

/* Called when the engine aborts inside a guarded region. */
void xppe_mark_broken(void);

/* The result table (xppe_data.c), for the API's other files: start a table
   of nrows rows with these column names, then fill it cell by cell. */
int xppe_result_start(int nrows, int ncols, const char *const *names);
void xppe_result_put(int col, int row, double value);

/* Run the statements between XPPE_GUARD_BEGIN and XPPE_GUARD_END in a
   guarded region. If the engine calls exit() inside it, `cleanup` runs and
   the enclosing function returns XPPE_ERR_FATAL. */
#define XPPE_GUARD_BEGIN(cleanup)                                      \
  do {                                                                 \
    jmp_buf guard_jb;                                                  \
    if (setjmp(guard_jb)) {                                            \
      cleanup;                                                         \
      xppe_mark_broken();                                              \
      xppe_end_call();                                                 \
      return XPPE_ERR_FATAL;                                           \
    }                                                                  \
    xpps_guard_set(&guard_jb);

#define XPPE_GUARD_END                                                 \
    xpps_guard_set(NULL);                                              \
  } while (0)

#endif
