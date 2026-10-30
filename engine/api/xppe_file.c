/*
 * Files by their UTF-8 names, for the engine API's own writers. On Windows
 * fopen() reads a name in the ANSI code page, so a path such as
 * C:\Users\José\run.mat would fail or name another file; this opens it as
 * UTF-16 instead. Elsewhere it is fopen().
 */
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "xppe_internal.h"

FILE *xppe_fopen_utf8(const char *path, const char *mode) {
#ifdef _WIN32
  wchar_t wp[2048], wm[16];
  if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, 2048) <= 0) return NULL;
  if (MultiByteToWideChar(CP_UTF8, 0, mode, -1, wm, 16) <= 0) return NULL;
  return _wfopen(wp, wm);
#else
  return fopen(path, mode);
#endif
}
