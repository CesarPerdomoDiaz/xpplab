// The only C++ in the application: thin wrappers that let the C code call
// Dear ImGui's SDL3 backends. Everything else goes through cimgui.
#include "gui_backend.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

extern "C" bool gui_backend_init(SDL_Window *window, SDL_Renderer *renderer) {
  return ImGui_ImplSDL3_InitForSDLRenderer(window, renderer) &&
         ImGui_ImplSDLRenderer3_Init(renderer);
}

extern "C" void gui_backend_process_event(const SDL_Event *event) {
  ImGui_ImplSDL3_ProcessEvent(event);
}

extern "C" void gui_backend_new_frame(void) {
  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
}

// ImGui draws in points. The backend scales its clip rectangles to pixels
// but passes the vertices on unscaled, expecting SDL's render scale to do
// that (as in Dear ImGui's SDL3 example). Without it a Retina window, 2
// pixels per point, got the interface in its top-left quarter, clipped in
// the wrong places. The scale is reset so that pixel reads (screenshots,
// figure export) stay in pixels.
extern "C" void gui_backend_render(SDL_Renderer *renderer) {
  ImGuiIO &io = ImGui::GetIO();
  SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
  SDL_SetRenderScale(renderer, 1.0f, 1.0f);
}

extern "C" void gui_backend_shutdown(void) {
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
}
