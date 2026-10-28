/*
 * Linearisation and controllability for the engine API (see xppe.h).
 *
 * The derivatives of the right-hand side are central differences of
 * xppe_rhs(), extrapolated to zero step by Ridders' method; the
 * eigenvalues come from XPPAUT's own routine (the one Sing pts uses); the
 * rank of the controllability matrix from its singular values. Python's
 * Model.linearize() and the desktop app both call these functions, so the
 * two give the same numbers, bit for bit.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "xppe.h"
#include "xppe_internal.h"

#include "xpplim.h"

extern int NODE, NMarkov, NUPAR;
extern char upar_names[MAXPAR][11];
extern double EVEC_ERR;
extern int EVEC_ITER;
void eigen(int n, double *a, double *ev, double *work, int *ierr);
void get_evec(double *a, double *anew, double *b, double *bp, int n, int maxit,
              double err, int *ipivot, double eval, int *ierr);

#define RIDDERS_ROWS 10
#define RIDDERS_SHRINK 1.4

/* One derivative being taken: of the right-hand side at x0 with respect to
   state variable j, or (par != NULL) to a parameter whose value is p0. n
   counts the Markov variables too, as xppe_count() does: the right-hand
   side has no rows for them and does not read them, so their rows and
   columns of A are zero. */
typedef struct {
  int n, j;
  const char *par;
  double p0;
  const double *x0;
  double *xs, *fp, *fm; /* scratch, n values each */
} Diff;

/* The central difference (f(+h) - f(-h)) / 2h. */
static int central(Diff *d, double h, double *out) {
  int i, rc;
  memset(d->fp, 0, sizeof(double) * (size_t)d->n);
  memset(d->fm, 0, sizeof(double) * (size_t)d->n);
  if (d->par) {
    if ((rc = xppe_set_number(d->par, d->p0 + h)) != XPPE_OK) return rc;
    if ((rc = xppe_rhs(0.0, d->x0, d->fp)) != XPPE_OK) return rc;
    if ((rc = xppe_set_number(d->par, d->p0 - h)) != XPPE_OK) return rc;
    if ((rc = xppe_rhs(0.0, d->x0, d->fm)) != XPPE_OK) return rc;
  } else {
    memcpy(d->xs, d->x0, sizeof(double) * (size_t)d->n);
    d->xs[d->j] = d->x0[d->j] + h;
    if ((rc = xppe_rhs(0.0, d->xs, d->fp)) != XPPE_OK) return rc;
    d->xs[d->j] = d->x0[d->j] - h;
    if ((rc = xppe_rhs(0.0, d->xs, d->fm)) != XPPE_OK) return rc;
  }
  for (i = 0; i < d->n; i++) out[i] = (d->fp[i] - d->fm[i]) / (2 * h);
  return XPPE_OK;
}

/* Ridders' method (Numerical Recipes' dfridr, for all n components at
   once): central differences at steps shrinking from start * scale,
   extrapolated to zero step in Neville's tableau. Each component keeps the
   extrapolation with the smallest error estimate. */
static int ridders(Diff *d, double scale, double start, double *best,
                   double *err) {
  const double fac2 = RIDDERS_SHRINK * RIDDERS_SHRINK;
  const int n = d->n;
  double *rows = malloc(sizeof(double) * (size_t)(2 * RIDDERS_ROWS * n));
  double *prev = rows, *cur, *t, h = start * scale, fac;
  int i, k, c, worse, rc = XPPE_OK;
  if (!rows) return XPPE_ERR_RANGE;
  cur = rows + RIDDERS_ROWS * n;
  for (c = 0; c < n; c++) {
    best[c] = 0;
    err[c] = INFINITY;
  }
  for (i = 0; i < RIDDERS_ROWS; i++) {
    if ((rc = central(d, h, cur)) != XPPE_OK) break;
    fac = fac2;
    for (k = 1; k <= i; k++) {
      double *tk = cur + k * n, *tk1 = cur + (k - 1) * n, *pk1 = prev + (k - 1) * n;
      for (c = 0; c < n; c++) {
        double e;
        tk[c] = (tk1[c] * fac - pk1[c]) / (fac - 1.0);
        e = fmax(fabs(tk[c] - tk1[c]), fabs(tk[c] - pk1[c]));
        if (e <= err[c]) {
          best[c] = tk[c];
          err[c] = e;
        }
      }
      fac *= fac2;
    }
    /* Stop once the highest-order estimate gets worse for every component:
       rounding noise has won. */
    worse = i > 0;
    for (c = 0; c < n && worse; c++)
      worse = fabs(cur[i * n + c] - prev[(i - 1) * n + c]) >= 2 * err[c];
    if (worse) break;
    t = prev;
    prev = cur;
    cur = t;
    h /= RIDDERS_SHRINK;
  }
  free(rows);
  return rc;
}

/* A derivative with no step to choose: Ridders' method from three starting
   steps, coarse to fine, each component keeping the estimate with the
   smallest error. With step > 0, one central difference at that relative
   step (the errors are then NaN). */
static int derivative(Diff *d, double scale, double step, double *out,
                      double *err) {
  static const double starts[3] = {1e-1, 1e-3, 1e-5};
  double *di, *ei;
  int s, c, rc = XPPE_OK;
  if (step > 0) {
    for (c = 0; c < d->n; c++) err[c] = NAN;
    return central(d, step * scale, out);
  }
  di = malloc(sizeof(double) * (size_t)(2 * d->n));
  if (!di) return XPPE_ERR_RANGE;
  ei = di + d->n;
  for (s = 0; s < 3 && rc == XPPE_OK; s++) {
    if ((rc = ridders(d, scale, starts[s], s ? di : out, s ? ei : err)) != XPPE_OK)
      break;
    if (s)
      for (c = 0; c < d->n; c++)
        if (ei[c] < err[c]) {
          out[c] = di[c];
          err[c] = ei[c];
        }
  }
  free(di);
  return rc;
}

static int find_parameter(const char *name) {
  int i;
  for (i = 0; i < NUPAR; i++)
    if (xppe_same_name(name, upar_names[i])) return i;
  return -1;
}

int xppe_linearize(const double *x, int ninputs, const char *const *inputs,
                   double step, double *A, double *B, double *A_error,
                   double *B_error) {
  Diff d;
  double *col, *cerr, *saved, *scratch;
  char why[256];
  int rc, n, i, j, k;
  if ((rc = xppe_require_model()) != XPPE_OK) return rc;
  n = NODE + NMarkov;
  if (!x || !A || ninputs < 0 || (ninputs > 0 && (!inputs || !B))) {
    xppe_set_error("give the point, A, and for inputs their names and B", NULL);
    return XPPE_ERR_RANGE;
  }
  for (k = 0; k < ninputs; k++)
    if (!inputs[k] || find_parameter(inputs[k]) < 0) {
      xppe_set_error("no parameter %s", inputs[k] ? inputs[k] : "(null)");
      return XPPE_ERR_NAME;
    }
  /* a column of A or B and its errors, then Diff's scratch, then the
     inputs' values */
  scratch = malloc(sizeof(double) * (size_t)(5 * n + ninputs + 1));
  if (!scratch) {
    xppe_set_error("out of memory", NULL);
    return XPPE_ERR_RANGE;
  }
  memset(&d, 0, sizeof d);
  col = scratch;
  cerr = col + n;
  d.xs = cerr + n;
  d.fp = d.xs + n;
  d.fm = d.fp + n;
  saved = d.fm + n;
  d.n = n;
  d.x0 = x;
  for (k = 0; k < ninputs; k++) xppe_get(inputs[k], &saved[k]);

  for (j = 0; j < n && rc == XPPE_OK; j++) {
    d.j = j;
    rc = derivative(&d, fabs(x[j]) > 1.0 ? fabs(x[j]) : 1.0, step, col, cerr);
    for (i = 0; i < n && rc == XPPE_OK; i++) {
      A[i * n + j] = col[i];
      if (A_error) A_error[i * n + j] = cerr[i];
    }
  }
  for (k = 0; k < ninputs && rc == XPPE_OK; k++) {
    d.par = inputs[k];
    d.p0 = saved[k];
    rc = derivative(&d, fabs(d.p0) > 1.0 ? fabs(d.p0) : 1.0, step, col, cerr);
    for (i = 0; i < n && rc == XPPE_OK; i++) {
      B[i * ninputs + k] = col[i];
      if (B_error) B_error[i * ninputs + k] = cerr[i];
    }
    if (rc == XPPE_OK) rc = xppe_set_number(d.par, d.p0);
  }
  /* The parameters as they were, whatever happened, and the error that
     happened (setting them clears it). */
  snprintf(why, sizeof why, "%s", xppe_error());
  if (xppe_loaded())
    for (k = 0; k < ninputs; k++) xppe_set_number(inputs[k], saved[k]);
  if (rc != XPPE_OK) xppe_set_error("%s", why);
  free(scratch);
  return rc;
}

int xppe_eigenvalues(int n, const double *a, double *re, double *im) {
  double *h, *ev, *work;
  int i, j, ierr = 0;
  if (n < 1 || !a || !re || !im) {
    xppe_set_error("give an n x n matrix and room for n eigenvalues", NULL);
    return XPPE_ERR_RANGE;
  }
  h = malloc(sizeof(double) * (size_t)(n * n + 3 * n));
  if (!h) {
    xppe_set_error("out of memory", NULL);
    return XPPE_ERR_RANGE;
  }
  ev = h + n * n;
  work = ev + 2 * n;
  /* eigen() reads the matrix by columns, as do_sing_info() hands it over. */
  for (i = 0; i < n; i++)
    for (j = 0; j < n; j++) h[i + j * n] = a[i * n + j];
  eigen(n, h, ev, work, &ierr);
  for (i = 0; i < n; i++) {
    re[i] = ev[2 * i];
    im[i] = ev[2 * i + 1];
  }
  free(h);
  if (ierr != 0) {
    xppe_set_error("the eigenvalues did not converge", NULL);
    return XPPE_ERR_NUMERIC;
  }
  return XPPE_OK;
}

/* get_evec() (gear.c) takes the matrix as do_sing_info() hands it over,
   which is row by row here, and returns the vector with its largest entry 1
   (in size). */
int xppe_eigenvector(int n, const double *a, double lambda, double *v) {
  double *h, *anew, *b, *bp, norm = 0;
  int *ipivot, i, ierr = 0;
  if (n < 1 || !a || !v) {
    xppe_set_error("give an n x n matrix and room for n values", NULL);
    return XPPE_ERR_RANGE;
  }
  h = malloc(sizeof(double) * (size_t)(2 * n * n + 2 * n));
  ipivot = malloc(sizeof(int) * (size_t)n);
  if (!h || !ipivot) {
    free(h);
    free(ipivot);
    xppe_set_error("out of memory", NULL);
    return XPPE_ERR_RANGE;
  }
  anew = h + n * n;
  b = anew + n * n;
  bp = b + n;
  memcpy(h, a, sizeof(double) * (size_t)(n * n));
  get_evec(h, anew, b, bp, n, EVEC_ITER, EVEC_ERR, ipivot, lambda, &ierr);
  if (ierr == 0)
    for (i = 0; i < n; i++) norm += b[i] * b[i];
  if (ierr != 0 || !(norm > 0) || !isfinite(norm)) {
    free(h);
    free(ipivot);
    xppe_set_error("the eigenvector did not converge; is lambda a real "
                   "eigenvalue of the matrix?", NULL);
    return XPPE_ERR_NUMERIC;
  }
  norm = sqrt(norm);
  for (i = 0; i < n; i++) v[i] = b[i] / norm;
  free(h);
  free(ipivot);
  return XPPE_OK;
}

/* The singular values of the r x c matrix u (row-major, r <= c), largest
   first, by one-sided Jacobi rotations of its rows: rotate pairs of rows
   until all are orthogonal; their lengths are then the singular values. */
static int singular_values(int r, int c, double *u, double *sv) {
  int sweep, i, j, k, rotated = 1;
  for (sweep = 0; sweep < 100 && rotated; sweep++) {
    rotated = 0;
    for (i = 0; i < r - 1; i++)
      for (j = i + 1; j < r; j++) {
        double alpha = 0, beta = 0, gamma = 0, zeta, t, cs, sn;
        double *ui = u + (size_t)i * c, *uj = u + (size_t)j * c;
        for (k = 0; k < c; k++) {
          alpha += ui[k] * ui[k];
          beta += uj[k] * uj[k];
          gamma += ui[k] * uj[k];
        }
        if (gamma == 0 || fabs(gamma) <= 1e-15 * sqrt(alpha * beta)) continue;
        rotated = 1;
        zeta = (beta - alpha) / (2 * gamma);
        t = (zeta >= 0 ? 1.0 : -1.0) / (fabs(zeta) + sqrt(1 + zeta * zeta));
        cs = 1 / sqrt(1 + t * t);
        sn = cs * t;
        for (k = 0; k < c; k++) {
          double a = ui[k], b = uj[k];
          ui[k] = cs * a - sn * b;
          uj[k] = sn * a + cs * b;
        }
      }
  }
  for (i = 0; i < r; i++) {
    double s = 0;
    for (k = 0; k < c; k++) s += u[(size_t)i * c + k] * u[(size_t)i * c + k];
    sv[i] = sqrt(s);
  }
  /* largest first */
  for (i = 1; i < r; i++) {
    double s = sv[i];
    for (j = i; j > 0 && sv[j - 1] < s; j--) sv[j] = sv[j - 1];
    sv[j] = s;
  }
  return rotated ? XPPE_ERR_NUMERIC : XPPE_OK;
}

int xppe_controllability(int n, int m, const double *A, const double *B,
                         double tol, double *C, double *sv) {
  const int cols = n * m;
  double *u, *blk, *next;
  int i, j, k, l, rank = 0, rc;
  if (n < 1 || m < 0 || !A || (m > 0 && (!B || !C || !sv))) {
    xppe_set_error("give A (n x n), B (n x m), and room for C and n "
                   "singular values", NULL);
    return XPPE_ERR_RANGE;
  }
  if (m == 0) return 0;
  u = malloc(sizeof(double) * (size_t)(cols * n + 2 * n * m));
  if (!u) {
    xppe_set_error("out of memory", NULL);
    return XPPE_ERR_RANGE;
  }
  blk = u + (size_t)cols * n;
  next = blk + n * m;
  /* C = [B, AB, ..., A^(n-1) B], one block of m columns at a time */
  memcpy(blk, B, sizeof(double) * (size_t)(n * m));
  for (k = 0; k < n; k++) {
    for (i = 0; i < n; i++)
      for (j = 0; j < m; j++) C[(size_t)i * cols + k * m + j] = blk[i * m + j];
    if (k + 1 == n) break;
    for (i = 0; i < n; i++)
      for (j = 0; j < m; j++) {
        double s = 0;
        for (l = 0; l < n; l++) s += A[i * n + l] * blk[l * m + j];
        next[i * m + j] = s;
      }
    memcpy(blk, next, sizeof(double) * (size_t)(n * m));
  }
  for (i = 0; i < n * cols; i++)
    if (!isfinite(C[i])) {
      free(u);
      xppe_set_error("the controllability matrix has entries too large to "
                     "represent: A^(n-1) B overflows", NULL);
      return XPPE_ERR_NUMERIC;
    }
  /* Scaled by a power of two (exactly) so that the sums of squares neither
     overflow nor underflow. */
  {
    double big = 0, scale;
    int e;
    for (i = 0; i < n * cols; i++)
      if (fabs(C[i]) > big) big = fabs(C[i]);
    frexp(big > 0 ? big : 1.0, &e);
    scale = ldexp(1.0, -e);
    for (i = 0; i < n * cols; i++) u[i] = C[i] * scale;
    rc = singular_values(n, cols, u, sv);
    for (i = 0; i < n; i++) sv[i] /= scale;
  }
  free(u);
  if (rc != XPPE_OK) {
    xppe_set_error("the singular values did not converge", NULL);
    return rc;
  }
  /* The derivatives are good to about 1e-10, so the default cutoff is
     relative to the largest singular value. */
  if (!(tol > 0))
    tol = (double)(cols > n ? cols : n) * (sv[0] > 1e-300 ? sv[0] : 1e-300) * 1e-8;
  for (i = 0; i < n; i++)
    if (sv[i] > tol) rank++;
  return rank;
}
