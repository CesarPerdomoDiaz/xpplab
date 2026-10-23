/* The SDL3 platform and renderer backends of Dear ImGui, callable from C. */
#ifndef XPPS_GUI_BACKEND_H
#define XPPS_GUI_BACKEND_H

#include <stdbool.h>

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

bool gui_backend_init(SDL_Window *window, SDL_Renderer *renderer);
void gui_backend_process_event(const SDL_Event *event);
void gui_backend_new_frame(void);
void gui_backend_render(SDL_Renderer *renderer);
void gui_backend_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
