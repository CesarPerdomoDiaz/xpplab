/* The Kinescope window: capture a figure, step through the frames or play
   them, save them as an animated GIF or as PNG files. */
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "export.h"
#include "ui.h"

/* A texture for the frame shown, made again when the frame changes. */
static ImTextureRef_c frame_texture(App *a) {
  Kinescope *k = &a->ks;
  ImTextureRef_c ref;
  memset(&ref, 0, sizeof ref);
  if (k->tex && k->tex_frame != k->cur) {
    SDL_DestroyTexture((SDL_Texture *)k->tex);
    k->tex = NULL;
  }
  if (!k->tex && a->renderer && k->cur < k->n) {
    k->tex = SDL_CreateTextureFromSurface((SDL_Renderer *)a->renderer,
                                          (SDL_Surface *)k->frame[k->cur]);
    k->tex_frame = k->cur;
  }
  ref._TexID = (ImTextureID)(intptr_t)k->tex;
  return ref;
}

void ui_kinescope(App *a) {
  static const char *figures[] = {WIN_TIME, WIN_PHASE, WIN_AUTO, "Two-parameter diagram"};
  Kinescope *k = &a->ks;
  ImVec2_c avail;
  char cmd[64];
  if (!igBegin(WIN_KINE, &a->show_kine, 0)) {
    igEnd();
    return;
  }
  igSetNextItemWidth(170);
  igCombo_Str_arr("##source", &k->source, figures, FIG_COUNT, -1);
  Tooltip("The figure Capture takes, exactly as it is shown.");
  igSameLine(0, -1);
  if (Button("Capture")) {
    snprintf(cmd, sizeof cmd, "kinescope capture %s",
             (const char *[]){"time", "phase", "auto", "auto2"}[k->source]);
    app_command(a, cmd);
  }
  Tooltip("Take a picture of the figure (XPPAUT's Kinescope > Capture, K C). "
          "Change a parameter, run, capture again: the frames make a film.");
  igSameLine(0, -1);
  if (Button("Delete all") && k->n > 0) app_command(a, "kinescope clear");
  if (k->film.on) {
    igSameLine(0, 14);
    igTextColored(HEX(0xc96f00), "Filming run %d of %d", k->film.k + 1, k->film.n);
  }
  if (k->n == 0) {
    TextWrapped("No frames yet. Capture a figure as it changes, or film the Range: "
                "Workspace > Range > Film, which runs each value and captures the "
                "figure after each run (XPPAUT's Range with Movie).");
    igEnd();
    return;
  }
  if (k->cur >= k->n) k->cur = k->n - 1;
  if (Button("|<")) k->cur = 0;
  igSameLine(0, 4);
  if (Button("<")) k->cur = k->cur > 0 ? k->cur - 1 : k->n - 1;
  igSameLine(0, 4);
  if (Button(k->playing ? "Pause" : "Play")) {
    k->playing = !k->playing && k->n > 1;
    k->acc = 0;
  }
  igSameLine(0, 4);
  if (Button(">")) k->cur = k->cur + 1 < k->n ? k->cur + 1 : 0;
  igSameLine(0, 4);
  if (Button(">|")) k->cur = k->n - 1;
  igSameLine(0, 10);
  igText("%d / %d", k->cur + 1, k->n);
  igSameLine(0, 14);
  igSetNextItemWidth(140);
  igSliderInt("ms", &k->ms, 20, 2000, "%d ms a frame", ImGuiSliderFlags_Logarithmic);
  Tooltip("The time between frames, when playing and in the GIF.");
  igSameLine(0, -1);
  igCheckbox("Loop", &k->loop);
  if (Button("Save animated GIF...")) ui_dialog_kinescope(a, true);
  Tooltip("Every frame in one GIF that plays in any browser or slide "
          "(XPPAUT's Kinescope > Make AniGif).");
  igSameLine(0, -1);
  if (Button("Save frames as PNG...")) ui_dialog_kinescope(a, false);
  Tooltip("One PNG file per frame: NAME_001.png, NAME_002.png, ... "
          "(XPPAUT's Kinescope > Save).");
  /* The frame, as large as fits, keeping its shape. */
  avail = igGetContentRegionAvail();
  if (avail.x > 10 && avail.y > 10 && k->cur < k->n) {
    SDL_Surface *s = (SDL_Surface *)k->frame[k->cur];
    float sc = avail.x / (float)s->w;
    ImTextureRef_c tex = frame_texture(a);
    if (s->h * sc > avail.y) sc = avail.y / (float)s->h;
    if (tex._TexID) igImage(tex, V2(s->w * sc, s->h * sc), V2(0, 0), V2(1, 1));
  }
  igEnd();
}
