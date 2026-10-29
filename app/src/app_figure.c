/* Figures for papers, and the runs that go in them (app.h).

   Many runs: initial values drawn at random in a box (Model.sample_initial
   and Model.run_many), or both ways along an equilibrium's real
   eigenvectors (Model.shoot), kept with the global flag that stopped each.
   The draws come from SplitMix64, as in Python, so a seed gives the same
   states in both.

   The figure (fig.h) holds copies of the data its series show. Each change
   is written to the session as an xpplab.figure call: a series as it is
   added, from the same source in Python (fig[0].run(sol, "T", "V"),
   fig[0].diagram(m.auto.diagram, "V", ...)); settings as they change
   (fig[0].set(xlabel=...), fig[0][1].set(color=...)), by comparing the
   figure with what the script has (a->fg.shadow). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "app.h"
#include "export.h"

/* ---- Random draws ------------------------------------------------------------- */

typedef struct {
  unsigned long long s;
} SplitMix;

static unsigned long long sm_next(SplitMix *r) {
  unsigned long long z;
  r->s += 0x9E3779B97F4A7C15ULL;
  z = r->s;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

static double sm_uniform(SplitMix *r) {
  return (double)(sm_next(r) >> 11) * (1.0 / 9007199254740992.0);
}

/* ---- Many runs ------------------------------------------------------------------ */

void app_many_free(App *a) {
  RunSet *r = &a->many;
  int i;
  for (i = 0; i < r->n; i++) app_free_trajectory(&r->runs[i]);
  free(r->runs);
  free(r->event);
  free(r->lambda);
  free(r->sign);
  memset(r, 0, sizeof *r);
}

void app_many_reset(App *a) {
  ManyUI *m = &a->mu;
  int i;
  app_many_free(a);
  free(m->use);
  free(m->lo);
  free(m->hi);
  memset(m, 0, sizeof *m);
  m->n = 100;
  m->seed = 1;
  m->req_col = -1;
  m->req_lo = 0;
  m->req_hi = INFINITY;
  m->eps = 1e-5;
  m->shoot_total = 0;
  m->use = calloc((size_t)a->nvars + 1, sizeof(bool));
  m->lo = calloc((size_t)a->nvars + 1, sizeof(double));
  m->hi = calloc((size_t)a->nvars + 1, sizeof(double));
  if (!m->use || !m->lo || !m->hi) return;
  for (i = 0; i < a->nvars; i++) {
    double v = a->ic[i], d = fabs(v) > 0 ? fabs(v) : 1;
    m->lo[i] = v - d;
    m->hi[i] = v + d;
    m->use[i] = i < 2;
  }
}

static bool grow_set(RunSet *r, int n) {
  r->runs = calloc((size_t)n + 1, sizeof(Trajectory));
  r->event = calloc((size_t)n + 1, sizeof(int));
  r->lambda = calloc((size_t)n + 1, sizeof(double));
  r->sign = calloc((size_t)n + 1, sizeof(int));
  return r->runs && r->event && r->lambda && r->sign;
}

/* Run from the state x (all variables); the run goes into the set. */
static bool run_from(App *a, const double *x, RunSet *r) {
  xpp_engine_api *e = &a->eng->api;
  int i;
  for (i = 0; i < a->nvars; i++) e->xppe_set_number(a->vars[i], x[i]);
  if (e->xppe_run() != XPPE_OK) {
    app_log(a, "error: %s", e->xppe_error());
    return false;
  }
  app_fetch_run(a, &r->runs[r->n]);
  r->event[r->n] = e->xppe_halt_flag();
  r->n++;
  return true;
}

static void restore_ics(App *a, const double *ics) {
  int i;
  for (i = 0; i < a->nvars; i++) a->eng->api.xppe_set_number(a->vars[i], ics[i]);
}

static void count_events(App *a, const RunSet *r, char *out, size_t len) {
  int counts[64] = {0}, i, k = 0, nf = a->eng->api.xppe_flag_count();
  size_t used = 0;
  for (i = 0; i < r->n; i++)
    if (r->event[i] >= 0 && r->event[i] < 64) counts[r->event[i]]++;
  out[0] = '\0';
  for (i = 0; i < 64 && i <= nf; i++) {
    if (!counts[i]) continue;
    used += (size_t)snprintf(out + used, used < len ? len - used : 0, "%s%d %s", k++ ? ", " : "",
                             counts[i], i == 0 ? "ran to the end" : "");
    if (i > 0) {
      int sign = 0;
      const char *c = a->eng->api.xppe_flag_condition(i - 1, &sign);
      char cond[200];
      size_t m;
      snprintf(cond, sizeof cond, "%s", c ? c : "?");
      m = strlen(cond);
      while (m > 0 && cond[m - 1] == ' ') cond[--m] = '\0';
      used += (size_t)snprintf(out + used, used < len ? len - used : 0, "stopped by flag %d (%s)",
                               i, cond);
    }
  }
}

bool app_many_runs(App *a) {
  ManyUI *m = &a->mu;
  RunSet *r = &a->many;
  xpp_engine_api *e;
  double *ics, *x, *row;
  int ncols, tries, i, nbox = 0;
  bool outer, ok = true;
  char line[4096], num[64], nm[160], counts[512];
  size_t k = 0;
  SplitMix rng;
  if (!a->model_ok || !m->use) return false;
  e = &a->eng->api;
  for (i = 0; i < a->nvars; i++) nbox += m->use[i];
  if (m->n < 1 || nbox == 0) {
    app_log(a, "error: choose the number of runs and at least one variable to draw.");
    return false;
  }
  ncols = e->xppe_count(XPPE_COLUMNS);
  ics = malloc(sizeof(double) * (size_t)(a->nvars + 1));
  x = malloc(sizeof(double) * (size_t)(a->nvars + 1));
  row = malloc(sizeof(double) * (size_t)(ncols + 1));
  if (!ics || !x || !row) {
    free(ics);
    free(x);
    free(row);
    return false;
  }
  outer = echo_begin(a);
  for (i = 0; i < a->nvars; i++) e->xppe_get(a->vars[i], &ics[i]);
  app_many_free(a);
  if (!grow_set(r, m->n)) {
    echo_end(a);
    free(ics);
    free(x);
    free(row);
    return false;
  }
  rng.s = (unsigned long long)(long long)m->seed;
  /* The draws first, as Model.sample_initial() makes them. */
  {
    double *starts = malloc(sizeof(double) * (size_t)m->n * (size_t)(a->nvars + 1));
    int got = 0;
    int req = m->req_col >= 0 && m->req_col < a->ncols ? m->req_col : -1;
    /* a->cols has T first even when the model's "only" leaves it out */
    int engine_col = req >= 0 ? (a->time_missing ? req - 1 : req) : -1;
    if (!starts) ok = false;
    for (tries = 0; ok && tries < 100000 && got < m->n; tries++) {
      memcpy(x, ics, sizeof(double) * (size_t)a->nvars);
      for (i = 0; i < a->nvars; i++)
        if (m->use[i]) x[i] = m->lo[i] + (m->hi[i] - m->lo[i]) * sm_uniform(&rng);
      if (engine_col >= 0) {
        if (e->xppe_columns_at(0.0, x, row, ncols) < 0 || !(row[engine_col] >= m->req_lo) ||
            !(row[engine_col] <= m->req_hi))
          continue;
      }
      memcpy(starts + (size_t)got * (size_t)a->nvars, x, sizeof(double) * (size_t)a->nvars);
      got++;
    }
    if (ok && got < m->n) {
      app_log(a, "error: only %d of %d draws met the condition in 100000 tries.", got, m->n);
      ok = false;
    }
    for (i = 0; ok && i < m->n; i++)
      if (!run_from(a, starts + (size_t)i * (size_t)a->nvars, r)) ok = false;
    free(starts);
  }
  restore_ics(a, ics);
  if (ok && outer) {
    k += (size_t)snprintf(line + k, sizeof line - k, "many = m.run_many(m.sample_initial(%d, {",
                          m->n);
    nbox = 0;
    for (i = 0; i < a->nvars; i++) {
      char lo[64];
      if (!m->use[i]) continue;
      k += (size_t)snprintf(line + k, sizeof line - k, "%s%s: (%s, %s)", nbox++ ? ", " : "",
                            echo_str(a->vars[i], nm, sizeof nm), echo_num(m->lo[i], lo, sizeof lo),
                            echo_num(m->hi[i], num, sizeof num));
    }
    k += (size_t)snprintf(line + k, sizeof line - k, "}");
    if (m->req_col >= 0 && m->req_col < a->ncols) {
      char lo[64];
      k += (size_t)snprintf(line + k, sizeof line - k, ", require={%s: (%s, %s)}",
                            echo_str(a->cols[m->req_col], nm, sizeof nm),
                            echo_num(m->req_lo, lo, sizeof lo),
                            echo_num(m->req_hi, num, sizeof num));
    }
    snprintf(line + k, sizeof line - k, ", seed=%d))", m->seed);
    echo_line(a, "%s", line);
  }
  echo_end(a);
  free(ics);
  free(x);
  free(row);
  if (!ok) {
    app_many_free(a);
    return false;
  }
  r->show = true;
  r->shoot = false;
  snprintf(r->what, sizeof r->what, "%d runs from random initial values", r->n);
  count_events(a, r, counts, sizeof counts);
  app_log(a, "Many runs: %d from random initial values (seed %d): %s.", r->n, m->seed, counts);
  app_status(a, "%d runs: %s", r->n, counts);
  return true;
}

bool app_shoot(App *a) {
  ManyUI *m = &a->mu;
  RunSet *r = &a->many;
  xpp_engine_api *e;
  int n = a->nvars, i, k;
  double *x0, *re, *im, *A, *v, *ics, *x, total = 0, dt = 0;
  bool outer, ok = true;
  char num[64], tot[64], counts[512];
  static const char *dirs[] = {"unstable", "stable", "all"};
  if (!a->model_ok) return false;
  e = &a->eng->api;
  x0 = calloc((size_t)n + 1, sizeof(double));
  re = calloc((size_t)n + 1, sizeof(double));
  im = calloc((size_t)n + 1, sizeof(double));
  A = calloc((size_t)(n * n) + 1, sizeof(double));
  v = calloc((size_t)n + 1, sizeof(double));
  ics = calloc((size_t)n + 1, sizeof(double));
  x = calloc((size_t)n + 1, sizeof(double));
  if (!x0 || !re || !im || !A || !v || !ics || !x) {
    ok = false;
    goto done;
  }
  outer = echo_begin(a);
  for (i = 0; i < n; i++) e->xppe_get(a->vars[i], &ics[i]);
  memcpy(x0, ics, sizeof(double) * (size_t)n);
  /* as Model.shoot(): Newton's method from the initial values, then
     Model.linearize() there and its eigenvalues */
  if (e->xppe_equilibrium(x0, re, im) != XPPE_OK) {
    app_log(a, "error: no equilibrium found from the initial values: %s", e->xppe_error());
    echo_end(a);
    ok = false;
    goto done;
  }
  if (e->xppe_linearize(x0, 0, NULL, 0.0, A, NULL, NULL, NULL) != XPPE_OK ||
      e->xppe_eigenvalues(n, A, re, im) != XPPE_OK) {
    app_log(a, "error: %s", e->xppe_error());
    echo_end(a);
    ok = false;
    goto done;
  }
  e->xppe_get("TOTAL", &total);
  e->xppe_get("DT", &dt);
  app_many_free(a);
  if (!grow_set(r, 2 * n)) {
    echo_end(a);
    ok = false;
    goto done;
  }
  if (m->shoot_total > 0) e->xppe_set_number("TOTAL", m->shoot_total);
  for (k = 0; ok && k < n; k++) {
    int s;
    bool forward;
    if (im[k] != 0 || re[k] == 0) continue;
    forward = re[k] > 0;
    if ((m->dirs == 0 && !forward) || (m->dirs == 1 && forward)) continue;
    if (e->xppe_eigenvector(n, A, re[k], v) != XPPE_OK) {
      app_log(a, "error: %s", e->xppe_error());
      ok = false;
      break;
    }
    e->xppe_set_number("DT", forward ? fabs(dt) : -fabs(dt));
    for (s = 1; s >= -1; s -= 2) {
      for (i = 0; i < n; i++) x[i] = x0[i] + s * m->eps * v[i];
      r->lambda[r->n] = re[k];
      r->sign[r->n] = s;
      if (!run_from(a, x, r)) {
        ok = false;
        break;
      }
    }
  }
  e->xppe_set_number("TOTAL", total);
  e->xppe_set_number("DT", dt);
  restore_ics(a, ics);
  if (ok && outer) {
    if (m->shoot_total > 0)
      echo_line(a, "many = m.shoot(eps=%s, total=%s, directions=\"%s\")",
                echo_num(m->eps, num, sizeof num), echo_num(m->shoot_total, tot, sizeof tot),
                dirs[m->dirs < 0 || m->dirs > 2 ? 0 : m->dirs]);
    else
      echo_line(a, "many = m.shoot(eps=%s, directions=\"%s\")", echo_num(m->eps, num, sizeof num),
                dirs[m->dirs < 0 || m->dirs > 2 ? 0 : m->dirs]);
  }
  echo_end(a);
  if (ok && r->n == 0) {
    app_log(a, "The equilibrium has no real %s directions to shoot along.",
            m->dirs == 1 ? "stable" : m->dirs == 0 ? "unstable" : "");
    ok = false;
  }
  if (ok) {
    r->show = true;
    r->shoot = true;
    snprintf(r->what, sizeof r->what, "%d runs along the eigenvectors", r->n);
    count_events(a, r, counts, sizeof counts);
    app_log(a, "Shot %d runs from the equilibrium along its eigenvectors: %s.", r->n, counts);
    app_status(a, "Shot %d runs: %s", r->n, counts);
  } else if (r->n == 0) {
    app_many_free(a);
  }
done:
  free(x0);
  free(re);
  free(im);
  free(A);
  free(v);
  free(ics);
  free(x);
  return ok;
}

/* RunList.to_mat(): run, event, then the columns, every run after another. */
static bool write_many_mat(App *a, const char *path) {
  RunSet *r = &a->many;
  int nc = r->runs[0].ncols + 2, rows = 0, at = 0, k, i, j;
  const char **nm = calloc((size_t)nc, sizeof *nm);
  double **col = calloc((size_t)nc, sizeof *col);
  bool ok = nm && col;
  for (k = 0; k < r->n; k++) rows += r->runs[k].nrows;
  for (j = 0; ok && j < nc; j++)
    ok = (col[j] = calloc((size_t)(rows > 0 ? rows : 1), sizeof(double))) != NULL;
  if (ok) {
    nm[0] = "run";
    nm[1] = "event";
    for (j = 2; j < nc; j++) nm[j] = r->runs[0].names[j - 2];
    if (strcmp(nm[2], "T") == 0) nm[2] = "t";
    for (k = 0; k < r->n; k++)
      for (i = 0; i < r->runs[k].nrows; i++, at++) {
        col[0][at] = k;
        col[1][at] = r->event[k];
        for (j = 2; j < nc; j++) col[j][at] = r->runs[k].cols[j - 2][i];
      }
    ok = app_write_mat(a, path, nc, nm, (const double *const *)col, rows, 0, NULL, NULL);
  }
  if (col)
    for (j = 0; j < nc; j++) free(col[j]);
  free(col);
  free(nm);
  return ok;
}

bool app_export_many_csv(App *a, const char *path) {
  RunSet *r = &a->many;
  FILE *f;
  char num[40], p[2100], q[2200];
  int k, i, j;
  if (r->n == 0) {
    app_log(a, "error: there are no runs to export (Workspace > Many runs)");
    return false;
  }
  if (app_is_mat(path)) {
    if (!write_many_mat(a, path)) return false;
  } else {
    f = fopen(path, "w");
    if (!f) {
      app_log(a, "error: cannot write %s", path);
      return false;
    }
    fprintf(f, "run,event");
    for (j = 0; j < r->runs[0].ncols; j++) fprintf(f, ",%s", r->runs[0].names[j]);
    fputc('\n', f);
    for (k = 0; k < r->n; k++)
      for (i = 0; i < r->runs[k].nrows; i++) {
        fprintf(f, "%d,%d", k, r->event[k]);
        for (j = 0; j < r->runs[k].ncols; j++) {
          app_format_double(num, sizeof num, r->runs[k].cols[j][i]);
          fprintf(f, ",%s", num);
        }
        fputc('\n', f);
      }
    fclose(f);
  }
  if (echo_begin(a)) {
    echo_abs_path(path, p, sizeof p);
    echo_line(a, "many.%s(%s)", app_table_method(path), echo_str(p, q, sizeof q));
  }
  echo_end(a);
  app_log(a, "Exported %d runs to %s", r->n, path);
  return true;
}

/* ---- Formulas -------------------------------------------------------------------- */

typedef struct {
  const char *s;
  const double *x;
  int n;
  double *tmp;
  char err[160];
} Expr;

/* One token's text into tok; returns its kind: 'n' number, 'a' name, the
   character for an operator, 0 at the end. */
static int token(Expr *e, char *tok, size_t len, bool take) {
  const char *s = e->s;
  size_t k = 0;
  int kind;
  while (*s == ' ' || *s == '\t') s++;
  if (!*s) {
    kind = 0;
  } else if ((*s >= '0' && *s <= '9') || (*s == '.' && s[1] >= '0' && s[1] <= '9')) {
    char *end;
    strtod(s, &end);
    while (s < end && k + 1 < len) tok[k++] = *s++;
    s = end;
    kind = 'n';
  } else if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || *s == '_') {
    while (((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || *s == '_' ||
            (*s >= '0' && *s <= '9')) &&
           k + 1 < len)
      tok[k++] = *s++;
    kind = 'a';
  } else if (s[0] == '*' && s[1] == '*') {
    s += 2;
    kind = '^';
  } else {
    kind = *s++;
  }
  tok[k] = '\0';
  if (take) e->s = s;
  return kind;
}

static bool expr_sum(Expr *e, double *out);

static bool expr_atom(Expr *e, double *out) {
  char tok[64];
  int kind = token(e, tok, sizeof tok, true), i;
  if (kind == 0) {
    snprintf(e->err, sizeof e->err, "the formula ends too soon");
    return false;
  }
  if (kind == '(') {
    if (!expr_sum(e, out)) return false;
    if (token(e, tok, sizeof tok, true) != ')') {
      snprintf(e->err, sizeof e->err, "a ( is not closed");
      return false;
    }
    return true;
  }
  if (kind == 'n') {
    double v = strtod(tok, NULL);
    for (i = 0; i < e->n; i++) out[i] = v;
    return true;
  }
  if (kind == 'a') {
    static const char *names[] = {"sqrt", "exp", "log", "ln", "log10", "sin", "cos", "tan",
                                  "asin", "acos", "atan", "sinh", "cosh", "tanh", "abs", NULL};
    static double (*fns[])(double) = {sqrt, exp, log, log, log10, sin, cos, tan,
                                      asin, acos, atan, sinh, cosh, tanh, fabs};
    if (!strcmp(tok, "x")) {
      memcpy(out, e->x, sizeof(double) * (size_t)e->n);
      return true;
    }
    if (!strcmp(tok, "pi") || !strcmp(tok, "e")) {
      double v = tok[0] == 'p' ? 3.14159265358979323846 : 2.71828182845904523536;
      for (i = 0; i < e->n; i++) out[i] = v;
      return true;
    }
    for (i = 0; names[i]; i++)
      if (!strcmp(tok, names[i])) {
        int j;
        if (token(e, tok, sizeof tok, true) != '(') {
          snprintf(e->err, sizeof e->err, "%s needs (...)", names[i]);
          return false;
        }
        if (!expr_sum(e, out)) return false;
        if (token(e, tok, sizeof tok, true) != ')') {
          snprintf(e->err, sizeof e->err, "a ( is not closed");
          return false;
        }
        for (j = 0; j < e->n; j++) out[j] = fns[i](out[j]);
        return true;
      }
    snprintf(e->err, sizeof e->err, "unknown name '%s': the formula's variable is x", tok);
    return false;
  }
  snprintf(e->err, sizeof e->err, "unexpected '%c'", kind);
  return false;
}

static bool expr_unary(Expr *e, double *out);

static bool expr_power(Expr *e, double *out) {
  char tok[64];
  if (!expr_atom(e, out)) return false;
  if (token(e, tok, sizeof tok, false) == '^') {
    double *rhs = malloc(sizeof(double) * (size_t)(e->n + 1));
    int i;
    bool ok;
    token(e, tok, sizeof tok, true);
    if (!rhs) return false;
    ok = expr_unary(e, rhs);
    for (i = 0; ok && i < e->n; i++) out[i] = pow(out[i], rhs[i]);
    free(rhs);
    return ok;
  }
  return true;
}

static bool expr_unary(Expr *e, double *out) {
  char tok[64];
  int kind = token(e, tok, sizeof tok, false), i;
  if (kind == '-' || kind == '+') {
    token(e, tok, sizeof tok, true);
    if (!expr_unary(e, out)) return false;
    if (kind == '-')
      for (i = 0; i < e->n; i++) out[i] = -out[i];
    return true;
  }
  return expr_power(e, out);
}

static bool expr_term(Expr *e, double *out) {
  char tok[64];
  int kind, i;
  double *rhs;
  if (!expr_unary(e, out)) return false;
  rhs = malloc(sizeof(double) * (size_t)(e->n + 1));
  if (!rhs) return false;
  while ((kind = token(e, tok, sizeof tok, false)) == '*' || kind == '/') {
    token(e, tok, sizeof tok, true);
    if (!expr_unary(e, rhs)) {
      free(rhs);
      return false;
    }
    for (i = 0; i < e->n; i++) out[i] = kind == '*' ? out[i] * rhs[i] : out[i] / rhs[i];
  }
  free(rhs);
  return true;
}

static bool expr_sum(Expr *e, double *out) {
  char tok[64];
  int kind, i;
  double *rhs;
  if (!expr_term(e, out)) return false;
  rhs = malloc(sizeof(double) * (size_t)(e->n + 1));
  if (!rhs) return false;
  while ((kind = token(e, tok, sizeof tok, false)) == '+' || kind == '-') {
    token(e, tok, sizeof tok, true);
    if (!expr_term(e, rhs)) {
      free(rhs);
      return false;
    }
    for (i = 0; i < e->n; i++) out[i] = kind == '+' ? out[i] + rhs[i] : out[i] - rhs[i];
  }
  free(rhs);
  return true;
}

bool app_eval_expression(const char *expr, const double *x, double *y, int n, char *err,
                         size_t errlen) {
  Expr e;
  char tok[64];
  memset(&e, 0, sizeof e);
  e.s = expr;
  e.x = x;
  e.n = n;
  if (!expr_sum(&e, y)) {
    snprintf(err, errlen, "%s", e.err);
    return false;
  }
  if (token(&e, tok, sizeof tok, false) != 0) {
    snprintf(err, errlen, "unexpected '%s'", tok[0] ? tok : "?");
    return false;
  }
  if (errlen) err[0] = '\0';
  return true;
}

/* ---- Diagram quantities ------------------------------------------------------------- */

typedef struct {
  double re, im;
} Cx;

static int cmp_eig(const void *p, const void *q) {
  const Cx *a = p, *b = q;
  if (a->re != b->re) return a->re > b->re ? -1 : 1;
  if (a->im != b->im) return a->im > b->im ? -1 : 1;
  return 0;
}

static int cmp_mult(const void *p, const void *q) {
  const Cx *a = p, *b = q;
  double ma = hypot(a->re, a->im), mb = hypot(b->re, b->im);
  if (ma != mb) return ma > mb ? -1 : 1;
  return cmp_eig(p, q);
}

static int find_col(App *a, const char *name) {
  int j;
  for (j = 0; j < a->ncols; j++)
    if (SDL_strcasecmp(a->cols[j], name) == 0) return j;
  return -1;
}

double app_auto_quantity(App *a, int i, const char *name, int which, const double *along) {
  const xppe_auto_point_info *p;
  xpp_engine_api *e = &a->eng->api;
  char low[APP_NAME_LEN];
  int k, kk, v;
  if (i < 0 || i >= a->au.count) return NAN;
  p = &a->au.info[i];
  if (!SDL_strcasecmp(name, "period")) return p->period;
  if (!SDL_strcasecmp(name, "norm")) return p->norm;
  {
    const char *n1 = e->xppe_auto_parameter_name(p->par1_slot);
    const char *n2 = p->par2_slot >= 0 ? e->xppe_auto_parameter_name(p->par2_slot) : NULL;
    if (n1 && !SDL_strcasecmp(name, n1)) return p->par1;
    if (n2 && n2[0] && !SDL_strcasecmp(name, n2)) return p->two_parameter ? p->par2 : NAN;
  }
  for (k = 0; name[k] && k + 1 < (int)sizeof low; k++)
    low[k] = (char)((name[k] >= 'A' && name[k] <= 'Z') ? name[k] - 'A' + 'a' : name[k]);
  low[k] = '\0';
  if ((low[0] == 'r' && low[1] == 'e' && low[2] >= '1' && low[2] <= '9') ||
      (low[0] == 'i' && low[1] == 'm' && low[2] >= '1' && low[2] <= '9')) {
    char *end;
    double re[64], im[64];
    Cx ev[64];
    int n = a->nvars > 64 ? 64 : a->nvars;
    kk = (int)strtol(low + 2, &end, 10) - 1;
    if (*end || p->periodic || kk < 0 || kk >= n) return NAN;
    e->xppe_auto_point_data(i, XPPE_AUTO_EIG_RE, re, n);
    e->xppe_auto_point_data(i, XPPE_AUTO_EIG_IM, im, n);
    for (k = 0; k < n; k++) {
      /* the stored exp(eigenvalue): log|mu| + i arg(mu) */
      ev[k].re = log(hypot(re[k], im[k]));
      ev[k].im = atan2(im[k], re[k]);
    }
    qsort(ev, (size_t)n, sizeof(Cx), cmp_eig);
    return low[0] == 'r' ? ev[kk].re : ev[kk].im;
  }
  if (low[0] == 'm' && low[1] == 'u' && low[2] >= '1' && low[2] <= '9') {
    char *end;
    double re[64], im[64];
    Cx mu[64];
    int n = a->nvars > 64 ? 64 : a->nvars;
    kk = (int)strtol(low + 2, &end, 10) - 1;
    if (!p->periodic || kk < 0 || kk >= n || *end != '_') return NAN;
    if (abs(p->point) == 1) return NAN; /* the Hopf point: no multipliers */
    e->xppe_auto_point_data(i, XPPE_AUTO_EIG_RE, re, n);
    e->xppe_auto_point_data(i, XPPE_AUTO_EIG_IM, im, n);
    for (k = 0; k < n; k++) {
      mu[k].re = re[k];
      mu[k].im = im[k];
    }
    qsort(mu, (size_t)n, sizeof(Cx), cmp_mult);
    if (!strcmp(end, "_re")) return mu[kk].re;
    if (!strcmp(end, "_im")) return mu[kk].im;
    if (!strcmp(end, "_abs")) return hypot(mu[kk].re, mu[kk].im);
    return NAN;
  }
  if (along) return along[i];
  v = app_find_var(a, name);
  if (v < 0) return NAN;
  return which ? a->au.umin[(size_t)i * a->nvars + v] : a->au.umax[(size_t)i * a->nvars + v];
}

/* Set the model's parameters to those of diagram point i. */
static void point_parameters(App *a, int i) {
  xpp_engine_api *e = &a->eng->api;
  int n = e->xppe_auto_parameter_count(), s;
  double vals[16];
  if (n > 16) n = 16;
  e->xppe_auto_point_data(i, XPPE_AUTO_PARAMS, vals, n);
  for (s = 0; s < n; s++) {
    const char *nm = e->xppe_auto_parameter_name(s);
    if (nm && nm[0] && app_find_par(a, nm) >= 0) e->xppe_set_number(nm, vals[s]);
  }
}

bool app_auto_along(App *a, const char *column, double *lo, double *hi) {
  xpp_engine_api *e = &a->eng->api;
  int col = find_col(a, column), ncols, i, j, rows;
  double *par, *row, *u0, *buf = NULL;
  if (col < 0 || !a->model_ok) return false;
  ncols = e->xppe_count(XPPE_COLUMNS);
  par = malloc(sizeof(double) * (size_t)(a->npars + 1));
  row = malloc(sizeof(double) * (size_t)(ncols + 1));
  u0 = malloc(sizeof(double) * (size_t)(a->nvars + 1));
  if (!par || !row || !u0) {
    free(par);
    free(row);
    free(u0);
    return false;
  }
  for (i = 0; i < a->npars; i++) e->xppe_get(a->pars[i], &par[i]);
  for (i = 0; i < a->au.count; i++) {
    const xppe_auto_point_info *p = &a->au.info[i];
    lo[i] = hi[i] = NAN;
    if (p->two_parameter || (p->periodic && !p->label)) continue;
    point_parameters(a, i);
    if (!p->periodic) {
      e->xppe_auto_point_data(i, XPPE_AUTO_U0, u0, a->nvars);
      if (e->xppe_columns_at(0.0, u0, row, ncols) > 0)
        lo[i] = hi[i] = row[a->time_missing ? col - 1 : col];
      continue;
    }
    if (e->xppe_auto_orbit(p->label) != XPPE_OK) continue;
    rows = e->xppe_rows();
    free(buf);
    buf = malloc(sizeof(double) * (size_t)(rows + 1));
    if (!buf) break;
    e->xppe_column(a->time_missing ? col - 1 : col, buf, rows);
    for (j = 0; j < rows; j++) {
      if (j == 0 || buf[j] < lo[i]) lo[i] = buf[j];
      if (j == 0 || buf[j] > hi[i]) hi[i] = buf[j];
    }
  }
  for (i = 0; i < a->npars; i++) e->xppe_set_number(a->pars[i], par[i]);
  free(buf);
  free(par);
  free(row);
  free(u0);
  return true;
}

/* ---- The figure ----------------------------------------------------------------------- */

static const char *line_names[] = {"none", "-", "--", ":", "-."};
static const char *marker_names[] = {"None", "o", "s", "^", "D", "x", "+", "*", "."};
static const char *legend_names[] = {"none", "best", "upper right", "upper left",
                                     "lower left", "lower right", "outside"};

void app_fig_reset(App *a) {
  FigUI *g = &a->fg;
  if (g->fig.npanels) fig_free(&g->fig);
  fig_init(&g->fig);
  g->shadow = g->fig;
  g->echoed = false;
  g->panel = g->series = 0;
  g->dirty = true;
}

/* A float setting as the shortest decimal that is the same float. */
static const char *fnum(float v, char *buf, size_t len) {
  char t[40];
  int p;
  for (p = 1; p < 10; p++) {
    snprintf(t, sizeof t, "%.*g", p, (double)v);
    if ((float)strtod(t, NULL) == v) break;
  }
  return echo_num(strtod(t, NULL), buf, len);
}

static void color_hex(const float c[3], char *out) {
  snprintf(out, 8, "#%02x%02x%02x", (int)(c[0] * 255 + 0.5f), (int)(c[1] * 255 + 0.5f),
           (int)(c[2] * 255 + 0.5f));
}

/* The figure's Python, once: "fig = xs.Figure(...)". */
static void echo_new_figure(App *a) {
  FigUI *g = &a->fg;
  char w[64], h[64], fs[64];
  if (g->echoed) return;
  echo_line(a, "fig = xs.Figure(%s, %s, rows=%d, cols=%d%s, fontsize=%s)",
            fnum(g->fig.width_mm, w, sizeof w), fnum(g->fig.height_mm, h, sizeof h),
            g->fig.rows, g->fig.cols, g->fig.font == FIG_FONT_SERIF ? ", font=\"serif\"" : "",
            fnum(g->fig.font_pt, fs, sizeof fs));
  g->echoed = true;
  a->echo.plt = true;
}

/* The series' look as keyword arguments, only where it differs from ref. */
static void style_args(const FigSeries *s, const FigSeries *ref, char *out, size_t len) {
  size_t k = 0;
  char b1[300], b2[64], b3[64];
  out[0] = '\0';
#define ADD(...) k += (size_t)snprintf(out + k, k < len ? len - k : 0, __VA_ARGS__)
  if (strcmp(s->label, ref->label)) ADD(", label=%s", echo_str(s->label, b1, sizeof b1));
  if (memcmp(s->color, ref->color, sizeof s->color)) {
    color_hex(s->color, b2);
    ADD(", color=\"%s\"", b2);
  }
  if (s->line != ref->line) ADD(", line=\"%s\"", line_names[s->line]);
  if (s->width != ref->width) ADD(", width=%s", fnum(s->width, b2, sizeof b2));
  if (s->marker != ref->marker)
    ADD(s->marker ? ", marker=\"%s\"" : ", marker=%s", marker_names[s->marker]);
  if (s->msize != ref->msize) ADD(", markersize=%s", fnum(s->msize, b2, sizeof b2));
  if (s->mfill != ref->mfill) ADD(", markerfill=%s", s->mfill ? "True" : "False");
  /* the markers' colour follows the line's unless set apart */
  if (memcmp(s->mcolor, ref->mcolor, sizeof s->mcolor) &&
      (memcmp(s->mcolor, s->color, sizeof s->color) ||
       memcmp(ref->mcolor, ref->color, sizeof ref->color))) {
    color_hex(s->mcolor, b2);
    ADD(", markercolor=\"%s\"", b2);
  }
  if (s->every != ref->every) ADD(", every=%d", s->every);
  if (s->sx != ref->sx || s->sy != ref->sy)
    ADD(", scale=(%s, %s)", echo_num(s->sx, b2, sizeof b2), echo_num(s->sy, b3, sizeof b3));
#undef ADD
}

/* What a new series looks like in Python before any keyword. */
static void python_defaults(const FigPanel *p, int index, FigSeries *d) {
  memset(d, 0, sizeof *d);
  fig_color(index, d->color);
  (void)p;
  /* markercolor None means the line's colour */
  memcpy(d->mcolor, d->color, sizeof d->color);
  d->line = FIG_LINE_SOLID;
  d->width = 1.5f;
  d->msize = 6;
  d->mfill = true;
  d->every = 1;
  d->sx = d->sy = 1;
}

/* Add a series, write its line ("fig[P].CALL, style)") and remember it. */
static FigSeries *add_series(App *a, int panel, const double *x, const double *y, int n,
                             const char *call) {
  FigUI *g = &a->fg;
  FigPanel *p;
  FigSeries *s;
  if (panel < 0 || panel >= g->fig.npanels) panel = 0;
  p = &g->fig.p[panel];
  s = fig_add(p, x, y, n);
  if (!s) {
    app_log(a, "error: the panel holds %d series at most.", FIG_MAX_SERIES);
    return NULL;
  }
  s->py = SDL_strdup(call);
  g->panel = panel;
  g->series = p->nseries - 1;
  g->dirty = true;
  a->show_figure = true;
  return s;
}

/* After the caller set the new series' look: its line, and the shadow. */
static void echo_series(App *a, int panel, const char *call) {
  FigUI *g = &a->fg;
  FigPanel *p = &g->fig.p[panel];
  FigSeries *s = &p->s[p->nseries - 1], d;
  char style[1200];
  bool outer = echo_begin(a);
  if (outer) {
    echo_new_figure(a);
    python_defaults(p, p->nseries - 1, &d);
    style_args(s, &d, style, sizeof style);
    echo_line(a, "fig[%d].%s%s)", panel, call, style);
  }
  echo_end(a);
  g->shadow.p[panel].s[p->nseries - 1] = *s;
  g->shadow.p[panel].nseries = p->nseries;
}

bool app_fig_new(App *a, float width_mm, float height_mm, int rows, int cols) {
  FigUI *g = &a->fg;
  bool outer;
  if (rows < 1 || cols < 1 || rows * cols > FIG_MAX_PANELS) return false;
  fig_free(&g->fig);
  fig_init(&g->fig);
  if (width_mm > 5) g->fig.width_mm = width_mm;
  if (height_mm > 5) g->fig.height_mm = height_mm;
  fig_set_grid(&g->fig, rows, cols);
  g->shadow = g->fig;
  g->echoed = false;
  g->panel = g->series = 0;
  g->dirty = true;
  a->show_figure = true;
  outer = echo_begin(a);
  if (outer) echo_new_figure(a);
  echo_end(a);
  return true;
}

bool app_fig_grid(App *a, int rows, int cols) {
  FigUI *g = &a->fg;
  bool outer;
  if (rows < 1 || cols < 1 || rows * cols > FIG_MAX_PANELS) return false;
  if (rows * cols + (g->fig.npanels - g->fig.rows * g->fig.cols) > FIG_MAX_PANELS) return false;
  app_fig_sync_echo(a);
  fig_set_grid(&g->fig, rows, cols);
  g->shadow = g->fig;
  if (g->panel >= g->fig.npanels) g->panel = 0;
  g->dirty = true;
  outer = echo_begin(a);
  if (outer) {
    echo_new_figure(a);
    echo_line(a, "fig.grid(%d, %d)", rows, cols);
  }
  echo_end(a);
  return true;
}

int app_fig_inset(App *a, int parent) {
  FigUI *g = &a->fg;
  FigPanel *p;
  bool outer;
  if (g->fig.npanels >= FIG_MAX_PANELS || parent < 0 || parent >= g->fig.rows * g->fig.cols)
    return -1;
  app_fig_sync_echo(a);
  p = &g->fig.p[g->fig.npanels];
  fig_panel_init(p);
  p->inset = true;
  p->parent = parent;
  p->legend = FIG_LEGEND_NONE;
  g->fig.npanels++;
  g->shadow.p[g->fig.npanels - 1] = *p;
  g->shadow.npanels = g->fig.npanels;
  g->panel = g->fig.npanels - 1;
  g->dirty = true;
  outer = echo_begin(a);
  if (outer) {
    echo_new_figure(a);
    echo_line(a, "fig.inset(%d, (0.55, 0.55, 0.4, 0.4))", parent);
  }
  echo_end(a);
  return g->fig.npanels - 1;
}

void app_fig_remove(App *a, int panel, int k) {
  FigUI *g = &a->fg;
  bool outer;
  if (panel < 0 || panel >= g->fig.npanels || k < 0 || k >= g->fig.p[panel].nseries) return;
  app_fig_sync_echo(a);
  fig_remove(&g->fig.p[panel], k);
  memmove(&g->shadow.p[panel].s[k], &g->shadow.p[panel].s[k + 1],
          sizeof(FigSeries) * (size_t)(g->shadow.p[panel].nseries - k - 1));
  g->shadow.p[panel].nseries--;
  if (g->series >= g->fig.p[panel].nseries) g->series = g->fig.p[panel].nseries - 1;
  g->dirty = true;
  outer = echo_begin(a);
  if (outer) echo_line(a, "fig[%d].remove(%d)", panel, k);
  echo_end(a);
}

static int col_index(App *a, int c) { return c; } /* a->traj columns are a->cols */

bool app_fig_add_run(App *a, int panel, int xcol, int ycol) {
  char call[600], xs[160], ys[160];
  FigSeries *s;
  if (!a->have_traj || xcol < 0 || ycol < 0 || xcol >= a->traj.ncols || ycol >= a->traj.ncols)
    return false;
  snprintf(call, sizeof call, "run(sol, %s, %s", echo_str(a->cols[xcol], xs, sizeof xs),
           echo_str(a->cols[ycol], ys, sizeof ys));
  s = add_series(a, panel, a->traj.cols[col_index(a, xcol)], a->traj.cols[col_index(a, ycol)],
                 a->traj.nrows, call);
  if (!s) return false;
  snprintf(s->label, FIG_TEXT, "%s", a->cols[ycol]);
  echo_series(a, a->fg.panel, call);
  return true;
}

bool app_fig_add_runs(App *a, int panel, int xcol, int ycol, int event) {
  RunSet *r = &a->many;
  char call[600], xs[160], ys[160];
  double *x, *y;
  int i, j, n = 0, total = 0;
  FigSeries *s;
  if (r->n == 0 || xcol < 0 || ycol < 0 || xcol >= a->ncols || ycol >= a->ncols) return false;
  for (i = 0; i < r->n; i++)
    if (event < 0 || r->event[i] == event) total += r->runs[i].nrows + 1;
  if (total == 0) return false;
  x = malloc(sizeof(double) * (size_t)total);
  y = malloc(sizeof(double) * (size_t)total);
  if (!x || !y) {
    free(x);
    free(y);
    return false;
  }
  for (i = 0; i < r->n; i++) {
    if (event >= 0 && r->event[i] != event) continue;
    if (n > 0) {
      x[n] = y[n] = NAN;
      n++;
    }
    for (j = 0; j < r->runs[i].nrows; j++, n++) {
      x[n] = r->runs[i].cols[xcol][j];
      y[n] = r->runs[i].cols[ycol][j];
    }
  }
  if (event >= 0)
    snprintf(call, sizeof call, "runs(many, %s, %s, event=%d", echo_str(a->cols[xcol], xs, sizeof xs),
             echo_str(a->cols[ycol], ys, sizeof ys), event);
  else
    snprintf(call, sizeof call, "runs(many, %s, %s", echo_str(a->cols[xcol], xs, sizeof xs),
             echo_str(a->cols[ycol], ys, sizeof ys));
  s = add_series(a, panel, x, y, n, call);
  free(x);
  free(y);
  if (!s) return false;
  s->width = 0.8f;
  if (event == 0) snprintf(s->label, FIG_TEXT, "ran to the end");
  else if (event > 0) {
    int sign = 0;
    const char *c = a->eng->api.xppe_flag_condition(event - 1, &sign);
    snprintf(s->label, FIG_TEXT, "stopped: %s", c ? c : "?");
    /* drop the trailing space XPPAUT keeps */
    while (s->label[0] && s->label[strlen(s->label) - 1] == ' ') s->label[strlen(s->label) - 1] = '\0';
  }
  echo_series(a, a->fg.panel, call);
  return true;
}

static bool is_aux(App *a, const char *name) {
  int c = find_col(a, name);
  return c >= 0 && app_find_var(a, name) < 0 && SDL_strcasecmp(name, "T") != 0;
}

bool app_fig_add_diagram(App *a, int panel, const FigDiagram *d) {
  int n = a->au.count, i, k = 0, lo = 0, hi = n - 1;
  double *x, *y, *along = NULL, *along_lo = NULL, *wv = NULL;
  char call[1200], nm[160], xn[160];
  size_t c = 0;
  bool aux = is_aux(a, d->y), waux = d->where[0] && is_aux(a, d->where);
  FigSeries *s;
  if (n == 0) {
    app_log(a, "error: compute a bifurcation diagram first.");
    return false;
  }
  if (d->lab1 && d->lab2) {
    int i1 = -1, i2 = -1;
    for (i = 0; i < n; i++) {
      if (a->au.info[i].label == d->lab1 && i1 < 0) i1 = i;
      if (a->au.info[i].label == d->lab2 && i2 < 0) i2 = i;
    }
    if (i1 < 0 || i2 < 0) {
      app_log(a, "error: labels %d and %d are not both in the diagram.", d->lab1, d->lab2);
      return false;
    }
    lo = i1 < i2 ? i1 : i2;
    hi = i1 < i2 ? i2 : i1;
  }
  x = malloc(sizeof(double) * (size_t)(3 * n + 2));
  y = malloc(sizeof(double) * (size_t)(3 * n + 2));
  if (aux) {
    along_lo = malloc(sizeof(double) * (size_t)(n + 1));
    along = malloc(sizeof(double) * (size_t)(n + 1));
  }
  if (!x || !y || (aux && (!along || !along_lo))) {
    free(x);
    free(y);
    free(along);
    free(along_lo);
    return false;
  }
  if (d->where[0]) {
    wv = malloc(sizeof(double) * (size_t)(2 * n + 2));
    if (!wv) {
      free(x);
      free(y);
      free(along);
      free(along_lo);
      return false;
    }
    if (waux) {
      app_auto_along(a, d->where, wv, wv + n);
      if (!d->where_which) memmove(wv, wv + n, sizeof(double) * (size_t)n);
    } else {
      /* a variable, a parameter, the period, an eigenvalue, ... */
      for (i = 0; i < n; i++) wv[i] = app_auto_quantity(a, i, d->where, d->where_which, NULL);
    }
  }
  if (aux) {
    app_auto_along(a, d->y, along_lo, along);
    if (d->which)
      for (i = 0; i < n; i++) along[i] = along_lo[i];
    /* The orbits replaced the engine's run, as Auto.along() does in the
       script; the run shown stays the script's sol. */
  }
  {
    /* as diagram_xy() in xpplab.figure */
#define KEEP(j)                                                                         \
  ((j) >= lo && (j) <= hi && (!wv || (wv[j] >= d->where_lo && wv[j] <= d->where_hi)) && \
   (d->stable < 0 || a->au.info[j].stable == d->stable) &&                              \
   (d->periodic < 0 || a->au.info[j].periodic == d->periodic) &&                        \
   (d->any_branch || a->au.info[j].branch == d->branch))
#define FOLLOWS(j)                                                                      \
  ((j) > 0 && a->au.info[j].branch == a->au.info[(j) - 1].branch &&                     \
   a->au.info[j].periodic == a->au.info[(j) - 1].periodic &&                            \
   abs(a->au.info[j].point) == abs(a->au.info[(j) - 1].point) + 1)
#define XV(j) (d->x[0] ? app_auto_quantity(a, j, d->x, d->which, NULL) : a->au.info[j].par1)
#define YV(j) app_auto_quantity(a, j, d->y, d->which, along)
    for (i = 0; i < n; i++) {
      bool starts;
      if (!KEEP(i)) continue;
      starts = !(i > 0 && KEEP(i - 1) && FOLLOWS(i));
      if (starts) {
        if (k > 0) {
          x[k] = y[k] = NAN;
          k++;
        }
        if (i > 0 && FOLLOWS(i) && lo <= i - 1) {
          x[k] = XV(i - 1);
          y[k] = YV(i - 1);
          k++;
        }
      }
      x[k] = XV(i);
      y[k] = YV(i);
      k++;
    }
#undef KEEP
#undef FOLLOWS
#undef XV
#undef YV
  }
#define ADD(...) c += (size_t)snprintf(call + c, c < sizeof call ? sizeof call - c : 0, __VA_ARGS__)
  ADD("diagram(m.auto.diagram, %s", echo_str(d->y, nm, sizeof nm));
  if (d->x[0]) ADD(", x=%s", echo_str(d->x, xn, sizeof xn));
  if (d->which) ADD(", which=\"min\"");
  if (d->stable >= 0) ADD(", stable=%s", d->stable ? "True" : "False");
  if (d->periodic >= 0) ADD(", periodic=%s", d->periodic ? "True" : "False");
  if (!d->any_branch) ADD(", branch=%d", d->branch);
  if (d->lab1 && d->lab2) ADD(", between=(%d, %d)", d->lab1, d->lab2);
  if (d->where[0]) {
    char wn[160], l1[64], h1[64];
    ADD(", where=(%s, \"%s\", %s, %s)", echo_str(d->where, wn, sizeof wn),
        d->where_which ? "min" : "max", echo_num(d->where_lo, l1, sizeof l1),
        echo_num(d->where_hi, h1, sizeof h1));
  }
  if (aux || waux) ADD(", model=m");
#undef ADD
  s = add_series(a, panel, x, y, k, call);
  free(x);
  free(y);
  free(along);
  free(along_lo);
  free(wv);
  if (!s) return false;
  snprintf(s->label, FIG_TEXT, "%s", d->y);
  if (d->stable == 0) s->line = FIG_LINE_DASHED;
  if (d->stable >= 0) {
    /* XPPAUT's colours: steady red/black, orbits green/blue */
    unsigned col = d->periodic == 1 ? (d->stable == 0 ? 0x1f77b4 : 0x2ca02c)
                                    : (d->stable == 0 ? 0x000000 : 0xd62728);
    s->color[0] = ((col >> 16) & 255) / 255.0f;
    s->color[1] = ((col >> 8) & 255) / 255.0f;
    s->color[2] = (col & 255) / 255.0f;
    memcpy(s->mcolor, s->color, sizeof s->color);
  }
  echo_series(a, a->fg.panel, call);
  return true;
}

bool app_fig_add_sweep(App *a, int panel, bool min) {
  SweepUI *w = &a->sw;
  char call[200];
  FigSeries *s;
  if (w->n <= 0 || w->result_mode != SWEEP_MAXMIN || w->result_kind != 0 ||
      a->echo.sweep_mode != SWEEP_MAXMIN) {
    app_log(a, "error: run a Range that records the max and min of a variable first.");
    return false;
  }
  snprintf(call, sizeof call, "line(sweep_values, [r[%d] for r in sweep]", min ? 1 : 0);
  s = add_series(a, panel, w->x, min ? w->ymin : w->ymax, w->n, call);
  if (!s) return false;
  s->line = FIG_LINE_NONE;
  s->marker = FIG_MARK_POINT;
  snprintf(s->label, FIG_TEXT, "%s %s (Range)", a->echo.sweep_var, min ? "min" : "max");
  echo_series(a, a->fg.panel, call);
  return true;
}

bool app_fig_add_function(App *a, int panel, const char *expr, double lo, double hi, int n) {
  double *x, *y;
  char err[200], call[700], ex[400], l[64], h[64];
  int i;
  bool ok;
  FigSeries *s;
  if (n < 2) n = 400;
  x = malloc(sizeof(double) * (size_t)n);
  y = malloc(sizeof(double) * (size_t)n);
  if (!x || !y) {
    free(x);
    free(y);
    return false;
  }
  /* as numpy.linspace */
  for (i = 0; i < n; i++) x[i] = lo + (hi - lo) * i / (n - 1);
  x[n - 1] = hi;
  ok = app_eval_expression(expr, x, y, n, err, sizeof err);
  if (!ok) {
    app_log(a, "error: %s", err);
    free(x);
    free(y);
    return false;
  }
  snprintf(call, sizeof call, "function(%s, (%s, %s)%s", echo_str(expr, ex, sizeof ex),
           echo_num(lo, l, sizeof l), echo_num(hi, h, sizeof h), n == 400 ? "" : ", n=");
  if (n != 400) snprintf(call + strlen(call), sizeof call - strlen(call), "%d", n);
  s = add_series(a, panel, x, y, n, call);
  free(x);
  free(y);
  if (!s) return false;
  snprintf(s->label, FIG_TEXT, "$%s$", expr);
  {
    /* plain text when the formula is not mathtext */
    char msg[160];
    if (!fig_text_check(s->label, msg, sizeof msg)) snprintf(s->label, FIG_TEXT, "%s", expr);
  }
  echo_series(a, a->fg.panel, call);
  return true;
}

bool app_fig_add_curve(App *a, int panel, const char *yname, int xvar, double lo, double hi,
                       int n) {
  xpp_engine_api *e = &a->eng->api;
  double *x, *y, *state, *row, *f;
  int i, nv = a->nvars, ncols, deriv = -1, col = -1;
  char call[700], yn[160], xn[160], l[64], h[64];
  size_t len = strlen(yname);
  bool ok = true;
  FigSeries *s;
  if (!a->model_ok || xvar < 0 || xvar >= nv) return false;
  if (n < 2) n = 400;
  if (len > 1 && yname[len - 1] == '\'') {
    char v[APP_NAME_LEN];
    snprintf(v, sizeof v, "%.*s", (int)(len - 1), yname);
    deriv = app_find_var(a, v);
    if (deriv < 0) {
      app_log(a, "error: %s is not a variable.", v);
      return false;
    }
  } else {
    col = find_col(a, yname);
    if (col < 0) {
      app_log(a, "error: no column %s.", yname);
      return false;
    }
  }
  ncols = e->xppe_count(XPPE_COLUMNS);
  x = malloc(sizeof(double) * (size_t)n);
  y = malloc(sizeof(double) * (size_t)n);
  state = malloc(sizeof(double) * (size_t)(nv + 1));
  row = malloc(sizeof(double) * (size_t)(ncols + 1));
  f = malloc(sizeof(double) * (size_t)(nv + 1));
  if (!x || !y || !state || !row || !f) ok = false;
  for (i = 0; ok && i < nv; i++) e->xppe_get(a->vars[i], &state[i]);
  for (i = 0; ok && i < n; i++) {
    x[i] = i == n - 1 ? hi : lo + (hi - lo) * i / (n - 1);
    state[xvar] = x[i];
    if (deriv >= 0) {
      if (e->xppe_rhs(0.0, state, f) != XPPE_OK) ok = false;
      y[i] = f[deriv];
    } else {
      if (e->xppe_columns_at(0.0, state, row, ncols) < 0) ok = false;
      y[i] = row[a->time_missing ? col - 1 : col];
    }
  }
  s = NULL;
  if (ok) {
    snprintf(call, sizeof call, "curve(m, %s, %s, (%s, %s)", echo_str(yname, yn, sizeof yn),
             echo_str(a->vars[xvar], xn, sizeof xn), echo_num(lo, l, sizeof l),
             echo_num(hi, h, sizeof h));
    if (n != 400) snprintf(call + strlen(call), sizeof call - strlen(call), ", n=%d", n);
    s = add_series(a, panel, x, y, n, call);
  }
  free(x);
  free(y);
  free(state);
  free(row);
  free(f);
  if (!s) return false;
  snprintf(s->label, FIG_TEXT, "%s", yname);
  echo_series(a, a->fg.panel, call);
  return true;
}

bool app_fig_add_equilibria(App *a, int panel, int xvar, int yvar) {
  double x[APP_MAX_EQUILIBRIA], y[APP_MAX_EQUILIBRIA];
  char call[3000], b[64];
  size_t c = 0;
  int i;
  FigSeries *s;
  if (a->neq == 0 || xvar < 0 || yvar < 0 || xvar >= a->nvars || yvar >= a->nvars) {
    app_log(a, "error: find equilibria first (Equilibrium in the phase plane).");
    return false;
  }
  for (i = 0; i < a->neq; i++) {
    x[i] = a->eq[i].x[xvar];
    y[i] = a->eq[i].x[yvar];
  }
  /* the points themselves: the script has no list of them */
  c += (size_t)snprintf(call + c, sizeof call - c, "line([");
  for (i = 0; i < a->neq; i++)
    c += (size_t)snprintf(call + c, sizeof call - c, "%s%s", i ? ", " : "", echo_num(x[i], b, sizeof b));
  c += (size_t)snprintf(call + c, sizeof call - c, "], [");
  for (i = 0; i < a->neq; i++)
    c += (size_t)snprintf(call + c, sizeof call - c, "%s%s", i ? ", " : "", echo_num(y[i], b, sizeof b));
  snprintf(call + c, sizeof call - c, "]");
  s = add_series(a, panel, x, y, a->neq, call);
  if (!s) return false;
  s->line = FIG_LINE_NONE;
  s->marker = FIG_MARK_X;
  s->msize = 6;
  s->color[0] = s->color[1] = s->color[2] = 0;
  memcpy(s->mcolor, s->color, sizeof s->color);
  snprintf(s->label, FIG_TEXT, "equilibria");
  echo_series(a, a->fg.panel, call);
  return true;
}

/* ---- The plots of the other windows ----------------------------------------------------- */

static int new_panel_for_plot(App *a) {
  FigUI *g = &a->fg;
  int k;
  /* the first empty grid panel, else a new figure of one */
  for (k = 0; k < g->fig.rows * g->fig.cols; k++)
    if (g->fig.p[k].nseries == 0) return k;
  app_fig_new(a, g->fig.width_mm, g->fig.height_mm, 1, 1);
  return 0;
}

bool app_fig_from_plot(App *a, int figure) {
  FigUI *g = &a->fg;
  int panel, j;
  bool any = false;
  FigPanel *p;
  if (!a->model_ok) return false;
  panel = new_panel_for_plot(a);
  p = &g->fig.p[panel];
  if (figure == FIG_TIME) {
    if (!a->have_traj) return false;
    for (j = 1; j < a->ncols; j++)
      if (a->plot_col[j]) any |= app_fig_add_run(a, panel, 0, j);
    snprintf(p->xlabel, FIG_TEXT, "$t$");
  } else if (figure == FIG_PHASE) {
    int cx = -1, cy = -1;
    for (j = 0; j < a->ncols; j++) {
      if (!SDL_strcasecmp(a->cols[j], a->vars[a->pp.xvar])) cx = j;
      if (!SDL_strcasecmp(a->cols[j], a->vars[a->pp.yvar])) cy = j;
    }
    if (a->have_traj && cx >= 0 && cy >= 0) any |= app_fig_add_run(a, panel, cx, cy);
    if (a->many.n > 0 && a->many.show && cx >= 0 && cy >= 0) {
      int e, seen[64] = {0};
      for (j = 0; j < a->many.n; j++)
        if (a->many.event[j] >= 0 && a->many.event[j] < 64) seen[a->many.event[j]] = 1;
      for (e = 0; e < 64; e++)
        if (seen[e]) any |= app_fig_add_runs(a, panel, cx, cy, e);
    }
    if (a->neq > 0) any |= app_fig_add_equilibria(a, panel, a->pp.xvar, a->pp.yvar);
    snprintf(p->xlabel, FIG_TEXT, "%s", a->vars[a->pp.xvar]);
    snprintf(p->ylabel, FIG_TEXT, "%s", a->vars[a->pp.yvar]);
  } else if (figure == FIG_AUTO) {
    FigDiagram d;
    const char *y = a->au.yvar >= 0 && a->au.yvar < a->nvars ? a->vars[a->au.yvar] : "norm";
    int st, per, w;
    memset(&d, 0, sizeof d);
    snprintf(d.y, sizeof d.y, "%s", y);
    d.any_branch = true;
    for (per = 0; per <= 1; per++)
      for (st = 1; st >= 0; st--)
        for (w = 0; w <= (per && a->au.yvar >= 0 ? 1 : 0); w++) {
          bool have = false;
          int i;
          for (i = 0; i < a->au.count; i++)
            if (!a->au.info[i].two_parameter && a->au.info[i].periodic == per &&
                a->au.info[i].stable == st)
              have = true;
          if (!have) continue;
          d.stable = st;
          d.periodic = per;
          d.which = w;
          if (app_fig_add_diagram(a, panel, &d)) {
            FigSeries *s = &p->s[p->nseries - 1];
            static const char *names[2][2] = {{"unstable steady states", "stable steady states"},
                                              {"unstable orbits", "stable orbits"}};
            snprintf(s->label, FIG_TEXT, "%s", w ? "" : names[per][st]);
            any = true;
          }
        }
    if (a->au.count > 0) {
      const char *pn = a->eng->api.xppe_auto_parameter_name(a->au.info[0].par1_slot);
      snprintf(p->xlabel, FIG_TEXT, "%s", pn ? pn : "");
    }
    snprintf(p->ylabel, FIG_TEXT, "%s", y);
  } else if (figure == FIG_AUTO2) {
    FigDiagram d;
    int i;
    const char *n2 = NULL;
    memset(&d, 0, sizeof d);
    d.any_branch = true;
    d.stable = d.periodic = -1;
    for (i = 0; i < a->au.count; i++)
      if (a->au.info[i].two_parameter) {
        n2 = a->eng->api.xppe_auto_parameter_name(a->au.info[i].par2_slot);
        break;
      }
    if (!n2) return false;
    snprintf(d.y, sizeof d.y, "%s", n2);
    any = app_fig_add_diagram(a, panel, &d);
  }
  g->dirty = true;
  app_fig_sync_echo(a);
  if (any) app_status(a, "Added the plot to the figure's panel %d.", panel + 1);
  return any;
}

/* ---- Settings to the session ------------------------------------------------------------ */

static void panel_diff(App *a, int k, const FigPanel *p, const FigPanel *q) {
  char line[2000], b1[600], b2[64], b3[64];
  size_t c = 0;
#define ADD(...) c += (size_t)snprintf(line + c, c < sizeof line ? sizeof line - c : 0, __VA_ARGS__)
  if (strcmp(p->title, q->title)) ADD(", title=%s", echo_str(p->title, b1, sizeof b1));
  if (strcmp(p->xlabel, q->xlabel)) ADD(", xlabel=%s", echo_str(p->xlabel, b1, sizeof b1));
  if (strcmp(p->ylabel, q->ylabel)) ADD(", ylabel=%s", echo_str(p->ylabel, b1, sizeof b1));
  if (p->autox != q->autox || (!p->autox && (p->xmin != q->xmin || p->xmax != q->xmax))) {
    if (p->autox) ADD(", xlim=None");
    else ADD(", xlim=(%s, %s)", echo_num(p->xmin, b2, sizeof b2), echo_num(p->xmax, b3, sizeof b3));
  }
  if (p->autoy != q->autoy || (!p->autoy && (p->ymin != q->ymin || p->ymax != q->ymax))) {
    if (p->autoy) ADD(", ylim=None");
    else ADD(", ylim=(%s, %s)", echo_num(p->ymin, b2, sizeof b2), echo_num(p->ymax, b3, sizeof b3));
  }
  if (p->logx != q->logx) ADD(", logx=%s", p->logx ? "True" : "False");
  if (p->logy != q->logy) ADD(", logy=%s", p->logy ? "True" : "False");
  if (p->grid != q->grid) ADD(", grid=%s", p->grid ? "True" : "False");
  if (p->xstep != q->xstep) {
    if (p->xstep > 0) ADD(", xstep=%s", echo_num(p->xstep, b2, sizeof b2));
    else ADD(", xstep=None");
  }
  if (p->ystep != q->ystep) {
    if (p->ystep > 0) ADD(", ystep=%s", echo_num(p->ystep, b2, sizeof b2));
    else ADD(", ystep=None");
  }
  if (p->legend != q->legend) ADD(", legend=\"%s\"", legend_names[p->legend]);
  if (p->equal != q->equal) ADD(", equal=%s", p->equal ? "True" : "False");
#undef ADD
  if (c) echo_line(a, "fig[%d].set(%s)", k, line + 2);
  if (p->inset && (p->ix != q->ix || p->iy != q->iy || p->iw != q->iw || p->ih != q->ih)) {
    char x[64], y[64], w[64], h[64];
    echo_line(a, "fig[%d].inset = (%d, (%s, %s, %s, %s))", k, p->parent, fnum(p->ix, x, sizeof x),
              fnum(p->iy, y, sizeof y), fnum(p->iw, w, sizeof w), fnum(p->ih, h, sizeof h));
  }
}

void app_fig_sync_echo(App *a) {
  FigUI *g = &a->fg;
  Figure *f = &g->fig, *s = &g->shadow;
  char line[800], b[64], c[64];
  size_t k = 0;
  int i, j;
  bool outer;
  if (!g->echoed || !a->echo.loaded) return;
  outer = echo_begin(a);
  if (!outer) {
    echo_end(a);
    return;
  }
  if (f->width_mm != s->width_mm)
    k += (size_t)snprintf(line + k, sizeof line - k, ", width=%s", fnum(f->width_mm, b, sizeof b));
  if (f->height_mm != s->height_mm)
    k += (size_t)snprintf(line + k, sizeof line - k, ", height=%s", fnum(f->height_mm, c, sizeof c));
  if (f->font != s->font)
    k += (size_t)snprintf(line + k, sizeof line - k, ", font=\"%s\"",
                          f->font == FIG_FONT_SERIF ? "serif" : "sans");
  if (f->font_pt != s->font_pt)
    k += (size_t)snprintf(line + k, sizeof line - k, ", fontsize=%s", fnum(f->font_pt, b, sizeof b));
  if (k) echo_line(a, "fig.set(%s)", line + 2);
  for (i = 0; i < f->npanels && i < s->npanels; i++) {
    panel_diff(a, i, &f->p[i], &s->p[i]);
    for (j = 0; j < f->p[i].nseries && j < s->p[i].nseries; j++) {
      char style[1200];
      style_args(&f->p[i].s[j], &s->p[i].s[j], style, sizeof style);
      if (style[0]) echo_line(a, "fig[%d][%d].set(%s)", i, j, style + 2);
    }
  }
  echo_end(a);
  /* the shadow keeps the settings only: its data pointers are the figure's */
  {
    Figure keep = *f;
    *s = keep;
  }
}

bool app_fig_export(App *a, const char *path) {
  FigUI *g = &a->fg;
  char err[300], p[2100], q[2200];
  bool outer, ok;
  app_fig_sync_echo(a);
  ok = fig_write(&g->fig, path, err, sizeof err);
  if (!ok) {
    app_log(a, "error: %s", err);
    snprintf(g->error, sizeof g->error, "%s", err);
    return false;
  }
  snprintf(g->path, sizeof g->path, "%s", path);
  g->error[0] = '\0';
  outer = echo_begin(a);
  if (outer) {
    echo_new_figure(a);
    echo_abs_path(path, p, sizeof p);
    echo_str(p, q, sizeof q);
    {
      size_t n = strlen(path);
      bool png = n > 4 && SDL_strcasecmp(path + n - 4, ".png") == 0;
      if (png) echo_line(a, "fig.save(%s, dpi=%d)", q, g->fig.dpi);
      else echo_line(a, "fig.save(%s)", q);
    }
  }
  echo_end(a);
  app_log(a, "Saved the figure: %s", path);
  app_status(a, "Saved the figure as %s", path);
  return true;
}
