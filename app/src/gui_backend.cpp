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

extern "C" void gui_backend_render(SDL_Renderer *renderer) {
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

extern "C" void gui_backend_shutdown(void) {
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
}
