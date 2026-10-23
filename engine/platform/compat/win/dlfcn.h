/* Minimal <dlfcn.h> for Windows, implemented in dlfcn_win.c on top of
   LoadLibrary. XPPAUT uses it to load user right-hand-side functions. */
#ifndef XPP_COMPAT_DLFCN_H
#define XPP_COMPAT_DLFCN_H
#define RTLD_LAZY 1
#define RTLD_NOW 2
#define RTLD_GLOBAL 4
#define RTLD_LOCAL 0
void *dlopen(const char *file, int mode);
void *dlsym(void *handle, const char *name);
int dlclose(void *handle);
char *dlerror(void);
#endif
