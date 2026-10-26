/* XPP Lab's own Python. The console needs a Python 3 with NumPy; when the
   computer has none, python fetch downloads one: a CPython build from the
   python-build-standalone project (made to be unpacked anywhere and run
   from there), checked against the SHA-256 digest below, unpacked into the
   app's settings folder and given NumPy and Matplotlib with pip. Nothing
   outside that folder changes, and deleting it removes everything.

   The steps run as child processes (curl and tar, which Windows 10 and
   later, macOS and Linux have; then the new Python's pip), watched once a
   frame so the app keeps drawing. Everything goes to a folder of its own
   first and is moved into place only at the end, so a download stopped
   half-way leaves nothing behind that the console would try to use.

   $XPPLAB_PYTHON_URL and $XPPLAB_PYTHON_SHA256 replace the archive (a copy
   on a lab's own server, say; both are needed), and $XPPLAB_RUNTIME_DIR the
   folder it goes to. */
#include "pyfetch.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "pyconsole.h"
#include "sha256.h"

/* ---- What is downloaded --------------------------------------------------------- */

#define PBS_RELEASE "20261013"
#define PBS_PYTHON "3.13.16"
#define PBS_URL                                                                      \
  "https://github.com/astral-sh/python-build-standalone/releases/download/" PBS_RELEASE \
  "/cpython-" PBS_PYTHON "%2B" PBS_RELEASE "-"
#define PACKAGES "numpy matplotlib"
#define PACKAGES_MB 33 /* what pip downloads for them, about (Windows, 2026) */

typedef struct {
  const char *target; /* the build's platform */
  long long bytes;
  const char *sha256;
} Archive;

static const Archive archive_here = {
#if defined(_WIN32) && (defined(__aarch64__) || defined(_M_ARM64))
    "aarch64-pc-windows-msvc", 20997909,
    "9c324563049caf05f54d8dedc382621d75e2cd652a19415256681d324009cb67"
#elif defined(_WIN32)
    "x86_64-pc-windows-msvc", 21987516,
    "7aba3985d4cee1c7cacb963ca6aa08a5d3e695d7f9609d9fabc56f22ddcf5a06"
#elif defined(__APPLE__) && defined(__aarch64__)
    "aarch64-apple-darwin", 25268458,
    "16a5332fb0dcb77f163fc8f8cbccd95be1644cc53e68e5d2a79f50afd6680a0e"
#elif defined(__APPLE__) && defined(__x86_64__)
    "x86_64-apple-darwin", 25001913,
    "f507ae04eb7747cd97cb484e8c5d83936cbfc52d9b08a1f35c00b47c707b1ae1"
#elif defined(__linux__) && defined(__aarch64__)
    "aarch64-unknown-linux-gnu", 29331623,
    "69a30ed26198178732c37b8de0b6b5191479fbacfe526954da776befe38f18e7"
#elif defined(__linux__) && defined(__x86_64__)
    "x86_64-unknown-linux-gnu", 35099487,
    "d0080766b6b74aeb81e1949ef0d9a20e6c17f7b160fb295a69b936ccb17660be"
#else
    NULL, 0, NULL
#endif
};

#ifdef _WIN32
#define EXE_IN "python/python.exe"
#define EXE_W "python/pythonw.exe" /* the same, without a console window */
#else
#define EXE_IN "python/bin/python3"
#define EXE_W EXE_IN
#endif
#define MARKER "python/xpplab-runtime.txt" /* written last: the install is whole */

const char *pyfetch_about(void) { return "Python " PBS_PYTHON " with NumPy and Matplotlib"; }

int pyfetch_megabytes(void) {
  const char *url = getenv("XPPLAB_PYTHON_URL");
  if (url && *url) return 0; /* unknown */
  return archive_here.target ? (int)((archive_here.bytes + 500000) / 1000000) + PACKAGES_MB : 0;
}

/* ---- Folders and files ------------------------------------------------------------ */

/* The folder that holds python/ (and python-fetch/ while it downloads),
   ending with a separator. */
static void root(char *out, size_t len) {
  const char *dir = getenv("XPPLAB_RUNTIME_DIR");
  char *pref;
  if (dir && *dir) {
    size_t n = strlen(dir);
    snprintf(out, len, "%s%s", dir, dir[n - 1] == '/' || dir[n - 1] == '\\' ? "" : "/");
    return;
  }
  pref = app_pref_dir();
  snprintf(out, len, "%s", pref ? pref : "./");
  SDL_free(pref);
}

void pyfetch_folder(char *out, size_t len) {
  char r[1024];
  root(r, sizeof r);
  snprintf(out, len, "%spython", r);
}

static bool is_file(const char *p) {
  SDL_PathInfo info;
  return SDL_GetPathInfo(p, &info) && info.type == SDL_PATHTYPE_FILE;
}

static long long file_size(const char *p) {
  SDL_PathInfo info;
  return SDL_GetPathInfo(p, &info) ? (long long)info.size : -1;
}

bool pyfetch_python(char *out, size_t len) {
  char r[1024], marker[1100];
  root(r, sizeof r);
  snprintf(marker, sizeof marker, "%s" MARKER, r);
  snprintf(out, len, "%s" EXE_IN, r);
  return is_file(marker) && is_file(out);
}

static void remove_tree(const char *path);

static SDL_EnumerationResult SDLCALL remove_entry(void *userdata, const char *dir,
                                                  const char *name) {
  char path[1100];
  (void)userdata;
  snprintf(path, sizeof path, "%s%s", dir, name); /* dir ends with a separator */
  remove_tree(path);
  return SDL_ENUM_CONTINUE;
}

static void remove_tree(const char *path) {
  SDL_PathInfo info;
  if (!SDL_GetPathInfo(path, &info)) return;
  if (info.type == SDL_PATHTYPE_DIRECTORY) SDL_EnumerateDirectory(path, remove_entry, NULL);
  SDL_RemovePath(path);
}

/* ---- The steps ------------------------------------------------------------------------ */

enum { IDLE, DOWNLOAD, VERIFY, UNPACK, PIP, CHECK };

static int step = IDLE;
static bool fetching;          /* python fetch, not just python install */
static SDL_Process *proc;
static bool proc_exit_known;   /* Windows reports no exit status for console tools */
static char tail[1024];        /* the end of the step's output, for an error */
static char line[1024];        /* pip's output not ended by a new line yet */
static bool show_output;       /* pip's lines go to the command window */
static char dir_tmp[1100], archive[1100], exe[1100], status_text[160];
static const char *want_sha;
static long long want_bytes;
static SDL_IOStream *hashing;
static Sha256 sha;
static long long hashed;
static Uint64 shown_at;        /* when progress was last written */

bool pyfetch_running(void) { return step != IDLE; }

const char *pyfetch_status(float *fraction) {
  *fraction = -1;
  if (step == DOWNLOAD && want_bytes > 0) {
    long long n = file_size(archive);
    *fraction = n > 0 ? (float)((double)n / (double)want_bytes) : 0;
    if (*fraction > 1) *fraction = 1;
  } else if (step == VERIFY && want_bytes > 0) {
    *fraction = (float)((double)hashed / (double)want_bytes);
  }
  return status_text;
}

/* On Windows a console program started from a window opens a console
   window of its own unless it runs in the background, which costs its exit
   status: then the result of the step is checked instead. pythonw.exe is a
   window program and needs neither. */
static bool needs_background(const char *exe_path) {
#ifdef _WIN32
  size_t k = strlen(exe_path);
  return !(k >= 11 && SDL_strcasecmp(exe_path + k - 11, "pythonw.exe") == 0);
#else
  (void)exe_path;
  return false;
#endif
}

/* Start a child with its output (and errors) coming back to the app. */
static bool spawn(const char *const *args) {
  SDL_PropertiesID props = SDL_CreateProperties();
  SDL_Environment *env = pyconsole_environment();
  bool background = needs_background(args[0]);
  /* For the new Python, none of another Python's settings. */
  SDL_UnsetEnvironmentVariable(env, "PYTHONHOME");
  SDL_UnsetEnvironmentVariable(env, "PYTHONPATH");
  SDL_SetEnvironmentVariable(env, "PYTHONNOUSERSITE", "1", true);
  SDL_SetEnvironmentVariable(env, "PYTHONIOENCODING", "utf-8", true);
  SDL_SetEnvironmentVariable(env, "PYTHONUNBUFFERED", "1", true);
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
  SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
  SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, background);
  proc_exit_known = !background;
  proc = SDL_CreateProcessWithProperties(props);
  SDL_DestroyProperties(props);
  SDL_DestroyEnvironment(env);
  tail[0] = line[0] = '\0';
  if (!proc) {
    snprintf(tail, sizeof tail, "cannot start %s: %s", args[0], SDL_GetError());
    return false;
  }
  return true;
}

/* Keep the end of the output: an error is usually there. */
static void keep(const char *s, size_t n) {
  size_t t = strlen(tail), room = sizeof tail - 1;
  if (n >= room) {
    s += n - room;
    n = room;
    t = 0;
  } else if (t + n > room) {
    memmove(tail, tail + (t + n - room), room - n);
    t = room - n;
  }
  memcpy(tail + t, s, n);
  tail[t + n] = '\0';
}

/* The child's output so far: kept for errors, and shown line by line when
   asked (pip's). */
static void drain(App *a) {
  char buf[4096];
  size_t n, k, t;
  SDL_IOStream *out = proc ? SDL_GetProcessOutput(proc) : NULL;
  while (out && (n = SDL_ReadIO(out, buf, sizeof buf)) > 0) {
    keep(buf, n);
    for (k = 0; show_output && k < n; k++) {
      if (buf[k] == '\n' || buf[k] == '\r') {
        if (line[0]) app_log(a, "  %s", line);
        line[0] = '\0';
      } else if ((t = strlen(line)) + 1 < sizeof line) {
        line[t] = buf[k];
        line[t + 1] = '\0';
      }
    }
  }
}

/* The child has ended: true, with its exit status where it is known. */
static bool ended(App *a, int *status) {
  *status = 0;
  drain(a);
  if (!proc || !SDL_WaitProcess(proc, false, status)) return false;
  drain(a);
  if (show_output && line[0]) app_log(a, "  %s", line);
  line[0] = '\0';
  SDL_DestroyProcess(proc);
  proc = NULL;
  return true;
}

/* The last line of the child's output, for an error message. */
static const char *last_line(void) {
  char *p = tail + strlen(tail);
  while (p > tail && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ')) *--p = '\0';
  while (p > tail && p[-1] != '\n' && p[-1] != '\r') p--;
  if (strstr(p, "--help") && p > tail) { /* curl's "try --help": the line before says why */
    char *q = p - 1;
    while (q > tail && (q[-1] == '\n' || q[-1] == '\r')) q--;
    while (q > tail && q[-1] != '\n' && q[-1] != '\r') q--;
    for (p = q; *q; q++)
      if (*q == '\n' || *q == '\r') *q = ' ';
  }
  return p;
}

static void finish(App *a, bool ok, const char *fmt, ...) {
  char msg[1200];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  if (proc) {
    SDL_KillProcess(proc, true);
    SDL_DestroyProcess(proc);
    proc = NULL;
  }
  if (hashing) SDL_CloseIO(hashing);
  hashing = NULL;
  if (fetching) remove_tree(dir_tmp);
  step = IDLE;
  app_log(a, "%s%s", ok ? "" : "error: ", msg);
  if (fetching) pyconsole_fetched(a, ok);
}

/* The tools: Windows' own curl and tar (an MSYS or Git tar on the PATH
   would take C: for the name of a remote host). */
static void tool(char *out, size_t len, const char *name) {
#ifdef _WIN32
  const char *win = getenv("SystemRoot");
  snprintf(out, len, "%s\\System32\\%s.exe", win ? win : "C:\\Windows", name);
  if (!is_file(out)) snprintf(out, len, "%s.exe", name);
#else
  snprintf(out, len, "%s", name);
#endif
}

static char curl[1100], tar[1100];
static const char *url;
static char url_buf[600];

static bool start_download(void) {
  const char *args[] = {curl, "-fsSL", "--retry", "3", "--connect-timeout", "30",
                        "-o", archive, url, NULL};
  tool(curl, sizeof curl, "curl");
  if (spawn(args)) return true;
#ifndef _WIN32
  { /* no curl: wget */
    const char *wget[] = {"wget", "-q", "-O", archive, url, NULL};
    if (spawn(wget)) return true;
    snprintf(tail, sizeof tail, "neither curl nor wget can be started");
  }
#endif
  return false;
}

static bool start_unpack(void) {
  const char *args[] = {tar, "-xzf", archive, "-C", dir_tmp, NULL};
  tool(tar, sizeof tar, "tar");
  return spawn(args);
}

/* pythonw.exe beside a python.exe: the same Python, without a window. */
static void windowless(char *py, size_t len) {
#ifdef _WIN32
  size_t k = strlen(py);
  char w[1100];
  if (k > 10 && SDL_strcasecmp(py + k - 10, "python.exe") == 0) {
    snprintf(w, sizeof w, "%.*spythonw.exe", (int)(k - 10), py);
    if (is_file(w)) snprintf(py, len, "%s", w);
  }
#else
  (void)py, (void)len;
#endif
}

static bool start_pip(const char *packages) {
  static char pkgs[512];
  const char *args[48];
  char *p;
  int n = 0;
  snprintf(pkgs, sizeof pkgs, "%s", packages);
  args[n++] = exe;
  args[n++] = "-m";
  args[n++] = "pip";
  args[n++] = "install";
  args[n++] = "--disable-pip-version-check";
  args[n++] = "--no-warn-script-location";
  args[n++] = "--progress-bar";
  args[n++] = "off";
  for (p = strtok(pkgs, " \t"); p && n < 46; p = strtok(NULL, " \t")) args[n++] = p;
  args[n] = NULL;
  show_output = true;
  return spawn(args);
}

/* The new Python imports what the console needs and says its version. */
static bool start_check(void) {
  const char *args[] = {
      exe, "-c", "import sys, numpy, matplotlib; print('Python', sys.version.split()[0])",
      NULL};
  show_output = false;
  return spawn(args);
}

bool pyfetch_start(App *a) {
  char r[1024], installed[1100], old[1100];
  const char *sha = getenv("XPPLAB_PYTHON_SHA256");
  if (step != IDLE) {
    app_log(a, "%s ...", status_text);
    return true;
  }
  if (pyfetch_python(installed, sizeof installed)) {
    app_log(a, "XPP Lab's Python is installed: %s", installed);
    return true;
  }
  url = getenv("XPPLAB_PYTHON_URL");
  if (url && *url) {
    if (!sha || strlen(sha) != 64) {
      app_log(a, "error: XPPLAB_PYTHON_URL needs XPPLAB_PYTHON_SHA256, the archive's "
                 "SHA-256 digest (64 hex digits)");
      return false;
    }
    want_sha = sha;
    want_bytes = 0;
  } else if (archive_here.target) {
    /* PBS_URL holds a %2B (the + of the name): not a format */
    snprintf(url_buf, sizeof url_buf, "%s%s-install_only_stripped.tar.gz", PBS_URL,
             archive_here.target);
    url = url_buf;
    want_sha = archive_here.sha256;
    want_bytes = archive_here.bytes;
  } else {
    app_log(a, "error: there is no Python to download for this computer; install "
               "Python 3 with NumPy and choose it: python use PATH");
    return false;
  }
  root(r, sizeof r);
  /* A python folder there without the marker is not XPP Lab's: leave it. */
  snprintf(old, sizeof old, "%spython", r);
  if (SDL_GetPathInfo(old, NULL)) {
    app_log(a, "error: %s is in the way (it is not a Python that XPP Lab installed); "
               "move it, or set XPPLAB_RUNTIME_DIR to another folder", old);
    return false;
  }
  snprintf(dir_tmp, sizeof dir_tmp, "%spython-fetch", r);
  snprintf(archive, sizeof archive, "%s/python.tar.gz", dir_tmp);
  remove_tree(dir_tmp); /* what a stopped download left */
  if (!SDL_CreateDirectory(dir_tmp)) {
    app_log(a, "error: cannot create %s: %s", dir_tmp, SDL_GetError());
    return false;
  }
  fetching = true;
  show_output = false;
  step = DOWNLOAD;
  if (!start_download()) {
    finish(a, false, "cannot download Python: %s", tail);
    return false;
  }
  shown_at = SDL_GetTicks();
  if (want_bytes > 0)
    app_log(a, "Downloading %s (%.0f MB, then about %d MB of packages) from %s ...",
            pyfetch_about(), want_bytes / 1e6, PACKAGES_MB, url);
  else
    app_log(a, "Downloading Python from %s ...", url);
  snprintf(status_text, sizeof status_text, "Downloading Python");
  return true;
}

bool pyfetch_pip(App *a, const char *python, const char *packages) {
  if (step != IDLE) {
    app_log(a, "error: wait until this has finished: %s", status_text);
    return false;
  }
  fetching = false;
  snprintf(exe, sizeof exe, "%s", python);
  windowless(exe, sizeof exe);
  if (!start_pip(packages)) {
    app_log(a, "error: %s", tail);
    return false;
  }
  step = PIP;
  app_log(a, "pip install %s (with %s) ...", packages, python);
  snprintf(status_text, sizeof status_text, "Installing %s", packages);
  return true;
}

void pyfetch_cancel(App *a) {
  if (step == IDLE) return;
  finish(a, false, "%s was stopped", fetching ? "Installing Python" : "pip");
}

/* The digest of the download, four megabytes a frame. */
static bool verified(void) {
  static unsigned char buf[1 << 16];
  int k;
  for (k = 0; k < 64; k++) {
    size_t n = SDL_ReadIO(hashing, buf, sizeof buf);
    if (n == 0) return true;
    sha256_update(&sha, buf, n);
    hashed += (long long)n;
  }
  return false;
}

/* Into place, then the marker that says the install is whole. */
static bool install(char *err, size_t len) {
  char r[1024], from[1100], to[1100], marker[1100];
  SDL_IOStream *f;
  root(r, sizeof r);
  snprintf(from, sizeof from, "%s/python", dir_tmp);
  snprintf(to, sizeof to, "%spython", r);
  if (!SDL_RenamePath(from, to)) {
    snprintf(err, len, "cannot move %s to %s: %s", from, to, SDL_GetError());
    return false;
  }
  snprintf(marker, sizeof marker, "%s" MARKER, r);
  if (!(f = SDL_IOFromFile(marker, "w"))) {
    snprintf(err, len, "cannot write %s: %s", marker, SDL_GetError());
    return false;
  }
  SDL_IOprintf(f, "XPP Lab's Python for its console: %s\nfrom %s\nwith pip install %s\n",
               pyfetch_about(), url, PACKAGES);
  SDL_CloseIO(f);
  return true;
}

void pyfetch_poll(App *a) {
  int status = 0;
  if (step == IDLE) return;
  if (step == DOWNLOAD) {
    long long got;
    bool cut;
    if (!ended(a, &status)) {
      if (want_bytes > 0 && SDL_GetTicks() - shown_at > 5000) {
        shown_at = SDL_GetTicks();
        app_log(a, "  %.0f of %.0f MB", file_size(archive) / 1e6, want_bytes / 1e6);
      }
      return;
    }
    /* Where curl's exit status is unknown, the size tells (and the digest
       after it). */
    got = file_size(archive);
    cut = want_bytes > 0 && got != want_bytes;
    if ((proc_exit_known && status != 0) || got <= 0 || cut) {
      if (last_line()[0])
        finish(a, false, "cannot download %s: %s", url, last_line());
      else if (cut)
        finish(a, false, "the download stopped at %.1f of %.1f MB; python fetch tries again",
               got / 1e6, want_bytes / 1e6);
      else
        finish(a, false, "cannot download %s: nothing came", url);
      return;
    }
    if (!(hashing = SDL_IOFromFile(archive, "rb"))) {
      finish(a, false, "cannot read %s: %s", archive, SDL_GetError());
      return;
    }
    sha256_init(&sha);
    hashed = 0;
    if (want_bytes <= 0) want_bytes = file_size(archive);
    step = VERIFY;
    snprintf(status_text, sizeof status_text, "Checking the download");
    return;
  }
  if (step == VERIFY) {
    char got[65];
    if (!verified()) return;
    SDL_CloseIO(hashing);
    hashing = NULL;
    sha256_hex(&sha, got);
    if (SDL_strcasecmp(got, want_sha) != 0) {
      finish(a, false, "the download is not the expected file (its SHA-256 is %s, not "
                       "%s); nothing was installed", got, want_sha);
      return;
    }
    step = UNPACK;
    if (!start_unpack()) {
      finish(a, false, "cannot unpack Python: %s", tail);
      return;
    }
    app_log(a, "  checked (SHA-256 %.12s...); unpacking ...", got);
    snprintf(status_text, sizeof status_text, "Unpacking Python");
    return;
  }
  if (step == UNPACK) {
    if (!ended(a, &status)) return;
    snprintf(exe, sizeof exe, "%s/" EXE_IN, dir_tmp);
    if ((proc_exit_known && status != 0) || !is_file(exe)) {
      finish(a, false, "cannot unpack Python: %s",
             last_line()[0] ? last_line() : "there is no Python in the archive");
      return;
    }
    SDL_RemovePath(archive);
    windowless(exe, sizeof exe);
    step = PIP;
    if (!start_pip(PACKAGES)) {
      finish(a, false, "cannot run pip: %s", tail);
      return;
    }
    app_log(a, "  installing NumPy and Matplotlib with pip ...");
    snprintf(status_text, sizeof status_text, "Installing NumPy and Matplotlib");
    return;
  }
  if (step == PIP) {
    if (!ended(a, &status)) return;
    show_output = false;
    if (proc_exit_known && status != 0) {
      finish(a, false, "pip failed (exit status %d): %s", status, last_line());
      return;
    }
    if (!fetching) {
      step = IDLE;
      app_log(a, pyconsole_running() ? "pip has finished; the running Python can import "
                                       "what is new (python restart for new versions of "
                                       "modules already imported)."
                                     : "pip has finished.");
      return;
    }
    step = CHECK;
    if (!start_check()) {
      finish(a, false, "cannot run the new Python: %s", tail);
      return;
    }
    snprintf(status_text, sizeof status_text, "Trying the new Python");
    return;
  }
  if (step == CHECK) {
    char err[1200], where[1100], version[64];
    if (!ended(a, &status)) return;
    if (status != 0 || strncmp(last_line(), "Python ", 7) != 0) {
      finish(a, false, "the new Python does not work: %s", last_line());
      return;
    }
    snprintf(version, sizeof version, "%s", last_line());
    if (!install(err, sizeof err)) {
      finish(a, false, "%s", err);
      return;
    }
    pyfetch_folder(where, sizeof where);
    finish(a, true, "%s with NumPy and Matplotlib is installed in %s.", version, where);
  }
}
