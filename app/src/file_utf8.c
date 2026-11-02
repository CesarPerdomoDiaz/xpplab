/* See file_utf8.h. */
#include "file_utf8.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

FILE *app_fopen_utf8(const char *path, const char *mode) {
#ifdef _WIN32
  wchar_t wp[2048], wm[16];
  if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, 2048) <= 0) return NULL;
  if (MultiByteToWideChar(CP_UTF8, 0, mode, -1, wm, 16) <= 0) return NULL;
  return _wfopen(wp, wm);
#else
  return fopen(path, mode);
#endif
}
