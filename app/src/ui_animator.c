/* The animator: an XPPAUT .ani file played over the run (XPPAUT's Viewer).
   The engine evaluates each frame with XPPAUT's code; this draws it. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

/* XPPAUT's colours: 0 black, 20-29 red ... purple; computed colours run
   along the same scale. */
static ImU32 ani_colour(const xppe_ani_item *it) {
  static const unsigned named[10] = {0xff0000, 0xff4500, 0xff8c00, 0xffb000,
                                     0xe8d800, 0x9acd32, 0x00a000, 0x00a0a0,
                                     0x0000ff, 0x8a2be2};
  if (it->color >= 20 && it->color <= 29) return igGetColorU32_Vec4(HEX(named[it->color - 20]));
  if (it->color >= 0) return igGetColorU32_Vec4(HEX(0x000000));
  {
    float h = 0.83f * (1.0f - (float)(it->level < 0 ? 0 : it->level > 1 ? 1 : it->level));
    float r, g, b;
    igColorConvertHSVtoRGB(h, 1.0f, 0.9f, &r, &g, &b);
    return igGetColorU32_Vec4(V4(r, g, b, 1));
  }
}

static CometTrail *trail_for(AniUI *v, int command) {
  int i;
  for (i = 0; i < v->ncomets; i++)
    if (v->comet[i].command == command) return &v->comet[i];
  if (v->ncomets == APP_MAX_COMETS) return NULL;
  memset(&v->comet[v->ncomets], 0, sizeof v->comet[0]);
  v->comet[v->ncomets].command = command;
  return &v->comet[v->ncomets++];
}

void ui_animator(App *a) {
  AniUI *v = &a->ani;
  xpp_engine_api *x;
  ImDrawList *dl;
  ImVec2_c pos, size, org;
  double sx, sy, s;
  int rows, n, i, ngrab;
  bool g_active = false, g_release = false;
  if (!igBegin(WIN_ANI, &a->show_ani, 0)) {
    igEnd();
    return;
  }
  if (!a->model_ok) {
    igTextDisabled("Open a model and an animation (.ani) file to play it here.");
    igEnd();
    return;
  }
  x = &a->eng->api;
  if (Button("Open .ani...")) ui_dialog_open_ani(a);
  Tooltip("An XPPAUT animation file: lines, circles, text and comets whose "
          "positions are formulas of the variables.");
  if (!v->loaded) {
    igSameLine(0, -1);
    igTextDisabled("No animation loaded. Many example models come with one "
                   "(pend, doubpend, lorenz2, ...).");
    igEnd();
    return;
  }
  if (!a->have_traj) {
    igSameLine(0, -1);
    if (Button("Run")) app_run(a);
    igSameLine(0, -1);
    igTextDisabled("Run the model; the animation plays the run.");
    igEnd();
    return;
  }
  rows = a->traj.nrows;
  igSameLine(0, -1);
  if (Button(v->playing ? "Pause" : "Play")) {
    v->playing = !v->playing;
    if (v->playing && v->row >= rows - 1) v->row = 0;
    v->show_initial = false;
  }
  igSameLine(0, -1);
  if (Button("|<")) {
    v->row = 0;
    v->playing = false;
    v->show_initial = false;
  }
  Tooltip("Back to the start.");
  igSameLine(0, -1);
  igSetNextItemWidth(220);
  if (igSliderInt("##row", &v->row, 0, rows > 0 ? rows - 1 : 0, "row %d", 0)) {
    v->playing = false;
    v->show_initial = false;
  }
  igSameLine(0, -1);
  igSetNextItemWidth(120);
  igSliderFloat("speed", &v->speed, 1, 2000, "%.0f rows/s",
                ImGuiSliderFlags_Logarithmic);
  igSameLine(0, -1);
  igCheckbox("Loop", &v->loop);
  igSameLine(0, -1);
  igTextDisabled("t = %.4g", rows > 0 ? a->traj.cols[0][v->row < rows ? v->row : rows - 1] : 0.0);
  ngrab = x->xppe_ani_grab_count();
  if (ngrab > 0) {
    igSameLine(0, -1);
    if (igCheckbox("Grab", &v->grab)) {
      v->dragging = false;
      v->show_initial = v->grab;
      v->init_serial = a->run_serial;
      if (v->grab) {
        v->playing = false;
        v->row = 0;
      }
    }
    Tooltip("Show the grab points of the file and drag one with the mouse. "
            "Releasing sets the values the file names (initial values or "
            "parameters) and runs the model if the file says runnow=1. The "
            "mode ends after one drag. The command: ani grab X Y X2 Y2.");
  }
  if (v->has_null) {
    igSetNextItemWidth(110);
    Combo("##ncpar", &v->nc_par, a->pars, a->npars);
    igSameLine(0, -1);
    igSetNextItemWidth(60);
    igInputInt("steps##nc", &v->nc_steps, 0, 0, 0);
    igSameLine(0, -1);
    igSetNextItemWidth(80);
    InputDouble("from##nc", &v->nc_lo, "%.4g");
    igSameLine(0, -1);
    igSetNextItemWidth(80);
    InputDouble("to##nc", &v->nc_hi, "%.4g");
    igSameLine(0, -1);
    if (Button("Freeze range") && v->nc_par >= 0 && v->nc_par < a->npars)
      app_ani_nullclines(a, a->pars[v->nc_par], v->nc_steps, v->nc_lo, v->nc_hi);
    Tooltip("xnull and ynull lines draw the nullclines of the phase plane's "
            "variables. A line that picks a number k draws the k-th of a set "
            "frozen over a range of the parameter (wcstim: s0 from 0 to 1.2 "
            "in 12 steps). The command: ani nullclines PAR STEPS LO HI.");
  }

  /* Play: advance by speed x elapsed time. */
  if (v->playing && rows > 0) {
    int step;
    v->acc += igGetIO_Nil()->DeltaTime * v->speed;
    step = (int)v->acc;
    v->acc -= step;
    v->row += step;
    if (v->row >= rows) {
      v->row = v->loop ? 0 : rows - 1;
      if (!v->loop) v->playing = false;
    }
  }
  if (v->row >= rows) v->row = rows - 1;
  if (v->row < 0) v->row = 0;
  if (v->row < v->last_row || v->row == 0) v->ncomets = 0; /* trails restart */

  pos = igGetCursorScreenPos();
  size = igGetContentRegionAvail();
  if (size.x < 40 || size.y < 40) {
    igEnd();
    return;
  }
  /* The animation's window, as large as fits, with y upwards. */
  sx = size.x / (v->xhi - v->xlo);
  sy = size.y / (v->yhi - v->ylo);
  s = sx < sy ? sx : sy;
  org = V2(pos.x + (float)(size.x - s * (v->xhi - v->xlo)) / 2,
           pos.y + (float)(size.y - s * (v->yhi - v->ylo)) / 2);
#define PX(X) (org.x + (float)(((X) - v->xlo) * s))
#define PY(Y) (org.y + (float)((v->yhi - (Y)) * s))
  if (v->show_initial && v->init_serial != a->run_serial) v->show_initial = false;
  if (v->grab) {
    /* the canvas is a button, so that a press on it is not a drag of the window */
    bool pressed;
    ImVec2_c m;
    double mx, my;
    igInvisibleButton("##anigrab", size, 0);
    pressed = igIsItemActivated();
    g_active = igIsItemActive();
    g_release = igIsItemDeactivated();
    m = igGetMousePos();
    mx = v->xlo + (m.x - org.x) / s;
    my = v->yhi - (m.y - org.y) / s;
    if (pressed && !v->dragging) {
      if (app_ani_grab_press(a, mx, my)) v->drag_time = igGetTime();
    } else if (v->dragging && g_active && (mx != v->drag_x || my != v->drag_y)) {
      /* only a move is an event; its velocity needs the time since the last */
      double now = igGetTime();
      app_ani_grab_drag(a, mx, my, now - v->drag_time);
      v->drag_time = now;
      v->drag_x = mx;
      v->drag_y = my;
    }
  }
  dl = igGetWindowDrawList();
  ImDrawList_PushClipRect(dl, pos, V2(pos.x + size.x, pos.y + size.y), true);
  ImDrawList_AddRectFilled(dl, org,
                           V2(org.x + (float)(s * (v->xhi - v->xlo)),
                              org.y + (float)(s * (v->yhi - v->ylo))),
                           igGetColorU32_Vec4(HEX(0xffffff)), 0, 0);
  /* x equal to y happens for a moment while the axes are swapped: wait */
  if (v->has_null && a->pp.xvar != a->pp.yvar &&
      (v->null_dirty || v->nc_x != a->pp.xvar || v->nc_y != a->pp.yvar))
    app_ani_nullclines(a, NULL, 0, 0, 0);
  if (v->grab && v->row != 0) v->row = 0;
  /* while dragging, the values the drag set; in Grab mode, or after a grab
     that did not run, the initial values; otherwise the row of the run */
  if (v->dragging)
    n = x->xppe_ani_frame_current();
  else if (v->grab || v->show_initial)
    n = x->xppe_ani_frame_initial();
  else
    n = x->xppe_ani_frame(v->row);
  for (i = 0; i < n; i++) {
    xppe_ani_item it;
    ImU32 col;
    float w;
    if (x->xppe_ani_get(i, &it) != XPPE_OK) continue;
    col = ani_colour(&it);
    w = it.thickness > 0 ? (float)it.thickness : 1.0f;
    switch (it.shape) {
    case XPPE_ANI_LINE:
      ImDrawList_AddLine(dl, V2(PX(it.x1), PY(it.y1)), V2(PX(it.x2), PY(it.y2)),
                         col, w, 0);
      break;
    case XPPE_ANI_NULLCLINE: {
      int ns = x->xppe_ani_segments(i, NULL, 0), k;
      double *sg = ns > 0 ? malloc(sizeof(double) * 4 * (size_t)ns) : NULL;
      if (!sg) break;
      x->xppe_ani_segments(i, sg, ns);
      for (k = 0; k < ns; k++)
        ImDrawList_AddLine(dl, V2(PX(sg[4 * k]), PY(sg[4 * k + 1])),
                           V2(PX(sg[4 * k + 2]), PY(sg[4 * k + 3])), col, w, 0);
      free(sg);
      break;
    }
    case XPPE_ANI_RECT: {
      ImVec2_c p1 = V2(PX(it.x1 < it.x2 ? it.x1 : it.x2), PY(it.y1 > it.y2 ? it.y1 : it.y2));
      ImVec2_c p2 = V2(PX(it.x1 < it.x2 ? it.x2 : it.x1), PY(it.y1 > it.y2 ? it.y2 : it.y1));
      if (it.filled)
        ImDrawList_AddRectFilled(dl, p1, p2, col, 0, 0);
      else
        ImDrawList_AddRect(dl, p1, p2, col, 0, w, 0);
      break;
    }
    case XPPE_ANI_ELLIPSE: {
      ImVec2_c c = V2(PX(it.x1), PY(it.y1));
      ImVec2_c r = V2((float)(fabs(it.x2) * s), (float)(fabs(it.y2) * s));
      if (it.filled)
        ImDrawList_AddEllipseFilled(dl, c, r, col, 0, 0);
      else
        ImDrawList_AddEllipse(dl, c, r, col, 0, 0, w, 0);
      break;
    }
    case XPPE_ANI_TEXT: {
      float fs = igGetFontSize() * (0.8f + 0.3f * (float)it.font_size);
      ImDrawList_AddText_FontPtr(dl, it.serif ? g_fonts.ui : g_fonts.mono, fs,
                                 V2(PX(it.x1), PY(it.y1) - fs), col, it.text,
                                 NULL, 0, NULL);
      break;
    }
    case XPPE_ANI_COMET: {
      CometTrail *t = trail_for(v, it.command);
      int k, keep = it.trail < APP_COMET_POINTS ? it.trail : APP_COMET_POINTS;
      if (!t) break;
      if (v->row != v->last_row || t->n == 0) {
        if (t->n == keep && keep > 0) {
          memmove(t->x, t->x + 1, sizeof(float) * (size_t)(keep - 1));
          memmove(t->y, t->y + 1, sizeof(float) * (size_t)(keep - 1));
          memmove(t->col, t->col + 1, sizeof(unsigned) * (size_t)(keep - 1));
          t->n--;
        }
        if (t->n < keep) {
          t->x[t->n] = (float)it.x1;
          t->y[t->n] = (float)it.y1;
          t->col[t->n] = col;
          t->n++;
        }
      }
      for (k = 0; k < t->n; k++) {
        ImVec2_c p = V2(PX(t->x[k]), PY(t->y[k]));
        if (it.thickness < 0)
          ImDrawList_AddCircleFilled(dl, p, (float)-it.thickness, t->col[k], 0);
        else if (k > 0)
          ImDrawList_AddLine(dl, V2(PX(t->x[k - 1]), PY(t->y[k - 1])), p,
                             t->col[k], w, 0);
      }
      break;
    }
    default:
      break;
    }
  }
  if (v->grab) {
    /* the grab points as crosses the size of their tolerance */
    for (i = 0; i < ngrab; i++) {
      double gx, gy, tol;
      if (x->xppe_ani_grab_point(i, &gx, &gy, &tol) != XPPE_OK) continue;
      ImDrawList_AddLine(dl, V2(PX(gx - tol), PY(gy - tol)), V2(PX(gx + tol), PY(gy + tol)),
                         igGetColorU32_Vec4(HEX(0x000000)), 1.5f, 0);
      ImDrawList_AddLine(dl, V2(PX(gx - tol), PY(gy + tol)), V2(PX(gx + tol), PY(gy - tol)),
                         igGetColorU32_Vec4(HEX(0x000000)), 1.5f, 0);
    }
    if (v->dragging && (g_release || !g_active)) {
      ImDrawList_PopClipRect(dl);
      app_ani_grab_release(a);
      v->last_row = v->row;
      igEnd();
      return;
    }
  }
#undef PX
#undef PY
  ImDrawList_PopClipRect(dl);
  v->last_row = v->row;
  igEnd();
}
