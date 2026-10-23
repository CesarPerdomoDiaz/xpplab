/*
 * Null Xlib: just enough of the Xlib API for XPPAUT 8.0 to compile and run
 * in batch (-silent) mode on systems without X11. Every call is a no-op.
 * This is a build aid for the reference oracle, not a windowing system.
 */
#ifndef NULLX_XLIB_H
#define NULLX_XLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long XID;
typedef XID Window;
typedef XID Drawable;
typedef XID Pixmap;
typedef XID Cursor;
typedef XID Font;
typedef XID Colormap;
typedef XID GContext;
typedef XID KeySym;
typedef unsigned long Atom;
typedef unsigned long Time;
typedef unsigned long VisualID;
typedef unsigned char KeyCode;
typedef int Bool;
typedef int Status;
typedef char *XPointer;

#define True 1
#define False 0
#define None 0L
#define Success 0
#define CurrentTime 0L
#define AllPlanes (~0UL)
#define PointerRoot 1L
#define NoSymbol 0L
#define CopyFromParent 0L
#define InputOutput 1
#define InputOnly 2

typedef struct _XGC *GC;

typedef struct {
  VisualID visualid;
  int class;
  unsigned long red_mask, green_mask, blue_mask;
  int bits_per_rgb;
  int map_entries;
} Visual;

typedef struct {
  Window root;
  int width, height;
  int mwidth, mheight;
  int ndepths;
  Visual *root_visual;
  int root_depth;
  Colormap cmap;
  unsigned long white_pixel, black_pixel;
} Screen;

typedef struct _XDisplay {
  int default_screen;
  int nscreens;
  Screen *screens;
} Display;

/* Visual classes */
#define StaticGray 0
#define GrayScale 1
#define StaticColor 2
#define PseudoColor 3
#define TrueColor 4
#define DirectColor 5

/* Event types */
#define KeyPress 2
#define KeyRelease 3
#define ButtonPress 4
#define ButtonRelease 5
#define MotionNotify 6
#define EnterNotify 7
#define LeaveNotify 8
#define FocusIn 9
#define FocusOut 10
#define KeymapNotify 11
#define Expose 12
#define GraphicsExpose 13
#define NoExpose 14
#define VisibilityNotify 15
#define CreateNotify 16
#define DestroyNotify 17
#define UnmapNotify 18
#define MapNotify 19
#define MapRequest 20
#define ReparentNotify 21
#define ConfigureNotify 22
#define ConfigureRequest 23
#define GravityNotify 24
#define ResizeRequest 25
#define CirculateNotify 26
#define CirculateRequest 27
#define PropertyNotify 28
#define SelectionClear 29
#define SelectionRequest 30
#define SelectionNotify 31
#define ColormapNotify 32
#define ClientMessage 33
#define MappingNotify 34

/* Event masks */
#define NoEventMask 0L
#define KeyPressMask (1L << 0)
#define KeyReleaseMask (1L << 1)
#define ButtonPressMask (1L << 2)
#define ButtonReleaseMask (1L << 3)
#define EnterWindowMask (1L << 4)
#define LeaveWindowMask (1L << 5)
#define PointerMotionMask (1L << 6)
#define PointerMotionHintMask (1L << 7)
#define Button1MotionMask (1L << 8)
#define Button2MotionMask (1L << 9)
#define Button3MotionMask (1L << 10)
#define Button4MotionMask (1L << 11)
#define Button5MotionMask (1L << 12)
#define ButtonMotionMask (1L << 13)
#define KeymapStateMask (1L << 14)
#define ExposureMask (1L << 15)
#define VisibilityChangeMask (1L << 16)
#define StructureNotifyMask (1L << 17)
#define ResizeRedirectMask (1L << 18)
#define SubstructureNotifyMask (1L << 19)
#define SubstructureRedirectMask (1L << 20)
#define FocusChangeMask (1L << 21)
#define PropertyChangeMask (1L << 22)
#define ColormapChangeMask (1L << 23)
#define OwnerGrabButtonMask (1L << 24)

/* Key/button state masks */
#define ShiftMask (1 << 0)
#define LockMask (1 << 1)
#define ControlMask (1 << 2)
#define Mod1Mask (1 << 3)
#define Mod2Mask (1 << 4)
#define Mod3Mask (1 << 5)
#define Mod4Mask (1 << 6)
#define Mod5Mask (1 << 7)
#define Button1Mask (1 << 8)
#define Button2Mask (1 << 9)
#define Button3Mask (1 << 10)
#define Button4Mask (1 << 11)
#define Button5Mask (1 << 12)
#define Button1 1
#define Button2 2
#define Button3 3
#define Button4 4
#define Button5 5

/* Focus */
#define RevertToNone 0
#define RevertToPointerRoot 1
#define RevertToParent 2

/* GC functions */
#define GXclear 0x0
#define GXand 0x1
#define GXandReverse 0x2
#define GXcopy 0x3
#define GXandInverted 0x4
#define GXnoop 0x5
#define GXxor 0x6
#define GXor 0x7
#define GXnor 0x8
#define GXequiv 0x9
#define GXinvert 0xa
#define GXorReverse 0xb
#define GXcopyInverted 0xc
#define GXorInverted 0xd
#define GXnand 0xe
#define GXset 0xf

#define LineSolid 0
#define LineOnOffDash 1
#define LineDoubleDash 2
#define CapNotLast 0
#define CapButt 1
#define CapRound 2
#define CapProjecting 3
#define JoinMiter 0
#define JoinRound 1
#define JoinBevel 2
#define FillSolid 0
#define FillTiled 1
#define FillStippled 2
#define FillOpaqueStippled 3

#define GCFunction (1L << 0)
#define GCPlaneMask (1L << 1)
#define GCForeground (1L << 2)
#define GCBackground (1L << 3)
#define GCLineWidth (1L << 4)
#define GCLineStyle (1L << 5)
#define GCCapStyle (1L << 6)
#define GCJoinStyle (1L << 7)
#define GCFillStyle (1L << 8)
#define GCFont (1L << 14)

/* Window attribute masks */
#define CWBackPixmap (1L << 0)
#define CWBackPixel (1L << 1)
#define CWBorderPixmap (1L << 2)
#define CWBorderPixel (1L << 3)
#define CWBitGravity (1L << 4)
#define CWWinGravity (1L << 5)
#define CWBackingStore (1L << 6)
#define CWBackingPlanes (1L << 7)
#define CWBackingPixel (1L << 8)
#define CWOverrideRedirect (1L << 9)
#define CWSaveUnder (1L << 10)
#define CWEventMask (1L << 11)
#define CWDontPropagate (1L << 12)
#define CWColormap (1L << 13)
#define CWCursor (1L << 14)

#define NotUseful 0
#define WhenMapped 1
#define Always 2

#define ForgetGravity 0
#define NorthWestGravity 1
#define StaticGravity 10

#define IsUnmapped 0
#define IsUnviewable 1
#define IsViewable 2

#define XYBitmap 0
#define XYPixmap 1
#define ZPixmap 2
#define LSBFirst 0
#define MSBFirst 1

#define DoRed (1 << 0)
#define DoGreen (1 << 1)
#define DoBlue (1 << 2)

#define BadAlloc 11

typedef struct {
  unsigned long pixel;
  unsigned short red, green, blue;
  char flags;
  char pad;
} XColor;

typedef struct {
  int function;
  unsigned long plane_mask;
  unsigned long foreground;
  unsigned long background;
  int line_width;
  int line_style;
  int cap_style;
  int join_style;
  int fill_style;
  int fill_rule;
  int arc_mode;
  Pixmap tile;
  Pixmap stipple;
  int ts_x_origin;
  int ts_y_origin;
  Font font;
  int subwindow_mode;
  Bool graphics_exposures;
  int clip_x_origin;
  int clip_y_origin;
  Pixmap clip_mask;
  int dash_offset;
  char dashes;
} XGCValues;

typedef struct {
  short lbearing, rbearing, width, ascent, descent;
  unsigned short attributes;
} XCharStruct;

typedef struct {
  void *ext_data;
  Font fid;
  unsigned direction;
  unsigned min_char_or_byte2;
  unsigned max_char_or_byte2;
  unsigned min_byte1;
  unsigned max_byte1;
  Bool all_chars_exist;
  unsigned default_char;
  int n_properties;
  void *properties;
  XCharStruct min_bounds;
  XCharStruct max_bounds;
  XCharStruct *per_char;
  int ascent;
  int descent;
} XFontStruct;

typedef struct {
  int x, y;
  int width, height;
  int border_width;
  int depth;
  Visual *visual;
  Window root;
  int class;
  int bit_gravity;
  int win_gravity;
  int backing_store;
  unsigned long backing_planes;
  unsigned long backing_pixel;
  Bool save_under;
  Colormap colormap;
  Bool map_installed;
  int map_state;
  long all_event_masks;
  long your_event_mask;
  long do_not_propagate_mask;
  Bool override_redirect;
  Screen *screen;
} XWindowAttributes;

typedef struct {
  Pixmap background_pixmap;
  unsigned long background_pixel;
  Pixmap border_pixmap;
  unsigned long border_pixel;
  int bit_gravity;
  int win_gravity;
  int backing_store;
  unsigned long backing_planes;
  unsigned long backing_pixel;
  Bool save_under;
  long event_mask;
  long do_not_propagate_mask;
  Bool override_redirect;
  Colormap colormap;
  Cursor cursor;
} XSetWindowAttributes;

typedef struct _XImage {
  int width, height;
  int xoffset;
  int format;
  char *data;
  int byte_order;
  int bitmap_unit;
  int bitmap_bit_order;
  int bitmap_pad;
  int depth;
  int bytes_per_line;
  int bits_per_pixel;
  unsigned long red_mask;
  unsigned long green_mask;
  unsigned long blue_mask;
  XPointer obdata;
} XImage;

typedef struct {
  short x, y;
} XPoint;

typedef struct {
  short x1, y1, x2, y2;
} XSegment;

typedef struct {
  short x, y;
  unsigned short width, height;
} XRectangle;

/* Events */
typedef struct {
  int type;
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window window;
} XAnyEvent;

typedef struct {
  int type;
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window window;
  Window root;
  Window subwindow;
  Time time;
  int x, y;
  int x_root, y_root;
  unsigned int state;
  unsigned int keycode;
  Bool same_screen;
} XKeyEvent;
typedef XKeyEvent XKeyPressedEvent;
typedef XKeyEvent XKeyReleasedEvent;

typedef struct {
  int type;
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window window;
  Window root;
  Window subwindow;
  Time time;
  int x, y;
  int x_root, y_root;
  unsigned int state;
  unsigned int button;
  Bool same_screen;
} XButtonEvent;

typedef struct {
  int type;
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window window;
  Window root;
  Window subwindow;
  Time time;
  int x, y;
  int x_root, y_root;
  unsigned int state;
  char is_hint;
  Bool same_screen;
} XMotionEvent;

typedef struct {
  int type;
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window window;
  Window root;
  Window subwindow;
  Time time;
  int x, y;
  int x_root, y_root;
  int mode;
  int detail;
  Bool same_screen;
  Bool focus;
  unsigned int state;
} XCrossingEvent;

typedef struct {
  int type;
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window window;
  int x, y;
  int width, height;
  int count;
} XExposeEvent;

typedef struct {
  int type;
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window event;
  Window window;
  int x, y;
  int width, height;
  int border_width;
  Window above;
  Bool override_redirect;
} XConfigureEvent;

typedef struct {
  int type;
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window window;
  Atom message_type;
  int format;
  union {
    char b[20];
    short s[10];
    long l[5];
  } data;
} XClientMessageEvent;

typedef struct {
  int type;
  unsigned long serial;
  Bool send_event;
  Display *display;
  Window window;
  int mode;
  int detail;
} XFocusChangeEvent;

typedef union _XEvent {
  int type;
  XAnyEvent xany;
  XKeyEvent xkey;
  XButtonEvent xbutton;
  XMotionEvent xmotion;
  XCrossingEvent xcrossing;
  XExposeEvent xexpose;
  XConfigureEvent xconfigure;
  XClientMessageEvent xclient;
  XFocusChangeEvent xfocus;
  long pad[24];
} XEvent;

/* Display macros */
#define DefaultScreen(dpy) ((dpy) ? (dpy)->default_screen : 0)
#define ScreenOfDisplay(dpy, scr) (&((dpy)->screens[scr]))
#define RootWindow(dpy, scr) (nullx_screen(dpy, scr)->root)
#define DefaultRootWindow(dpy) RootWindow(dpy, DefaultScreen(dpy))
#define DefaultColormap(dpy, scr) (nullx_screen(dpy, scr)->cmap)
#define DefaultDepth(dpy, scr) (nullx_screen(dpy, scr)->root_depth)
#define DefaultVisual(dpy, scr) (nullx_screen(dpy, scr)->root_visual)
#define DisplayWidth(dpy, scr) (nullx_screen(dpy, scr)->width)
#define DisplayHeight(dpy, scr) (nullx_screen(dpy, scr)->height)
#define BlackPixel(dpy, scr) (nullx_screen(dpy, scr)->black_pixel)
#define WhitePixel(dpy, scr) (nullx_screen(dpy, scr)->white_pixel)
#define DisplayPlanes(dpy, scr) (nullx_screen(dpy, scr)->root_depth)

Screen *nullx_screen(Display *dpy, int scr);

/* Functions used by XPPAUT */
Display *XOpenDisplay(const char *name);
int XCloseDisplay(Display *dpy);
int XFlush(Display *dpy);
int XSync(Display *dpy, Bool discard);
int (*XSynchronize(Display *dpy, Bool onoff))(Display *);
int XBell(Display *dpy, int percent);
int XPending(Display *dpy);
int XNextEvent(Display *dpy, XEvent *ev);
Status XSendEvent(Display *dpy, Window w, Bool propagate, long mask, XEvent *ev);
int XFree(void *data);

Window XCreateSimpleWindow(Display *dpy, Window parent, int x, int y,
                           unsigned int width, unsigned int height,
                           unsigned int border_width, unsigned long border,
                           unsigned long background);
int XDestroyWindow(Display *dpy, Window w);
int XDestroySubwindows(Display *dpy, Window w);
int XMapWindow(Display *dpy, Window w);
int XMapRaised(Display *dpy, Window w);
int XMapSubwindows(Display *dpy, Window w);
int XUnmapWindow(Display *dpy, Window w);
int XUnmapSubwindows(Display *dpy, Window w);
int XRaiseWindow(Display *dpy, Window w);
int XLowerWindow(Display *dpy, Window w);
int XMoveWindow(Display *dpy, Window w, int x, int y);
int XResizeWindow(Display *dpy, Window w, unsigned int width,
                  unsigned int height);
int XMoveResizeWindow(Display *dpy, Window w, int x, int y,
                      unsigned int width, unsigned int height);
int XIconifyWindow(Display *dpy, Window w, int screen);
int XSelectInput(Display *dpy, Window w, long mask);
int XClearWindow(Display *dpy, Window w);
int XClearArea(Display *dpy, Window w, int x, int y, unsigned int width,
               unsigned int height, Bool exposures);
int XSetWindowBorderWidth(Display *dpy, Window w, unsigned int width);
int XSetWindowBorder(Display *dpy, Window w, unsigned long pixel);
int XSetWindowBackground(Display *dpy, Window w, unsigned long pixel);
int XSetWindowBackgroundPixmap(Display *dpy, Window w, Pixmap p);
int XChangeWindowAttributes(Display *dpy, Window w, unsigned long mask,
                            XSetWindowAttributes *attr);
Status XGetWindowAttributes(Display *dpy, Window w, XWindowAttributes *attr);
Status XGetGeometry(Display *dpy, Drawable d, Window *root, int *x, int *y,
                    unsigned int *width, unsigned int *height,
                    unsigned int *border, unsigned int *depth);
int XSetInputFocus(Display *dpy, Window focus, int revert_to, Time time);
int XGetInputFocus(Display *dpy, Window *focus, int *revert_to);
int XDefineCursor(Display *dpy, Window w, Cursor c);
Cursor XCreateFontCursor(Display *dpy, unsigned int shape);
Atom XInternAtom(Display *dpy, const char *name, Bool only_if_exists);
Status XSetWMProtocols(Display *dpy, Window w, Atom *protocols, int count);
int XSetStandardProperties(Display *dpy, Window w, const char *window_name,
                           const char *icon_name, Pixmap icon_pixmap,
                           char **argv, int argc, void *hints);

GC XCreateGC(Display *dpy, Drawable d, unsigned long mask, XGCValues *values);
int XFreeGC(Display *dpy, GC gc);
int XSetForeground(Display *dpy, GC gc, unsigned long pixel);
int XSetBackground(Display *dpy, GC gc, unsigned long pixel);
int XSetFunction(Display *dpy, GC gc, int function);
int XSetFont(Display *dpy, GC gc, Font font);
int XSetLineAttributes(Display *dpy, GC gc, unsigned int width, int line_style,
                       int cap_style, int join_style);
int XSetDashes(Display *dpy, GC gc, int offset, const char *dashes, int n);
int XSetFillStyle(Display *dpy, GC gc, int style);

int XDrawString(Display *dpy, Drawable d, GC gc, int x, int y,
                const char *s, int len);
int XDrawLine(Display *dpy, Drawable d, GC gc, int x1, int y1, int x2,
              int y2);
int XDrawPoint(Display *dpy, Drawable d, GC gc, int x, int y);
int XDrawRectangle(Display *dpy, Drawable d, GC gc, int x, int y,
                   unsigned int width, unsigned int height);
int XFillRectangle(Display *dpy, Drawable d, GC gc, int x, int y,
                   unsigned int width, unsigned int height);
int XDrawArc(Display *dpy, Drawable d, GC gc, int x, int y,
             unsigned int width, unsigned int height, int a1, int a2);
int XFillArc(Display *dpy, Drawable d, GC gc, int x, int y,
             unsigned int width, unsigned int height, int a1, int a2);
int XCopyArea(Display *dpy, Drawable src, Drawable dst, GC gc, int sx, int sy,
              unsigned int width, unsigned int height, int dx, int dy);

Pixmap XCreatePixmap(Display *dpy, Drawable d, unsigned int width,
                     unsigned int height, unsigned int depth);
int XFreePixmap(Display *dpy, Pixmap p);
Pixmap XCreateBitmapFromData(Display *dpy, Drawable d, const char *data,
                             unsigned int width, unsigned int height);
Pixmap XCreatePixmapFromBitmapData(Display *dpy, Drawable d, char *data,
                                   unsigned int width, unsigned int height,
                                   unsigned long fg, unsigned long bg,
                                   unsigned int depth);
int XReadBitmapFileData(const char *filename, unsigned int *width,
                        unsigned int *height, unsigned char **data, int *x_hot,
                        int *y_hot);
XImage *XGetImage(Display *dpy, Drawable d, int x, int y, unsigned int width,
                  unsigned int height, unsigned long plane_mask, int format);
unsigned long XGetPixel(XImage *img, int x, int y);
int XDestroyImage(XImage *img);

XFontStruct *XLoadQueryFont(Display *dpy, const char *name);
int XUnloadFont(Display *dpy, Font font);
int XTextWidth(XFontStruct *font, const char *s, int count);

Status XAllocColor(Display *dpy, Colormap cmap, XColor *color);
Status XParseColor(Display *dpy, Colormap cmap, const char *spec,
                   XColor *color);
int XQueryColors(Display *dpy, Colormap cmap, XColor *colors, int n);

int XLookupString(XKeyEvent *ev, char *buf, int nbytes, KeySym *keysym,
                  void *status);
KeyCode XKeysymToKeycode(Display *dpy, KeySym keysym);

#ifdef __cplusplus
}
#endif

#endif
