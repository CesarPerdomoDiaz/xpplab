cmake_minimum_required(VERSION 3.20)
project(XPPStudio VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(XPPS_BUILD_CLI "Build the command-line tools" ON)
option(XPPS_BUILD_TESTS "Build the regression tests" ON)
option(XPPS_BUILD_APP "Build the desktop application (needs SDL3)" ON)
option(XPPS_FETCH_SDL3 "Build SDL3 from source if it is not installed" OFF)

add_subdirectory(engine)
if(XPPS_BUILD_CLI)
  add_subdirectory(cli)
endif()
if(XPPS_BUILD_APP)
  find_package(SDL3 CONFIG QUIET)
  if(SDL3_FOUND OR XPPS_FETCH_SDL3)
    add_subdirectory(app)
  else()
    message(STATUS "SDL3 not found: the desktop application is not built "
                   "(set XPPS_FETCH_SDL3=ON to build SDL3 from source)")
  endif()
endif()
if(XPPS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Packages: a portable zip on Windows, a disk image on macOS, a tarball on
# Linux, each with the desktop app, the engine and the example models.
set(CPACK_PACKAGE_NAME "XPPStudio")
set(CPACK_PACKAGE_VENDOR "XPP Studio contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "XPPAUT's engine with a modern interface")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE)
set(CPACK_COMPONENTS_ALL app)
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
if(WIN32)
  set(CPACK_GENERATOR ZIP)
elseif(APPLE)
  set(CPACK_GENERATOR DragNDrop)
  set(CPACK_DMG_VOLUME_NAME "XPP Studio")
  # The GPL needs no click-through agreement; without this, opening the
  # .dmg shows the licence and asks the user to accept it.
  set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE OFF)
else()
  set(CPACK_GENERATOR TGZ)
endif()
# The same names in every release, so that the README can link to
# releases/latest/download/<name> and the links never break.
if(WIN32)
  set(XPPS_PACKAGE_OS "Windows-x64")
elseif(APPLE)
  set(XPPS_PACKAGE_OS "macOS")           # universal in release builds
else()
  set(XPPS_PACKAGE_OS "Linux-${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(CPACK_PACKAGE_FILE_NAME "XPPStudio-${XPPS_PACKAGE_OS}")
include(CPack)
