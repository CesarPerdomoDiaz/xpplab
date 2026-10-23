# XPP Studio desktop application: C on top of Dear ImGui (through cimgui),
# ImPlot (through cimplot) and SDL3.

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED CONFIG)

set(TP ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# Dear ImGui, ImPlot and their C bindings, as one static library.
add_library(xpps_gui STATIC
  ${TP}/imgui/imgui.cpp
  ${TP}/imgui/imgui_draw.cpp
  ${TP}/imgui/imgui_tables.cpp
  ${TP}/imgui/imgui_widgets.cpp
  ${TP}/imgui/imgui_demo.cpp
  ${TP}/imgui/backends/imgui_impl_sdl3.cpp
  ${TP}/imgui/backends/imgui_impl_sdlrenderer3.cpp
  ${TP}/implot/implot.cpp
  ${TP}/implot/implot_items.cpp
  ${TP}/implot/implot_demo.cpp
  ${TP}/cimgui/cimgui.cpp
  ${TP}/cimplot/cimplot.cpp
  src/gui_backend.cpp
)
target_include_directories(xpps_gui PUBLIC
  ${TP} ${TP}/imgui ${TP}/imgui/backends ${TP}/implot ${TP}/cimgui ${TP}/cimplot)
target_link_libraries(xpps_gui PUBLIC SDL3::SDL3)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(xpps_gui PRIVATE -w)
endif()

# Fonts and documentation are embedded in the executable.
set(GEN ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN})
function(embed_file input symbol output)
  file(READ ${input} hex HEX)
  string(LENGTH "${hex}" hexlen)
  math(EXPR len "${hexlen} / 2")
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
  file(WRITE ${output}
    "/* Generated from ${input}. */\n"
    "const unsigned char ${symbol}[] = {${bytes} 0};\n"
    "const unsigned int ${symbol}_size = ${len};\n")
endfunction()
embed_file(${TP}/imgui/fonts/Roboto-Medium.ttf font_roboto ${GEN}/font_roboto.c)
embed_file(${TP}/imgui/fonts/Cousine-Regular.ttf font_cousine ${GEN}/font_cousine.c)

file(GLOB DOC_PAGES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/docs/manual/*.md)
set(DOC_SOURCES "")
set(DOC_TABLE "")
set(DOC_DECLS "")
foreach(page ${DOC_PAGES})
  get_filename_component(stem ${page} NAME_WE)
  string(MAKE_C_IDENTIFIER "doc_${stem}" sym)
  embed_file(${page} ${sym} ${GEN}/${sym}.c)
  list(APPEND DOC_SOURCES ${GEN}/${sym}.c)
  string(APPEND DOC_DECLS "extern const unsigned char ${sym}[];\n")
  string(APPEND DOC_TABLE "  {\"${stem}\", (const char *)${sym}},\n")
endforeach()
file(WRITE ${GEN}/docs_index.c
  "/* Generated: the embedded manual pages. */\n"
  "#include \"docs.h\"\n${DOC_DECLS}"
  "const doc_page doc_pages[] = {\n${DOC_TABLE}  {0, 0}};\n")

add_executable(xppstudio WIN32
  src/main.c
  src/app.c
  src/commands.c
  src/docs.c
  src/ui_common.c
  src/ui_menu.c
  src/ui_workspace.c
  src/ui_editor.c
  src/ui_plots.c
  src/ui_auto.c
  src/ui_console.c
  src/ui_help.c
  src/ui_browser.c
  src/ui_data.c
  src/export.c
  ${GEN}/font_roboto.c
  ${GEN}/font_cousine.c
  ${GEN}/docs_index.c
  ${DOC_SOURCES}
  ${PROJECT_SOURCE_DIR}/engine/api/xpp_instance.c
)
target_include_directories(xppstudio PRIVATE src ${PROJECT_SOURCE_DIR}/engine/api ${TP})
target_link_libraries(xppstudio PRIVATE xpps_gui SDL3::SDL3)
if(NOT WIN32)
  target_link_libraries(xppstudio PRIVATE ${CMAKE_DL_LIBS} m)
endif()
if(MINGW)
  target_link_options(xppstudio PRIVATE -static-libgcc -static-libstdc++)
endif()
add_dependencies(xppstudio xppe)
target_compile_definitions(xppstudio PRIVATE
  XPPS_VERSION="${PROJECT_VERSION}")

# The engine library and the example models sit next to the executable.
add_custom_command(TARGET xppstudio POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:xppe>
          $<TARGET_FILE_DIR:xppstudio>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3>
          $<TARGET_FILE_DIR:xppstudio>
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${PROJECT_SOURCE_DIR}/engine/xppaut/ode
          $<TARGET_FILE_DIR:xppstudio>/examples)
