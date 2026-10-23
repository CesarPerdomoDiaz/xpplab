/* Minimal getrusage() for MinGW-w64 reference builds. */
#ifndef XPP_COMPAT_SYS_RESOURCE_H
#define XPP_COMPAT_SYS_RESOURCE_H
#include <sys/time.h>
#include <time.h>
#define RUSAGE_SELF 0
struct rusage {
  struct timeval ru_utime;
  struct timeval ru_stime;
};
static inline int getrusage(int who, struct rusage *u) {
  clock_t c = clock();
  (void)who;
  u->ru_utime.tv_sec = (long)(c / CLOCKS_PER_SEC);
  u->ru_utime.tv_usec = (long)((c % CLOCKS_PER_SEC) * (1000000L / CLOCKS_PER_SEC));
  u->ru_stime.tv_sec = 0;
  u->ru_stime.tv_usec = 0;
  return 0;
}
#endif
