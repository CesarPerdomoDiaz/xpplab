/* dlopen() family on top of LoadLibrary. See win/dlfcn.h. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include "win/dlfcn.h"

static char dl_last_error[128];
static int dl_have_error;

static void dl_set_error(const char *what) {
  snprintf(dl_last_error, sizeof dl_last_error, "%s failed (Windows error %lu)",
           what, (unsigned long)GetLastError());
  dl_have_error = 1;
}

void *dlopen(const char *file, int mode) {
  HMODULE h;
  (void)mode;
  h = file ? LoadLibraryA(file) : GetModuleHandleA(NULL);
  if (!h) dl_set_error("LoadLibrary");
  return (void *)h;
}

void *dlsym(void *handle, const char *name) {
  FARPROC p = GetProcAddress((HMODULE)handle, name);
  if (!p) dl_set_error("GetProcAddress");
  return (void *)p;
}

int dlclose(void *handle) {
  if (FreeLibrary((HMODULE)handle)) return 0;
  dl_set_error("FreeLibrary");
  return -1;
}

char *dlerror(void) {
  if (!dl_have_error) return NULL;
  dl_have_error = 0;
  return dl_last_error;
}
#endif
