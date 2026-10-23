/*
 * Null Xlib implementation. Every call succeeds and draws nothing, so
 * XPPAUT's batch (-silent) mode runs on hosts without an X server.
 */
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

static Visual nullx_visual = {0, TrueColor, 0xff0000, 0x00ff00, 0x0000ff, 8,
                              256};
static Screen nullx_screens[1] = {
    {1, 1024, 768, 300, 225, 1, &nullx_visual, 24, 1, 0xffffff, 0}};
static Display nullx_display = {0, 1, nullx_screens};
static XID nullx_next_id = 100;

static XID new_id(void) { return nullx_next_id++; }

Screen *nullx_screen(Display *dpy, int scr) {
  (void)dpy;
  (void)scr;
  return &nullx_screens[0];
}

Display *XOpenDisplay(const char *name) {
  (void)name;
  return &nullx_display;
}
int XCloseDisplay(Display *dpy) { (void)dpy; return 0; }
int XFlush(Display *dpy) { (void)dpy; return 0; }
int XSync(Display *dpy, Bool discard) { (void)dpy; (void)discard; return 0; }
int (*XSynchronize(Display *dpy, Bool onoff))(Display *) {
  (void)dpy;
  (void)onoff;
  return NULL;
}
int XBell(Display *dpy, int percent) { (void)dpy; (void)percent; return 0; }
int XPending(Display *dpy) { (void)dpy; return 0; }
int XNextEvent(Display *dpy, XEvent *ev) {
  (void)dpy;
  memset(ev, 0, sizeof(*ev));
  ev->type = ClientMessage;
  return 0;
}
Status XSendEvent(Display *dpy, Window w, Bool propagate, long mask,
                  XEvent *ev) {
  (void)dpy; (void)w; (void)propagate; (void)mask; (void)ev;
  return 1;
}
int XFree(void *data) { free(data); return 0; }

Window XCreateSimpleWindow(Display *dpy, Window parent, int x, int y,
                           unsigned int width, unsigned int height,
                           unsigned int border_width, unsigned long border,
                           unsigned long background) {
  (void)dpy; (void)parent; (void)x; (void)y; (void)width; (void)height;
  (void)border_width; (void)border; (void)background;
  return new_id();
}
int XDestroyWindow(Display *dpy, Window w) { (void)dpy; (void)w; return 0; }
int XDestroySubwindows(Display *dpy, Window w) { (void)dpy; (void)w; return 0; }
int XMapWindow(Display *dpy, Window w) { (void)dpy; (void)w; return 0; }
int XMapRaised(Display *dpy, Window w) { (void)dpy; (void)w; return 0; }
int XMapSubwindows(Display *dpy, Window w) { (void)dpy; (void)w; return 0; }
int XUnmapWindow(Display *dpy, Window w) { (void)dpy; (void)w; return 0; }
int XUnmapSubwindows(Display *dpy, Window w) { (void)dpy; (void)w; return 0; }
int XRaiseWindow(Display *dpy, Window w) { (void)dpy; (void)w; return 0; }
int XLowerWindow(Display *dpy, Window w) { (void)dpy; (void)w; return 0; }
int XMoveWindow(Display *dpy, Window w, int x, int y) {
  (void)dpy; (void)w; (void)x; (void)y;
  return 0;
}
int XResizeWindow(Display *dpy, Window w, unsigned int width,
                  unsigned int height) {
  (void)dpy; (void)w; (void)width; (void)height;
  return 0;
}
int XMoveResizeWindow(Display *dpy, Window w, int x, int y,
                      unsigned int width, unsigned int height) {
  (void)dpy; (void)w; (void)x; (void)y; (void)width; (void)height;
  return 0;
}
int XIconifyWindow(Display *dpy, Window w, int screen) {
  (void)dpy; (void)w; (void)screen;
  return 1;
}
int XSelectInput(Display *dpy, Window w, long mask) {
  (void)dpy; (void)w; (void)mask;
  return 0;
}
int XClearWindow(Display *dpy, Window w) { (void)dpy; (void)w; return 0; }
int XClearArea(Display *dpy, Window w, int x, int y, unsigned int width,
               unsigned int height, Bool exposures) {
  (void)dpy; (void)w; (void)x; (void)y; (void)width; (void)height;
  (void)exposures;
  return 0;
}
int XSetWindowBorderWidth(Display *dpy, Window w, unsigned int width) {
  (void)dpy; (void)w; (void)width;
  return 0;
}
int XSetWindowBorder(Display *dpy, Window w, unsigned long pixel) {
  (void)dpy; (void)w; (void)pixel;
  return 0;
}
int XSetWindowBackground(Display *dpy, Window w, unsigned long pixel) {
  (void)dpy; (void)w; (void)pixel;
  return 0;
}
int XSetWindowBackgroundPixmap(Display *dpy, Window w, Pixmap p) {
  (void)dpy; (void)w; (void)p;
  return 0;
}
int XChangeWindowAttributes(Display *dpy, Window w, unsigned long mask,
                            XSetWindowAttributes *attr) {
  (void)dpy; (void)w; (void)mask; (void)attr;
  return 0;
}
Status XGetWindowAttributes(Display *dpy, Window w, XWindowAttributes *attr) {
  (void)dpy; (void)w;
  memset(attr, 0, sizeof(*attr));
  attr->width = 640;
  attr->height = 480;
  attr->depth = 24;
  attr->visual = &nullx_visual;
  attr->map_state = IsViewable;
  attr->screen = &nullx_screens[0];
  return 1;
}
Status XGetGeometry(Display *dpy, Drawable d, Window *root, int *x, int *y,
                    unsigned int *width, unsigned int *height,
                    unsigned int *border, unsigned int *depth) {
  (void)dpy; (void)d;
  *root = nullx_screens[0].root;
  *x = 0;
  *y = 0;
  *width = 640;
  *height = 480;
  *border = 0;
  *depth = 24;
  return 1;
}
int XSetInputFocus(Display *dpy, Window focus, int revert_to, Time time) {
  (void)dpy; (void)focus; (void)revert_to; (void)time;
  return 0;
}
int XGetInputFocus(Display *dpy, Window *focus, int *revert_to) {
  (void)dpy;
  *focus = None;
  *revert_to = RevertToParent;
  return 0;
}
int XDefineCursor(Display *dpy, Window w, Cursor c) {
  (void)dpy; (void)w; (void)c;
  return 0;
}
Cursor XCreateFontCursor(Display *dpy, unsigned int shape) {
  (void)dpy; (void)shape;
  return new_id();
}
Atom XInternAtom(Display *dpy, const char *name, Bool only_if_exists) {
  (void)dpy; (void)name; (void)only_if_exists;
  return (Atom)new_id();
}
Status XSetWMProtocols(Display *dpy, Window w, Atom *protocols, int count) {
  (void)dpy; (void)w; (void)protocols; (void)count;
  return 1;
}
int XSetStandardProperties(Display *dpy, Window w, const char *window_name,
                           const char *icon_name, Pixmap icon_pixmap,
                           char **argv, int argc, void *hints) {
  (void)dpy; (void)w; (void)window_name; (void)icon_name; (void)icon_pixmap;
  (void)argv; (void)argc; (void)hints;
  return 0;
}

struct _XGC {
  XGCValues values;
};

GC XCreateGC(Display *dpy, Drawable d, unsigned long mask, XGCValues *values) {
  GC gc = calloc(1, sizeof(*gc));
  (void)dpy; (void)d; (void)mask;
  if (gc && values) gc->values = *values;
  return gc;
}
int XFreeGC(Display *dpy, GC gc) { (void)dpy; free(gc); return 0; }
int XSetForeground(Display *dpy, GC gc, unsigned long pixel) {
  (void)dpy;
  if (gc) gc->values.foreground = pixel;
  return 0;
}
int XSetBackground(Display *dpy, GC gc, unsigned long pixel) {
  (void)dpy;
  if (gc) gc->values.background = pixel;
  return 0;
}
int XSetFunction(Display *dpy, GC gc, int function) {
  (void)dpy;
  if (gc) gc->values.function = function;
  return 0;
}
int XSetFont(Display *dpy, GC gc, Font font) {
  (void)dpy;
  if (gc) gc->values.font = font;
  return 0;
}
int XSetLineAttributes(Display *dpy, GC gc, unsigned int width, int line_style,
                       int cap_style, int join_style) {
  (void)dpy;
  if (gc) {
    gc->values.line_width = (int)width;
    gc->values.line_style = line_style;
    gc->values.cap_style = cap_style;
    gc->values.join_style = join_style;
  }
  return 0;
}
int XSetDashes(Display *dpy, GC gc, int offset, const char *dashes, int n) {
  (void)dpy; (void)gc; (void)offset; (void)dashes; (void)n;
  return 0;
}
int XSetFillStyle(Display *dpy, GC gc, int style) {
  (void)dpy;
  if (gc) gc->values.fill_style = style;
  return 0;
}

int XDrawString(Display *dpy, Drawable d, GC gc, int x, int y, const char *s,
                int len) {
  (void)dpy; (void)d; (void)gc; (void)x; (void)y; (void)s; (void)len;
  return 0;
}
int XDrawLine(Display *dpy, Drawable d, GC gc, int x1, int y1, int x2,
              int y2) {
  (void)dpy; (void)d; (void)gc; (void)x1; (void)y1; (void)x2; (void)y2;
  return 0;
}
int XDrawPoint(Display *dpy, Drawable d, GC gc, int x, int y) {
  (void)dpy; (void)d; (void)gc; (void)x; (void)y;
  return 0;
}
int XDrawRectangle(Display *dpy, Drawable d, GC gc, int x, int y,
                   unsigned int width, unsigned int height) {
  (void)dpy; (void)d; (void)gc; (void)x; (void)y; (void)width; (void)height;
  return 0;
}
int XFillRectangle(Display *dpy, Drawable d, GC gc, int x, int y,
                   unsigned int width, unsigned int height) {
  (void)dpy; (void)d; (void)gc; (void)x; (void)y; (void)width; (void)height;
  return 0;
}
int XDrawArc(Display *dpy, Drawable d, GC gc, int x, int y,
             unsigned int width, unsigned int height, int a1, int a2) {
  (void)dpy; (void)d; (void)gc; (void)x; (void)y; (void)width; (void)height;
  (void)a1; (void)a2;
  return 0;
}
int XFillArc(Display *dpy, Drawable d, GC gc, int x, int y,
             unsigned int width, unsigned int height, int a1, int a2) {
  (void)dpy; (void)d; (void)gc; (void)x; (void)y; (void)width; (void)height;
  (void)a1; (void)a2;
  return 0;
}
int XCopyArea(Display *dpy, Drawable src, Drawable dst, GC gc, int sx, int sy,
              unsigned int width, unsigned int height, int dx, int dy) {
  (void)dpy; (void)src; (void)dst; (void)gc; (void)sx; (void)sy;
  (void)width; (void)height; (void)dx; (void)dy;
  return 0;
}

Pixmap XCreatePixmap(Display *dpy, Drawable d, unsigned int width,
                     unsigned int height, unsigned int depth) {
  (void)dpy; (void)d; (void)width; (void)height; (void)depth;
  return new_id();
}
int XFreePixmap(Display *dpy, Pixmap p) { (void)dpy; (void)p; return 0; }
Pixmap XCreateBitmapFromData(Display *dpy, Drawable d, const char *data,
                             unsigned int width, unsigned int height) {
  (void)dpy; (void)d; (void)data; (void)width; (void)height;
  return new_id();
}
Pixmap XCreatePixmapFromBitmapData(Display *dpy, Drawable d, char *data,
                                   unsigned int width, unsigned int height,
                                   unsigned long fg, unsigned long bg,
                                   unsigned int depth) {
  (void)dpy; (void)d; (void)data; (void)width; (void)height; (void)fg;
  (void)bg; (void)depth;
  return new_id();
}
int XReadBitmapFileData(const char *filename, unsigned int *width,
                        unsigned int *height, unsigned char **data, int *x_hot,
                        int *y_hot) {
  (void)filename;
  *width = 0;
  *height = 0;
  *data = NULL;
  if (x_hot) *x_hot = -1;
  if (y_hot) *y_hot = -1;
  return BitmapOpenFailed;
}
XImage *XGetImage(Display *dpy, Drawable d, int x, int y, unsigned int width,
                  unsigned int height, unsigned long plane_mask, int format) {
  XImage *img = calloc(1, sizeof(*img));
  (void)dpy; (void)d; (void)x; (void)y; (void)plane_mask;
  if (!img) return NULL;
  img->width = (int)width;
  img->height = (int)height;
  img->format = format;
  img->depth = 24;
  img->bits_per_pixel = 32;
  img->bytes_per_line = (int)width * 4;
  img->red_mask = nullx_visual.red_mask;
  img->green_mask = nullx_visual.green_mask;
  img->blue_mask = nullx_visual.blue_mask;
  img->data = calloc((size_t)width * height + 1, 4);
  return img;
}
unsigned long XGetPixel(XImage *img, int x, int y) {
  (void)img; (void)x; (void)y;
  return 0;
}
int XDestroyImage(XImage *img) {
  if (img) {
    free(img->data);
    free(img);
  }
  return 0;
}

XFontStruct *XLoadQueryFont(Display *dpy, const char *name) {
  XFontStruct *f = calloc(1, sizeof(*f));
  (void)dpy; (void)name;
  if (!f) return NULL;
  f->fid = new_id();
  f->ascent = 11;
  f->descent = 3;
  f->max_bounds.width = 8;
  f->min_bounds.width = 8;
  f->max_bounds.ascent = 11;
  f->max_bounds.descent = 3;
  return f;
}
int XUnloadFont(Display *dpy, Font font) { (void)dpy; (void)font; return 0; }
int XTextWidth(XFontStruct *font, const char *s, int count) {
  (void)s;
  return count * (font ? font->max_bounds.width : 8);
}

Status XAllocColor(Display *dpy, Colormap cmap, XColor *color) {
  (void)dpy; (void)cmap;
  color->pixel = ((unsigned long)(color->red >> 8) << 16) |
                 ((unsigned long)(color->green >> 8) << 8) |
                 (unsigned long)(color->blue >> 8);
  return 1;
}
Status XParseColor(Display *dpy, Colormap cmap, const char *spec,
                   XColor *color) {
  unsigned int r = 0, g = 0, b = 0;
  (void)dpy; (void)cmap;
  if (spec && spec[0] == '#' && strlen(spec) == 7) {
    char buf[3] = {0, 0, 0};
    buf[0] = spec[1]; buf[1] = spec[2]; r = (unsigned)strtoul(buf, NULL, 16);
    buf[0] = spec[3]; buf[1] = spec[4]; g = (unsigned)strtoul(buf, NULL, 16);
    buf[0] = spec[5]; buf[1] = spec[6]; b = (unsigned)strtoul(buf, NULL, 16);
  }
  color->red = (unsigned short)(r * 257);
  color->green = (unsigned short)(g * 257);
  color->blue = (unsigned short)(b * 257);
  color->flags = DoRed | DoGreen | DoBlue;
  return 1;
}
int XQueryColors(Display *dpy, Colormap cmap, XColor *colors, int n) {
  int i;
  (void)dpy; (void)cmap;
  for (i = 0; i < n; i++) {
    colors[i].red = (unsigned short)(((colors[i].pixel >> 16) & 0xff) * 257);
    colors[i].green = (unsigned short)(((colors[i].pixel >> 8) & 0xff) * 257);
    colors[i].blue = (unsigned short)((colors[i].pixel & 0xff) * 257);
  }
  return 0;
}

int XLookupString(XKeyEvent *ev, char *buf, int nbytes, KeySym *keysym,
                  void *status) {
  (void)ev; (void)status;
  if (nbytes > 0) buf[0] = 0;
  if (keysym) *keysym = NoSymbol;
  return 0;
}
KeyCode XKeysymToKeycode(Display *dpy, KeySym keysym) {
  (void)dpy; (void)keysym;
  return 0;
}

Status XStringListToTextProperty(char **list, int count, XTextProperty *tp) {
  (void)list; (void)count;
  memset(tp, 0, sizeof(*tp));
  return 1;
}
void XSetWMProperties(Display *dpy, Window w, XTextProperty *window_name,
                      XTextProperty *icon_name, char **argv, int argc,
                      XSizeHints *normal_hints, XWMHints *wm_hints,
                      XClassHint *class_hints) {
  (void)dpy; (void)w; (void)window_name; (void)icon_name; (void)argv;
  (void)argc; (void)normal_hints; (void)wm_hints; (void)class_hints;
}
void XSetWMName(Display *dpy, Window w, XTextProperty *tp) {
  (void)dpy; (void)w; (void)tp;
}
Status XGetIconSizes(Display *dpy, Window w, XIconSize **list, int *count) {
  (void)dpy; (void)w;
  *list = NULL;
  *count = 0;
  return 0;
}

Bool XkbBell(Display *dpy, Window w, int percent, Atom name) {
  (void)dpy; (void)w; (void)percent; (void)name;
  return True;
}
