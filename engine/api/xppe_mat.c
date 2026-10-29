/*
 * MATLAB Level 5 MAT-files for the engine API (see xppe.h): the desktop app
 * and Python's to_mat() both call xppe_write_mat(), so they write the same
 * bytes and neither needs SciPy.
 *
 * The file is little-endian ("IM"), uncompressed, with every data element
 * padded to 8 bytes. The header text is fixed (no date, host or version), so
 * the same data always gives the same file. Each column is a double column
 * vector; the parameters, if any, are the fields of a 1-by-1 struct named
 * "params". The rules for turning names into MATLAB names are in
 * docs/manual/ (xppe_mat_name() below does exactly that).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xppe.h"
#include "xppe_internal.h"

#define MAT_NAME_MAX 63
#define MAT_FIELD_LEN (MAT_NAME_MAX + 1)

enum { MI_INT8 = 1, MI_INT32 = 5, MI_UINT32 = 6, MI_DOUBLE = 9, MI_MATRIX = 14 };
enum { MX_STRUCT = 2, MX_DOUBLE = 6 };

typedef struct {
  FILE *f;
  int ok;
} Out;

static void put_bytes(Out *o, const void *p, size_t n) {
  if (o->ok && n && fwrite(p, 1, n, o->f) != n) o->ok = 0;
}

static void put_u32(Out *o, uint32_t v) {
  unsigned char b[4];
  b[0] = (unsigned char)(v & 0xff);
  b[1] = (unsigned char)((v >> 8) & 0xff);
  b[2] = (unsigned char)((v >> 16) & 0xff);
  b[3] = (unsigned char)((v >> 24) & 0xff);
  put_bytes(o, b, 4);
}

static void put_double(Out *o, double d) {
  uint64_t u;
  unsigned char b[8];
  int i;
  memcpy(&u, &d, 8);
  for (i = 0; i < 8; i++) b[i] = (unsigned char)((u >> (8 * i)) & 0xff);
  put_bytes(o, b, 8);
}

static void put_zeros(Out *o, size_t n) {
  static const unsigned char z[8] = {0};
  put_bytes(o, z, n);
}

static size_t pad8(size_t n) { return (n + 7) & ~(size_t)7; }

/* MATLAB's iskeyword list: such a name cannot be a variable or a field. */
static int is_keyword(const char *s) {
  static const char *const words[] = {
      "break", "case",   "catch",  "classdef", "continue", "else",   "elseif",
      "end",   "for",    "function", "global", "if",       "otherwise", "parfor",
      "persistent", "return", "spmd", "switch", "try",     "while"};
  size_t i;
  for (i = 0; i < sizeof words / sizeof *words; i++)
    if (strcmp(s, words[i]) == 0) return 1;
  return 0;
}

/* A MATLAB name from any text: every character that is not a letter, digit
   or underscore becomes "_" (a non-ASCII character, however many bytes of
   UTF-8, becomes one); if the result does not start with a letter, "x" goes
   in front; at most 63 characters are kept; a MATLAB keyword gets "_" added. */
static void clean_name(const char *in, char *out) {
  size_t n = 0;
  const unsigned char *p;
  char tmp[2 * MAT_NAME_MAX + 4];
  for (p = (const unsigned char *)in; *p && n < MAT_NAME_MAX + 1; p++) {
    if ((*p >= 0x80 && *p < 0xC0)) continue; /* UTF-8 continuation byte */
    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
        *p == '_')
      tmp[n++] = (char)*p;
    else
      tmp[n++] = '_';
  }
  tmp[n] = 0;
  if (!((tmp[0] >= 'a' && tmp[0] <= 'z') || (tmp[0] >= 'A' && tmp[0] <= 'Z'))) {
    memmove(tmp + 1, tmp, n + 1);
    tmp[0] = 'x';
    n++;
  }
  if (n > MAT_NAME_MAX) n = MAT_NAME_MAX;
  memcpy(out, tmp, n);
  out[n] = 0;
  if (is_keyword(out)) strcat(out, "_");
}

/* Make names[i] unique among names[0..i-1] and the extra reserved name:
   add _2, _3, ... (cutting the name so that the suffix fits). */
static void make_unique(char (*names)[MAT_FIELD_LEN], int i, const char *reserved) {
  char base[MAT_FIELD_LEN], suffix[16];
  int k, j, clash;
  strcpy(base, names[i]);
  for (k = 1;; k++) {
    clash = reserved && strcmp(names[i], reserved) == 0;
    for (j = 0; j < i && !clash; j++) clash = strcmp(names[j], names[i]) == 0;
    if (!clash) return;
    snprintf(suffix, sizeof suffix, "_%d", k + 1);
    {
      size_t keep = strlen(base), sl = strlen(suffix);
      if (keep + sl > MAT_NAME_MAX) keep = MAT_NAME_MAX - sl;
      memcpy(names[i], base, keep);
      strcpy(names[i] + keep, suffix);
    }
  }
}

static void clean_names(int n, const char *const *in, char (*out)[MAT_FIELD_LEN],
                        const char *reserved) {
  int i;
  for (i = 0; i < n; i++) {
    clean_name(in[i] ? in[i] : "", out[i]);
    make_unique(out, i, reserved);
  }
}

/* The size of a name element: up to 4 characters fit in its tag. */
static size_t name_element_size(size_t len) { return len <= 4 ? 8 : 8 + pad8(len); }

static void put_name(Out *o, const char *name) {
  size_t len = strlen(name);
  if (len == 0) {
    put_u32(o, MI_INT8);
    put_u32(o, 0);
  } else if (len <= 4) {
    put_u32(o, ((uint32_t)len << 16) | MI_INT8);
    put_bytes(o, name, len);
    put_zeros(o, 4 - len);
  } else {
    put_u32(o, MI_INT8);
    put_u32(o, (uint32_t)len);
    put_bytes(o, name, len);
    put_zeros(o, pad8(len) - len);
  }
}

/* The tag, array flags and dimensions that start a matrix of class `cls`
   with body_size bytes after the tag, then its name. */
static void put_matrix_start(Out *o, size_t body_size, int cls, const char *name,
                             uint32_t rows, uint32_t cols) {
  put_u32(o, MI_MATRIX);
  put_u32(o, (uint32_t)body_size);
  put_u32(o, MI_UINT32);
  put_u32(o, 8);
  put_u32(o, (uint32_t)cls);
  put_u32(o, 0);
  put_u32(o, MI_INT32);
  put_u32(o, 8);
  put_u32(o, rows);
  put_u32(o, cols);
  put_name(o, name);
}

static size_t double_body_size(size_t namelen, size_t count) {
  return 16 + 16 + name_element_size(namelen) + 8 + 8 * count;
}

static void put_double_matrix(Out *o, const char *name, const double *v, int count) {
  int i;
  put_matrix_start(o, double_body_size(strlen(name), (size_t)count), MX_DOUBLE, name,
                   (uint32_t)count, 1);
  put_u32(o, MI_DOUBLE);
  put_u32(o, (uint32_t)(8 * (size_t)count));
  for (i = 0; i < count; i++) put_double(o, v[i]);
}

int xppe_write_mat(const char *path, int ncols, const char *const *names,
                   const double *const *cols, int nrows, int nparams,
                   const char *const *pnames, const double *pvalues) {
  static const char text[] = "MATLAB 5.0 MAT-file, written by XPP Lab";
  char (*cn)[MAT_FIELD_LEN] = NULL;
  char (*pn)[MAT_FIELD_LEN] = NULL;
  Out o;
  size_t total, nf;
  int i, rc = XPPE_OK;

  if (!path || ncols < 0 || nrows < 0 || nparams < 0 || (ncols > 0 && (!names || !cols)) ||
      (nparams > 0 && (!pnames || !pvalues))) {
    xppe_set_error("%s", "xppe_write_mat: bad arguments");
    return XPPE_ERR_RANGE;
  }
  for (i = 0; i < ncols; i++)
    if (!cols[i] && nrows > 0) {
      xppe_set_error("%s", "xppe_write_mat: a column has no data");
      return XPPE_ERR_RANGE;
    }
  /* each element's size must fit the 32-bit size fields */
  if ((double)nrows * 8.0 + 1024.0 > 4.0e9 ||
      (double)ncols * ((double)nrows * 8.0 + 1024.0) > 4.0e9) {
    xppe_set_error("%s", "xppe_write_mat: the data is too large for a MAT-file");
    return XPPE_ERR_RANGE;
  }
  cn = calloc((size_t)(ncols > 0 ? ncols : 1), MAT_FIELD_LEN);
  pn = calloc((size_t)(nparams > 0 ? nparams : 1), MAT_FIELD_LEN);
  if (!cn || !pn) {
    free(cn);
    free(pn);
    xppe_set_error("%s", "xppe_write_mat: out of memory");
    return XPPE_ERR_STATE;
  }
  clean_names(ncols, names, cn, nparams > 0 ? "params" : NULL);
  clean_names(nparams, pnames, pn, NULL);

  o.f = xppe_fopen_utf8(path, "wb");
  o.ok = o.f != NULL;
  if (!o.f) {
    free(cn);
    free(pn);
    xppe_set_error("cannot write %s", path);
    return XPPE_ERR_LOAD;
  }
  /* the 128-byte header */
  {
    char head[116];
    memset(head, ' ', sizeof head);
    memcpy(head, text, sizeof text - 1);
    put_bytes(&o, head, sizeof head);
    put_zeros(&o, 8);            /* no subsystem data */
    put_bytes(&o, "\x00\x01", 2); /* version 0x0100 */
    put_bytes(&o, "IM", 2);       /* little-endian */
  }
  for (i = 0; i < ncols; i++) put_double_matrix(&o, cn[i], cols[i], nrows);

  if (nparams > 0) {
    nf = (size_t)nparams;
    total = 16 + 16 + name_element_size(6) + 8 + (8 + pad8(nf * MAT_FIELD_LEN));
    total += nf * (8 + double_body_size(0, 1));
    put_matrix_start(&o, total, MX_STRUCT, "params", 1, 1);
    put_u32(&o, ((uint32_t)4 << 16) | MI_INT32); /* field name length, small element */
    put_u32(&o, MAT_FIELD_LEN);
    put_u32(&o, MI_INT8);
    put_u32(&o, (uint32_t)(nf * MAT_FIELD_LEN));
    for (i = 0; i < nparams; i++) put_bytes(&o, pn[i], MAT_FIELD_LEN);
    put_zeros(&o, pad8(nf * MAT_FIELD_LEN) - nf * MAT_FIELD_LEN);
    for (i = 0; i < nparams; i++) put_double_matrix(&o, "", pvalues + i, 1);
  }
  if (fclose(o.f) != 0) o.ok = 0;
  if (!o.ok) {
    xppe_set_error("cannot write %s", path);
    rc = XPPE_ERR_LOAD;
  }
  free(cn);
  free(pn);
  return rc;
}
