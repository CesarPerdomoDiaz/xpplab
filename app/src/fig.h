/*
 * Figures for papers: a page of panels, each with its curves, labels,
 * ticks and legend, drawn from one description to the screen, PNG, PDF,
 * SVG and EPS.
 *
 * fig_build() lays a figure out into a list of paths in points (1/72 inch,
 * y downwards from the top left): curves, markers, axes, and text, whose
 * letters are outlines from the figure's font. Every output draws the same
 * list, so the preview, the PNG and the vector files agree to the point.
 * Text takes mathtext, matplotlib's subset of TeX between dollar signs
 * ("$\Gamma_e$ [deg]"), and the fonts are matplotlib's (DejaVu Sans, STIX),
 * so a figure redrawn from the Python echo with xpplab.figure looks the same.
 */
#ifndef XPPS_FIG_H
#define XPPS_FIG_H

#include <stdbool.h>
#include <stddef.h>

#define FIG_MAX_PANELS 12
#define FIG_MAX_SERIES 64
#define FIG_TEXT 256

enum { FIG_LINE_NONE, FIG_LINE_SOLID, FIG_LINE_DASHED, FIG_LINE_DOTTED,
       FIG_LINE_DASHDOT, FIG_LINE_COUNT };
enum { FIG_MARK_NONE, FIG_MARK_CIRCLE, FIG_MARK_SQUARE, FIG_MARK_TRIANGLE,
       FIG_MARK_DIAMOND, FIG_MARK_X, FIG_MARK_PLUS, FIG_MARK_STAR,
       FIG_MARK_POINT, FIG_MARK_COUNT };
enum { FIG_LEGEND_NONE, FIG_LEGEND_BEST, FIG_LEGEND_UPPER_RIGHT,
       FIG_LEGEND_UPPER_LEFT, FIG_LEGEND_LOWER_LEFT, FIG_LEGEND_LOWER_RIGHT,
       FIG_LEGEND_OUTSIDE, FIG_LEGEND_COUNT };
enum { FIG_FONT_SANS, FIG_FONT_SERIF, FIG_FONT_COUNT };

/* One curve or set of markers. NaN in x or y breaks the line. */
typedef struct {
  char label[FIG_TEXT];  /* legend entry; empty: not in the legend */
  int n;
  double *x, *y;         /* owned */
  double sx, sy;         /* shown as x*sx, y*sy (radians as degrees, ...) */
  float color[3];
  int line;              /* FIG_LINE_* */
  float width;           /* line width in points */
  int marker;            /* FIG_MARK_* */
  float msize;           /* marker size in points */
  bool mfill;            /* filled markers */
  float mcolor[3];       /* marker edge (and fill) colour */
  int every;             /* a marker every this many points (1: all) */
  /* The Python that adds the same series to a figure, for the echo:
     "p.line(sol["T"], sol["GAM"])" with p the panel. Set by app_figure.c. */
  char *py;
} FigSeries;

typedef struct {
  char title[FIG_TEXT], xlabel[FIG_TEXT], ylabel[FIG_TEXT];
  bool autox, autoy;           /* limits from the data */
  double xmin, xmax, ymin, ymax;
  bool logx, logy, grid;
  double xstep, ystep;         /* tick spacing; 0: chosen */
  int legend;                  /* FIG_LEGEND_* */
  bool equal;                  /* the same scale on both axes */
  /* An inset: drawn inside panel `parent`, at (x0, y0, w, h) in fractions
     of that panel's axes, from its lower left corner. */
  bool inset;
  int parent;
  float ix, iy, iw, ih;
  int nseries;
  FigSeries s[FIG_MAX_SERIES];
} FigPanel;

typedef struct {
  float width_mm, height_mm;   /* the page */
  int rows, cols;              /* the first rows*cols panels fill a grid */
  int font;                    /* FIG_FONT_* */
  float font_pt;               /* labels; ticks and legend are 0.85 of it,
                                  the title 1.1 */
  int npanels;
  FigPanel p[FIG_MAX_PANELS];
  int dpi;                     /* PNG resolution */
} Figure;

/* ---- The figure --------------------------------------------------------- */

void fig_init(Figure *f);                 /* empty, one panel, 8.6 x 6.5 cm */
void fig_free(Figure *f);                 /* the series' data */
void fig_panel_init(FigPanel *p);
/* Add a series with a copy of the data; NULL when the panel is full. */
FigSeries *fig_add(FigPanel *p, const double *x, const double *y, int n);
void fig_remove(FigPanel *p, int k);
void fig_series_free(FigSeries *s);
/* The default colours, in order (matplotlib's "tab10"). */
void fig_color(int k, float rgb[3]);
/* Change the grid; panels beyond it are kept as insets or dropped. */
void fig_set_grid(Figure *f, int rows, int cols);

/* ---- Paths ---------------------------------------------------------------- */

enum { FIG_MOVE, FIG_LINE, FIG_QUAD, FIG_CUBIC, FIG_CLOSE };
enum { FIG_OP_FILL, FIG_OP_STROKE, FIG_OP_CLIP, FIG_OP_UNCLIP };

typedef struct {
  int op;                 /* FIG_OP_* */
  float rgb[3];
  float width;            /* stroke width (pt) */
  float dash[4];          /* on, off, on, off (pt); dash[0] == 0: solid */
  int first, count;       /* commands in FigDL.cmd */
} FigOp;

/* A command and its points: MOVE/LINE one, QUAD two, CUBIC three. */
typedef struct {
  int type;
  float p[6];
} FigCmd;

/* Where each panel landed, to map the mouse to data. */
typedef struct {
  float x0, y0, x1, y1;      /* the axes' box, points */
  double xmin, xmax, ymin, ymax;
  bool logx, logy, valid;
} FigAxes;

typedef struct {
  float width, height;       /* points */
  FigOp *op;
  int nop, capop;
  FigCmd *cmd;
  int ncmd, capcmd;
  FigAxes axes[FIG_MAX_PANELS];
} FigDL;

void fig_build(const Figure *f, FigDL *dl);
void fig_dl_free(FigDL *dl);
/* The data limits a panel shows: its own, or from its series. */
void fig_limits(const FigPanel *p, double lim[4]);
/* Data to points and back, for an axes box of fig_build(). */
void fig_to_page(const FigAxes *a, double x, double y, float *px, float *py);
void fig_from_page(const FigAxes *a, float px, float py, double *x, double *y);

/* ---- Text ------------------------------------------------------------------ */

/* Whether the fonts are there (fig_font.c embeds them). */
bool fig_fonts_ok(void);
/* Lay out a label (mathtext) of size pt with its left end of baseline at
   (x, y), rotated by angle degrees anticlockwise, adding its letters to dl
   as filled paths. Returns the width; *ascent and *descent (may be NULL)
   get its extent above and below the baseline. With dl NULL it only
   measures. */
float fig_text(FigDL *dl, const char *s, int font, float pt, float x, float y,
               float angle, const float rgb[3], float *ascent, float *descent);
/* Whether a label's mathtext parses; else a message. */
bool fig_text_check(const char *s, char *msg, size_t len);

/* ---- Output ----------------------------------------------------------------- */

/* An RGBA image of the figure, w x h pixels (the page scaled to fit), on
   white. The caller frees it. */
unsigned char *fig_raster(const FigDL *dl, int w, int h);
bool fig_write_png(const FigDL *dl, int dpi, const char *path);
bool fig_write_svg(const FigDL *dl, const char *path);
bool fig_write_pdf(const FigDL *dl, const char *path);
bool fig_write_eps(const FigDL *dl, const char *path);
/* By the file's extension: .png, .svg, .pdf, .eps. */
bool fig_write(const Figure *f, const char *path, char *err, size_t errlen);

#endif
