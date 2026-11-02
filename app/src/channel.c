/* The local channel: Python (a notebook, a script, a coding agent) drives
   the running app through it. The app listens on 127.0.0.1 only, on a port
   the system chooses, and writes the port and a random token to a file only
   its user can read (~/.xpplab/channel.json, or $XPPLAB_CHANNEL). A client
   sends one JSON object per line with the token and an operation; the app
   answers each with one line. Every action goes through app_command(), as
   if typed in the command window, so the plots, the Workspace and the
   session's Python follow. xpplab.app.connect() is the client. */
#ifdef _WIN32
#define _CRT_RAND_S /* rand_s(), before stdlib.h */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
typedef SOCKET sock_t;
#define BAD_SOCK INVALID_SOCKET
#define close_sock closesocket
#define WOULD_BLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
typedef int sock_t;
#define BAD_SOCK (-1)
#define close_sock close
#define WOULD_BLOCK (errno == EAGAIN || errno == EWOULDBLOCK)
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0 /* macOS: SO_NOSIGPIPE on the socket instead */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "file_utf8.h"
#include "channel.h"

#define MAX_CLIENTS 4

typedef struct {
  sock_t s;
  char *in;   /* what has come in, up to a new line */
  size_t nin, capin;
  char *out;  /* what is still to go out */
  size_t nout, capout, sent;
} Client;

static sock_t listener = BAD_SOCK;
static Client clients[MAX_CLIENTS];
static char token[33];
static char file_path[1100];
static int port;

static void set_nonblocking(sock_t s) {
#ifdef _WIN32
  u_long on = 1;
  ioctlsocket(s, FIONBIO, &on);
#else
  fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
}

static bool random_token(void) {
  unsigned char b[16];
  int i;
#ifdef _WIN32
  for (i = 0; i < 16; i += 4) {
    unsigned int v;
    if (rand_s(&v) != 0) return false;
    memcpy(b + i, &v, 4);
  }
#else
  FILE *f = fopen("/dev/urandom", "rb");
  if (!f || fread(b, 1, 16, f) != 16) {
    if (f) fclose(f);
    return false;
  }
  fclose(f);
#endif
  for (i = 0; i < 16; i++) snprintf(token + 2 * i, 3, "%02x", b[i]);
  return true;
}

/* ~/.xpplab/channel.json, or the file $XPPLAB_CHANNEL names. */
static bool channel_file(char *out, size_t len) {
  const char *env = SDL_getenv("XPPLAB_CHANNEL");
  char dir[1024];
#ifdef _WIN32
  const char *home = SDL_getenv("USERPROFILE");
#else
  const char *home = SDL_getenv("HOME");
#endif
  if (env && *env) {
    snprintf(out, len, "%s", env);
    return true;
  }
  if (!home || !*home) return false;
  snprintf(dir, sizeof dir, "%s/.xpplab", home);
#ifdef _WIN32
  SDL_CreateDirectory(dir); /* by its UTF-8 name */
#else
  mkdir(dir, 0700);
#endif
  snprintf(out, len, "%s/channel.json", dir);
  return true;
}

static bool write_file(const char *path) {
  FILE *f;
#ifdef _WIN32
  f = app_fopen_utf8(path, "w"); /* the user's profile: only the user can read it */
#else
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  f = fd >= 0 ? fdopen(fd, "w") : NULL;
#endif
  if (!f) return false;
  fprintf(f, "{\"port\": %d, \"token\": \"%s\", \"pid\": %lu}\n", port, token,
#ifdef _WIN32
          (unsigned long)GetCurrentProcessId()
#else
          (unsigned long)getpid()
#endif
  );
  fclose(f);
  return true;
}

bool channel_start(App *a) {
  struct sockaddr_in addr;
  socklen_t alen = sizeof addr;
#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
#endif
  if (listener != BAD_SOCK) return true;
  if (!random_token() || !channel_file(file_path, sizeof file_path)) return false;
  listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener == BAD_SOCK) return false;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* this machine only */
  addr.sin_port = 0;                             /* any free port */
  if (bind(listener, (struct sockaddr *)&addr, sizeof addr) != 0 ||
      listen(listener, MAX_CLIENTS) != 0 ||
      getsockname(listener, (struct sockaddr *)&addr, &alen) != 0) {
    close_sock(listener);
    listener = BAD_SOCK;
    return false;
  }
  port = ntohs(addr.sin_port);
  set_nonblocking(listener);
  if (!write_file(file_path)) {
    close_sock(listener);
    listener = BAD_SOCK;
    return false;
  }
  memset(clients, 0, sizeof clients);
  {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) clients[i].s = BAD_SOCK;
  }
  (void)a;
  return true;
}

static void drop(Client *c) {
  if (c->s != BAD_SOCK) close_sock(c->s);
  free(c->in);
  free(c->out);
  memset(c, 0, sizeof *c);
  c->s = BAD_SOCK;
}

void channel_stop(App *a) {
  char text[256] = "";
  FILE *f;
  int i;
  (void)a;
  if (listener == BAD_SOCK) return;
  for (i = 0; i < MAX_CLIENTS; i++) drop(&clients[i]);
  close_sock(listener);
  listener = BAD_SOCK;
  /* Remove the file unless another app has written its own since. */
  if ((f = app_fopen_utf8(file_path, "r")) != NULL) {
    size_t n = fread(text, 1, sizeof text - 1, f);
    text[n] = 0;
    fclose(f);
    if (strstr(text, token)) SDL_RemovePath(file_path);
  }
}

const char *channel_path(void) { return file_path; }

int channel_clients(void) {
  int i, n = 0;
  for (i = 0; i < MAX_CLIENTS; i++) n += clients[i].s != BAD_SOCK;
  return n;
}

/* ---- Replies ------------------------------------------------------------------ */

static void put(Client *c, const char *s, size_t n) {
  if (c->nout + n + 1 > c->capout) {
    size_t cap = (c->nout + n + 1) * 2;
    char *p = realloc(c->out, cap);
    if (!p) return;
    c->out = p;
    c->capout = cap;
  }
  memcpy(c->out + c->nout, s, n);
  c->nout += n;
}

static void puts_(Client *c, const char *s) { put(c, s, strlen(s)); }

static void put_str(Client *c, const char *s, size_t n) {
  size_t i;
  char u[8];
  put(c, "\"", 1);
  for (i = 0; i < n; i++) {
    unsigned char ch = (unsigned char)s[i];
    if (ch == '"' || ch == '\\') {
      put(c, "\\", 1);
      put(c, (const char *)&s[i], 1);
    } else if (ch == '\n') {
      put(c, "\\n", 2);
    } else if (ch < 0x20) {
      snprintf(u, sizeof u, "\\u%04x", ch);
      put(c, u, 6);
    } else {
      put(c, (const char *)&s[i], 1);
    }
  }
  put(c, "\"", 1);
}

/* Numbers exactly, as Python's json reads them (NaN and Infinity too). */
static void put_num(Client *c, double v) {
  char b[64];
  if (v != v)
    puts_(c, "NaN");
  else if (v > 1e308 * 10)
    puts_(c, "Infinity");
  else if (v < -1e308 * 10)
    puts_(c, "-Infinity");
  else {
    app_format_double(b, sizeof b, v);
    puts_(c, b);
  }
}

static void put_names(Client *c, char (*names)[APP_NAME_LEN], int n) {
  int i;
  put(c, "[", 1);
  for (i = 0; i < n; i++) {
    if (i) put(c, ", ", 2);
    put_str(c, names[i], strlen(names[i]));
  }
  put(c, "]", 1);
}

/* The value of "key" in a one-line JSON object, when it is a string. */
static bool json_str(const char *obj, const char *key, char *out, size_t len) {
  char pat[64];
  const char *p;
  size_t k = 0;
  snprintf(pat, sizeof pat, "\"%s\"", key);
  if (!(p = strstr(obj, pat))) return false;
  p += strlen(pat);
  while (*p == ' ' || *p == '\t') p++;
  if (*p++ != ':') return false;
  while (*p == ' ' || *p == '\t') p++;
  if (*p++ != '"') return false;
  while (*p && *p != '"' && k + 1 < len) {
    if (*p == '\\' && p[1]) {
      p++;
      if (*p == 'n') out[k++] = '\n';
      else if (*p == 't') out[k++] = '\t';
      else if (*p == 'u' && p[1] && p[2] && p[3] && p[4]) {
        char hex[5] = {p[1], p[2], p[3], p[4], 0};
        long u = strtol(hex, NULL, 16);
        out[k++] = u < 128 ? (char)u : '?';
        p += 4;
      } else out[k++] = *p;
      p++;
    } else {
      out[k++] = *p++;
    }
  }
  out[k] = '\0';
  return *p == '"';
}

static bool has_error(const char *s, size_t n) {
  size_t i;
  for (i = 0; i < n; i++)
    if ((i == 0 || s[i - 1] == '\n') && n - i >= 5 && !strncmp(s + i, "error", 5)) return true;
  return false;
}

/* One request; the reply ends with a new line. */
static void serve(App *a, Client *c, const char *line) {
  char op[32], arg[1100], given[64];
  int i, j;
  if (!json_str(line, "token", given, sizeof given) || strcmp(given, token) != 0) {
    puts_(c, "{\"ok\": false, \"error\": \"wrong token\"}\n");
    c->sent = (size_t)-1; /* hang up once it is sent */
    return;
  }
  if (!json_str(line, "op", op, sizeof op)) op[0] = '\0';
  arg[0] = '\0';
  json_str(line, "text", arg, sizeof arg);
  if (!strcmp(op, "hello")) {
    puts_(c, "{\"ok\": true, \"app\": \"XPP Lab\", \"version\": ");
    put_str(c, XPPS_VERSION, strlen(XPPS_VERSION));
    puts_(c, ", \"model\": ");
    put_str(c, a->model_ok ? a->model_path : "", a->model_ok ? strlen(a->model_path) : 0);
    puts_(c, "}\n");
    app_status(a, "A Python client is connected");
  } else if (!strcmp(op, "command")) {
    size_t log0 = a->log_len, echo0 = a->echo.len;
    char quiet[4] = "";
    json_str(line, "quiet", quiet, sizeof quiet);
    if (strcmp(quiet, "1") != 0) app_log(a, ">> %s", arg); /* not for the console */
    app_command(a, arg);
    puts_(c, "{\"ok\": ");
    puts_(c, has_error(a->log + log0, a->log_len - log0) ? "false" : "true");
    puts_(c, ", \"log\": ");
    put_str(c, a->log + log0, a->log_len >= log0 ? a->log_len - log0 : 0);
    puts_(c, ", \"python\": ");
    put_str(c, a->echo.text ? a->echo.text + echo0 : "",
            a->echo.text && a->echo.len >= echo0 ? a->echo.len - echo0 : 0);
    puts_(c, "}\n");
  } else if (!strcmp(op, "get")) {
    double v = 0;
    if (a->model_ok && a->eng->api.xppe_get(arg, &v) == XPPE_OK) {
      puts_(c, "{\"ok\": true, \"value\": ");
      put_num(c, v);
      puts_(c, "}\n");
    } else {
      puts_(c, "{\"ok\": false, \"error\": ");
      put_str(c, "no value of that name", 21);
      puts_(c, "}\n");
    }
  } else if (!strcmp(op, "names") || !strcmp(op, "values")) {
    puts_(c, "{\"ok\": true, \"variables\": ");
    put_names(c, a->vars, a->nvars);
    puts_(c, ", \"parameters\": ");
    put_names(c, a->pars, a->npars);
    puts_(c, ", \"columns\": ");
    put_names(c, a->cols, a->ncols);
    puts_(c, ", \"initial_values\": [");
    for (i = 0; i < a->nvars; i++) {
      if (i) put(c, ", ", 2);
      put_num(c, a->ic[i]);
    }
    puts_(c, "], \"parameter_values\": [");
    for (i = 0; i < a->npars; i++) {
      if (i) put(c, ", ", 2);
      put_num(c, a->par[i].value);
    }
    puts_(c, "]}\n");
  } else if (!strcmp(op, "run")) {
    const Trajectory *t = &a->traj;
    if (!a->have_traj) {
      puts_(c, "{\"ok\": false, \"error\": \"there is no run yet\"}\n");
      return;
    }
    puts_(c, "{\"ok\": true, \"columns\": [");
    for (j = 0; j < t->ncols; j++) {
      if (j) put(c, ", ", 2);
      put_str(c, t->names[j], strlen(t->names[j]));
    }
    puts_(c, "], \"data\": [");
    for (j = 0; j < t->ncols; j++) {
      put(c, j ? ", [" : "[", j ? 3 : 1);
      for (i = 0; i < t->nrows; i++) {
        if (i) put(c, ",", 1);
        put_num(c, t->cols[j][i]);
      }
      put(c, "]", 1);
    }
    puts_(c, "]}\n");
  } else {
    puts_(c, "{\"ok\": false, \"error\": \"unknown operation\"}\n");
  }
}

void channel_poll(App *a) {
  int i;
  if (listener == BAD_SOCK) return;
  for (;;) { /* new clients */
    sock_t s = accept(listener, NULL, NULL);
    if (s == BAD_SOCK) break;
    for (i = 0; i < MAX_CLIENTS && clients[i].s != BAD_SOCK; i++) {}
    if (i == MAX_CLIENTS) {
      close_sock(s);
      continue;
    }
    set_nonblocking(s);
#ifdef SO_NOSIGPIPE
    {
      int one = 1;
      setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
    }
#endif
    clients[i].s = s;
  }
  for (i = 0; i < MAX_CLIENTS; i++) {
    Client *c = &clients[i];
    char buf[4096], *nl;
    int n;
    if (c->s == BAD_SOCK) continue;
    while ((n = (int)recv(c->s, buf, sizeof buf, 0)) > 0) {
      if (c->nin + (size_t)n + 1 > c->capin) {
        size_t cap = (c->nin + (size_t)n + 1) * 2;
        char *p = realloc(c->in, cap);
        if (!p) break;
        c->in = p;
        c->capin = cap;
      }
      memcpy(c->in + c->nin, buf, (size_t)n);
      c->nin += (size_t)n;
      c->in[c->nin] = '\0';
    }
    if (n == 0 || (n < 0 && !WOULD_BLOCK)) {
      drop(c);
      continue;
    }
    /* Each whole line is a request; one a frame, so the app draws between. */
    if (c->in && (nl = memchr(c->in, '\n', c->nin)) != NULL && c->sent != (size_t)-1) {
      size_t used = (size_t)(nl - c->in) + 1;
      *nl = '\0';
      serve(a, c, c->in);
      memmove(c->in, c->in + used, c->nin - used);
      c->nin -= used;
      if (c->in) c->in[c->nin] = '\0';
    }
    if (c->nout > 0) {
      bool hang_up = c->sent == (size_t)-1;
      size_t from = hang_up ? 0 : c->sent;
      n = (int)send(c->s, c->out + from, (int)(c->nout - from), MSG_NOSIGNAL);
      if (n > 0) from += (size_t)n;
      if (from >= c->nout) {
        c->nout = 0;
        from = 0;
        if (hang_up) {
          drop(c);
          continue;
        }
      }
      if (!hang_up) c->sent = from;
    }
  }
}
