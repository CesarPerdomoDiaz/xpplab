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

add_subdirectory(engine)
if(XPPS_BUILD_CLI)
  add_subdirectory(cli)
endif()
if(XPPS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
