/*
 * XPP Studio - the desktop application.
 *
 *   xppstudio [model.ode] [--cmd "command"]... [--then "command"]...
 *             [--screenshot out.bmp] [--frames N] [--size WxH] [--reset-layout]
 *
 * --cmd runs a command-window command after start-up (repeatable); --then
 * runs one after the first few frames, once the windows exist.
 * --screenshot renders N frames (default 12), saves the window to a BMP
 * file and exits; it is used to check the interface automatically.
 */
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

static void apply_scale(App *a, float dpi) {
  ImGuiStyle *s = igGetStyle();
  *s = base_style;
  ui_apply_theme(a);
  ImGuiStyle_ScaleAllSizes(s, dpi * a->ui_scale);
  s->FontSizeBase = 15.0f;
  s->FontScaleDpi = dpi * a->ui_scale;
}

static int file_exists(const char *path) {
  FILE *f = fopen(path, "rb");
  if (f) fclose(f);
  return f != NULL;
}

int main(int argc, char **argv) {
  static App app;
  static char ini_path[1100];
  SDL_Window *window;
  SDL_Renderer *renderer;
  const char *base;
  const char *model = NULL, *shot = NULL;
  const char *cmds[32], *later[32];
  int ncmds = 0, nlater = 0, frames = 12, width = 1500, height = 920, frame = 0, i;
  bool reset_layout = false, applied_dark;
  float applied_scale, dpi;
  char *pref;

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--cmd") && i + 1 < argc && ncmds < 32)
      cmds[ncmds++] = argv[++i];
    else if (!strcmp(argv[i], "--then") && i + 1 < argc && nlater < 32)
      later[nlater++] = argv[++i];
    else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc)
      shot = argv[++i];
    else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
      frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--size") && i + 1 < argc)
      sscanf(argv[++i], "%dx%d", &width, &height);
    else if (!strcmp(argv[i], "--reset-layout"))
      reset_layout = true;
    else if (argv[i][0] != '-')
      model = argv[i];
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  window = SDL_CreateWindow("XPP Studio", width, height,
                            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                (shot ? 0 : SDL_WINDOW_MAXIMIZED));
  if (!window) {
    fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
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
  if (!renderer) {
    fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
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
    io->IniFilename = shot ? NULL : ini_path;
    if (shot || !file_exists(ini_path)) reset_layout = true;
    g_fonts.ui = add_font(font_roboto, font_roboto_size);
    g_fonts.mono = add_font(font_cousine, font_cousine_size);
    io->FontDefault = g_fonts.ui;
  }
  base_style = *igGetStyle();
  gui_backend_init(window, renderer);

  base = SDL_GetBasePath();
  app_init(&app, base ? base : "./", shot != NULL);
  app.window = window;
  app.reset_layout = reset_layout;
  dpi = SDL_GetWindowDisplayScale(window);
  if (dpi <= 0) dpi = 1;
  apply_scale(&app, dpi);
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
      gui_backend_process_event(&ev);
      if (ev.type == SDL_EVENT_QUIT) app.quit = true;
      if (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
          ev.window.windowID == SDL_GetWindowID(window))
        app.quit = true;
      if (ev.type == SDL_EVENT_DROP_FILE && ev.drop.data) app_open(&app, ev.drop.data);
      if (ev.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        dpi = SDL_GetWindowDisplayScale(window);
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
    if (!shot) frame++;

    gui_backend_new_frame();
    igNewFrame();
    ui_draw(&app);
    igRender();
    SDL_SetRenderDrawColor(renderer, 30, 31, 34, 255);
    SDL_RenderClear(renderer);
    gui_backend_render(renderer);
    export_process_pending(&app, renderer, igGetIO_Nil()->DisplayFramebufferScale.x);

    if (shot && ++frame >= frames) {
      SDL_Surface *s = SDL_RenderReadPixels(renderer, NULL);
      if (!s || !SDL_SaveBMP(s, shot))
        fprintf(stderr, "screenshot failed: %s\n", SDL_GetError());
      SDL_DestroySurface(s);
      app.quit = true;
    }
    SDL_RenderPresent(renderer);
  }

  app_shutdown(&app);
  gui_backend_shutdown();
  ImPlot_DestroyContext(NULL);
  igDestroyContext(NULL);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
