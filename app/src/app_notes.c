/* Free annotations on plots: text, arrows and markers in data coordinates,
   XPPAUT's Text, etc. menu. The plot windows keep theirs in a->nt and draw
   them with ImPlot (ui_notes.c); the Figure window keeps them in the panel
   (fig.h) and writes them with the figure. Each action is written to the
   session as the xpplab call that makes the same annotation. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "export.h"

static const char *plot_names[APP_PLOTS] = {"time", "phase", "auto", "auto2"};
static const char *marker_names[] = {"o", "o", "s", "^", "D", "x", "+", "*", "."};

void app_note_init(App *a) {
  NoteUI *n = &a->nt;
  memset(n, 0, sizeof *n);
  n->mode = -1;
  n->marker = FIG_MARK_CIRCLE;
  snprintf(n->text, sizeof n->text, "label");
}

void app_notes_reset(App *a) {
  memset(a->nt.n, 0, sizeof a->nt.n);
  a->nt.mode = -1;
  a->nt.have_start = false;
  a->echo.notes = false;
}

void app_note_defaults(const App *a, int kind, FigNote *n) {
  memset(n, 0, sizeof *n);
  n->kind = kind;
  n->pt = a->nt.pt;
  n->marker = a->nt.marker;
  memcpy(n->rgb, a->nt.rgb, sizeof n->rgb);
  if (kind == FIG_NOTE_TEXT) snprintf(n->text, FIG_TEXT, "%s", a->nt.text);
}

/* The Python call that makes the annotation, after "fig[K]." or
   "notes[...].": text(1.0, 2.0, "hi", size=12.0). */
void app_note_call(const FigNote *n, char *out, size_t len) {
  char b1[64], b2[64], b3[64], b4[64], s[FIG_TEXT * 2 + 8], c[40] = "";
  if (n->rgb[0] != 0 || n->rgb[1] != 0 || n->rgb[2] != 0)
    snprintf(c, sizeof c, ", color=\"#%02x%02x%02x\"", (int)(n->rgb[0] * 255 + 0.5f),
             (int)(n->rgb[1] * 255 + 0.5f), (int)(n->rgb[2] * 255 + 0.5f));
  if (n->kind == FIG_NOTE_TEXT) {
    char sz[40] = "";
    if (n->pt > 0) snprintf(sz, sizeof sz, ", size=%s", echo_num(n->pt, b3, sizeof b3));
    snprintf(out, len, "text(%s, %s, %s%s%s)", echo_num(n->x, b1, sizeof b1),
             echo_num(n->y, b2, sizeof b2), echo_str(n->text, s, sizeof s), sz, c);
  } else if (n->kind == FIG_NOTE_ARROW) {
    snprintf(out, len, "arrow(%s, %s, %s, %s%s)", echo_num(n->x, b1, sizeof b1),
             echo_num(n->y, b2, sizeof b2), echo_num(n->x2, b3, sizeof b3),
             echo_num(n->y2, b4, sizeof b4), c);
  } else {
    int m = n->marker > 0 && n->marker < FIG_MARK_COUNT ? n->marker : FIG_MARK_CIRCLE;
    snprintf(out, len, "marker(%s, %s, \"%s\"%s)", echo_num(n->x, b1, sizeof b1),
             echo_num(n->y, b2, sizeof b2), marker_names[m], c);
  }
}

static bool usable(App *a, const FigNote *n) {
  char msg[200];
  if (n->kind == FIG_NOTE_TEXT) {
    if (!n->text[0]) {
      app_log(a, "error: annotate text: there is no text to write");
      return false;
    }
    if (!fig_text_check(n->text, msg, sizeof msg)) {
      app_log(a, "error: annotate text: %s", msg);
      return false;
    }
  }
  if (!isfinite(n->x) || !isfinite(n->y) ||
      (n->kind == FIG_NOTE_ARROW && (!isfinite(n->x2) || !isfinite(n->y2)))) {
    app_log(a, "error: annotate: the position must be a number");
    return false;
  }
  return true;
}

bool app_note_add(App *a, int where, const FigNote *in) {
  FigNote n = *in;
  bool outer;
  char call[1400];
  if (where == APP_NOTE_FIGURE) return app_fig_note_add(a, a->fg.panel, &n);
  if (where < 0 || where >= APP_PLOTS || !usable(a, &n)) return false;
  if (a->nt.n[where] >= FIG_MAX_NOTES) {
    app_log(a, "error: the %s plot has %d annotations already; clear some first",
            plot_names[where], FIG_MAX_NOTES);
    return false;
  }
  if (n.kind == FIG_NOTE_TEXT && n.pt <= 0) n.pt = 12;
  a->nt.note[where][a->nt.n[where]++] = n;
  outer = echo_begin(a);
  if (outer) {
    if (!a->echo.notes) {
      echo_line(a, "notes = xs.Annotations()");
      a->echo.notes = true;
    }
    app_note_call(&n, call, sizeof call);
    echo_line(a, "notes[\"%s\"].%s", plot_names[where], call);
  }
  echo_end(a);
  return true;
}

void app_note_clear(App *a, int where, int k) {
  bool outer;
  if (where == APP_NOTE_FIGURE) {
    app_fig_note_clear(a, a->fg.panel, k);
    return;
  }
  if (where < 0 || where >= APP_PLOTS) return;
  if (k >= a->nt.n[where]) {
    app_log(a, "error: the %s plot has %d annotations", plot_names[where], a->nt.n[where]);
    return;
  }
  if (a->nt.n[where] == 0) return;
  if (k < 0) {
    a->nt.n[where] = 0;
  } else {
    memmove(&a->nt.note[where][k], &a->nt.note[where][k + 1],
            sizeof(FigNote) * (size_t)(a->nt.n[where] - k - 1));
    a->nt.n[where]--;
  }
  outer = echo_begin(a);
  if (outer && a->echo.notes) {
    if (k < 0) echo_line(a, "notes[\"%s\"].clear_annotations()", plot_names[where]);
    else echo_line(a, "notes[\"%s\"].clear_annotations(%d)", plot_names[where], k);
  }
  echo_end(a);
}

void app_note_click(App *a, int where, double x, double y) {
  NoteUI *t = &a->nt;
  FigNote n;
  if (t->mode < 0 || t->plot != where) return;
  if (t->mode == FIG_NOTE_ARROW && !t->have_start) {
    t->have_start = true;
    t->x0 = x;
    t->y0 = y;
    app_status(a, "Click where the arrow points.");
    return;
  }
  app_note_defaults(a, t->mode, &n);
  n.x = t->mode == FIG_NOTE_ARROW ? t->x0 : x;
  n.y = t->mode == FIG_NOTE_ARROW ? t->y0 : y;
  n.x2 = x;
  n.y2 = y;
  t->mode = -1;
  t->have_start = false;
  app_note_add(a, where, &n);
}
