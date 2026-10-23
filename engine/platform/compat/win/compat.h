/* Force-included when building unmodified XPPAUT 8.0 with MinGW-w64.
   Pull in the CRT headers first, then rename the CRT functions that
   XPPAUT redefines with different signatures. */
#ifndef XPP_REFBUILD_COMPAT_H
#define XPP_REFBUILD_COMPAT_H
#include <string.h>
#include <stdlib.h>
#define strupr xpp_strupr
#define strlwr xpp_strlwr
#endif
