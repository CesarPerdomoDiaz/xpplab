/*
 * Control design for the engine API (see xppe.h): the feedback gain that
 * gives a linear system the poles asked for, and the closed-loop model
 * written as .ode text. Python's Linearization.place_poles() and
 * closed_loop() and the desktop app all call these functions, so they get
 * the same gain and the same model, bit for bit.
 *
 * One input: Ackermann's formula. Several: the method of Kautsky, Nichols
 * and Van Dooren (1985, "method 0"), which uses the spare inputs to keep
 * the closed-loop eigenvectors as nearly orthogonal as it can. The bases it
 * needs come from Gram-Schmidt, applied twice, in complex arithmetic.
 */
#include <complex.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xppe.h"
#include "xppe_internal.h"

typedef double complex cplx;

static int fail(int rc, const char *text) {
  xppe_set_error("%s", text);
  return rc;
}

/* ---- Small dense linear algebra -------------------------------------------
 *
 * A set of vectors of length n is stored one vector after another:
 * V[j * n + i] is entry i of vector j. Matrices passed through the API are
 * row-major, as everywhere in xppe.h.
 */

static double vnorm(int n, const cplx *v) {
  double s = 0;
  int i;
  for (i = 0; i < n; i++) s += creal(v[i]) * creal(v[i]) + cimag(v[i]) * cimag(v[i]);
  return sqrt(s);
}

/* Take from v its components along the q orthonormal vectors Q, twice,
   which leaves it orthogonal to them to rounding. Returns what is left of
   its length. */
static double orthogonalize(int n, const cplx *Q, int q, cplx *v) {
  int pass, i, j;
  for (pass = 0; pass < 2; pass++)
    for (j = 0; j < q; j++) {
      const cplx *u = Q + (size_t)j * n;
      cplx d = 0;
      for (i = 0; i < n; i++) d += conj(u[i]) * v[i];
      for (i = 0; i < n; i++) v[i] -= d * u[i];
    }
  return vnorm(n, v);
}

/* W (n vectors) receives an orthonormal basis of C^n: first one of the
   span of the k vectors V, then one of its orthogonal complement. A vector
   of V adds a direction only if more than tol times the length of the
   longest one is left after taking away the directions found so far. The
   complement is completed with the unit vectors, the one with the most
   left over first. Returns the dimension of the span, or -1 when out of
   memory. Real vectors give a real basis. */
static int split_basis(int n, int k, const cplx *V, double tol, cplx *W) {
  cplx *v = malloc(sizeof(cplx) * (size_t)n);
  double big = 0;
  int i, j, q, r = 0;
  if (!v) return -1;
  for (j = 0; j < k; j++) {
    double s = vnorm(n, V + (size_t)j * n);
    if (s > big) big = s;
  }
  for (j = 0; j < k && r < n; j++) {
    double s;
    memcpy(v, V + (size_t)j * n, sizeof(cplx) * (size_t)n);
    s = orthogonalize(n, W, r, v);
    if (big > 0 && s > tol * big) {
      for (i = 0; i < n; i++) W[(size_t)r * n + i] = v[i] / s;
      r++;
    }
  }
  for (q = r; q < n; q++) {
    double best = -1, s;
    int pick = 0;
    for (j = 0; j < n; j++) {
      for (i = 0; i < n; i++) v[i] = i == j;
      s = orthogonalize(n, W, q, v);
      if (s > best) {
        best = s;
        pick = j;
      }
    }
    for (i = 0; i < n; i++) v[i] = i == pick;
    s = orthogonalize(n, W, q, v);
    for (i = 0; i < n; i++) W[(size_t)q * n + i] = v[i] / s;
  }
  free(v);
  return r;
}

/* Solve the n x n system M y = b for nb right-hand sides (M and b
   row-major, b n x nb, overwritten by y) by Gaussian elimination with
   partial pivoting. M is destroyed. False when a pivot vanishes. */
static int solve_real(int n, double *M, int nb, double *b) {
  int i, j, k, p;
  for (k = 0; k < n; k++) {
    double big = 0;
    p = k;
    for (i = k; i < n; i++)
      if (fabs(M[i * n + k]) > big) {
        big = fabs(M[i * n + k]);
        p = i;
      }
    if (!(big > 0)) return 0;
    if (p != k) {
      for (j = 0; j < n; j++) {
        double t = M[k * n + j];
        M[k * n + j] = M[p * n + j];
        M[p * n + j] = t;
      }
      for (j = 0; j < nb; j++) {
        double t = b[k * nb + j];
        b[k * nb + j] = b[p * nb + j];
        b[p * nb + j] = t;
      }
    }
    for (i = k + 1; i < n; i++) {
      double f = M[i * n + k] / M[k * n + k];
      if (f == 0) continue;
      for (j = k; j < n; j++) M[i * n + j] -= f * M[k * n + j];
      for (j = 0; j < nb; j++) b[i * nb + j] -= f * b[k * nb + j];
    }
  }
  for (k = n - 1; k >= 0; k--)
    for (j = 0; j < nb; j++) {
      double s = b[k * nb + j];
      for (i = k + 1; i < n; i++) s -= M[k * n + i] * b[i * nb + j];
      b[k * nb + j] = s / M[k * n + k];
    }
  return 1;
}

/* The inverse (row-major) of the n x n matrix whose columns are the
   vectors X, by Gauss-Jordan elimination with partial pivoting. False when
   a pivot vanishes or out of memory. */
static int invert(int n, const cplx *X, cplx *Xi) {
  cplx *M = malloc(sizeof(cplx) * (size_t)n * n);
  int i, j, k, p;
  if (!M) return 0;
  for (i = 0; i < n; i++)
    for (j = 0; j < n; j++) {
      M[i * n + j] = X[(size_t)j * n + i];
      Xi[i * n + j] = i == j;
    }
  for (k = 0; k < n; k++) {
    double big = 0;
    cplx piv;
    p = k;
    for (i = k; i < n; i++)
      if (cabs(M[i * n + k]) > big) {
        big = cabs(M[i * n + k]);
        p = i;
      }
    if (!(big > 0)) {
      free(M);
      return 0;
    }
    if (p != k)
      for (j = 0; j < n; j++) {
        cplx t = M[k * n + j];
        M[k * n + j] = M[p * n + j];
        M[p * n + j] = t;
        t = Xi[k * n + j];
        Xi[k * n + j] = Xi[p * n + j];
        Xi[p * n + j] = t;
      }
    piv = M[k * n + k];
    for (j = 0; j < n; j++) {
      M[k * n + j] /= piv;
      Xi[k * n + j] /= piv;
    }
    for (i = 0; i < n; i++) {
      cplx f = M[i * n + k];
      if (i == k || f == 0) continue;
      for (j = 0; j < n; j++) {
        M[i * n + j] -= f * M[k * n + j];
        Xi[i * n + j] -= f * Xi[k * n + j];
      }
    }
  }
  free(M);
  return 1;
}

/* The 1-norm (largest column sum of absolute values) of an n x n matrix,
   given as vectors (its columns) or row-major. */
static double norm1(int n, const cplx *M, int as_vectors) {
  double best = 0;
  int i, j;
  for (j = 0; j < n; j++) {
    double s = 0;
    for (i = 0; i < n; i++) s += cabs(as_vectors ? M[(size_t)j * n + i] : M[i * n + j]);
    if (s > best) best = s;
  }
  return best;
}

/* ---- Pole placement -------------------------------------------------------- */

/* Each complex pole must have its own conjugate among the others (Python's
   _check_conjugate() makes the same test). partner[j] receives the index
   of the conjugate of the first pole of each pair, second[j] marks the
   other one. */
static int pair_poles(int n, const cplx *poles, int *partner, int *second) {
  int i, j;
  for (j = 0; j < n; j++) {
    partner[j] = -1;
    second[j] = 0;
  }
  for (j = 0; j < n; j++) {
    double tol = 1e-12 * (cabs(poles[j]) > 1 ? cabs(poles[j]) : 1.0);
    if (second[j] || cimag(poles[j]) == 0) continue;
    for (i = j + 1; i < n; i++)
      if (!second[i] && cabs(poles[i] - conj(poles[j])) <= tol) {
        partner[j] = i;
        second[i] = 1;
        break;
      }
    if (partner[j] < 0) {
      char text[200];
      snprintf(text, sizeof text, "complex pole %.6g%+.6gi has no conjugate "
               "partner: poles of a real system come in conjugate pairs",
               creal(poles[j]), cimag(poles[j]));
      return fail(XPPE_ERR_RANGE, text);
    }
  }
  return XPPE_OK;
}

/* One input: K = e_n^T C^-1 phi(A), with C = [B, AB, ..., A^(n-1) B] and
   phi the polynomial whose roots are the poles. */
static int ackermann(int n, const double *A, const double *B, const cplx *poles,
                     const int *second, double *K) {
  const size_t nn = (size_t)n * n;
  double *coef = calloc((size_t)n + 1, sizeof(double));
  double *phi = malloc(sizeof(double) * (3 * nn + 2 * (size_t)n));
  double *next, *Ct, *w, *col;
  int deg = 0, i, j, l, k, ok;
  if (!coef || !phi) {
    free(coef);
    free(phi);
    return fail(XPPE_ERR_RANGE, "out of memory");
  }
  next = phi + nn;
  Ct = next + nn;
  w = Ct + nn;
  col = w + n;
  /* the coefficients of prod (s - pole), highest power first, in real
     arithmetic: a conjugate pair a +- bi gives s^2 - 2a s + a^2 + b^2 */
  coef[0] = 1;
  for (j = 0; j < n; j++) {
    double a = creal(poles[j]), b = cimag(poles[j]);
    if (second[j]) continue;
    if (b == 0) {
      for (k = deg + 1; k >= 1; k--) coef[k] -= a * coef[k - 1];
      deg += 1;
    } else {
      double q = a * a + b * b;
      for (k = deg + 2; k >= 1; k--) {
        coef[k] -= 2 * a * coef[k - 1];
        if (k >= 2) coef[k] += q * coef[k - 2];
      }
      deg += 2;
    }
  }
  /* phi(A) by Horner's rule */
  memset(phi, 0, sizeof(double) * nn);
  for (k = 0; k <= n; k++) {
    for (i = 0; i < n; i++)
      for (l = 0; l < n; l++) {
        double s = 0;
        for (j = 0; j < n; j++) s += phi[i * n + j] * A[j * n + l];
        next[i * n + l] = s;
      }
    memcpy(phi, next, sizeof(double) * nn);
    for (i = 0; i < n; i++) phi[i * n + i] += coef[k];
  }
  /* C^T w = e_n; row k of C^T is A^k B */
  for (i = 0; i < n; i++) col[i] = B[i];
  for (k = 0; k < n; k++) {
    for (i = 0; i < n; i++) Ct[k * n + i] = col[i];
    for (i = 0; i < n; i++) {
      double s = 0;
      for (j = 0; j < n; j++) s += A[i * n + j] * col[j];
      w[i] = s;
    }
    memcpy(col, w, sizeof(double) * (size_t)n);
  }
  for (i = 0; i < n; i++) w[i] = i == n - 1;
  ok = solve_real(n, Ct, 1, w);
  if (ok)
    for (l = 0; l < n; l++) {
      double s = 0;
      for (i = 0; i < n; i++) s += w[i] * phi[i * n + l];
      K[l] = s;
    }
  free(coef);
  free(phi);
  if (!ok)
    return fail(XPPE_ERR_NUMERIC, "cannot place poles: the controllability "
                                  "matrix is singular");
  return XPPE_OK;
}

/* Several inputs: choose closed-loop eigenvectors X, one in each allowed
   space S_j = null(U1^H (A - pole_j I)) (U1 spans the directions that B
   cannot push), as nearly orthogonal to each other as possible; then
   A - B K = X diag(poles) X^-1 gives K. */
static int kautsky(int n, int m, const double *A, const double *B,
                   const cplx *poles, const int *partner, const int *second,
                   double *K) {
  const size_t nn = (size_t)n * n;
  cplx *U = malloc(sizeof(cplx) * (nn * ((size_t)n + 5) + (size_t)n * m + n));
  double *Mr = malloc(sizeof(double) * (2 * nn + (size_t)n * m + (size_t)m * m + n));
  int *dim = malloc(sizeof(int) * (size_t)n);
  cplx *S, *X, *Xi, *rest, *comp, *Bv, *col;
  double *Z, *ZZ, *Y, *norms, imag_big = 0, real_big = 0;
  int r, p, i, j, l, a, sweep, rc = XPPE_OK;
  if (!U || !Mr || !dim) {
    free(U);
    free(Mr);
    free(dim);
    return fail(XPPE_ERR_RANGE, "out of memory");
  }
  S = U + nn;          /* a basis of S_j for each pole j, n vectors each */
  X = S + nn * n;      /* the eigenvectors */
  Xi = X + nn;         /* X^-1, row-major */
  rest = Xi + nn;      /* vectors to split */
  comp = rest + nn;    /* the split */
  Bv = comp + nn;      /* the columns of B */
  col = Bv + (size_t)n * m;
  Z = Mr + nn;
  ZZ = Z + (size_t)n * m;
  Y = ZZ + (size_t)m * m;
  norms = Y + nn;
  /* U: range(B) (r vectors, U0), then the directions B cannot push (U1) */
  for (j = 0; j < m; j++)
    for (i = 0; i < n; i++) Bv[(size_t)j * n + i] = B[i * m + j];
  r = split_basis(n, m, Bv, 1e-10, U);
  if (r < 0) {
    rc = fail(XPPE_ERR_RANGE, "out of memory");
    goto done;
  }
  if (r == 0) {
    rc = fail(XPPE_ERR_RANGE, "cannot place poles: the inputs do not act on "
                              "the state (B is zero)");
    goto done;
  }
  p = n - r;
  /* Z = U0^T B, B in the basis U0 (r x m, real) */
  for (a = 0; a < r; a++)
    for (j = 0; j < m; j++) {
      double s = 0;
      for (i = 0; i < n; i++) s += creal(U[(size_t)a * n + i]) * B[i * m + j];
      Z[a * m + j] = s;
    }
  /* the allowed spaces; a real pole gets a real one */
  for (j = 0; j < n; j++) {
    cplx lam = cimag(poles[j]) == 0 ? creal(poles[j]) : poles[j];
    cplx *Sj = S + nn * (size_t)j;
    if (second[j]) continue;
    if (p == 0) {
      for (l = 0; l < n; l++)
        for (i = 0; i < n; i++) Sj[(size_t)l * n + i] = i == l;
      dim[j] = n;
      continue;
    }
    /* null(M), M = U1^H (A - lam I), is the complement of M's rows,
       conjugated */
    for (a = 0; a < p; a++) {
      const cplx *u1 = U + (size_t)(r + a) * n;
      for (i = 0; i < n; i++) {
        cplx s = 0;
        for (l = 0; l < n; l++) s += conj(u1[l]) * (A[l * n + i] - (l == i ? lam : 0));
        rest[(size_t)a * n + i] = conj(s);
      }
    }
    l = split_basis(n, p, rest, 1e-12, comp);
    if (l < 0) {
      rc = fail(XPPE_ERR_RANGE, "out of memory");
      goto done;
    }
    dim[j] = n - l;
    memcpy(Sj, comp + (size_t)l * n, sizeof(cplx) * (size_t)dim[j] * n);
  }
  /* start from the first vector of each space */
  for (j = 0; j < n; j++) {
    const cplx *Sj = S + nn * (size_t)j;
    double s;
    if (second[j]) continue;
    s = vnorm(n, Sj);
    for (i = 0; i < n; i++) X[(size_t)j * n + i] = Sj[i] / s;
    if (partner[j] >= 0)
      for (i = 0; i < n; i++) X[(size_t)partner[j] * n + i] = conj(X[(size_t)j * n + i]);
  }
  for (sweep = 0; sweep < 50; sweep++) {
    double moved = 0;
    for (j = 0; j < n; j++) {
      const cplx *Sj = S + nn * (size_t)j;
      int cnt = 0, q, rho, c, big, b1 = -1, b2 = -1;
      double s;
      cplx phase;
      if (second[j]) continue;
      for (l = 0; l < n; l++)
        if (l != j && l != partner[j])
          memcpy(rest + (size_t)cnt++ * n, X + (size_t)l * n, sizeof(cplx) * (size_t)n);
      rho = split_basis(n, cnt, rest, 1e-12, comp);
      if (rho < 0) {
        rc = fail(XPPE_ERR_RANGE, "out of memory");
        goto done;
      }
      q = n - rho;
      /* the complement of the others, projected into S_j: S_j S_j^H u */
      for (c = 0; c < q; c++) {
        const cplx *u = comp + (size_t)(rho + c) * n;
        cplx *out = rest + (size_t)c * n;
        for (i = 0; i < n; i++) out[i] = 0;
        for (l = 0; l < dim[j]; l++) {
          const cplx *sl = Sj + (size_t)l * n;
          cplx d = 0;
          for (i = 0; i < n; i++) d += conj(sl[i]) * u[i];
          for (i = 0; i < n; i++) out[i] += d * sl[i];
        }
        norms[c] = vnorm(n, out);
        if (b1 < 0 || norms[c] > norms[b1]) {
          b2 = b1;
          b1 = c;
        } else if (b2 < 0 || norms[c] > norms[b2]) {
          b2 = c;
        }
      }
      if (b1 < 0) continue;
      memcpy(col, rest + (size_t)b1 * n, sizeof(cplx) * (size_t)n);
      /* a complex pair needs a genuinely complex vector, or it and its
         conjugate would be parallel */
      if (partner[j] >= 0 && b2 >= 0)
        for (i = 0; i < n; i++) col[i] += I * rest[(size_t)b2 * n + i];
      s = vnorm(n, col);
      if (s < 1e-14) continue;
      big = 0;
      for (i = 0; i < n; i++) {
        col[i] /= s;
        if (cabs(col[i]) > cabs(col[big])) big = i;
      }
      phase = conj(col[big]) / cabs(col[big]);
      for (i = 0; i < n; i++) col[i] *= phase;
      if (cimag(poles[j]) == 0) {
        /* a real pole needs a real eigenvector, or A - B K comes out
           complex; the real part, in which the largest entry survives,
           still lies in S_j */
        for (i = 0; i < n; i++) col[i] = creal(col[i]);
        s = vnorm(n, col);
        for (i = 0; i < n; i++) col[i] /= s;
      }
      for (i = 0; i < n; i++) {
        double d = cabs(col[i] - X[(size_t)j * n + i]);
        if (d > moved) moved = d;
      }
      memcpy(X + (size_t)j * n, col, sizeof(cplx) * (size_t)n);
      if (partner[j] >= 0)
        for (i = 0; i < n; i++) X[(size_t)partner[j] * n + i] = conj(col[i]);
    }
    if (moved < 1e-10) break;
  }
  if (!invert(n, X, Xi) || norm1(n, X, 1) * norm1(n, Xi, 0) > 1e12) {
    rc = fail(XPPE_ERR_NUMERIC, "cannot place these poles: a pole is repeated "
                                "more often than there are inputs");
    goto done;
  }
  /* M = X diag(poles) X^-1 = A - B K */
  for (i = 0; i < n; i++)
    for (l = 0; l < n; l++) {
      cplx s = 0;
      for (j = 0; j < n; j++) s += X[(size_t)j * n + i] * poles[j] * Xi[j * n + l];
      Mr[i * n + l] = creal(s);
      if (fabs(cimag(s)) > imag_big) imag_big = fabs(cimag(s));
      if (fabs(creal(s)) > real_big) real_big = fabs(creal(s));
    }
  if (imag_big > 1e-8 * (real_big > 1 ? real_big : 1)) {
    rc = fail(XPPE_ERR_NUMERIC, "cannot place these poles: the gain is not real");
    goto done;
  }
  /* B K = A - M = U0 Z K, so K = Z^+ U0^T (A - M), Z^+ = Z^T (Z Z^T)^-1 */
  for (a = 0; a < r; a++)
    for (l = 0; l < n; l++) {
      double s = 0;
      for (i = 0; i < n; i++)
        s += creal(U[(size_t)a * n + i]) * (A[i * n + l] - Mr[i * n + l]);
      Y[a * n + l] = s;
    }
  for (a = 0; a < r; a++)
    for (i = 0; i < r; i++) {
      double s = 0;
      for (j = 0; j < m; j++) s += Z[a * m + j] * Z[i * m + j];
      ZZ[a * r + i] = s;
    }
  if (!solve_real(r, ZZ, n, Y)) {
    rc = fail(XPPE_ERR_NUMERIC, "cannot place poles: the inputs are dependent");
    goto done;
  }
  for (j = 0; j < m; j++)
    for (l = 0; l < n; l++) {
      double s = 0;
      for (a = 0; a < r; a++) s += Z[a * m + j] * Y[a * n + l];
      K[j * n + l] = s;
    }
done:
  free(U);
  free(Mr);
  free(dim);
  return rc;
}

int xppe_place_poles(int n, int m, const double *A, const double *B,
                     const double *re, const double *im, double *K) {
  cplx *poles;
  int *partner, i, rc, rank;
  double *C;
  if (n < 1 || m < 0 || !A || !re || !im || (m > 0 && (!B || !K)))
    return fail(XPPE_ERR_RANGE, "give A (n x n), B (n x m), n poles and room "
                                "for K (m x n)");
  if (m == 0)
    return fail(XPPE_ERR_RANGE, "cannot place poles: no inputs were chosen");
  for (i = 0; i < n; i++)
    if (!isfinite(re[i]) || !isfinite(im[i]))
      return fail(XPPE_ERR_RANGE, "the poles must be finite numbers");
  C = malloc(sizeof(double) * ((size_t)n * n * m + n));
  poles = malloc(sizeof(cplx) * (size_t)n);
  partner = malloc(sizeof(int) * 2 * (size_t)n);
  if (!C || !poles || !partner) {
    free(C);
    free(poles);
    free(partner);
    return fail(XPPE_ERR_RANGE, "out of memory");
  }
  for (i = 0; i < n; i++) poles[i] = re[i] + I * im[i];
  rc = pair_poles(n, poles, partner, partner + n);
  if (rc == XPPE_OK) {
    rank = xppe_controllability(n, m, A, B, 0.0, C, C + (size_t)n * n * m);
    if (rank < 0) {
      rc = rank;
    } else if (rank < n) {
      char text[300];
      snprintf(text, sizeof text, "cannot place poles: the pair (A, B) is not "
               "controllable (rank %d of %d states); the poles of the "
               "uncontrollable part cannot be moved", rank, n);
      rc = fail(XPPE_ERR_NUMERIC, text);
    } else if (m == 1) {
      rc = ackermann(n, A, B, poles, partner + n, K);
    } else {
      rc = kautsky(n, m, A, B, poles, partner, partner + n, K);
    }
  }
  free(C);
  free(poles);
  free(partner);
  return rc;
}

/* ---- The closed-loop model ------------------------------------------------- */

/* A growing piece of text. */
typedef struct {
  char *s;
  size_t len, cap;
  int oom;
} Text;

static void put(Text *t, const char *s, size_t k) {
  if (t->oom) return;
  if (t->len + k + 1 > t->cap) {
    size_t cap = t->cap ? t->cap : 256;
    char *grown;
    while (t->len + k + 1 > cap) cap *= 2;
    grown = realloc(t->s, cap);
    if (!grown) {
      t->oom = 1;
      return;
    }
    t->s = grown;
    t->cap = cap;
  }
  memcpy(t->s + t->len, s, k);
  t->len += k;
  t->s[t->len] = '\0';
}

static void put_str(Text *t, const char *s) { put(t, s, strlen(s)); }

/* The shortest text that reads back as the same double (Python's repr,
   but a whole number has no ".0"). */
static void put_num(Text *t, double v) {
  char buf[40];
  int p;
  for (p = 1; p <= 17; p++) {
    snprintf(buf, sizeof buf, "%.*g", p, v);
    if (strtod(buf, NULL) == v) break;
  }
  put_str(t, buf);
}

static int same_n(const char *a, const char *b, size_t k) {
  size_t i;
  for (i = 0; i < k; i++)
    if (toupper((unsigned char)a[i]) != toupper((unsigned char)b[i])) return 0;
  return 1;
}

/* The lines of the model, without their line ends. A NULL line marks
   where a feedback law goes. */
typedef struct {
  char **line;
  int n, cap;
} Lines;

static int add_line(Lines *L, char *s) {
  if (L->n == L->cap) {
    int cap = L->cap ? 2 * L->cap : 64;
    char **grown = realloc(L->line, sizeof(char *) * (size_t)cap);
    if (!grown) return 0;
    L->line = grown;
    L->cap = cap;
  }
  L->line[L->n++] = s;
  return 1;
}

static void free_lines(Lines *L) {
  int i;
  for (i = 0; i < L->n; i++) free(L->line[i]);
  free(L->line);
  L->line = NULL;
  L->n = L->cap = 0;
}

static char *copy_span(const char *s, size_t k) {
  char *c = malloc(k + 1);
  if (c) {
    memcpy(c, s, k);
    c[k] = '\0';
  }
  return c;
}

/* A parameter line, as Python's _PAR_LINE matched it: blanks, the word p,
   par, param, params, parameter or parameters (any case), not followed by
   blanks and =, ( or ' (a variable, a function or an equation of that
   name). *word receives the end of the word, *body where the rest starts
   after blanks. */
static int par_line(const char *s, size_t *word, const char **body) {
  static const char *const words[] = {"p", "par", "param", "params", "parameter",
                                      "parameters"};
  const char *w, *e;
  size_t k, i;
  for (w = s; isspace((unsigned char)*w); w++)
    ;
  for (e = w; isalnum((unsigned char)*e) || *e == '_'; e++)
    ;
  k = (size_t)(e - w);
  for (i = 0; i < sizeof words / sizeof *words; i++)
    if (strlen(words[i]) == k && same_n(w, words[i], k)) break;
  if (i == sizeof words / sizeof *words) return 0;
  while (isspace((unsigned char)*e)) e++;
  if (*e == '=' || *e == '(' || *e == '\'') return 0;
  *word = (size_t)(w - s) + k;
  *body = e;
  return 1;
}

/* The items of a parameter line's body: blanks around = taken out, then
   split at commas and blanks. */
static char *items_text(const char *body) {
  Text t = {0};
  const char *c;
  put(&t, "", 0);
  for (c = body; *c; c++) {
    if (isspace((unsigned char)*c)) {
      const char *d = c;
      while (isspace((unsigned char)*d)) d++;
      if (*d == '=') {
        c = d - 1;
        continue;
      }
    }
    put(&t, c, 1);
    if (*c == '=')
      while (isspace((unsigned char)c[1])) c++;
  }
  if (t.oom) {
    free(t.s);
    return NULL;
  }
  return t.s;
}

/* Take parameter name off its parameter line (as Python's
   _remove_parameter() did); a NULL line follows each line it was taken
   from. */
static int remove_parameter(Lines *L, const char *name) {
  Lines out = {0};
  int i, found = 0;
  for (i = 0; i < L->n; i++) {
    char *line = L->line[i], *items;
    const char *body, *c;
    size_t word, e;
    Text kept = {0};
    int dropped = 0;
    if (!line || !par_line(line, &word, &body)) {
      if (!add_line(&out, line)) goto oom;
      L->line[i] = NULL;
      continue;
    }
    if (!(items = items_text(body))) goto oom;
    put(&kept, "", 0);
    for (c = items; *c;) {
      const char *end, *eq;
      size_t k, nk;
      while (*c == ',' || isspace((unsigned char)*c)) c++;
      if (!*c) break;
      for (end = c; *end && *end != ',' && !isspace((unsigned char)*end); end++)
        ;
      k = (size_t)(end - c);
      eq = memchr(c, '=', k);
      nk = eq ? (size_t)(eq - c) : k;
      if (nk == strlen(name) && same_n(c, name, nk)) {
        dropped = 1;
      } else {
        if (kept.len) put_str(&kept, ", ");
        put(&kept, c, k);
      }
      c = end;
    }
    free(items);
    if (kept.oom) {
      free(kept.s);
      goto oom;
    }
    if (!dropped) {
      free(kept.s);
      if (!add_line(&out, line)) goto oom;
      L->line[i] = NULL;
      continue;
    }
    found = 1;
    e = strlen(line);
    while (e > 0 && isspace((unsigned char)line[e - 1])) e--;
    if (e > 0 && line[e - 1] == '\\') {
      free(kept.s);
      free_lines(&out);
      return fail(XPPE_ERR_RANGE, "the parameter line of an input is continued "
                                  "with a backslash and cannot be rewritten");
    }
    if (kept.len) {
      Text t = {0};
      put(&t, line, word);
      put_str(&t, " ");
      put_str(&t, kept.s);
      if (t.oom || !add_line(&out, t.s)) {
        free(t.s);
        free(kept.s);
        goto oom;
      }
    }
    free(kept.s);
    if (!add_line(&out, NULL)) goto oom; /* the feedback law goes here */
  }
  if (!found) {
    char text[400];
    /* give the lines back as they were */
    free(L->line);
    *L = out;
    snprintf(text, sizeof text, "input '%s' is not declared on a line starting "
             "with p, par, param or parameters, so the feedback law cannot "
             "replace it", name);
    return fail(XPPE_ERR_NAME, text);
  }
  free_lines(L);
  *L = out;
  return XPPE_OK;
oom:
  free_lines(&out);
  return fail(XPPE_ERR_RANGE, "out of memory");
}

/* XPPAUT reads a model line into 1024 characters, so a law longer than
   this is split: partial sums of a few terms each, then sums of those. */
#define LAW_LINE 900
#define TERMS_PER_PART 8
#define NAMES_PER_SUM 48

/* Whether an identifier of source starts with prefix (ignoring case). */
static int prefix_taken(const char *source, const char *prefix) {
  const size_t k = strlen(prefix);
  const char *s = source;
  while (*s) {
    if (isalpha((unsigned char)*s) || *s == '_') {
      const char *w = s;
      while (isalnum((unsigned char)*s) || *s == '_') s++;
      if ((size_t)(s - w) >= k && same_n(w, prefix, k)) return 1;
    } else {
      s++;
    }
  }
  return 0;
}

static void put_term(Text *t, double k, const char *state, double x) {
  put_str(t, "-(");
  put_num(t, k);
  put_str(t, ")*(");
  put_str(t, state);
  put_str(t, "-(");
  put_num(t, x);
  put_str(t, "))");
}

static void put_part_name(Text *t, const char *prefix, int input, int level, int k) {
  char name[64];
  snprintf(name, sizeof name, "%s%d%c%d", prefix, input + 1, 'a' + level, k + 1);
  put_str(t, name);
}

/* The law u = u0 - K (x - x*) of input j, on one line when it fits, else
   as partial sums kf1a1, kf1a2, ... (sums of sums kf1b1, ... when there are
   many) and u = u0 + kf1a1 + ... */
static void put_law(Text *t, const char *prefix, int j, const char *input, double u0,
                    int n, const char *const *states, const double *x,
                    const double *Krow) {
  Text one = {0};
  int l, count = 0, level = 0;
  put_str(&one, input);
  put_str(&one, "=");
  put_num(&one, u0);
  for (l = 0; l < n; l++) put_term(&one, Krow[l], states[l], x[l]);
  if (!one.oom && one.len <= LAW_LINE) {
    put(t, one.s, one.len);
    put_str(t, "\n");
    free(one.s);
    return;
  }
  free(one.s);
  for (l = 0; l < n; l += TERMS_PER_PART) {
    int q;
    put_part_name(t, prefix, j, 0, count++);
    put_str(t, "=");
    for (q = l; q < n && q < l + TERMS_PER_PART; q++) put_term(t, Krow[q], states[q], x[q]);
    put_str(t, "\n");
  }
  while (count > NAMES_PER_SUM) {
    int next = 0, g, q;
    for (g = 0; g < count; g += NAMES_PER_SUM) {
      put_part_name(t, prefix, j, level + 1, next++);
      put_str(t, "=");
      for (q = g; q < count && q < g + NAMES_PER_SUM; q++) {
        if (q > g) put_str(t, "+");
        put_part_name(t, prefix, j, level, q);
      }
      put_str(t, "\n");
    }
    count = next;
    level++;
  }
  put_str(t, input);
  put_str(t, "=");
  put_num(t, u0);
  for (l = 0; l < count; l++) {
    put_str(t, "+");
    put_part_name(t, prefix, j, level, l);
  }
  put_str(t, "\n");
}

int xppe_closed_loop(const char *source, int m, const char *const *inputs,
                     const double *u0, int n, const char *const *states,
                     const double *x, const double *K, char *out, int size) {
  Lines L = {0};
  Text t = {0};
  const char *s, *e;
  char prefix[32] = "kf";
  int i, j, rc = XPPE_OK, first, len;
  if (!source || m < 1 || !inputs || !u0 || n < 1 || !states || !x || !K ||
      size < 0 || (size > 0 && !out))
    return fail(XPPE_ERR_RANGE, "give the model text, the inputs with their "
                                "values, the states with the point, and K");
  /* the lines, as Python's splitlines() cuts them at \n, \r\n or \r */
  for (s = source; *s; s = e) {
    char *line;
    for (e = s; *e && *e != '\n' && *e != '\r'; e++)
      ;
    if (!(line = copy_span(s, (size_t)(e - s))) || !add_line(&L, line)) {
      free(line);
      rc = fail(XPPE_ERR_RANGE, "out of memory");
      goto done;
    }
    if (*e == '\r' && e[1] == '\n') e++;
    if (*e) e++;
  }
  for (i = 0; i < m; i++)
    if ((rc = remove_parameter(&L, inputs[i])) != XPPE_OK) goto done;
  /* XPPAUT evaluates fixed variables in the order written, and a fixed
     variable may use an input before its parameter line. The laws use only
     the states, so they stand first, after the comments that open the file:
     everything that uses an input then sees the feedback. */
  first = L.n;
  for (i = 0; i < L.n && first == L.n; i++) {
    const char *c = L.line[i];
    if (!c) continue;
    while (isspace((unsigned char)*c)) c++;
    if (*c && *c != '#') first = i;
  }
  for (i = 0; i < 8 && prefix_taken(source, prefix); i++) strcat(prefix, "x");
  for (i = 0; i <= L.n; i++) {
    if (i == first) {
      put_str(&t, "# feedback u = u0 - K (x - x*), from place_poles\n");
      for (j = 0; j < m; j++)
        put_law(&t, prefix, j, inputs[j], u0[j], n, states, x, K + (size_t)j * n);
    }
    if (i < L.n && L.line[i]) {
      put_str(&t, L.line[i]);
      put_str(&t, "\n");
    }
  }
  if (t.oom || !t.s) {
    rc = fail(XPPE_ERR_RANGE, "out of memory");
    goto done;
  }
  if (t.len > 0x7ffffff0u) {
    rc = fail(XPPE_ERR_RANGE, "the model is too long");
    goto done;
  }
  len = (int)t.len;
  if (size > 0) {
    size_t k = t.len < (size_t)size - 1 ? t.len : (size_t)size - 1;
    memcpy(out, t.s, k);
    out[k] = '\0';
  }
  rc = len;
done:
  free_lines(&L);
  free(t.s);
  return rc;
}
