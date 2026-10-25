/*
 * xpp_instance.h - open several independent XPPAUT engines in one program.
 *
 * The engine keeps its model in global variables, so each model needs its
 * own copy of the engine library. xpp_instance_open() copies the library
 * to a private temporary file, loads it and resolves every function of
 * xppe.h into a table:
 *
 *     xpp_instance *e = xpp_instance_open(NULL, err, sizeof err);
 *     e->api.xppe_load("lecar.ode", 0, NULL);
 *     e->api.xppe_run();
 *     xpp_instance_close(e);
 *
 * Closing an instance unloads its copy and frees everything the model used.
 */
#ifndef XPP_INSTANCE_H
#define XPP_INSTANCE_H

#include <stddef.h>

#include "xppe.h"

#ifdef __cplusplus
extern "C" {
#endif

/* X(return type, name, parameter list) for every engine API function. */
#define XPP_ENGINE_FUNCTIONS(X)                                               \
  X(int, xppe_api_version, (void))                                            \
  X(const char *, xppe_version, (void))                                       \
  X(int, xppe_load, (const char *, int, const char *const *))                 \
  X(const char *, xppe_log, (void))                                           \
  X(const char *, xppe_error, (void))                                         \
  X(int, xppe_count, (int))                                                   \
  X(const char *, xppe_name, (int, int))                                      \
  X(int, xppe_get, (const char *, double *))                                  \
  X(int, xppe_set, (const char *, const char *))                              \
  X(int, xppe_set_number, (const char *, double))                             \
  X(int, xppe_apply_set, (const char *))                                      \
  X(int, xppe_run, (void))                                                    \
  X(int, xppe_rows, (void))                                                   \
  X(int, xppe_column, (int, double *, int))                                   \
  X(int, xppe_ic_last, (void))                                                \
  X(int, xppe_equilibrium, (double *, double *, double *))                    \
  X(int, xppe_rhs, (double, const double *, double *))                        \
  X(int, xppe_nullcline,                                                      \
    (int, int, int, double, double, double, double, int, float *, int))       \
  X(int, xppe_manifold_count, (void))                                         \
  X(int, xppe_manifold, (int, int *))                                         \
  X(int, xppe_equilibrium_sweep,                                              \
    (const char *, const double *, int, const double *))                      \
  X(int, xppe_find_equilibria, (int, double, const double *, const double *)) \
  X(int, xppe_found_equilibrium, (int, double *, double *, double *))         \
  X(int, xppe_fourier, (const char *))                                        \
  X(int, xppe_power, (const char *))                                          \
  X(int, xppe_spectrum, (int, const char *, const char *, int, int))          \
  X(int, xppe_histogram, (const char *, int, double, double, const char *))   \
  X(int, xppe_histogram2d,                                                    \
    (const char *, int, double, double, const char *, int, double, double))   \
  X(int, xppe_autocorrelation, (const char *, int, double, double))           \
  X(int, xppe_correlation, (const char *, const char *, int, int))            \
  X(int, xppe_mean, (const char *, double *, double *))                       \
  X(int, xppe_lyapunov, (double *))                                           \
  X(int, xppe_adjoint, (int, double))                                         \
  X(int, xppe_hfunction, (int, const char *const *))                          \
  X(int, xppe_fit, (const char *, int, int, const char *, const char *,       \
                    const char *, double, double, int))                       \
  X(int, xppe_result_rows, (void))                                            \
  X(int, xppe_result_count, (void))                                           \
  X(const char *, xppe_result_name, (int))                                    \
  X(int, xppe_result_column, (int, double *, int))                            \
  X(int, xppe_bvp_count, (void))                                              \
  X(int, xppe_bvp, (int, double, double))                                     \
  X(int, xppe_ani_load, (const char *))                                       \
  X(int, xppe_ani_window, (double *, double *, double *, double *))           \
  X(int, xppe_ani_frame, (int))                                               \
  X(int, xppe_ani_get, (int, xppe_ani_item *))                                \
  X(int, xppe_auto_get, (const char *, double *))                             \
  X(int, xppe_auto_set, (const char *, double))                               \
  X(int, xppe_auto_parameters, (const char *, const char *))                  \
  X(int, xppe_auto_set_workdir, (const char *))                               \
  X(int, xppe_auto_reset, (void))                                             \
  X(int, xppe_auto_run, (const char *))                                       \
  X(int, xppe_auto_grab, (int))                                               \
  X(int, xppe_auto_ungrab, (void))                                            \
  X(int, xppe_auto_parameter_count, (void))                                   \
  X(const char *, xppe_auto_parameter_name, (int))                            \
  X(int, xppe_auto_main_slot, (int))                                          \
  X(int, xppe_auto_count, (void))                                             \
  X(int, xppe_auto_point, (int, xppe_auto_point_info *))                      \
  X(int, xppe_auto_point_data, (int, int, double *, int))

typedef struct {
#define XPP_FIELD(ret, name, params) ret(*name) params;
  XPP_ENGINE_FUNCTIONS(XPP_FIELD)
#undef XPP_FIELD
} xpp_engine_api;

typedef struct xpp_instance {
  xpp_engine_api api;
  char workdir[1024]; /* a private scratch directory (AUTO files go here) */
  void *handle_;
  char libcopy_[1024];
} xpp_instance;

/* The file name of the engine library on this platform. */
const char *xpp_instance_library_name(void);

/* Open a new engine from the library at `library_path`. Returns NULL and
   writes a message to err on failure. */
xpp_instance *xpp_instance_open(const char *library_path, char *err,
                                size_t errlen);
void xpp_instance_close(xpp_instance *e);

#ifdef __cplusplus
}
#endif

#endif
