/* Figure export: PNG (exactly what is on screen) and SVG (a vector figure
   drawn from the data, for papers). */
#ifndef XPPS_EXPORT_H
#define XPPS_EXPORT_H

#include <stdbool.h>

#include "app.h"

typedef enum { FIG_TIME, FIG_PHASE, FIG_AUTO, FIG_AUTO2, FIG_COUNT } FigureKind;

/* The screen rectangle and axis limits of each figure, updated as the
   plots are drawn. */
typedef struct {
  float x0, y0, x1, y1;      /* in ImGui (window) coordinates */
  double xmin, xmax, ymin, ymax;
  bool valid;
  int frame; /* the ImGui frame it was last drawn in */
} FigureState;

extern FigureState g_figures[FIG_COUNT];

/* Whether the figure was drawn in this frame: on the screen now, not in a
   tab behind another (ui_common.c). */
bool figure_on_screen(FigureKind kind);

const char *figure_name(FigureKind kind);

/* Write a PNG of the figure at the end of the current frame. */
void export_png(FigureKind kind, const char *path);
/* Hand a picture of the figure to the Kinescope at the end of the frame. */
void export_capture(FigureKind kind);
bool export_capture_waiting(void);
/* Called by the main loop after rendering, before presenting. */
void export_process_pending(App *a, void *sdl_renderer, float fb_scale);
/* Write an SDL_Surface as a PNG file. */
bool export_surface_png(void *sdl_surface, const char *path);

bool export_svg(App *a, FigureKind kind, const char *path);

#endif
