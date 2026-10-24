/* The 3D view: three columns of the run in a box that the mouse turns
   (XPPAUT's 3D plots, with its theta and phi view angles). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
  double lo[3], hi[3];
  double cth, sth, cph, sph;
  ImVec2_c centre;
  float scale;
} Projection;

/* The box's corners go to [-1,1]^3, then XPPAUT's own rotation (make_rot
   in graphics.c), so a model's theta and phi look as they do in XPPAUT:
   phi = 90 is a side view, phi = 0 looks straight down. */
static ImVec2_c project(const Projection *p, const double q[3]) {
  double u[3], sx, sy;
  int i;
  ImVec2_c s;
  for (i = 0; i < 3; i++)
    u[i] = 2 * (q[i] - p->lo[i]) / (p->hi[i] - p->lo[i]) - 1;
  sx = p->cth * u[0] + p->sth * u[1];
  sy = -p->cph * p->sth * u[0] + p->cph * p->cth * u[1] + p->sph * u[2];
  s.x = p->centre.x + p->scale * (float)sx;
  s.y = p->centre.y - p->scale * (float)sy;
  return s;
}

static void data_box(App *a, double lo[3], double hi[3]) {
  const View3D *v = &a->v3;
  int cols[3] = {v->x, v->y, v->z}, c, i, k;
  for (c = 0; c < 3; c++) {
    lo[c] = 1e300;
    hi[c] = -1e300;
  }
  for (k = -1; k < a->nfrozen; k++) {
    const Trajectory *t = k < 0 ? &a->traj : &a->frozen[k];
    if (k < 0 && !a->have_traj) continue;
    for (c = 0; c < 3; c++) {
      if (cols[c] < 0 || cols[c] >= t->ncols) continue;
      for (i = 0; i < t->nrows; i++) {
        double q = t->cols[cols[c]][i];
        if (q < lo[c]) lo[c] = q;
        if (q > hi[c]) hi[c] = q;
      }
    }
  }
  for (c = 0; c < 3; c++) {
    if (!(hi[c] > lo[c])) {
      double m = lo[c] < 1e300 ? lo[c] : 0;
      lo[c] = m - 1;
      hi[c] = m + 1;
    }
  }
}

static void draw_curve(ImDrawList *dl, const Projection *p, const Trajectory *t,
                       const View3D *v, ImU32 col, float width) {
  ImVec2_c last = {0, 0};
  int i;
  if (v->x >= t->ncols || v->y >= t->ncols || v->z >= t->ncols) return;
  for (i = 0; i < t->nrows; i++) {
    double q[3] = {t->cols[v->x][i], t->cols[v->y][i], t->cols[v->z][i]};
    ImVec2_c s = project(p, q);
    if (i > 0) ImDrawList_AddLine(dl, last, s, col, width, 0);
    last = s;
  }
}

static void column_combo(App *a, const char *label, int *col) {
  igSetNextItemWidth(100);
  if (igBeginCombo(label, *col >= 0 && *col < a->ncols ? a->cols[*col] : "", 0)) {
    int j;
    for (j = 0; j < a->ncols; j++)
      if (igSelectable_Bool(a->cols[j], j == *col, 0, V2(0, 0))) *col = j;
    igEndCombo();
  }
  igSameLine(0, -1);
}

void ui_view3d(App *a) {
  View3D *v = &a->v3;
  ImDrawList *dl;
  ImVec2_c pos, size;
  Projection p;
  double th, ph;
  int k, c, e;
  bool dark = a->dark;
  ImU32 ink, faint, bg;
  static const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                   {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  if (v->front > 0 && --v->front == 0) igSetNextWindowFocus();
  if (!igBegin(WIN_VIEW3D, &a->show_view3d, 0)) {
    igEnd();
    return;
  }
  if (!a->model_ok || a->ncols < 3) {
    igTextDisabled("Open a model with at least three columns to see it in 3D.");
    igEnd();
    return;
  }
  column_combo(a, "x", &v->x);
  column_combo(a, "y", &v->y);
  column_combo(a, "z", &v->z);
  igSetNextItemWidth(90);
  igSliderFloat("theta", &v->theta, -180, 180, "%.0f", 0);
  igSameLine(0, -1);
  igSetNextItemWidth(90);
  igSliderFloat("phi", &v->phi, -180, 180, "%.0f", 0);
  igSameLine(0, -1);
  if (Button("Reset")) {
    app_view3d_reset(a);
    v = &a->v3;
  }
  Tooltip("The view, box and columns the model asks for (its @ options).");
  igSameLine(0, -1);
  igCheckbox("Box from the data", &v->auto_box);
  igTextDisabled("Drag to turn, scroll to zoom, double-click to reset the "
                 "zoom.");

  pos = igGetCursorScreenPos();
  size = igGetContentRegionAvail();
  if (size.x < 50 || size.y < 50) {
    igEnd();
    return;
  }
  igInvisibleButton("##view3d", size, 0);
  if (igIsItemActive()) {
    ImVec2_c d = igGetMouseDragDelta(ImGuiMouseButton_Left, 0);
    v->theta += d.x * 0.4f;
    v->phi += d.y * 0.4f;
    if (v->theta > 180) v->theta -= 360;
    if (v->theta < -180) v->theta += 360;
    if (v->phi > 90) v->phi = 90;
    if (v->phi < -90) v->phi = -90;
    igResetMouseDragDelta(ImGuiMouseButton_Left);
  }
  if (igIsItemHovered(0)) {
    float wheel = igGetIO_Nil()->MouseWheel;
    if (wheel != 0) v->zoom *= powf(1.1f, wheel);
    if (igIsMouseDoubleClicked_Nil(ImGuiMouseButton_Left)) v->zoom = 1;
  }

  if (v->auto_box || !(v->box[0] < v->box[1] && v->box[2] < v->box[3] &&
                       v->box[4] < v->box[5])) {
    data_box(a, p.lo, p.hi);
  } else {
    for (c = 0; c < 3; c++) {
      p.lo[c] = v->box[2 * c];
      p.hi[c] = v->box[2 * c + 1];
    }
  }
  th = v->theta * M_PI / 180;
  ph = v->phi * M_PI / 180;
  p.cth = cos(th);
  p.sth = sin(th);
  p.cph = cos(ph);
  p.sph = sin(ph);
  p.centre = V2(pos.x + size.x / 2, pos.y + size.y / 2);
  /* the box's corners reach sqrt(3) from the centre in any view */
  p.scale = 0.27f * (size.x < size.y ? size.x : size.y) * v->zoom;

  dl = igGetWindowDrawList();
  bg = igGetColorU32_Vec4(dark ? HEX(0x1e1f22) : HEX(0xffffff));
  ink = igGetColorU32_Vec4(dark ? HEX(0xd0d0d0) : HEX(0x303030));
  faint = igGetColorU32_Vec4(dark ? V4(0.6f, 0.6f, 0.6f, 0.35f)
                                  : V4(0.3f, 0.3f, 0.3f, 0.30f));
  ImDrawList_PushClipRect(dl, pos, V2(pos.x + size.x, pos.y + size.y), true);
  ImDrawList_AddRectFilled(dl, pos, V2(pos.x + size.x, pos.y + size.y), bg, 0, 0);
  /* the box, its three labelled axes, and the low and high values */
  {
    ImVec2_c corner[8];
    for (k = 0; k < 8; k++) {
      double q[3] = {(k & 1) ? p.hi[0] : p.lo[0], (k & 2) ? p.hi[1] : p.lo[1],
                     (k & 4) ? p.hi[2] : p.lo[2]};
      corner[k] = project(&p, q);
    }
    for (e = 0; e < 12; e++)
      ImDrawList_AddLine(dl, corner[edges[e][0]], corner[edges[e][1]],
                         e == 0 || e == 4 || e == 8 ? ink : faint,
                         e == 0 || e == 4 || e == 8 ? 1.5f : 1.0f, 0);
    {
      const int axis_end[3] = {1, 2, 4};
      const int cols[3] = {v->x, v->y, v->z};
      for (c = 0; c < 3; c++) {
        char lab[96];
        ImVec2_c at = corner[axis_end[c]];
        snprintf(lab, sizeof lab, "%s %.3g", a->cols[cols[c]], p.hi[c]);
        ImDrawList_AddText_Vec2(dl, V2(at.x + 4, at.y - 4), ink, lab, NULL);
      }
      {
        char lab[64];
        snprintf(lab, sizeof lab, "(%.3g, %.3g, %.3g)", p.lo[0], p.lo[1], p.lo[2]);
        ImDrawList_AddText_Vec2(dl, V2(corner[0].x + 4, corner[0].y + 2), faint,
                                lab, NULL);
      }
    }
  }
  for (k = 0; k < a->nfrozen; k++)
    draw_curve(dl, &p, &a->frozen[k], v,
               igGetColorU32_Vec4(V4(0.12f, 0.47f, 0.71f, 0.35f)), 1.0f);
  if (a->have_traj)
    draw_curve(dl, &p, &a->traj, v, igGetColorU32_Vec4(HEX(0xd62728)), 1.6f);
  else
    ImDrawList_AddText_Vec2(dl, V2(pos.x + 12, pos.y + 12), ink,
                            "Run the model to see its trajectory here.", NULL);
  ImDrawList_PopClipRect(dl);
  igEnd();
}
