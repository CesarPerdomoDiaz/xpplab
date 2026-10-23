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

#ifdef __cplusplus
}
#endif

#define printf xpps_printf
#define exit xpps_exit

#endif
