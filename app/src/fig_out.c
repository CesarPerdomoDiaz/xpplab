/* Figures: the outputs (fig.h). PNG through a rasteriser of the same paths
   (stb_truetype's, which fills letters too), SVG, PDF and EPS as vectors. */
#include "fig.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* fig_font.c */
void *fig_shape_new(void);
void fig_shape_free(void *sh);
void fig_shape_clear(void *sh);
int fig_shape_count(void *sh);
void fig_shape_add(void *sh, int type, const float *p);
void fig_shape_fill(void *sh, unsigned char *rgba, int w, int h, float scale,
                    const float rgb[3], const int clip[4]);

/* stb_image_write (compiled in export.c) */
unsigned char *stbi_write_png_to_mem(const unsigned char *pixels, int stride_bytes, int x,
                                     int y, int n, int *out_len);
unsigned char *stbi_zlib_compress(unsigned char *data, int data_len, int *out_len,
                                  int quality);

/* A file by its UTF-8 name, also on Windows. */
static FILE *open_utf8(const char *path, const char *mode) {
#ifdef _WIN32
  wchar_t wp[1024], wm[8];
  if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, 1024) <= 0) return NULL;
  if (MultiByteToWideChar(CP_UTF8, 0, mode, -1, wm, 8) <= 0) return NULL;
  return _wfopen(wp, wm);
#else
  return fopen(path, mode);
#endif
}

/* ---- Flattening ------------------------------------------------------------------ */

typedef struct {
  float *p; /* x, y pairs */
  int n, cap;
} Poly;

static void poly_add(Poly *q, float x, float y) {
  if (q->n == q->cap) {
    int cap = q->cap ? 2 * q->cap : 256;
    float *t = realloc(q->p, sizeof(float) * 2 * (size_t)cap);
    if (!t) return;
    q->p = t;
    q->cap = cap;
  }
  q->p[2 * q->n] = x;
  q->p[2 * q->n + 1] = y;
  q->n++;
}

/* The subpaths of an op as polylines (curves cut into short lines), each
   handed to fn. */
static void each_polyline(const FigDL *dl, const FigOp *o, float tol,
                          void (*fn)(void *, const Poly *, bool), void *user) {
  Poly q = {0};
  float cx = 0, cy = 0, sx = 0, sy = 0;
  int i, k;
  bool closed = false;
  for (i = o->first; i < o->first + o->count; i++) {
    const FigCmd *c = &dl->cmd[i];
    switch (c->type) {
    case FIG_MOVE:
      if (q.n > 1) fn(user, &q, closed);
      q.n = 0;
      closed = false;
      poly_add(&q, c->p[0], c->p[1]);
      cx = sx = c->p[0];
      cy = sy = c->p[1];
      break;
    case FIG_LINE:
      poly_add(&q, c->p[0], c->p[1]);
      cx = c->p[0];
      cy = c->p[1];
      break;
    case FIG_QUAD:
    case FIG_CUBIC: {
      float x0 = cx, y0 = cy, x1, y1, x2, y2, x3, y3, len;
      int n;
      if (c->type == FIG_QUAD) {
        x1 = x0 + 2.0f / 3 * (c->p[0] - x0);
        y1 = y0 + 2.0f / 3 * (c->p[1] - y0);
        x3 = c->p[2];
        y3 = c->p[3];
        x2 = x3 + 2.0f / 3 * (c->p[0] - x3);
        y2 = y3 + 2.0f / 3 * (c->p[1] - y3);
      } else {
        x1 = c->p[0];
        y1 = c->p[1];
        x2 = c->p[2];
        y2 = c->p[3];
        x3 = c->p[4];
        y3 = c->p[5];
      }
      len = fabsf(x1 - x0) + fabsf(y1 - y0) + fabsf(x2 - x1) + fabsf(y2 - y1) +
            fabsf(x3 - x2) + fabsf(y3 - y2);
      n = (int)(len / tol) + 1;
      if (n > 64) n = 64;
      for (k = 1; k <= n; k++) {
        float t = (float)k / n, u = 1 - t;
        poly_add(&q, u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3,
                 u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3);
      }
      cx = x3;
      cy = y3;
      break;
    }
    case FIG_CLOSE:
      if (q.n > 0 && (q.p[2 * q.n - 2] != sx || q.p[2 * q.n - 1] != sy)) poly_add(&q, sx, sy);
      closed = true;
      cx = sx;
      cy = sy;
      break;
    }
  }
  if (q.n > 1) fn(user, &q, closed);
  free(q.p);
}

/* ---- Raster ----------------------------------------------------------------------- */

typedef struct {
  void *shape;
  float hw;      /* half the stroke width, points */
  float dash[4];
  float scale;
} StrokeCtx;

/* A convex polygon, turned to go one way round so overlaps add up. */
static void add_poly(void *shape, const float *xy, int n) {
  float area = 0;
  int i;
  for (i = 0; i < n; i++) {
    int j = (i + 1) % n;
    area += xy[2 * i] * xy[2 * j + 1] - xy[2 * j] * xy[2 * i + 1];
  }
  for (i = 0; i < n; i++) {
    int k = area >= 0 ? i : n - 1 - i;
    fig_shape_add(shape, i == 0 ? FIG_MOVE : FIG_LINE, &xy[2 * k]);
  }
  fig_shape_add(shape, FIG_LINE, &xy[2 * (area >= 0 ? 0 : n - 1)]);
}

static void stroke_segment(StrokeCtx *s, float x0, float y0, float x1, float y1) {
  float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy), nx, ny, q[8];
  if (l <= 0) return;
  nx = -dy / l * s->hw;
  ny = dx / l * s->hw;
  q[0] = x0 + nx;
  q[1] = y0 + ny;
  q[2] = x1 + nx;
  q[3] = y1 + ny;
  q[4] = x1 - nx;
  q[5] = y1 - ny;
  q[6] = x0 - nx;
  q[7] = y0 - ny;
  add_poly(s->shape, q, 4);
}

/* A small octagon at a corner, so that thick bends have no notches. */
static void stroke_join(StrokeCtx *s, float x, float y) {
  float q[16];
  int i;
  for (i = 0; i < 8; i++) {
    q[2 * i] = x + s->hw * cosf(i * 0.785398f);
    q[2 * i + 1] = y + s->hw * sinf(i * 0.785398f);
  }
  add_poly(s->shape, q, 8);
}

static void stroke_poly(void *user, const Poly *q, bool closed) {
  StrokeCtx *s = user;
  bool joins = s->hw * s->scale > 1.0f;
  bool dashed = s->dash[0] > 0 && s->dash[0] + s->dash[1] + s->dash[2] + s->dash[3] > 0;
  int i, k = 0; /* the dash pattern's part we are in, and what is left of it */
  double rem = s->dash[0];
  (void)closed;
  for (i = 0; i + 1 < q->n; i++) {
    float x0 = q->p[2 * i], y0 = q->p[2 * i + 1], x1 = q->p[2 * i + 2], y1 = q->p[2 * i + 3];
    float dx = x1 - x0, dy = y1 - y0;
    double l = sqrt((double)dx * dx + (double)dy * dy), t = 0;
    if (!dashed) {
      stroke_segment(s, x0, y0, x1, y1);
      if (joins && i > 0) stroke_join(s, x0, y0);
      continue;
    }
    while (t < l) {
      double step = rem < l - t ? rem : l - t;
      if (k % 2 == 0 && step > 0)
        stroke_segment(s, (float)(x0 + dx * t / l), (float)(y0 + dy * t / l),
                       (float)(x0 + dx * (t + step) / l), (float)(y0 + dy * (t + step) / l));
      t += step;
      rem -= step;
      if (rem <= 1e-9) {
        int tries = 0;
        do {
          k = (k + 1) % 4;
          rem = s->dash[k];
        } while (rem <= 0 && ++tries < 4);
        if (rem <= 0) rem = 1;
      }
    }
  }
}

static void fill_poly_shape(void *user, const Poly *q, bool closed) {
  int i;
  (void)closed;
  for (i = 0; i < q->n; i++) fig_shape_add(user, i == 0 ? FIG_MOVE : FIG_LINE, &q->p[2 * i]);
}

static void bbox_poly(void *user, const Poly *q, bool closed) {
  float *b = user;
  int i;
  (void)closed;
  for (i = 0; i < q->n; i++) {
    if (q->p[2 * i] < b[0]) b[0] = q->p[2 * i];
    if (q->p[2 * i + 1] < b[1]) b[1] = q->p[2 * i + 1];
    if (q->p[2 * i] > b[2]) b[2] = q->p[2 * i];
    if (q->p[2 * i + 1] > b[3]) b[3] = q->p[2 * i + 1];
  }
}

unsigned char *fig_raster(const FigDL *dl, int w, int h) {
  unsigned char *rgba;
  void *shape;
  float scale;
  int i, clip[4] = {0, 0, w, h};
  if (w <= 0 || h <= 0 || dl->width <= 0 || dl->height <= 0) return NULL;
  rgba = malloc((size_t)w * (size_t)h * 4);
  shape = fig_shape_new();
  if (!rgba || !shape) {
    free(rgba);
    fig_shape_free(shape);
    return NULL;
  }
  memset(rgba, 255, (size_t)w * (size_t)h * 4);
  scale = w / dl->width < h / dl->height ? w / dl->width : h / dl->height;
  for (i = 0; i < dl->nop; i++) {
    const FigOp *o = &dl->op[i];
    if (o->op == FIG_OP_CLIP) {
      float b[4] = {1e30f, 1e30f, -1e30f, -1e30f};
      each_polyline(dl, o, 1, bbox_poly, b);
      clip[0] = (int)floorf(b[0] * scale);
      clip[1] = (int)floorf(b[1] * scale);
      clip[2] = (int)ceilf(b[2] * scale);
      clip[3] = (int)ceilf(b[3] * scale);
      continue;
    }
    if (o->op == FIG_OP_UNCLIP) {
      clip[0] = clip[1] = 0;
      clip[2] = w;
      clip[3] = h;
      continue;
    }
    fig_shape_clear(shape);
    if (o->op == FIG_OP_FILL) {
      int k;
      /* letters and markers keep their curves; the rasteriser flattens them */
      for (k = o->first; k < o->first + o->count; k++) {
        const FigCmd *c = &dl->cmd[k];
        if (c->type != FIG_CLOSE) fig_shape_add(shape, c->type, c->p);
      }
    } else {
      StrokeCtx s;
      float px = o->width * scale;
      s.shape = shape;
      s.scale = scale;
      /* lines thinner than a pixel are drawn one pixel wide */
      s.hw = (px < 1.0f ? 1.0f / scale : o->width) / 2;
      memcpy(s.dash, o->dash, sizeof s.dash);
      each_polyline(dl, o, 0.5f / scale, stroke_poly, &s);
    }
    if (fig_shape_count(shape) > 0)
      fig_shape_fill(shape, rgba, w, h, scale, o->rgb, clip);
  }
  fig_shape_free(shape);
  (void)fill_poly_shape;
  return rgba;
}

static unsigned long crc_table[256];

static unsigned long crc32_of(const unsigned char *buf, size_t len) {
  unsigned long c = 0xffffffffUL;
  size_t i;
  if (!crc_table[1]) {
    unsigned long n, k, v;
    for (n = 0; n < 256; n++) {
      v = n;
      for (k = 0; k < 8; k++) v = v & 1 ? 0xedb88320UL ^ (v >> 1) : v >> 1;
      crc_table[n] = v;
    }
  }
  for (i = 0; i < len; i++) c = crc_table[(c ^ buf[i]) & 0xff] ^ (c >> 8);
  return c ^ 0xffffffffUL;
}

static void put32(unsigned char *p, unsigned long v) {
  p[0] = (unsigned char)(v >> 24);
  p[1] = (unsigned char)(v >> 16);
  p[2] = (unsigned char)(v >> 8);
  p[3] = (unsigned char)v;
}

bool fig_write_png(const FigDL *dl, int dpi, const char *path) {
  int w, h, len = 0;
  unsigned char *rgba, *png, phys[21];
  FILE *f;
  bool ok;
  if (dpi < 10) dpi = 10;
  w = (int)(dl->width / 72.0f * dpi + 0.5f);
  h = (int)(dl->height / 72.0f * dpi + 0.5f);
  rgba = fig_raster(dl, w, h);
  if (!rgba) return false;
  png = stbi_write_png_to_mem(rgba, w * 4, w, h, 4, &len);
  free(rgba);
  if (!png) return false;
  /* a pHYs chunk after IHDR, so the image opens at its size on paper */
  put32(phys, 9);
  memcpy(phys + 4, "pHYs", 4);
  put32(phys + 8, (unsigned long)(dpi / 0.0254 + 0.5));
  put32(phys + 12, (unsigned long)(dpi / 0.0254 + 0.5));
  phys[16] = 1;
  put32(phys + 17, crc32_of(phys + 4, 13));
  f = open_utf8(path, "wb");
  if (!f) {
    free(png);
    return false;
  }
  ok = fwrite(png, 1, 33, f) == 33 && fwrite(phys, 1, 21, f) == 21 &&
       fwrite(png + 33, 1, (size_t)len - 33, f) == (size_t)len - 33;
  ok = (fclose(f) == 0) && ok;
  free(png);
  return ok;
}

/* ---- Vector paths ----------------------------------------------------------------- */

typedef struct {
  char *b;
  size_t n, cap;
} Buf;

static void bput(Buf *b, const char *fmt, ...) {
  va_list ap;
  int k;
  if (b->cap - b->n < 512) {
    size_t cap = b->cap ? 2 * b->cap : 65536;
    char *t = realloc(b->b, cap);
    if (!t) return;
    b->b = t;
    b->cap = cap;
  }
  va_start(ap, fmt);
  k = vsnprintf(b->b + b->n, b->cap - b->n, fmt, ap);
  va_end(ap);
  if (k > 0 && (size_t)k < b->cap - b->n) b->n += (size_t)k;
}

/* A number with at most two decimals, without trailing zeros. */
static const char *num(char *buf, float v) {
  char *e;
  snprintf(buf, 24, "%.2f", fabsf(v) < 0.005f ? 0.0f : v);
  e = buf + strlen(buf) - 1;
  while (*e == '0') *e-- = '\0';
  if (*e == '.') *e = '\0';
  return buf;
}

/* style 0: SVG; 1: PDF; 2: PostScript */
static void path_ops(Buf *b, const FigDL *dl, const FigOp *o, int style) {
  char a[24], c[24], d[24], e[24], g[24], h[24];
  float cx = 0, cy = 0, sx = 0, sy = 0;
  int i;
  for (i = o->first; i < o->first + o->count; i++) {
    const FigCmd *k = &dl->cmd[i];
    const float *p = k->p;
    float q[6];
    switch (k->type) {
    case FIG_MOVE:
      bput(b, style == 0 ? "M%s %s" : style == 1 ? "%s %s m\n" : "%s %s moveto\n",
           num(a, p[0]), num(c, p[1]));
      cx = sx = p[0];
      cy = sy = p[1];
      break;
    case FIG_LINE:
      bput(b, style == 0 ? "L%s %s" : style == 1 ? "%s %s l\n" : "%s %s lineto\n",
           num(a, p[0]), num(c, p[1]));
      cx = p[0];
      cy = p[1];
      break;
    case FIG_QUAD:
      if (style == 0) {
        bput(b, "Q%s %s %s %s", num(a, p[0]), num(c, p[1]), num(d, p[2]), num(e, p[3]));
        cx = p[2];
        cy = p[3];
        break;
      }
      q[0] = cx + 2.0f / 3 * (p[0] - cx);
      q[1] = cy + 2.0f / 3 * (p[1] - cy);
      q[2] = p[2] + 2.0f / 3 * (p[0] - p[2]);
      q[3] = p[3] + 2.0f / 3 * (p[1] - p[3]);
      q[4] = p[2];
      q[5] = p[3];
      p = q;
      /* fall through */
    case FIG_CUBIC:
      bput(b, style == 0 ? "C%s %s %s %s %s %s"
                         : style == 1 ? "%s %s %s %s %s %s c\n" : "%s %s %s %s %s %s curveto\n",
           num(a, p[0]), num(c, p[1]), num(d, p[2]), num(e, p[3]), num(g, p[4]), num(h, p[5]));
      cx = p[4];
      cy = p[5];
      break;
    case FIG_CLOSE:
      bput(b, style == 0 ? "Z" : style == 1 ? "h\n" : "closepath\n");
      cx = sx;
      cy = sy;
      break;
    }
  }
}

static bool save_buf(const Buf *b, const char *path) {
  FILE *f = open_utf8(path, "wb");
  bool ok;
  if (!f) return false;
  ok = fwrite(b->b, 1, b->n, f) == b->n;
  return (fclose(f) == 0) && ok;
}

static void hex(char *out, const float rgb[3]) {
  snprintf(out, 8, "#%02x%02x%02x", (int)(rgb[0] * 255 + 0.5f), (int)(rgb[1] * 255 + 0.5f),
           (int)(rgb[2] * 255 + 0.5f));
}

bool fig_write_svg(const FigDL *dl, const char *path) {
  Buf b = {0};
  char a[24], c[24], col[8];
  int i, depth = 0, nclip = 0;
  bool ok;
  bput(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%spt\" height=\"%spt\" "
           "viewBox=\"0 0 %s %s\">\n",
       num(a, dl->width), num(c, dl->height), num(a, dl->width), num(c, dl->height));
  bput(&b, "<!-- XPP Lab figure; text is drawn as outlines -->\n");
  for (i = 0; i < dl->nop; i++) {
    const FigOp *o = &dl->op[i];
    if (o->op == FIG_OP_UNCLIP) {
      if (depth > 0) {
        bput(&b, "</g>\n");
        depth--;
      }
      continue;
    }
    if (o->op == FIG_OP_CLIP) {
      bput(&b, "<clipPath id=\"c%d\"><path d=\"", nclip);
      path_ops(&b, dl, o, 0);
      bput(&b, "\"/></clipPath>\n<g clip-path=\"url(#c%d)\">\n", nclip++);
      depth++;
      continue;
    }
    hex(col, o->rgb);
    bput(&b, "<path d=\"");
    path_ops(&b, dl, o, 0);
    if (o->op == FIG_OP_FILL) {
      bput(&b, "\" fill=\"%s\"/>\n", col);
    } else {
      bput(&b, "\" fill=\"none\" stroke=\"%s\" stroke-width=\"%s\" stroke-linejoin=\"round\"",
           col, num(a, o->width));
      if (o->dash[0] > 0) {
        char d1[24], d2[24], d3[24], d4[24];
        bput(&b, " stroke-dasharray=\"%s %s %s %s\"", num(d1, o->dash[0]), num(d2, o->dash[1]),
             num(d3, o->dash[2]), num(d4, o->dash[3]));
      }
      bput(&b, "/>\n");
    }
  }
  while (depth-- > 0) bput(&b, "</g>\n");
  bput(&b, "</svg>\n");
  ok = b.b && save_buf(&b, path);
  free(b.b);
  return ok;
}

/* The page's drawing, in PDF (style 1) or PostScript (style 2) operators,
   in the figure's coordinates: the caller flips y. */
static void vector_body(Buf *b, const FigDL *dl, int style) {
  char a[24], c[24], d[24];
  int i;
  bput(b, style == 1 ? "1 j\n" : "1 setlinejoin\n");
  for (i = 0; i < dl->nop; i++) {
    const FigOp *o = &dl->op[i];
    if (o->op == FIG_OP_UNCLIP) {
      bput(b, style == 1 ? "Q\n" : "grestore\n");
      continue;
    }
    if (o->op == FIG_OP_CLIP) {
      bput(b, style == 1 ? "q\n" : "gsave newpath\n");
      path_ops(b, dl, o, style);
      bput(b, style == 1 ? "W n\n" : "clip newpath\n");
      continue;
    }
    if (style == 2) bput(b, "newpath\n");
    path_ops(b, dl, o, style);
    if (o->op == FIG_OP_FILL) {
      bput(b, style == 1 ? "%s %s %s rg f\n" : "%s %s %s setrgbcolor fill\n",
           num(a, o->rgb[0]), num(c, o->rgb[1]), num(d, o->rgb[2]));
    } else {
      char w[24];
      bput(b, style == 1 ? "%s %s %s RG %s w " : "%s %s %s setrgbcolor %s setlinewidth ",
           num(a, o->rgb[0]), num(c, o->rgb[1]), num(d, o->rgb[2]), num(w, o->width));
      if (o->dash[0] > 0) {
        char d1[24], d2[24], d3[24], d4[24];
        bput(b, "[%s %s %s %s] 0 %s ", num(d1, o->dash[0]), num(d2, o->dash[1]),
             num(d3, o->dash[2]), num(d4, o->dash[3]), style == 1 ? "d" : "setdash");
      } else
        bput(b, style == 1 ? "[] 0 d " : "[] 0 setdash ");
      bput(b, style == 1 ? "S\n" : "stroke\n");
    }
  }
}

bool fig_write_pdf(const FigDL *dl, const char *path) {
  Buf body = {0}, pdf = {0};
  unsigned char *z;
  int zlen = 0, i;
  long off[6];
  char w[24], h[24];
  bool ok;
  bput(&body, "1 0 0 -1 0 %s cm\n", num(h, dl->height));
  vector_body(&body, dl, 1);
  if (!body.b) return false;
  z = stbi_zlib_compress((unsigned char *)body.b, (int)body.n, &zlen, 8);
  free(body.b);
  if (!z) return false;
  bput(&pdf, "%%PDF-1.4\n%%\xe2\xe3\xcf\xd3\n");
  off[1] = (long)pdf.n;
  bput(&pdf, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
  off[2] = (long)pdf.n;
  bput(&pdf, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
  off[3] = (long)pdf.n;
  bput(&pdf, "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %s %s] "
             "/Contents 4 0 R /Resources << >> >>\nendobj\n",
       num(w, dl->width), num(h, dl->height));
  off[4] = (long)pdf.n;
  bput(&pdf, "4 0 obj\n<< /Length %d /Filter /FlateDecode >>\nstream\n", zlen);
  /* the compressed stream may be longer than one bput */
  while (pdf.cap - pdf.n < (size_t)zlen + 1024) {
    size_t cap = pdf.cap * 2 + (size_t)zlen;
    char *t = realloc(pdf.b, cap);
    if (!t) {
      free(z);
      free(pdf.b);
      return false;
    }
    pdf.b = t;
    pdf.cap = cap;
  }
  memcpy(pdf.b + pdf.n, z, (size_t)zlen);
  pdf.n += (size_t)zlen;
  free(z);
  bput(&pdf, "\nendstream\nendobj\n");
  off[5] = (long)pdf.n;
  bput(&pdf, "5 0 obj\n<< /Producer (XPP Lab) >>\nendobj\n");
  {
    long xref = (long)pdf.n;
    bput(&pdf, "xref\n0 6\n0000000000 65535 f \n");
    for (i = 1; i <= 5; i++) bput(&pdf, "%010ld 00000 n \n", off[i]);
    bput(&pdf, "trailer\n<< /Size 6 /Root 1 0 R /Info 5 0 R >>\nstartxref\n%ld\n%%%%EOF\n", xref);
  }
  ok = pdf.b && save_buf(&pdf, path);
  free(pdf.b);
  return ok;
}

bool fig_write_eps(const FigDL *dl, const char *path) {
  Buf b = {0};
  char w[24], h[24];
  bool ok;
  bput(&b, "%%!PS-Adobe-3.0 EPSF-3.0\n%%%%BoundingBox: 0 0 %d %d\n"
           "%%%%HiResBoundingBox: 0 0 %s %s\n%%%%Creator: XPP Lab\n%%%%Pages: 1\n"
           "%%%%EndComments\ngsave\n0 %s translate 1 -1 scale\n",
       (int)ceilf(dl->width), (int)ceilf(dl->height), num(w, dl->width), num(h, dl->height),
       num(h, dl->height));
  vector_body(&b, dl, 2);
  bput(&b, "grestore\nshowpage\n%%%%EOF\n");
  ok = b.b && save_buf(&b, path);
  free(b.b);
  return ok;
}

static bool ends_with(const char *s, const char *ext) {
  size_t n = strlen(s), m = strlen(ext), i;
  if (n < m) return false;
  for (i = 0; i < m; i++) {
    char a = s[n - m + i], c = ext[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (a != c) return false;
  }
  return true;
}

bool fig_write(const Figure *f, const char *path, char *err, size_t errlen) {
  FigDL dl;
  bool ok;
  if (!fig_fonts_ok()) {
    snprintf(err, errlen, "the figure fonts are missing from this build");
    return false;
  }
  memset(&dl, 0, sizeof dl);
  fig_build(f, &dl);
  if (ends_with(path, ".png")) ok = fig_write_png(&dl, f->dpi, path);
  else if (ends_with(path, ".svg")) ok = fig_write_svg(&dl, path);
  else if (ends_with(path, ".pdf")) ok = fig_write_pdf(&dl, path);
  else if (ends_with(path, ".eps") || ends_with(path, ".ps")) ok = fig_write_eps(&dl, path);
  else {
    fig_dl_free(&dl);
    snprintf(err, errlen, "give the file a .pdf, .svg, .eps or .png name");
    return false;
  }
  fig_dl_free(&dl);
  if (!ok) snprintf(err, errlen, "cannot write %s", path);
  else if (errlen) err[0] = '\0';
  return ok;
}
