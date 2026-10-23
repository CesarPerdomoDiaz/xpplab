/* Embedding layer internals, used by the engine API (api/xppe.c). */
#ifndef XPPS_EMBED_INTERNAL_H
#define XPPS_EMBED_INTERNAL_H

#include <setjmp.h>

/* Make the next xpps_exit() longjmp to jb instead of ending the process.
   The guard disarms itself when it fires; pass NULL to disarm it. */
void xpps_guard_set(jmp_buf *jb);
/* The code the engine passed to exit() when the guard last fired. */
int xpps_guard_code(void);

/* Return everything printed since the last call (malloc'd, caller frees). */
char *xpps_capture_take(void);
void xpps_capture_reset(void);

#endif
