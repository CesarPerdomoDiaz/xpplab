/*
 * The desktop app's plots follow their data until the user zooms or pans
 * them (app/src/ui.h, plot_view_update). This checks that decision frame by
 * frame, as ImGui would report the mouse: what stops following, what does
 * not, and what turns it back on. ImPlot's own zooming is not run here.
 */
#include <stdio.h>
#include <string.h>

#include "ui.h"

enum { LEFT, RIGHT };

static int failures;
static int frame = 100;

/* One frame: the mouse input, then (like ui_view_end) whether the plot was
   under the mouse at the end of it. Returns whether the view follows. */
static bool step(PlotView *v, unsigned shows, bool over, const PlotInput *mouse) {
  PlotInput in;
  bool follow;
  if (mouse)
    in = *mouse;
  else
    memset(&in, 0, sizeof in);
  in.frame = ++frame;
  in.shows = shows;
  follow = plot_view_update(v, &in);
  v->over = over;
  v->over_frame = frame;
  return follow;
}

static void expect(bool got, bool want, const char *what) {
  if (got != want) {
    printf("FAIL: %s: %s, expected %s\n", what, got ? "follows" : "zoomed",
           want ? "follows" : "zoomed");
    failures++;
  }
}

static PlotInput wheel(void) {
  PlotInput in;
  memset(&in, 0, sizeof in);
  in.wheel = true;
  return in;
}

static PlotInput press(int b) {
  PlotInput in;
  memset(&in, 0, sizeof in);
  in.clicked[b] = in.down[b] = true;
  return in;
}

static PlotInput drag(int b) {
  PlotInput in;
  memset(&in, 0, sizeof in);
  in.down[b] = in.dragging[b] = true;
  return in;
}

static PlotInput double_click(void) {
  PlotInput in = press(LEFT);
  in.double_click = true;
  return in;
}

int main(void) {
  PlotView v;
  PlotInput in;
  memset(&v, 0, sizeof v);

  expect(step(&v, 1, true, NULL), true, "a new plot");
  expect(step(&v, 1, true, NULL), true, "the mouse resting over it");

  /* The first turn of the wheel already stops following. */
  in = wheel();
  expect(step(&v, 1, true, &in), false, "the wheel over the plot");
  expect(step(&v, 1, true, NULL), false, "new data after zooming");
  expect(step(&v, 1, false, NULL), false, "the mouse leaving");

  in = double_click();
  expect(step(&v, 1, false, &in), false, "a double-click elsewhere");
  step(&v, 1, true, NULL);
  in = double_click();
  expect(step(&v, 1, true, &in), true, "a double-click on the plot");
  expect(step(&v, 1, true, NULL), true, "new data after the double-click");

  step(&v, 1, false, NULL);
  in = wheel();
  expect(step(&v, 1, false, &in), true, "the wheel away from the plot");

  /* A click without a drag (select a point, start a trajectory). */
  step(&v, 1, true, NULL);
  in = press(LEFT);
  expect(step(&v, 1, true, &in), true, "a left press");
  in.clicked[LEFT] = false;
  expect(step(&v, 1, true, &in), true, "holding the button still");
  expect(step(&v, 1, true, NULL), true, "a click without a drag");

  /* Left drag: pan. */
  in = press(LEFT);
  step(&v, 1, true, &in);
  in = drag(LEFT);
  expect(step(&v, 1, true, &in), false, "a left drag that began on the plot");

  /* A new model, or other quantities on the axes, show all again. */
  expect(step(&v, 2, true, NULL), true, "other quantities");

  /* Right drag: box zoom. */
  in = press(RIGHT);
  expect(step(&v, 2, true, &in), true, "a right press (the menu)");
  in = drag(RIGHT);
  expect(step(&v, 2, true, &in), false, "a right drag that began on the plot");
  expect(step(&v, 2, true, NULL), false, "after the box zoom");

  /* A drag that began elsewhere (a slider, a splitter) and crosses the plot. */
  v.zoomed = false; /* the Fit button */
  step(&v, 2, false, NULL);
  in = press(LEFT);
  step(&v, 2, false, &in);
  in = drag(LEFT);
  expect(step(&v, 2, true, &in), true, "a drag from elsewhere entering the plot");
  expect(step(&v, 2, true, &in), true, "... and over it");
  step(&v, 2, true, NULL); /* released */

  /* The hover of a plot not drawn for a while (a hidden tab) is stale. */
  frame += 50;
  in = wheel();
  expect(step(&v, 2, true, &in), true, "the wheel over a plot just shown again");
  in = wheel();
  expect(step(&v, 2, true, &in), false, "the wheel on the next frame");

  if (failures) {
    printf("%d check(s) failed\n", failures);
    return 1;
  }
  printf("plot view: all checks passed\n");
  return 0;
}
