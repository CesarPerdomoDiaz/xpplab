/* Private engine copies; see xpp_instance.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xpp_instance.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

const char *xpp_instance_library_name(void) {
#if defined(_WIN32)
  return "libxppe.dll";
#elif defined(__APPLE__)
  return "libxppe.dylib";
#else
  return "libxppe.so";
#endif
}

static void set_err(char *err, size_t errlen, const char *msg, const char *arg) {
  if (err && errlen) snprintf(err, errlen, msg, arg ? arg : "");
}

static int copy_file(const char *from, const char *to) {
  FILE *in = fopen(from, "rb"), *out;
  char buf[65536];
  size_t n;
  if (!in) return -1;
  out = fopen(to, "wb");
  if (!out) {
    fclose(in);
    return -1;
  }
  while ((n = fread(buf, 1, sizeof buf, in)) > 0)
    if (fwrite(buf, 1, n, out) != n) {
      fclose(in);
      fclose(out);
      return -1;
    }
  fclose(in);
  return fclose(out) == 0 ? 0 : -1;
}

static const char *base_name(const char *path) {
  const char *b = path, *p;
  for (p = path; *p; p++)
    if (*p == '/' || *p == '\\') b = p + 1;
  return b;
}

/* Make a fresh private directory and put its path in dir. */
static int make_private_dir(char *dir, size_t len) {
#ifdef _WIN32
  char tmp[MAX_PATH];
  static unsigned counter;
  DWORD n = GetTempPathA(sizeof tmp, tmp);
  int tries;
  if (n == 0 || n >= sizeof tmp) return -1;
  for (tries = 0; tries < 100; tries++) {
    snprintf(dir, len, "%sxppstudio-%lu-%lu-%u", tmp,
             (unsigned long)GetCurrentProcessId(),
             (unsigned long)GetTickCount(), counter++);
    if (CreateDirectoryA(dir, NULL)) return 0;
  }
  return -1;
#else
  const char *t = getenv("TMPDIR");
  snprintf(dir, len, "%s/xppstudio-XXXXXX", t && *t ? t : "/tmp");
  return mkdtemp(dir) ? 0 : -1;
#endif
}

static void remove_tree(const char *dir) {
#ifdef _WIN32
  char pattern[1100], path[1100];
  WIN32_FIND_DATAA fd;
  HANDLE h;
  snprintf(pattern, sizeof pattern, "%s\\*", dir);
  h = FindFirstFileA(pattern, &fd);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
        continue;
      snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        remove_tree(path);
      else
        DeleteFileA(path);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
  }
  RemoveDirectoryA(dir);
#else
  DIR *d = opendir(dir);
  struct dirent *e;
  char path[1100];
  struct stat st;
  if (d) {
    while ((e = readdir(d)) != NULL) {
      if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
        continue;
      snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
      if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        remove_tree(path);
      else
        unlink(path);
    }
    closedir(d);
  }
  rmdir(dir);
#endif
}

xpp_instance *xpp_instance_open(const char *library_path, char *err,
                                size_t errlen) {
  xpp_instance *e = calloc(1, sizeof *e);
  char dir[1024];
  if (!e) {
    set_err(err, errlen, "out of memory", NULL);
    return NULL;
  }
  if (make_private_dir(dir, sizeof dir) != 0) {
    set_err(err, errlen, "cannot create a temporary directory", NULL);
    free(e);
    return NULL;
  }
  snprintf(e->libcopy_, sizeof e->libcopy_, "%s/%s", dir,
           base_name(library_path));
  snprintf(e->workdir, sizeof e->workdir, "%s/work", dir);
  if (copy_file(library_path, e->libcopy_) != 0) {
    set_err(err, errlen, "cannot copy the engine library %s", library_path);
    remove_tree(dir);
    free(e);
    return NULL;
  }
#ifdef _WIN32
  CreateDirectoryA(e->workdir, NULL);
  e->handle_ = (void *)LoadLibraryA(e->libcopy_);
#else
  mkdir(e->workdir, 0700);
  e->handle_ = dlopen(e->libcopy_, RTLD_NOW | RTLD_LOCAL);
#endif
  if (!e->handle_) {
    set_err(err, errlen, "cannot load the engine library %s", library_path);
    remove_tree(dir);
    free(e);
    return NULL;
  }
#ifdef _WIN32
#define XPP_RESOLVE(ret, name, params)                                       \
  *(void **)&e->api.name = (void *)GetProcAddress((HMODULE)e->handle_, #name);
#else
#define XPP_RESOLVE(ret, name, params)                                       \
  *(void **)&e->api.name = dlsym(e->handle_, #name);
#endif
  XPP_ENGINE_FUNCTIONS(XPP_RESOLVE)
#undef XPP_RESOLVE
#define XPP_CHECK(ret, name, params)                                         \
  if (!e->api.name) {                                                        \
    set_err(err, errlen, "the engine library lacks %s", #name);             \
    xpp_instance_close(e);                                                   \
    return NULL;                                                             \
  }
  XPP_ENGINE_FUNCTIONS(XPP_CHECK)
#undef XPP_CHECK
  if (e->api.xppe_api_version() != XPPE_API_VERSION) {
    set_err(err, errlen, "the engine library has another API version", NULL);
    xpp_instance_close(e);
    return NULL;
  }
  e->api.xppe_auto_set_workdir(e->workdir);
  return e;
}

void xpp_instance_close(xpp_instance *e) {
  char dir[1024];
  char *slash;
  if (!e) return;
  if (e->handle_) {
#ifdef _WIN32
    FreeLibrary((HMODULE)e->handle_);
#else
    dlclose(e->handle_);
#endif
  }
  snprintf(dir, sizeof dir, "%s", e->libcopy_);
  slash = strrchr(dir, '/');
  if (slash) {
    *slash = '\0';
    remove_tree(dir);
  }
  free(e);
}
