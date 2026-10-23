/* Null Xlib: Xutil subset used by XPPAUT. See Xlib.h. */
#ifndef NULLX_XUTIL_H
#define NULLX_XUTIL_H
#include <X11/Xlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned char *value;
  Atom encoding;
  int format;
  unsigned long nitems;
} XTextProperty;

typedef struct {
  long flags;
  int x, y;
  int width, height;
  int min_width, min_height;
  int max_width, max_height;
  int width_inc, height_inc;
  struct {
    int x;
    int y;
  } min_aspect, max_aspect;
  int base_width, base_height;
  int win_gravity;
} XSizeHints;

typedef struct {
  long flags;
  Bool input;
  int initial_state;
  Pixmap icon_pixmap;
  Window icon_window;
  int icon_x, icon_y;
  Pixmap icon_mask;
  XID window_group;
} XWMHints;

typedef struct {
  char *res_name;
  char *res_class;
} XClassHint;

typedef struct {
  int min_width, min_height;
  int max_width, max_height;
  int width_inc, height_inc;
} XIconSize;

typedef struct {
  XPointer compose_ptr;
  int chars_matched;
} XComposeStatus;

#define USPosition (1L << 0)
#define USSize (1L << 1)
#define PPosition (1L << 2)
#define PSize (1L << 3)
#define PMinSize (1L << 4)
#define PMaxSize (1L << 5)
#define PResizeInc (1L << 6)
#define PAspect (1L << 7)
#define PBaseSize (1L << 8)
#define PWinGravity (1L << 9)

#define InputHint (1L << 0)
#define StateHint (1L << 1)
#define IconPixmapHint (1L << 2)
#define IconWindowHint (1L << 3)
#define IconPositionHint (1L << 4)
#define IconMaskHint (1L << 5)
#define WindowGroupHint (1L << 6)

#define WithdrawnState 0
#define NormalState 1
#define IconicState 3

#define BitmapSuccess 0
#define BitmapOpenFailed 1
#define BitmapFileInvalid 2
#define BitmapNoMemory 3

Status XStringListToTextProperty(char **list, int count, XTextProperty *tp);
void XSetWMProperties(Display *dpy, Window w, XTextProperty *window_name,
                      XTextProperty *icon_name, char **argv, int argc,
                      XSizeHints *normal_hints, XWMHints *wm_hints,
                      XClassHint *class_hints);
void XSetWMName(Display *dpy, Window w, XTextProperty *tp);
Status XGetIconSizes(Display *dpy, Window w, XIconSize **list, int *count);

#ifdef __cplusplus
}
#endif
#endif
