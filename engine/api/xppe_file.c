/*
 * Files by their UTF-8 names, for the engine API's own writers. On Windows
 * fopen() reads a name in the ANSI code page, so a path such as
 * C:\Users\José\run.mat would fail or name another file; this opens it as
 * UTF-16 instead. Elsewhere it is fopen().
 */
#include <stdio.h>
#include <string.h>

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

void xppe_ascii_path(char *path, size_t size) {
#ifdef _WIN32
  wchar_t w[1024], s[1024];
  char a[1024];
  const char *p;
  for (p = path; *p && !(*p & 0x80); p++) {
  }
  if (!*p) return;
  if (MultiByteToWideChar(CP_UTF8, 0, path, -1, w, 1024) > 0 &&
      GetShortPathNameW(w, s, 1024) > 0 &&
      WideCharToMultiByte(CP_UTF8, 0, s, -1, a, sizeof a, NULL, NULL) > 0 &&
      strlen(a) < size)
    strcpy(path, a);
#else
  (void)path;
  (void)size;
#endif
}
