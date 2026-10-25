# The XPPAUT 8.0 engine, built as a library without X11.
#
# Upstream sources live in xppaut/ and are kept as close to the original as
# possible. Everything that makes them build on modern compilers and on
# Windows/macOS without an X server lives in platform/.

include(CheckCCompilerFlag)

set(XPPAUT_SOURCES
    xppaut/abort.c
    xppaut/adj2.c
    xppaut/aniparse.c
    xppaut/array_print.c
    xppaut/arrayplot.c
    xppaut/autevd.c
    xppaut/autlib1.c
    xppaut/autlib2.c
    xppaut/autlib3.c
    xppaut/autlib4.c
    xppaut/autlib5.c
    xppaut/auto_nox.c
    xppaut/auto_x11.c
    xppaut/autpp.c
    xppaut/axes2.c
    xppaut/band.c
    xppaut/browse.c
    xppaut/cabs.c
    xppaut/calc.c
    xppaut/choice_box.c
    xppaut/color.c
    xppaut/comline.c
    xppaut/conpar2.c
    xppaut/cv2.c
    xppaut/cvband.c
    xppaut/cvdense.c
    xppaut/cvdiag.c
    xppaut/cvode.c
    xppaut/cvspgmr.c
    xppaut/d_imag.c
    xppaut/d_lg10.c
    xppaut/d_sign.c
    xppaut/dae_fun.c
    xppaut/del_stab.c
    xppaut/delay_handle.c
    xppaut/dense.c
    xppaut/derived.c
    xppaut/diagram.c
    xppaut/dialog_box.c
    xppaut/do_fit.c
    xppaut/dormpri.c
    xppaut/edit_rhs.c
    xppaut/eig_list.c
    xppaut/eispack.c
    xppaut/extra.c
    xppaut/fftn.c
    xppaut/flags.c
    xppaut/form_ode.c
    xppaut/gear.c
    xppaut/ggets.c
    xppaut/gogoauto.c
    xppaut/graf_par.c
    xppaut/graphics.c
    xppaut/histogram.c
    xppaut/i_dnnt.c
    xppaut/i_nint.c
    xppaut/init_conds.c
    xppaut/integrate.c
    xppaut/iterativ.c
    xppaut/kinescope.c
    xppaut/llnlmath.c
    xppaut/load_eqn.c
    xppaut/lunch-new.c
    xppaut/main.c
    xppaut/many_pops.c
    xppaut/markov.c
    xppaut/menu.c
    xppaut/menudrive.c
    xppaut/my_ps.c
    xppaut/my_rhs.c
    xppaut/my_svg.c
    xppaut/nullcline.c
    xppaut/numerics.c
    xppaut/odesol2.c
    xppaut/parserslow2.c
    xppaut/pop_list.c
    xppaut/pow_dd.c
    xppaut/pow_di.c
    xppaut/pow_ii.c
    xppaut/pp_shoot.c
    xppaut/r_lg10.c
    xppaut/read_dir.c
    xppaut/rubber.c
    xppaut/scrngif.c
    xppaut/setubv2.c
    xppaut/simplenet.c
    xppaut/spgmr.c
    xppaut/stiff.c
    xppaut/storage.c
    xppaut/tabular.c
    xppaut/torus.c
    xppaut/txtread.c
    xppaut/userbut.c
    xppaut/vector.c
    xppaut/volterra2.c
    xppaut/worker2.c
    xppaut/z_abs.c
    xppaut/z_exp.c
    xppaut/z_log.c
)

# All engine code is compiled once and linked two ways: a static library
# for the command-line tools and a shared library that exports the engine
# API (api/xppe.h) for Python and the desktop app.
add_library(xppengine_obj OBJECT
  ${XPPAUT_SOURCES}
  platform/nullx/nullx.c
  platform/embed/xpps_embed.c
  api/xppe.c
  api/xppe_auto.c
  api/xppe_analysis.c
  api/xppe_data.c
  api/xppe_bvp.c
  api/xppe_eq.c
  api/xppe_ani.c
  $<$<BOOL:${WIN32}>:platform/compat/dlfcn_win.c>
)

target_include_directories(xppengine_obj
  PUBLIC api
  PRIVATE xppaut platform/nullx/include platform/embed
)

target_compile_definitions(xppengine_obj PRIVATE
  AUTO CVODE_YES HAVEDLL NOERRNO NON_UNIX_STDIO MAXW=50
  MYSTR1=8.0 MYSTR2=1
  main=xpp_upstream_main
  XPPE_BUILD_SHARED
  $<$<BOOL:${WIN32}>:NORAND48>
)

if(WIN32)
  target_include_directories(xppengine_obj BEFORE PRIVATE platform/compat/win)
  target_compile_options(xppengine_obj PRIVATE
    "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/platform/compat/win/compat.h")
endif()
# printf() and exit() go through the embedding layer (see xpps_embed.h).
target_compile_options(xppengine_obj PRIVATE
  "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/platform/embed/xpps_embed.h")

# The upstream code predates C99 strictness: K&R-style declarations,
# implicit int, implicit declarations and tentative definitions shared
# between files. Modern GCC and Clang reject these by default.
target_compile_options(xppengine_obj PRIVATE -w -fcommon)
# Upstream's Makefile builds with -O2; match it rather than CMake's -O3.
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_options(xppengine_obj PRIVATE -O2)
endif()
foreach(flag -fpermissive
             -Wno-error=implicit-function-declaration
             -Wno-error=implicit-int
             -Wno-error=int-conversion
             -Wno-error=incompatible-pointer-types
             -Wno-error=incompatible-function-pointer-types
             -Wno-error=return-type)
  string(MAKE_C_IDENTIFIER "HAVE${flag}" var)
  check_c_compiler_flag(${flag} ${var})
  if(${var})
    target_compile_options(xppengine_obj PRIVATE ${flag})
  endif()
endforeach()

# Same numbers on every platform: no fused multiply-add contraction.
check_c_compiler_flag(-ffp-contract=off HAVE_FP_CONTRACT_OFF)
if(HAVE_FP_CONTRACT_OFF)
  target_compile_options(xppengine_obj PRIVATE -ffp-contract=off)
endif()
# Exported API only; everything else stays internal to the shared library.
if(NOT WIN32)
  target_compile_options(xppengine_obj PRIVATE -fvisibility=hidden)
endif()

set(XPPENGINE_LIBS "")
if(NOT WIN32)
  list(APPEND XPPENGINE_LIBS ${CMAKE_DL_LIBS})
endif()
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
  list(APPEND XPPENGINE_LIBS ${MATH_LIBRARY})
endif()

add_library(xppengine STATIC $<TARGET_OBJECTS:xppengine_obj>)
target_include_directories(xppengine PUBLIC api)
target_link_libraries(xppengine PUBLIC ${XPPENGINE_LIBS})

add_library(xppe SHARED $<TARGET_OBJECTS:xppengine_obj>)
target_include_directories(xppe PUBLIC api)
target_link_libraries(xppe PRIVATE ${XPPENGINE_LIBS})
if(WIN32)
  # A self-contained DLL: no libgcc/winpthread DLLs needed at run time.
  target_link_options(xppe PRIVATE -static-libgcc -static)
endif()

# Inside a Python wheel the library lives next to the package code.
if(SKBUILD)
  install(TARGETS xppe
    RUNTIME DESTINATION xpplab/_lib COMPONENT python
    LIBRARY DESTINATION xpplab/_lib COMPONENT python)
  # The MCP server (xpplab.mcp) offers agents the example models and the
  # manual's pages on the .ode format and AUTO.
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/engine/xppaut/ode/
    DESTINATION xpplab/examples COMPONENT python
    FILES_MATCHING PATTERN "*.ode" PATTERN "*.ani" PATTERN "*.tab" PATTERN "*.dat")
  install(FILES ${PROJECT_SOURCE_DIR}/docs/manual/odefile.md
                ${PROJECT_SOURCE_DIR}/docs/manual/auto.md
    DESTINATION xpplab/_docs COMPONENT python)
endif()
