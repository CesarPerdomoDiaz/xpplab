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

add_library(xppengine STATIC
  ${XPPAUT_SOURCES}
  platform/nullx/nullx.c
  $<$<BOOL:${WIN32}>:platform/compat/dlfcn_win.c>
)

target_include_directories(xppengine
  PUBLIC xppaut
  PUBLIC platform/nullx/include
)

target_compile_definitions(xppengine PRIVATE
  AUTO CVODE_YES HAVEDLL NOERRNO NON_UNIX_STDIO MAXW=50
  MYSTR1=8.0 MYSTR2=1
  main=xpp_upstream_main
  $<$<BOOL:${WIN32}>:NORAND48>
)

if(WIN32)
  target_include_directories(xppengine BEFORE PRIVATE platform/compat/win)
  target_compile_options(xppengine PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/platform/compat/win/compat.h)
endif()

# The upstream code predates C99 strictness: K&R-style declarations,
# implicit int, implicit declarations and tentative definitions shared
# between files. Modern GCC and Clang reject these by default.
target_compile_options(xppengine PRIVATE -w -fcommon)
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
    target_compile_options(xppengine PRIVATE ${flag})
  endif()
endforeach()

# Same numbers on every platform: no fused multiply-add contraction.
check_c_compiler_flag(-ffp-contract=off HAVE_FP_CONTRACT_OFF)
if(HAVE_FP_CONTRACT_OFF)
  target_compile_options(xppengine PUBLIC -ffp-contract=off)
endif()

if(NOT WIN32)
  target_link_libraries(xppengine PUBLIC ${CMAKE_DL_LIBS})
endif()
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
  target_link_libraries(xppengine PUBLIC ${MATH_LIBRARY})
endif()
