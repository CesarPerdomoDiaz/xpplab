/* Private engine copies; see xpp_instance.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

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

/* Paths are UTF-8. On Windows the wide-character calls take them; the ANSI
   ones read the active code page, which breaks a user folder such as
   a user name with accents. */
#ifdef _WIN32
static int to_wide(const char *s, wchar_t *w, int n) {
  return MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) > 0;
}

static int to_utf8(const wchar_t *w, char *s, int n) {
  return WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL) > 0;
}
#endif

static FILE *open_utf8(const char *path, const char *mode) {
#ifdef _WIN32
  wchar_t wp[2048], wm[8];
  if (!to_wide(path, wp, 2048) || !to_wide(mode, wm, 8)) return NULL;
  return _wfopen(wp, wm);
#else
  return fopen(path, mode);
#endif
}

static int copy_file(const char *from, const char *to) {
  FILE *in = open_utf8(from, "rb"), *out;
  char buf[65536];
  size_t n;
  if (!in) return -1;
  out = open_utf8(to, "wb");
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
  wchar_t tmp[MAX_PATH], wdir[MAX_PATH + 64];
  static unsigned counter;
  DWORD n = GetTempPathW(MAX_PATH, tmp);
  int tries;
  if (n == 0 || n >= MAX_PATH) return -1;
  for (tries = 0; tries < 100; tries++) {
    swprintf(wdir, sizeof wdir / sizeof *wdir, L"%lsxpplab-%lu-%lu-%u", tmp,
             (unsigned long)GetCurrentProcessId(),
             (unsigned long)GetTickCount(), counter++);
    if (CreateDirectoryW(wdir, NULL))
      return to_utf8(wdir, dir, (int)len) ? 0 : -1;
  }
  return -1;
#else
  const char *t = getenv("TMPDIR");
  snprintf(dir, len, "%s/xpplab-XXXXXX", t && *t ? t : "/tmp");
  return mkdtemp(dir) ? 0 : -1;
#endif
}

static void remove_tree(const char *dir) {
#ifdef _WIN32
  char path[1100], name[1100];
  wchar_t pattern[1100], wpath[1100];
  WIN32_FIND_DATAW fd;
  HANDLE h;
  if (!to_wide(dir, pattern, 1096)) return;
  wcscat(pattern, L"\\*");
  h = FindFirstFileW(pattern, &fd);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
        continue;
      if (!to_utf8(fd.cFileName, name, sizeof name)) continue;
      snprintf(path, sizeof path, "%s\\%s", dir, name);
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        remove_tree(path);
      else if (to_wide(path, wpath, 1100))
        DeleteFileW(wpath);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
  if (to_wide(dir, wpath, 1100)) RemoveDirectoryW(wpath);
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
  {
    wchar_t w[1100];
    if (to_wide(e->workdir, w, 1100)) CreateDirectoryW(w, NULL);
    e->handle_ = to_wide(e->libcopy_, w, 1100) ? (void *)LoadLibraryW(w) : NULL;
  }
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
