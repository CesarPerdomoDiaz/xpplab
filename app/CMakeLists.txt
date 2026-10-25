# XPP Lab desktop application: C on top of Dear ImGui (through cimgui),
# ImPlot (through cimplot) and SDL3.

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL3 from the system, or built from source when XPPS_FETCH_SDL3 is on
# (for systems that do not package SDL3 yet).
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
  include(FetchContent)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.16 # ImGui 1.93's SDL3 backends need SDL 3.4
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3)
endif()

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
  # Re-run CMake when the file changes, so an edited page reaches the app.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${input})
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

embed_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/icon_rgba.bin icon_rgba ${GEN}/icon_rgba.c)

set(APP_SOURCES
  src/main.c
  src/app.c
  src/app_analysis.c
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
  src/ui_analysis.c
  src/ui_view3d.c
  src/ui_animator.c
  src/export.c
  ${GEN}/font_roboto.c
  ${GEN}/font_cousine.c
  ${GEN}/icon_rgba.c
  ${GEN}/docs_index.c
  ${DOC_SOURCES}
  ${PROJECT_SOURCE_DIR}/engine/api/xpp_instance.c
)
if(WIN32)
  enable_language(RC)
  list(APPEND APP_SOURCES resources/xpplab.rc)
  set_source_files_properties(resources/xpplab.rc PROPERTIES OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/xpplab.manifest;${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")
endif()
if(APPLE)
  set(MACOSX_BUNDLE_ICON_FILE icon.icns)
  set_source_files_properties(resources/icon.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
  list(APPEND APP_SOURCES resources/icon.icns)
endif()

add_executable(xpplab WIN32 MACOSX_BUNDLE ${APP_SOURCES})
set_target_properties(xpplab PROPERTIES
  OUTPUT_NAME "xpplab"
  MACOSX_BUNDLE_BUNDLE_NAME "XPP Lab"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.xpplab.app"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in"
  INSTALL_RPATH "$ORIGIN"
  BUILD_RPATH "$ORIGIN")
target_include_directories(xpplab PRIVATE src ${PROJECT_SOURCE_DIR}/engine/api ${TP})
target_link_libraries(xpplab PRIVATE xpps_gui SDL3::SDL3)
if(NOT WIN32)
  target_link_libraries(xpplab PRIVATE ${CMAKE_DL_LIBS} m)
endif()
if(MINGW)
  # No MinGW runtime DLLs (libgcc, libstdc++, winpthread) at run time.
  target_link_options(xpplab PRIVATE -static)
endif()
add_dependencies(xpplab xppe)
target_compile_definitions(xpplab PRIVATE
  XPPS_VERSION="${PROJECT_VERSION}")

# The engine library and the example models go where SDL_GetBasePath()
# points: next to the executable, or Contents/Resources in a macOS bundle.
if(APPLE)
  set(APP_DATA_DIR $<TARGET_BUNDLE_CONTENT_DIR:xpplab>/Resources)
else()
  set(APP_DATA_DIR $<TARGET_FILE_DIR:xpplab>)
endif()
add_custom_command(TARGET xpplab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_DATA_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:xppe> ${APP_DATA_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3>
          $<TARGET_FILE_DIR:xpplab>
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${PROJECT_SOURCE_DIR}/engine/xppaut/ode ${APP_DATA_DIR}/examples)

# Install a self-contained folder (or bundle) and package it with CPack.
# Every package carries the licences: XPP Lab's, and the notices and
# texts for the parts by others (THIRD-PARTY-NOTICES.md).
set(XPPS_LICENCE_FILES ${PROJECT_SOURCE_DIR}/LICENSE
    ${PROJECT_SOURCE_DIR}/THIRD-PARTY-NOTICES.md)
if(APPLE)
  install(TARGETS xpplab BUNDLE DESTINATION . COMPONENT app)
  # In the disk image beside the app, and inside the app itself.
  install(FILES ${XPPS_LICENCE_FILES} DESTINATION . COMPONENT app)
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/licenses DESTINATION . COMPONENT app)
  install(FILES ${XPPS_LICENCE_FILES}
          DESTINATION xpplab.app/Contents/Resources COMPONENT app)
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/licenses
          DESTINATION xpplab.app/Contents/Resources COMPONENT app)
  # Copy SDL3 into the bundle and point the executable at it, then sign the
  # result ad hoc (Apple Silicon refuses to run modified unsigned code).
  # First drop the build leftovers among XPPAUT's examples (see below).
  install(CODE [[
    include(BundleUtilities)
    set(app "${CMAKE_INSTALL_PREFIX}/xpplab.app")
    file(GLOB junk "${app}/Contents/Resources/examples/*~"
         "${app}/Contents/Resources/examples/*.o" "${app}/Contents/Resources/examples/*.so"
         "${app}/Contents/Resources/examples/*.bak" "${app}/Contents/Resources/examples/a.out")
    if(junk)
      file(REMOVE ${junk})
    endif()
    fixup_bundle("${app}" "" "")
    execute_process(COMMAND codesign --force --deep --sign - "${app}")
  ]] COMPONENT app)
else()
  install(TARGETS xpplab RUNTIME DESTINATION . COMPONENT app)
  install(FILES $<TARGET_FILE:xppe> DESTINATION . COMPONENT app)
  if(WIN32)
    install(FILES $<TARGET_FILE:SDL3::SDL3> DESTINATION . COMPONENT app)
  elseif(TARGET SDL3-shared)
    # SDL3 built here (XPPS_FETCH_SDL3): install it with its soname links.
    install(TARGETS SDL3-shared LIBRARY DESTINATION . NAMELINK_SKIP COMPONENT app)
  else()
    install(IMPORTED_RUNTIME_ARTIFACTS SDL3::SDL3 LIBRARY DESTINATION .
            COMPONENT app)
  endif()
  # XPPAUT's examples, without the build leftovers in its tree (backups,
  # object files, a compiled a.out); the tree itself stays as upstream has it.
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/engine/xppaut/ode/ DESTINATION examples
          COMPONENT app
          PATTERN "*~" EXCLUDE PATTERN "*.o" EXCLUDE PATTERN "*.so" EXCLUDE
          PATTERN "*.bak" EXCLUDE PATTERN "a.out" EXCLUDE)
  install(FILES ${XPPS_LICENCE_FILES} ${PROJECT_SOURCE_DIR}/README.md
          DESTINATION . COMPONENT app)
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/licenses DESTINATION . COMPONENT app)
endif()
if(MINGW)
  # Copy the DLLs the packaged binaries need (e.g. SDL3's own dependencies
  # from MSYS2), but none of Windows' own.
  get_filename_component(XPPS_MINGW_BIN ${CMAKE_C_COMPILER} DIRECTORY)
  install(CODE "set(XPPS_MINGW_BIN \"${XPPS_MINGW_BIN}\")" COMPONENT app)
  install(CODE [[
    file(GET_RUNTIME_DEPENDENCIES
      EXECUTABLES "${CMAKE_INSTALL_PREFIX}/xpplab.exe"
      LIBRARIES "${CMAKE_INSTALL_PREFIX}/SDL3.dll" "${CMAKE_INSTALL_PREFIX}/libxppe.dll"
      DIRECTORIES "${XPPS_MINGW_BIN}"
      PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
      POST_EXCLUDE_REGEXES "[Ss][Yy][Ss][Tt][Ee][Mm]32" "[Ww][Ii][Nn][Dd][Oo][Ww][Ss]"
      RESOLVED_DEPENDENCIES_VAR deps
      UNRESOLVED_DEPENDENCIES_VAR missing)
    foreach(d IN LISTS deps)
      get_filename_component(name "${d}" NAME)
      if(NOT EXISTS "${CMAKE_INSTALL_PREFIX}/${name}")
        message(STATUS "Bundling ${name}")
        file(INSTALL "${d}" DESTINATION "${CMAKE_INSTALL_PREFIX}")
      endif()
    endforeach()
  ]] COMPONENT app)
endif()
