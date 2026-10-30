/*
 * xpplab-batch - XPP Lab without a window: XPPAUT's -silent mode, and the
 * desktop app's command language for scripts and pipelines.
 *
 *   xpplab-batch MODEL.ode                       run; write output.dat
 *   xpplab-batch MODEL.ode -outfile run.dat      the same, another file
 *   xpplab-batch MODEL.ode -c "set iapp=0.1" -c run -c "export run.csv"
 *   xpplab-batch MODEL.ode commands.txt          one command per line
 *
 * Every command of the app's command window works (run, set, range, eq,
 * auto ..., export, ...), except those that need the window: PNG figures
 * and the Kinescope. -c commands run after those of the files, in order.
 * The command window's text goes to standard output; the program stops at
 * the first error and exits with 1, unless --keep-going is given.
 *
 *   --script FILE.py   save the session as a Python script at the end
 *   --echo             also print each action as Python (">>> ...")
 *   --quiet            print only errors
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> /* UTF-8 arguments on Windows */

#include "file_utf8.h"
#include "actions.h"
#include "app.h"
#include "channel.h"
#include "export.h"
#include "pyconsole.h"
#include "pyfetch.h"

/* What the window would do. */
void ui_show_help(App *a, const char *page) {
  app_log(a, "The manual is in the desktop app (Help) and at "
             "https://cesarperdomodiaz.github.io/xpplab/%s%s",
          page && *page ? "manual/" : "", page && *page ? page : "");
}

void ui_console_prefill(App *a, const char *text) {
  app_log(a, "Type: %s...", text);
}
void ui_apply_theme(App *a) { (void)a; }
static void needs_window(App *a) { app_log(a, "error: this needs the app's window"); }
void ui_dialog_open(App *a) { needs_window(a); }
void ui_dialog_save_as(App *a) { needs_window(a); }
void ui_dialog_export_csv(App *a) { needs_window(a); }
void ui_dialog_save_script(App *a) { needs_window(a); }

bool figure_on_screen(FigureKind kind) {
  (void)kind;
  return false;
}

/* No channel for Python without a window. */
bool channel_start(App *a) {
  (void)a;
  return false;
}
void channel_stop(App *a) { (void)a; }
int channel_clients(void) { return 0; }
const char *channel_path(void) { return ""; }

/* No console either: a script runs Python itself. */
bool pyconsole_send(App *a, const char *line) {
  (void)line;
  app_log(a, "error: xpplab-batch has no Python console; run the Python with "
             "the xpplab package instead");
  return false;
}
void pyconsole_stop(App *a) { (void)a; }
bool pyconsole_start(App *a) { return pyconsole_send(a, ""); }
const char *pyconsole_prompt(void) { return "py>"; }
bool pyconsole_running(void) { return false; }
bool pyconsole_waiting(void) { return false; }
const char *pyconsole_python(void) { return ""; }
bool pyfetch_start(App *a) { return pyconsole_send(a, ""); }
bool pyfetch_pip(App *a, const char *python, const char *packages) {
  (void)python, (void)packages;
  return pyconsole_send(a, "");
}
void pyfetch_cancel(App *a) { (void)a; }
bool pyfetch_python(char *out, size_t len) {
  (void)out, (void)len;
  return false;
}

static size_t printed;
static bool quiet;

/* The command window's new lines, and whether one is an error. */
static bool flush_log(App *a) {
  bool error = false;
  const char *s, *e;
  if (a->log_len < printed) printed = 0; /* the log was trimmed */
  for (s = a->log + printed; *s; s = e + (*e == '\n')) {
    e = strchr(s, '\n');
    if (!e) e = s + strlen(s);
    if (!strncmp(s, "error", 5)) error = true;
    if (!quiet || !strncmp(s, "error", 5)) {
      fwrite(s, 1, (size_t)(e - s), error && quiet ? stderr : stdout);
      fputc('\n', error && quiet ? stderr : stdout);
    }
  }
  printed = a->log_len;
  fflush(stdout);
  return error;
}

static int usage(void) {
  fputs("xpplab-batch MODEL.ode [COMMANDS.txt]... [-c COMMAND]... [-outfile FILE]\n"
        "             [--script FILE.py] [--echo] [--quiet] [--keep-going]\n"
        "Without commands: run the model and write output.dat (XPPAUT's -silent).\n"
        "Type 'help' as a command for the list of commands.\n",
        stderr);
  return 2;
}

int main(int argc, char **argv) {
  static App app;
  static const char *cmds[512];
  const char *model = NULL, *outfile = "output.dat", *script = NULL, *base;
  int ncmds = 0, i, errors = 0;
  bool echo = false, keep_going = false;
  char line[4096];

  for (i = 1; i < argc; i++) {
    if ((!strcmp(argv[i], "-c") || !strcmp(argv[i], "--cmd")) && i + 1 < argc) {
      if (ncmds < 512) cmds[ncmds++] = argv[++i];
    } else if (!strcmp(argv[i], "-outfile") && i + 1 < argc)
      outfile = argv[++i];
    else if (!strcmp(argv[i], "--script") && i + 1 < argc)
      script = argv[++i];
    else if (!strcmp(argv[i], "--echo"))
      echo = true;
    else if (!strcmp(argv[i], "--quiet"))
      quiet = true;
    else if (!strcmp(argv[i], "--keep-going"))
      keep_going = true;
    else if (!strcmp(argv[i], "-silent"))
      ; /* XPPAUT's flag: this program is always silent */
    else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
      return usage();
    else if (argv[i][0] == '-')
      return fprintf(stderr, "xpplab-batch: unknown option %s\n", argv[i]), usage();
    else if (!model)
      model = argv[i];
    else { /* a file of commands, read now so that -c ones come after */
      FILE *f = app_fopen_utf8(argv[i], "r");
      if (!f) return fprintf(stderr, "xpplab-batch: cannot read %s\n", argv[i]), 2;
      while (fgets(line, sizeof line, f) && ncmds < 512) {
        char *s = line, *e;
        while (*s == ' ' || *s == '\t') s++;
        e = s + strlen(s);
        while (e > s && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = '\0';
        if (*s && *s != '#') cmds[ncmds++] = SDL_strdup(s);
      }
      fclose(f);
    }
  }
  if (!model) return usage();

  base = SDL_GetBasePath();
  app_init(&app, base ? base : "./", true);
  app.batch = true;
  app.live = false; /* a set does not run by itself: write run */
  app.echo.console = echo;
  if (!app_open(&app, model)) {
    flush_log(&app);
    fprintf(stderr, "xpplab-batch: cannot open %s\n", model);
    return 1;
  }
  flush_log(&app);
  if (ncmds == 0) { /* XPPAUT's -silent */
    cmds[ncmds++] = "run";
    SDL_snprintf(line, sizeof line, "export %s", outfile);
    cmds[ncmds++] = SDL_strdup(line);
  }
  for (i = 0; i < ncmds; i++) {
    size_t before;
    if (!quiet) printf(">> %s\n", cmds[i]);
    app.status[0] = '\0';
    before = app.log_len;
    app_command(&app, cmds[i]);
    /* The status bar's message, for a command that says nothing else
       (run, for one). */
    if (app.log_len == before && app.status[0] && !quiet) printf("%s\n", app.status);
    if (flush_log(&app)) {
      errors++;
      if (!keep_going) break;
    }
  }
  if (script) {
    echo_save(&app, script);
    flush_log(&app);
  }
  app_shutdown(&app);
  return errors ? 1 : 0;
}
