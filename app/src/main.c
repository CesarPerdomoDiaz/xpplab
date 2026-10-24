/*
 * XPP Studio - the desktop application.
 *
 *   xppstudio [model.ode] [--cmd "command"]... [--then "command"]...
 *             [--screenshot out.bmp] [--frames N] [--size WxH] [--reset-layout]
 *             [--record DIR [--fps N] [--script FILE] [--at FRAME "command"]...]
 *             [--log FILE]
 *
 * --cmd runs a command-window command after start-up (repeatable); --then
 * runs one after the first few frames, once the windows exist.
 * --screenshot renders N frames (default 12), saves the window to a BMP file
 * (a PNG if the name ends in .png) and exits; it is used to check the
 * interface automatically.
 * --record saves N frames (--frames) as DIR/frame_00000.png ... and exits,
 * with time running at --fps frames a second (default 20) whatever the
 * speed of the machine. --at and --script (lines "FRAME command") run
 * commands at given frames, so a recording can show the app at work; a
 * command written @command is not echoed in the command window.
 * tools/make_demo.py turns recordings into the README's animations.
 * --log writes the command window to FILE when the app exits, so that a
 * script can check what the commands did (tests/parity.py).
 */
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "export.h"
#include "gui_backend.h"
#include "ui.h"

extern const unsigned char font_roboto[], font_cousine[];
extern const unsigned int font_roboto_size, font_cousine_size;

static ImGuiStyle base_style;

static ImFont *add_font(const unsigned char *data, unsigned int size) {
  ImGuiIO *io = igGetIO_Nil();
  ImFontConfig *cfg = ImFontConfig_ImFontConfig();
  ImFont *f;
  cfg->FontDataOwnedByAtlas = false; /* the data is a static array */
  cfg->OversampleH = 2;
  f = ImFontAtlas_AddFontFromMemoryTTF(io->Fonts, (void *)data, (int)size, 15.0f,
                                       cfg, NULL);
  ImFontConfig_destroy(cfg);
  return f;
}

/* The scale for the interface's sizes: the user's display scaling (125%,
   150%, ...) without the pixel density. On a Retina Mac the window display
   scale is 2 because each point is 2x2 pixels; ImGui's backend already
   draws at that density (DisplayFramebufferScale), so scaling the sizes by
   it as well would make everything twice as large. On Windows the pixel
   density is 1 and this is the display scale. */
static float interface_scale(SDL_Window *window) {
  float scale = SDL_GetWindowDisplayScale(window);
  float density = SDL_GetWindowPixelDensity(window);
  if (scale <= 0) scale = 1;
  if (density <= 0) density = 1;
  return scale / density;
}

static void apply_scale(App *a, float dpi) {
  ImGuiStyle *s = igGetStyle();
  *s = base_style;
  ui_apply_theme(a);
  ImGuiStyle_ScaleAllSizes(s, dpi * a->ui_scale);
  s->FontSizeBase = 15.0f;
  s->FontScaleDpi = dpi * a->ui_scale;
}

/* Report a start-up failure. A Windows GUI program has no console, so in
   scripted runs (--screenshot, --record) the message also goes to <file>.err and the
   program exits with status 1. */
static const char *err_file;
static int failed; /* a scripted run reported an error: exit non-zero */

static void fail(const char *what) {
  fprintf(stderr, "%s: %s\n", what, SDL_GetError());
  if (err_file) {
    char path[1100];
    failed = 1;
    FILE *f;
    snprintf(path, sizeof path, "%s.err", err_file);
    f = fopen(path, "w");
    if (f) {
      fprintf(f, "%s: %s\n", what, SDL_GetError());
      fclose(f);
    }
  }
}

static int file_exists(const char *path) {
  FILE *f = fopen(path, "rb");
  if (f) fclose(f);
  return f != NULL;
}

/* The app changes into the folder of each model it opens, so a relative
   path given on the command line is made absolute first. */
static const char *absolute(const char *path, char *buf, size_t size) {
  char *cwd;
  if (!path || path[0] == '/' || path[0] == '\\' || (path[0] && path[1] == ':'))
    return path;
  cwd = SDL_GetCurrentDirectory(); /* ends with a separator */
  if (!cwd) return path;
  snprintf(buf, size, "%s%s", cwd, path);
  SDL_free(cwd);
  return buf;
}

/* Commands to run at given frames of a recording (--at, --script). */
typedef struct {
  int frame;
  char *cmd;
} Timed;

static Timed *timed;
static int ntimed, cap_timed;

static void add_timed(int frame, const char *cmd) {
  if (ntimed == cap_timed) {
    cap_timed = cap_timed ? 2 * cap_timed : 64;
    timed = SDL_realloc(timed, (size_t)cap_timed * sizeof *timed);
  }
  timed[ntimed].frame = frame;
  timed[ntimed++].cmd = SDL_strdup(cmd);
}

/* A script: one "FRAME command" per line; # starts a comment line. */
static bool read_script(const char *path) {
  char line[1200];
  FILE *f = fopen(path, "r");
  if (!f) return false;
  while (fgets(line, sizeof line, f)) {
    char *p = line, *end;
    long n;
    line[strcspn(line, "\r\n")] = '\0';
    while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '#') continue;
    n = strtol(p, &end, 10);
    if (end == p) continue;
    while (*end == ' ' || *end == '\t') end++;
    if (*end) add_timed((int)n, end);
  }
  fclose(f);
  return true;
}

int main(int argc, char **argv) {
  static App app;
  static char ini_path[1100], shot_path[1100], rec_path[1100], script_path[1100],
      log_path[1100];
  SDL_Window *window;
  SDL_Renderer *renderer;
  const char *base;
  const char *model = NULL, *shot = NULL, *record = NULL, *script = NULL, *logfile = NULL;
  const char *cmds[32], *later[32];
  int ncmds = 0, nlater = 0, frames = 12, width = 1500, height = 920, frame = 0, i;
  int fps = 20, saved = 0;
  bool reset_layout = false, applied_dark, shot_taken = false, scripted;
  const char *quit_reason = NULL;
  float applied_scale, dpi;
  char *pref;
  /* A recording starts once the windows have been laid out. */
  const int warmup = 6;

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--cmd") && i + 1 < argc && ncmds < 32)
      cmds[ncmds++] = argv[++i];
    else if (!strcmp(argv[i], "--then") && i + 1 < argc && nlater < 32)
      later[nlater++] = argv[++i];
    else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc)
      shot = argv[++i];
    else if (!strcmp(argv[i], "--record") && i + 1 < argc)
      record = argv[++i];
    else if (!strcmp(argv[i], "--fps") && i + 1 < argc)
      fps = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--script") && i + 1 < argc)
      script = argv[++i];
    else if (!strcmp(argv[i], "--log") && i + 1 < argc)
      logfile = argv[++i];
    else if (!strcmp(argv[i], "--at") && i + 2 < argc) {
      int n = atoi(argv[++i]);
      add_timed(n, argv[++i]);
    } else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
      frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--size") && i + 1 < argc)
      sscanf(argv[++i], "%dx%d", &width, &height);
    else if (!strcmp(argv[i], "--reset-layout"))
      reset_layout = true;
    else if (argv[i][0] != '-')
      model = argv[i];
  }
  if (fps < 1) fps = 20;

  shot = absolute(shot, shot_path, sizeof shot_path);
  record = absolute(record, rec_path, sizeof rec_path);
  logfile = absolute(logfile, log_path, sizeof log_path);
  scripted = shot || record;
  err_file = shot ? shot : record;
  if (script && !read_script(absolute(script, script_path, sizeof script_path))) {
    SDL_SetError("cannot read the script %s", script);
    fail("--script");
    return 1;
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fail("SDL_Init");
    return 1;
  }
  window = SDL_CreateWindow("XPP Studio", width, height,
                            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                (scripted ? 0 : SDL_WINDOW_MAXIMIZED));
  if (!window) {
    fail("SDL_CreateWindow");
    return 1;
  }
  {
    /* The window icon, embedded as 64x64 RGBA (tools/make_icon.py). */
    extern const unsigned char icon_rgba[];
    SDL_Surface *icon = SDL_CreateSurfaceFrom(64, 64, SDL_PIXELFORMAT_RGBA32,
                                              (void *)icon_rgba, 64 * 4);
    if (icon) {
      SDL_SetWindowIcon(window, icon);
      SDL_DestroySurface(icon);
    }
  }
  renderer = SDL_CreateRenderer(window, NULL);
  if (!renderer) /* no usable GPU: virtual machines, remote desktops */
    renderer = SDL_CreateRenderer(window, "software");
  if (!renderer) {
    fail("SDL_CreateRenderer");
    return 1;
  }
  SDL_SetRenderVSync(renderer, 1);

  igCreateContext(NULL);
  ImPlot_CreateContext();
  {
    ImGuiIO *io = igGetIO_Nil();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard |
                       ImGuiConfigFlags_DockingEnable;
    pref = SDL_GetPrefPath("XPPStudio", "XPPStudio");
    snprintf(ini_path, sizeof ini_path, "%slayout.ini", pref ? pref : "");
    SDL_free(pref);
    io->IniFilename = scripted ? NULL : ini_path;
    if (scripted || !file_exists(ini_path)) reset_layout = true;
    g_fonts.ui = add_font(font_roboto, font_roboto_size);
    g_fonts.mono = add_font(font_cousine, font_cousine_size);
    io->FontDefault = g_fonts.ui;
  }
  base_style = *igGetStyle();
  gui_backend_init(window, renderer);

  base = SDL_GetBasePath();
  app_init(&app, base ? base : "./", scripted);
  app.window = window;
  app.reset_layout = reset_layout;
  dpi = interface_scale(window);
  apply_scale(&app, dpi);
  {
    /* For bug reports from a Terminal, and in CI's log. */
    int w, h, pw, ph;
    SDL_GetWindowSize(window, &w, &h);
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    fprintf(stderr, "XPP Studio: window %dx%d (%dx%d pixels), display scale %.2f, "
            "pixel density %.2f, interface scale %.2f, renderer %s\n", w, h, pw, ph,
            (double)SDL_GetWindowDisplayScale(window),
            (double)SDL_GetWindowPixelDensity(window), (double)dpi,
            SDL_GetRendererName(renderer));
  }
  applied_dark = app.dark;
  applied_scale = app.ui_scale;

  if (model) {
    app_open(&app, model);
  } else if (app.nrecent == 0) {
    /* First start: open a classic example so there is something to see. */
    char path[1100];
    snprintf(path, sizeof path, "%s/lecar.ode", app.examples_dir);
    if (file_exists(path)) app_open(&app, path);
  }
  for (i = 0; i < ncmds; i++) {
    app_log(&app, ">> %s", cmds[i]);
    app_command(&app, cmds[i]);
  }

  while (!app.quit) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      /* A scripted run shows what the script does, not where the mouse
         happens to be. */
      bool input = (ev.type >= SDL_EVENT_KEY_DOWN && ev.type < SDL_EVENT_JOYSTICK_AXIS_MOTION);
      if (!(scripted && input)) gui_backend_process_event(&ev);
      if (ev.type == SDL_EVENT_QUIT) {
        app.quit = true;
        quit_reason = "the system asked the app to quit";
      }
      if (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
          ev.window.windowID == SDL_GetWindowID(window)) {
        app.quit = true;
        quit_reason = "the window was closed";
      }
      if (ev.type == SDL_EVENT_DROP_FILE && ev.drop.data) app_open(&app, ev.drop.data);
      if (ev.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        dpi = interface_scale(window);
        apply_scale(&app, dpi);
      }
    }
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
      SDL_Delay(20);
      continue;
    }
    if (app.dark != applied_dark || app.ui_scale != applied_scale) {
      apply_scale(&app, dpi);
      applied_dark = app.dark;
      applied_scale = app.ui_scale;
    }

    /* --then commands run once the windows have been drawn, so that, for
       example, figures exist and can be exported. */
    if (frame == 4)
      for (i = 0; i < nlater; i++) {
        app_log(&app, ">> %s", later[i]);
        app_command(&app, later[i]);
      }
    if (record && frame >= warmup)
      for (i = 0; i < ntimed; i++)
        if (timed[i].frame == frame - warmup) {
          const char *c = timed[i].cmd;
          if (c[0] == '@') /* quietly: not echoed in the command window */
            c++;
          else
            app_log(&app, ">> %s", c);
          app_command(&app, c);
        }
    if (!shot) frame++;

    gui_backend_new_frame();
    /* Recorded time runs at a steady rate, however long a frame takes to
       save: animations (the Animator, a spinning 3D view) stay smooth. */
    if (record) igGetIO_Nil()->DeltaTime = 1.0f / (float)fps;
    if (scripted) ImGuiIO_AddMousePosEvent(igGetIO_Nil(), -FLT_MAX, -FLT_MAX);
    igNewFrame();
    ui_draw(&app);
    if (app.quit && !quit_reason) quit_reason = "a quit command or shortcut";
    igRender();
    SDL_SetRenderDrawColor(renderer, 30, 31, 34, 255);
    SDL_RenderClear(renderer);
    gui_backend_render(renderer);
    export_process_pending(&app, renderer, igGetIO_Nil()->DisplayFramebufferScale.x);

    if (shot && ++frame >= frames) {
      SDL_Surface *s = SDL_RenderReadPixels(renderer, NULL);
      size_t len = strlen(shot);
      bool png = len > 4 && SDL_strcasecmp(shot + len - 4, ".png") == 0;
      if (!s || !(png ? export_surface_png(s, shot) : SDL_SaveBMP(s, shot)))
        fail("screenshot");
      SDL_DestroySurface(s);
      app.quit = true;
      shot_taken = true;
    }
    if (record && frame > warmup) {
      char path[1200];
      SDL_Surface *s = SDL_RenderReadPixels(renderer, NULL);
      snprintf(path, sizeof path, "%s/frame_%05d.png", record, saved++);
      if (!s || !export_surface_png(s, path)) {
        fail("record");
        app.quit = true;
      }
      SDL_DestroySurface(s);
      if (saved >= frames) {
        app.quit = true;
        shot_taken = true;
      }
    }
    SDL_RenderPresent(renderer);
  }
  if ((shot || record) && !shot_taken) {
    char why[256];
    snprintf(why, sizeof why, "stopped at frame %d of %d before the "
             "screenshot: %s", frame, frames, quit_reason ? quit_reason : "?");
    SDL_SetError("%s", why);
    fail("quit");
  }
  if (logfile) {
    FILE *f = fopen(logfile, "w");
    if (f) {
      if (app.log) fwrite(app.log, 1, app.log_len, f);
      fclose(f);
    }
  }

  app_shutdown(&app);
  gui_backend_shutdown();
  ImPlot_DestroyContext(NULL);
  igDestroyContext(NULL);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return failed ? 1 : 0;
}
