/* XPPAUT's Kinescope: pictures of a plot, kept in order, played back and
   saved as PNG frames or one animated GIF. The pictures are the figure's
   pixels as drawn (export.c reads them at the end of a frame). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "app.h"
#include "export.h"

void app_kine_capture(App *a, int figure) {
  a->ks.source = figure;
  export_capture((FigureKind)figure);
}

void app_kine_add(App *a, void *surface) {
  Kinescope *k = &a->ks;
  if (k->n >= APP_MAX_FRAMES) {
    SDL_DestroySurface((SDL_Surface *)surface);
    k->failed = true;
    app_log(a, "error: the Kinescope holds %d frames; delete them or save them first.",
            APP_MAX_FRAMES);
    return;
  }
  k->frame[k->n++] = surface;
  k->cur = k->n - 1;
  if (!k->film.on) app_status(a, "Kinescope: %d frame%s.", k->n, k->n == 1 ? "" : "s");
}

void app_kine_clear(App *a) {
  Kinescope *k = &a->ks;
  int i;
  for (i = 0; i < k->n; i++) SDL_DestroySurface((SDL_Surface *)k->frame[i]);
  if (k->tex) SDL_DestroyTexture((SDL_Texture *)k->tex);
  k->tex = NULL;
  k->n = k->cur = 0;
  k->playing = false;
}

/* Saving waits for a film, or a capture, still under way. */
static bool kine_busy(App *a) { return a->ks.film.on || export_capture_waiting(); }

void app_kine_frame(App *a, float dt) {
  Kinescope *k = &a->ks;
  char path[1024];
  if (k->film.on) app_film_step(a);
  if (!kine_busy(a) && k->save_after[0]) {
    snprintf(path, sizeof path, "%s", k->save_after);
    k->save_after[0] = '\0';
    app_kine_save_gif(a, path, k->save_ms);
  }
  if (!kine_busy(a) && k->frames_after[0]) {
    snprintf(path, sizeof path, "%s", k->frames_after);
    k->frames_after[0] = '\0';
    app_kine_save_frames(a, path);
  }
  if (!k->playing || k->n < 2) return;
  k->acc += 1000.0 * dt;
  if (k->acc < (k->ms > 0 ? k->ms : 1)) return;
  k->acc = 0;
  if (k->cur + 1 < k->n)
    k->cur++;
  else if (k->loop)
    k->cur = 0;
  else
    k->playing = false;
}

bool app_kine_save_frames(App *a, const char *base) {
  Kinescope *k = &a->ks;
  char path[1200];
  int i;
  if (kine_busy(a)) {
    snprintf(k->frames_after, sizeof k->frames_after, "%s", base);
    return true;
  }
  if (k->n == 0) {
    app_log(a, "error: the Kinescope has no frames yet.");
    return false;
  }
  for (i = 0; i < k->n; i++) {
    snprintf(path, sizeof path, "%s_%03d.png", base, i + 1);
    if (!export_surface_png(k->frame[i], path)) {
      app_log(a, "error: cannot write %s", path);
      return false;
    }
  }
  app_log(a, "Saved %d frames as %s_001.png ...", k->n, base);
  return true;
}

/* ---- Animated GIF ------------------------------------------------------------
   One palette for the whole film: the 256 most common colours (to 5 bits a
   channel, each the average of the pixels it stands for), every pixel
   mapped to the nearest. Plots have few colours, so this keeps them clean.
   The image data is LZW-coded as the GIF89a format sets out. */

typedef struct {
  FILE *f;
  unsigned char block[255];
  int nblock;
  uint32_t bits;
  int nbits;
} GifOut;

static void gif_byte(GifOut *g, int c) {
  g->block[g->nblock++] = (unsigned char)c;
  if (g->nblock == 255) {
    fputc(255, g->f);
    fwrite(g->block, 1, 255, g->f);
    g->nblock = 0;
  }
}

static void gif_code(GifOut *g, int code, int size) {
  g->bits |= (uint32_t)code << g->nbits;
  g->nbits += size;
  while (g->nbits >= 8) {
    gif_byte(g, (int)(g->bits & 0xff));
    g->bits >>= 8;
    g->nbits -= 8;
  }
}

static void gif_word(FILE *f, int v) {
  fputc(v & 0xff, f);
  fputc((v >> 8) & 0xff, f);
}

/* The LZW-coded indices of one frame, as data sub-blocks. */
static bool gif_lzw(FILE *f, const unsigned char *idx, size_t n) {
  enum { CLEAR = 256, END = 257, MAXCODES = 4096 };
  uint16_t *child = calloc((size_t)MAXCODES * 256, sizeof *child);
  GifOut g;
  int size = 9, next = END + 1, prefix;
  size_t i;
  if (!child) return false;
  memset(&g, 0, sizeof g);
  g.f = f;
  fputc(8, f); /* the minimum code size */
  gif_code(&g, CLEAR, size);
  prefix = idx[0];
  for (i = 1; i < n; i++) {
    int c = idx[i];
    uint16_t *slot = &child[(size_t)prefix * 256 + (size_t)c];
    if (*slot) {
      prefix = *slot;
      continue;
    }
    gif_code(&g, prefix, size);
    if (next < MAXCODES) {
      if (next == (1 << size)) size++;
      *slot = (uint16_t)next++;
    } else {
      gif_code(&g, CLEAR, size);
      memset(child, 0, (size_t)MAXCODES * 256 * sizeof *child);
      size = 9;
      next = END + 1;
    }
    prefix = c;
  }
  gif_code(&g, prefix, size);
  gif_code(&g, END, size);
  if (g.nbits > 0) gif_byte(&g, (int)(g.bits & 0xff));
  if (g.nblock > 0) {
    fputc(g.nblock, f);
    fwrite(g.block, 1, (size_t)g.nblock, f);
  }
  fputc(0, f);
  free(child);
  return true;
}

static int key15(const unsigned char *p) {
  return (p[0] >> 3) << 10 | (p[1] >> 3) << 5 | (p[2] >> 3);
}

typedef struct {
  long count;
  double r, g, b;
  int key;
} Bin;

static int by_count(const void *x, const void *y) {
  const Bin *a = x, *b = y;
  return a->count < b->count ? 1 : a->count > b->count ? -1 : a->key - b->key;
}

bool app_kine_save_gif(App *a, const char *path, int ms) {
  Kinescope *k = &a->ks;
  Bin *bins = NULL;
  unsigned char pal[256][3], *lut = NULL, *idx = NULL;
  int npal = 0, w = 0, h = 0, i, x, y, delay;
  bool ok = false;
  FILE *f = NULL;
  if (kine_busy(a)) { /* written once the pictures are in */
    snprintf(k->save_after, sizeof k->save_after, "%s", path);
    k->save_ms = ms;
    return true;
  }
  if (k->n == 0) {
    app_log(a, "error: the Kinescope has no frames yet.");
    return false;
  }
  /* Frames of other sizes (the window was resized) sit at the top left of
     the largest, on white. */
  for (i = 0; i < k->n; i++) {
    SDL_Surface *s = k->frame[i];
    if (s->w > w) w = s->w;
    if (s->h > h) h = s->h;
  }
  bins = calloc(32768, sizeof *bins);
  lut = malloc(32768);
  idx = malloc((size_t)w * (size_t)h);
  if (!bins || !lut || !idx) goto done;
  for (i = 0; i < 32768; i++) bins[i].key = i;
  for (i = 0; i < k->n; i++) {
    SDL_Surface *s = k->frame[i];
    for (y = 0; y < s->h; y++) {
      const unsigned char *p = (const unsigned char *)s->pixels + (size_t)y * (size_t)s->pitch;
      for (x = 0; x < s->w; x++, p += 3) {
        Bin *b = &bins[key15(p)];
        b->count++;
        b->r += p[0];
        b->g += p[1];
        b->b += p[2];
      }
    }
  }
  bins[32767].count++; /* white, for the margins */
  bins[32767].r += 255;
  bins[32767].g += 255;
  bins[32767].b += 255;
  qsort(bins, 32768, sizeof *bins, by_count);
  for (npal = 0; npal < 256 && bins[npal].count > 0; npal++) {
    pal[npal][0] = (unsigned char)(bins[npal].r / (double)bins[npal].count + 0.5);
    pal[npal][1] = (unsigned char)(bins[npal].g / (double)bins[npal].count + 0.5);
    pal[npal][2] = (unsigned char)(bins[npal].b / (double)bins[npal].count + 0.5);
  }
  for (i = 0; i < 32768; i++) {
    int r = ((i >> 10) & 31) * 8 + 4, g = ((i >> 5) & 31) * 8 + 4, b = (i & 31) * 8 + 4;
    int best = 0, j;
    long bd = -1;
    for (j = 0; j < npal; j++) {
      long dr = r - pal[j][0], dg = g - pal[j][1], db = b - pal[j][2];
      long d = dr * dr + dg * dg + db * db;
      if (bd < 0 || d < bd) {
        bd = d;
        best = j;
      }
    }
    lut[i] = (unsigned char)best;
  }
  if ((f = fopen(path, "wb")) == NULL) {
    app_log(a, "error: cannot write %s", path);
    goto done;
  }
  fwrite("GIF89a", 1, 6, f);
  gif_word(f, w);
  gif_word(f, h);
  fputc(0xF7, f); /* a global table of 256 colours */
  fputc(lut[32767], f);
  fputc(0, f);
  for (i = 0; i < 256; i++) {
    fputc(i < npal ? pal[i][0] : 0, f);
    fputc(i < npal ? pal[i][1] : 0, f);
    fputc(i < npal ? pal[i][2] : 0, f);
  }
  fputc(0x21, f); /* loop forever */
  fputc(0xFF, f);
  fputc(11, f);
  fwrite("NETSCAPE2.0", 1, 11, f);
  fputc(3, f);
  fputc(1, f);
  gif_word(f, 0);
  fputc(0, f);
  delay = (ms + 5) / 10;
  if (delay < 2) delay = 2; /* what viewers honour */
  for (i = 0; i < k->n; i++) {
    SDL_Surface *s = k->frame[i];
    memset(idx, lut[32767], (size_t)w * (size_t)h);
    for (y = 0; y < s->h; y++) {
      const unsigned char *p = (const unsigned char *)s->pixels + (size_t)y * (size_t)s->pitch;
      for (x = 0; x < s->w; x++, p += 3) idx[(size_t)y * (size_t)w + (size_t)x] = lut[key15(p)];
    }
    fputc(0x21, f); /* the delay before the next frame */
    fputc(0xF9, f);
    fputc(4, f);
    fputc(0x04, f);
    gif_word(f, delay);
    fputc(0, f);
    fputc(0, f);
    fputc(0x2C, f);
    gif_word(f, 0);
    gif_word(f, 0);
    gif_word(f, w);
    gif_word(f, h);
    fputc(0, f);
    if (!gif_lzw(f, idx, (size_t)w * (size_t)h)) goto done;
  }
  fputc(0x3B, f);
  ok = ferror(f) == 0;
  if (ok)
    app_log(a, "Saved %s: %d frame%s of %d x %d pixels, %d ms each.", path, k->n,
            k->n == 1 ? "" : "s", w, h, delay * 10);
  else
    app_log(a, "error: cannot write %s", path);
done:
  if (f) fclose(f);
  free(bins);
  free(lut);
  free(idx);
  return ok;
}
