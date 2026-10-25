/* The Python console: the command window can take Python, which runs in a
   process of its own (python -m xpplab.console, from the copy of the
   package next to the app). The two talk through the process's standard
   input and output, one JSON object per line (see xpplab/console.py). The
   Python reaches back into the app through the local channel, so m.run()
   there runs here and the plots follow. A crash or an endless loop in the
   user's code cannot take the app down: Stop ends the process. */
#include "pyconsole.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "channel.h"

#define QUEUE 16

static SDL_Process *proc;
static SDL_IOStream *to_py, *from_py;
static char *rbuf;           /* what has come from Python, up to a new line */
static size_t rlen, rcap;
static char code[16384];     /* the lines of a statement not complete yet */
static bool more, ready, busy;
static int next_id = 1;
static char partial[4096];   /* output not ended by a new line yet */
static char queue[QUEUE][1024]; /* lines typed while Python was busy */
static int nqueue;
static char python_used[1100];

/* Completion: the word asked about, and what came back. */
static int complete_id = -1;
static char completion[256];
static bool completion_ready;

static PyVar *vars_rows;
static int nvars_rows, vars_id = -1;

const char *pyconsole_prompt(void) { return more ? "..." : "py>"; }
bool pyconsole_running(void) { return proc != NULL; }
bool pyconsole_busy(void) { return busy; }
/* Lines waiting, a line running, or a statement not complete yet. */
bool pyconsole_waiting(void) { return proc && (busy || !ready || nqueue > 0 || more); }

/* ---- Finding a Python ------------------------------------------------------------ */

static bool is_file(const char *p) {
  SDL_PathInfo info;
  return SDL_GetPathInfo(p, &info) && info.type == SDL_PATHTYPE_FILE;
}

/* The user's choice, $XPPLAB_PYTHON, else the first python on PATH (on
   Windows pythonw.exe, which opens no console window), else the py
   launcher. */
static bool find_python(App *a, char *out, size_t len, bool *launcher) {
  const char *env = getenv("XPPLAB_PYTHON"), *path = getenv("PATH");
#ifdef _WIN32
  static const char *names[] = {"pythonw.exe", "python.exe"};
  const char sep = ';';
#else
  static const char *names[] = {"python3", "python"};
  const char sep = ':';
#endif
  char dir[1024], cand[1100];
  const char *p, *e;
  size_t k;
  *launcher = false;
  if (a->python[0]) {
    snprintf(out, len, "%s", a->python);
    return true;
  }
  if (env && *env) {
    snprintf(out, len, "%s", env);
    return true;
  }
  for (p = path; p && *p; p = *e ? e + 1 : e) {
    e = strchr(p, sep);
    if (!e) e = p + strlen(p);
    snprintf(dir, sizeof dir, "%.*s", (int)(e - p), p);
    if (!dir[0] || strstr(dir, "WindowsApps")) continue; /* the Store's stub */
    for (k = 0; k < sizeof names / sizeof names[0]; k++) {
      snprintf(cand, sizeof cand, "%s/%s", dir, names[k]);
      if (is_file(cand)) {
        snprintf(out, len, "%s", cand);
        return true;
      }
    }
  }
#ifdef _WIN32
  {
    const char *win = getenv("SystemRoot");
    snprintf(cand, sizeof cand, "%s/py.exe", win ? win : "C:/Windows");
    if (is_file(cand)) {
      snprintf(out, len, "%s", cand);
      *launcher = true;
      return true;
    }
  }
#endif
  return false;
}

/* ---- The process ----------------------------------------------------------------- */

static void reset(void) {
  proc = NULL;
  to_py = from_py = NULL;
  rlen = 0;
  more = ready = busy = false;
  code[0] = partial[0] = '\0';
  nqueue = 0;
  complete_id = vars_id = -1;
}

/* The app's environment, for the child. Handed over whole
   (SDL_CreateEnvironment(true)), CreateProcess refused it in the app with
   "cannot find the file specified"; copied variable by variable, leaving
   out names without a value and those starting with '=' (Windows' folder
   of each drive), it is accepted. */
static SDL_Environment *child_environment(void) {
  SDL_Environment *env = SDL_CreateEnvironment(false);
  char **vars = SDL_GetEnvironmentVariables(SDL_GetEnvironment());
  int i;
  for (i = 0; vars && vars[i]; i++) {
    char name[512];
    const char *eq = strchr(vars[i], '=');
    if (!eq || eq == vars[i] || !eq[1] || (size_t)(eq - vars[i]) >= sizeof name) continue;
    snprintf(name, sizeof name, "%.*s", (int)(eq - vars[i]), vars[i]);
    SDL_SetEnvironmentVariable(env, name, eq + 1, true);
  }
  SDL_free(vars);
  return env;
}

static bool start(App *a) {
  char python[1100], pkg[1100], pp[4200], *old;
  const char *args[8];
  bool launcher;
  int n = 0;
  SDL_Environment *env;
  SDL_PropertiesID props;
  if (proc) return true;
  if (!find_python(a, python, sizeof python, &launcher)) {
    app_log(a, "error: no Python found. Install Python 3 with NumPy (python.org), "
               "or choose one: python use C:/path/to/python.exe");
    return false;
  }
#ifdef _WIN32
  { /* pythonw.exe beside a python.exe: the same Python, without a console */
    size_t n = strlen(python);
    char w[1100];
    if (n > 10 && SDL_strcasecmp(python + n - 10, "python.exe") == 0) {
      snprintf(w, sizeof w, "%.*spythonw.exe", (int)(n - 10), python);
      if (is_file(w)) snprintf(python, sizeof python, "%s", w);
    }
  }
#endif
  channel_start(a); /* app and m in the console come through it */
  snprintf(pkg, sizeof pkg, "%spython", a->base_dir);
  old = getenv("PYTHONPATH");
#ifdef _WIN32
  snprintf(pp, sizeof pp, "%s%s%s", pkg, old && *old ? ";" : "", old ? old : "");
#else
  snprintf(pp, sizeof pp, "%s%s%s", pkg, old && *old ? ":" : "", old ? old : "");
#endif
  env = child_environment();
  SDL_SetEnvironmentVariable(env, "PYTHONPATH", pp, true);
  SDL_SetEnvironmentVariable(env, "XPPLAB_ENGINE", a->engine_path, true);
  if (channel_path()[0]) SDL_SetEnvironmentVariable(env, "XPPLAB_CHANNEL", channel_path(), true);
  SDL_SetEnvironmentVariable(env, "PYTHONIOENCODING", "utf-8", true);
  SDL_SetEnvironmentVariable(env, "PYTHONUNBUFFERED", "1", true);
  args[n++] = python;
  if (launcher) args[n++] = "-3";
  args[n++] = "-u";
  args[n++] = "-m";
  args[n++] = "xpplab.console";
  args[n] = NULL;
  props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_APP);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
  SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
  proc = SDL_CreateProcessWithProperties(props);
  SDL_DestroyProperties(props);
  SDL_DestroyEnvironment(env);
  if (!proc) {
    app_log(a, "error: cannot start %s: %s", python, SDL_GetError());
    reset();
    return false;
  }
  to_py = SDL_GetProcessInput(proc);
  from_py = SDL_GetProcessOutput(proc);
  snprintf(python_used, sizeof python_used, "%s", python);
  app_log(a, "Starting Python (%s) ...", python);
  return true;
}

bool pyconsole_start(App *a) { return start(a); }

void pyconsole_stop(App *a) {
  (void)a;
  if (!proc) return;
  SDL_KillProcess(proc, true);
  SDL_DestroyProcess(proc);
  reset();
}

/* ---- Messages to Python ------------------------------------------------------------- */

static void put_json_str(char *out, size_t len, const char *s) {
  size_t k = 0;
  if (len < 3) return;
  out[k++] = '"';
  for (; *s && k + 8 < len; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') {
      out[k++] = '\\';
      out[k++] = (char)c;
    } else if (c == '\n') {
      out[k++] = '\\';
      out[k++] = 'n';
    } else if (c == '\t') {
      out[k++] = '\\';
      out[k++] = 't';
    } else if (c < 0x20) {
      k += (size_t)snprintf(out + k, len - k, "\\u%04x", c);
    } else {
      out[k++] = (char)c;
    }
  }
  out[k++] = '"';
  out[k] = '\0';
}

static bool send_request(const char *op, const char *key, const char *text, int *id) {
  static char msg[40000];
  char *body = msg + 128;
  int n;
  if (!to_py) return false;
  put_json_str(body, sizeof msg - 256, text);
  *id = next_id++;
  n = snprintf(msg, 128, "{\"id\": %d, \"op\": \"%s\", \"%s\": ", *id, op, key);
  memmove(msg + n, body, strlen(body) + 1);
  strcat(msg, "}\n");
  return SDL_WriteIO(to_py, msg, strlen(msg)) == strlen(msg) && SDL_FlushIO(to_py);
}

static void exec_line(App *a, const char *line) {
  int id;
  size_t n = strlen(code);
  app_log(a, "%s %s", pyconsole_prompt(), line);
  if (more && n + 1 < sizeof code)
    snprintf(code + n, sizeof code - n, "\n%s", line);
  else
    snprintf(code, sizeof code, "%s", line);
  if (send_request("exec", "code", code, &id)) busy = true;
}

bool pyconsole_send(App *a, const char *line) {
  if (!proc && !start(a)) return false;
  if (busy || !ready) {
    if (nqueue < QUEUE) snprintf(queue[nqueue++], sizeof queue[0], "%s", line);
    return true;
  }
  exec_line(a, line);
  return true;
}

void pyconsole_complete(App *a, const char *word) {
  (void)a;
  if (!proc || !ready || busy) return;
  snprintf(completion, sizeof completion, "%s", word);
  completion_ready = false;
  send_request("complete", "text", word, &complete_id);
}

bool pyconsole_completion(char *out, size_t len) {
  if (!completion_ready) return false;
  completion_ready = false;
  snprintf(out, len, "%s", completion);
  return true;
}

void pyconsole_request_vars(App *a) {
  int id;
  (void)a;
  if (proc && ready && !busy && vars_id < 0 && send_request("vars", "text", "", &id)) vars_id = id;
}

const PyVar *pyconsole_vars(int *n) {
  *n = nvars_rows;
  return vars_rows;
}

/* ---- Messages from Python ------------------------------------------------------------ */

/* A JSON string's body from p (just after its opening quote), unescaped
   into a new buffer; *after points past its closing quote. */
static char *unescape(const char *p, const char **after) {
  char *out = malloc(strlen(p) * 2 + 1);
  size_t k = 0;
  if (!out) return NULL;
  while (*p && *p != '"') {
    if (*p == '\\' && p[1]) {
      p++;
      if (*p == 'n') out[k++] = '\n';
      else if (*p == 't') out[k++] = '\t';
      else if (*p == 'r') out[k++] = '\r';
      else if (*p == 'u' && p[1] && p[2] && p[3] && p[4]) {
        char hex[5] = {p[1], p[2], p[3], p[4], 0};
        long u = strtol(hex, NULL, 16);
        if (u < 0x80) {
          out[k++] = (char)u;
        } else if (u < 0x800) { /* as UTF-8 */
          out[k++] = (char)(0xC0 | (u >> 6));
          out[k++] = (char)(0x80 | (u & 0x3F));
        } else {
          out[k++] = (char)(0xE0 | (u >> 12));
          out[k++] = (char)(0x80 | ((u >> 6) & 0x3F));
          out[k++] = (char)(0x80 | (u & 0x3F));
        }
        p += 4;
      } else out[k++] = *p;
      p++;
    } else {
      out[k++] = *p++;
    }
  }
  out[k] = '\0';
  if (after) *after = *p ? p + 1 : p;
  return out;
}

/* The string value of "key", unescaped into a new buffer (NULL if absent). */
static char *json_take(const char *obj, const char *key) {
  char pat[48];
  const char *p;
  snprintf(pat, sizeof pat, "\"%s\": \"", key);
  if (!(p = strstr(obj, pat))) return NULL;
  return unescape(p + strlen(pat), NULL);
}

/* The next string of a JSON array from *p, or NULL at the array's end. */
static char *next_string(const char **p) {
  const char *q = *p;
  while (*q && *q != '"' && *q != ']') q++;
  if (*q != '"') return NULL;
  return unescape(q + 1, p);
}

static int json_int(const char *obj, const char *key) {
  char pat[48];
  const char *p;
  snprintf(pat, sizeof pat, "\"%s\": ", key);
  return (p = strstr(obj, pat)) ? atoi(p + strlen(pat)) : -1;
}

static bool json_true(const char *obj, const char *key) {
  char pat[48];
  snprintf(pat, sizeof pat, "\"%s\": true", key);
  return strstr(obj, pat) != NULL;
}

/* Output goes to the command window a line at a time. */
static void output(App *a, const char *text) {
  const char *nl;
  size_t n;
  while ((nl = strchr(text, '\n')) != NULL) {
    app_log(a, "%s%.*s", partial, (int)(nl - text), text);
    partial[0] = '\0';
    text = nl + 1;
  }
  n = strlen(partial);
  snprintf(partial + n, sizeof partial - n, "%s", text);
}

static void flush_partial(App *a) {
  if (partial[0]) app_log(a, "%s", partial);
  partial[0] = '\0';
}

static void free_vars(void) {
  free(vars_rows);
  vars_rows = NULL;
  nvars_rows = 0;
}

static void handle(App *a, const char *line) {
  char *text;
  int id = json_int(line, "id");
  if (line[0] != '{') { /* Python wrote outside the protocol: show it */
    app_log(a, "%s", line);
    return;
  }
  if (strstr(line, "\"ready\": ")) {
    ready = json_true(line, "ready");
    if (ready) {
      char *ver = json_take(line, "python"), *note = json_take(line, "note");
      app_log(a, "Python %s is ready: xs (xpplab), np, app and m (the model here) are "
                 "defined; m.run() runs it in the app.", ver ? ver : "");
      if (note && *note) app_log(a, "note: %s", note);
      free(ver);
      free(note);
    } else {
      text = json_take(line, "error");
      app_log(a, "error: %s (Python: %s)", text ? text : "Python could not start",
              python_used);
      free(text);
    }
    return;
  }
  if ((text = json_take(line, "text")) != NULL && strstr(line, "\"stream\": ")) {
    output(a, text);
    free(text);
    return;
  }
  free(text);
  if (json_true(line, "more")) {
    more = true;
    busy = false;
    return;
  }
  if (json_true(line, "done")) {
    flush_partial(a);
    more = false;
    busy = false;
    code[0] = '\0';
    free_vars(); /* the variables may have changed: ask again */
    vars_id = -1;
    return;
  }
  if (id >= 0 && id == complete_id && strstr(line, "\"matches\": [")) {
    /* One match completes the word; several are listed, and the word is
       completed as far as they agree. */
    const char *p = strstr(line, "\"matches\": [") + 12;
    char first[256] = "", *m;
    int count = 0;
    size_t common = 0;
    char list[2048] = "";
    while ((m = next_string(&p)) != NULL) {
      if (count == 0) {
        snprintf(first, sizeof first, "%s", m);
        common = strlen(first);
      } else {
        size_t i = 0;
        while (i < common && m[i] == first[i]) i++;
        common = i;
      }
      if (strlen(list) + strlen(m) + 3 < sizeof list) {
        if (count) strcat(list, "  ");
        strcat(list, m);
      }
      count++;
      free(m);
    }
    if (count > 1) app_log(a, "%s", list);
    if (count > 0) {
      first[common] = '\0';
      snprintf(completion, sizeof completion, "%s", first);
      completion_ready = true;
    }
    complete_id = -1;
    return;
  }
  if (id >= 0 && id == vars_id && strstr(line, "\"vars\": [")) {
    const char *p = strstr(line, "\"vars\": [") + 9;
    int cap = 64;
    free_vars();
    vars_rows = calloc((size_t)cap, sizeof *vars_rows);
    /* rows of [name, type, summary] */
    while (vars_rows && (p = strchr(p, '[')) != NULL) {
      PyVar *v;
      char *s[3] = {NULL, NULL, NULL};
      int k;
      const char *q = p + 1;
      for (k = 0; k < 3 && (s[k] = next_string(&q)) != NULL; k++) {}
      if (k < 3) {
        for (k = 0; k < 3; k++) free(s[k]);
        break;
      }
      if (nvars_rows == cap) {
        PyVar *grown = realloc(vars_rows, sizeof *vars_rows * (size_t)(cap *= 2));
        if (!grown) break;
        vars_rows = grown;
      }
      v = &vars_rows[nvars_rows++];
      snprintf(v->name, sizeof v->name, "%s", s[0]);
      snprintf(v->type, sizeof v->type, "%s", s[1]);
      snprintf(v->summary, sizeof v->summary, "%s", s[2]);
      for (k = 0; k < 3; k++) free(s[k]);
      p = strchr(q, ']');
      if (!p) break;
      p++;
    }
    return;
  }
}

void pyconsole_poll(App *a) {
  char buf[8192];
  size_t n;
  int code_exit = 0;
  if (!proc) return;
  while (from_py && (n = SDL_ReadIO(from_py, buf, sizeof buf)) > 0) {
    char *nl;
    if (rlen + n + 1 > rcap) {
      size_t cap = (rlen + n + 1) * 2;
      char *p = realloc(rbuf, cap);
      if (!p) break;
      rbuf = p;
      rcap = cap;
    }
    memcpy(rbuf + rlen, buf, n);
    rlen += n;
    rbuf[rlen] = '\0';
    while ((nl = strchr(rbuf, '\n')) != NULL) {
      *nl = '\0';
      if (nl > rbuf && nl[-1] == '\r') nl[-1] = '\0';
      handle(a, rbuf);
      rlen -= (size_t)(nl + 1 - rbuf);
      memmove(rbuf, nl + 1, rlen + 1);
    }
  }
  if (SDL_WaitProcess(proc, false, &code_exit)) {
    flush_partial(a);
    app_log(a, "%sPython stopped (exit status %d); the next Python line starts it again.",
            code_exit ? "error: " : "", code_exit);
    SDL_DestroyProcess(proc);
    reset();
    return;
  }
  if (ready && !busy && nqueue > 0) {
    char line[1024];
    snprintf(line, sizeof line, "%s", queue[0]);
    memmove(queue[0], queue[1], sizeof queue[0] * (size_t)(--nqueue));
    exec_line(a, line);
  }
}

const char *pyconsole_python(void) { return python_used; }
