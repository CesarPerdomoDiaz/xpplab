/*
 * Headless AUTO through the engine API: continue the equilibria of a model
 * in one parameter, then continue periodic orbits from the first Hopf point.
 * Usage: auto_smoke <model.ode> <parameter> [set-name|-] [diagram-output.txt]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xppe.h"

static void print_special(int from) {
  int i, n = xppe_auto_count();
  xppe_auto_point_info p;
  for (i = from; i < n; i++) {
    xppe_auto_point(i, &p);
    if (p.label != 0)
      printf("  lab=%-3d %-2s br=%-3d pt=%-4d %s=%-10.6g norm=%-10.6g %s%s\n",
             p.label, p.type_name, p.branch, p.point,
             xppe_auto_parameter_name(p.par1_slot), p.par1, p.norm,
             p.stable ? "stable" : "unstable",
             p.periodic ? " periodic" : "");
  }
}

static int first_label_of_type(const char *type) {
  int i, n = xppe_auto_count();
  xppe_auto_point_info p;
  for (i = 0; i < n; i++) {
    xppe_auto_point(i, &p);
    if (p.label && strcmp(p.type_name, type) == 0) return p.label;
  }
  return 0;
}

int main(int argc, char **argv) {
  double x[64], re[64], im[64];
  int i, n, rc, hb, before;

  if (argc < 3) {
    fprintf(stderr, "usage: auto_smoke model.ode parameter [set] [out.txt]\n");
    return 2;
  }
  if ((rc = xppe_load(argv[1], 0, NULL)) != XPPE_OK) {
    printf("load failed: %s\n%s", xppe_error(), xppe_log());
    return 1;
  }
  xppe_auto_set_workdir(".");
  if (argc > 3 && strcmp(argv[3], "-") != 0 &&
      xppe_apply_set(argv[3]) != XPPE_OK) {
    printf("set %s: %s\n", argv[3], xppe_error());
    return 1;
  }

  /* Start AUTO from an equilibrium, as one would in XPPAUT. */
  n = xppe_count(XPPE_VARIABLES);
  for (i = 0; i < n; i++) xppe_get(xppe_name(XPPE_VARIABLES, i), &x[i]);
  if (xppe_equilibrium(x, re, im) == XPPE_OK)
    for (i = 0; i < n; i++) xppe_set_number(xppe_name(XPPE_VARIABLES, i), x[i]);

  if ((rc = xppe_auto_parameters(argv[2], NULL)) != XPPE_OK) {
    printf("parameter: %s\n", xppe_error());
    return 1;
  }
  rc = xppe_auto_run("s");
  printf("steady states: rc=%d points=%d %s\n", rc, xppe_auto_count(),
         xppe_error());
  if (rc != XPPE_OK) {
    printf("%s", xppe_log());
    return 1;
  }
  print_special(0);

  hb = first_label_of_type("HB");
  if (hb) {
    before = xppe_auto_count();
    xppe_auto_grab(hb);
    rc = xppe_auto_run("p");
    printf("periodic orbits from HB (label %d): rc=%d new points=%d %s\n", hb,
           rc, xppe_auto_count() - before, xppe_error());
    print_special(before);
  } else {
    printf("no Hopf point found\n");
  }

  if (argc > 4) {
    FILE *f = fopen(argv[4], "w");
    xppe_auto_point_info p;
    n = xppe_auto_count();
    for (i = 0; i < n; i++) {
      double umax[64], umin[64];
      xppe_auto_point(i, &p);
      xppe_auto_point_data(i, XPPE_AUTO_UMAX, umax, 64);
      xppe_auto_point_data(i, XPPE_AUTO_UMIN, umin, 64);
      fprintf(f, "%d %d %d %d %.10g %.10g %.10g %.10g %.10g\n", p.branch,
              p.point, p.type, p.label, p.par1, p.norm, umax[0], umin[0],
              p.period);
    }
    fclose(f);
  }
  return 0;
}
