#include "tickit.h"
#include "taplib.h"

#include <stdio.h>

TickitRect *rect_init_strp(TickitRect *rect, const char *str)
{
  int top, left, bottom, right;
  if(sscanf(str, "(%d,%d)..(%d,%d)", &left, &top, &right, &bottom) == 4)
    tickit_rect_init_bounded(rect, top, left, bottom, right);

  return rect;
}

int is_rect(TickitRect *got, const char *expect, char *name)
{
  TickitRect exp;
  rect_init_strp(&exp, expect);

  if(got->top   != exp.top   ||
     got->left  != exp.left  ||
     got->lines != exp.lines ||
     got->cols  != exp.cols) {
    ok(0, name);
    diag("got (%d,%d)..(%d,%d) expected (%d,%d)..(%d,%d)",
        got->left, got->top, tickit_rect_right(got),  tickit_rect_bottom(got),
        exp.left,  exp.top,  tickit_rect_right(&exp), tickit_rect_bottom(&exp));
  }
  else {
    ok(1, name);
  }
}

int main(int argc, char *argv[])
{
  TickitRect rect1, rect2, rectOut;

  plan_tests(104);

  tickit_rect_init_sized(&rect1, 5, 10, 7, 20);

  is_int(rect1.top,    5, "rect1.top");
  is_int(rect1.left,  10, "rect1.left");
  is_int(rect1.lines,  7, "rect1.lines");
  is_int(rect1.cols,  20, "rect1.cols");
  is_int(tickit_rect_bottom(&rect1), 12, "tickit_rect_bottom(rect1)");
  is_int(tickit_rect_right(&rect1),  30, "tickit_rect_right(rect1)");

  tickit_rect_init_sized(&rect2, 0, 0, 25, 80);
  tickit_rect_intersect(&rectOut, &rect1, &rect2);

  is_int(rectOut.top,    5, "rectOut.top from intersect wholescreen");
  is_int(rectOut.left,  10, "rectOut.left from intersect wholescreen");
  is_int(rectOut.lines,  7, "rectOut.lines from intersect wholescreen");
  is_int(rectOut.cols,  20, "rectOut.cols from intersect wholescreen");
  is_int(tickit_rect_bottom(&rectOut), 12, "tickit_rect_bottom(rectOut) from intersect wholescreen");
  is_int(tickit_rect_right(&rectOut),  30, "tickit_rect_right(rectOut) from intersect wholescreen");

  tickit_rect_init_sized(&rect2, 10, 20, 15, 60);
  tickit_rect_intersect(&rectOut, &rect1, &rect2);

  is_int(rectOut.top,   10, "rectOut.top from intersect partial");
  is_int(rectOut.left,  20, "rectOut.left from intersect partial");
  is_int(rectOut.lines,  2, "rectOut.lines from intersect partial");
  is_int(rectOut.cols,  10, "rectOut.cols from intersect partial");
  is_int(tickit_rect_bottom(&rectOut), 12, "tickit_rect_bottom(rectOut) from intersect partial");
  is_int(tickit_rect_right(&rectOut),  30, "tickit_rect_right(rectOut) from intersect partial");

  tickit_rect_init_sized(&rect2, 20, 20, 5, 60);
  ok(!tickit_rect_intersect(&rectOut, &rect1, &rect2), "false from intersect outside");

  tickit_rect_init_sized(&rect2, 7, 12, 3, 10);
  ok(tickit_rect_contains(&rect1, &rect2), "tickit_rect_contains() for smaller");

  tickit_rect_init_sized(&rect2, 3, 10, 5, 12);
  ok(!tickit_rect_contains(&rect1, &rect2), "tickit_rect_contains() for overlap");

  tickit_rect_init_sized(&rect2, 3, 10, 5, 12);
  ok(tickit_rect_intersects(&rect1, &rect2), "tickit_rect_intersects() with overlap");

  tickit_rect_init_sized(&rect2, 14, 10, 3, 20);
  ok(!tickit_rect_intersects(&rect1, &rect2), "tickit_rect_intersects() with other");

  tickit_rect_init_sized(&rect2, 12, 10, 3, 20);
  ok(!tickit_rect_intersects(&rect1, &rect2), "tickit_rect_intersects() with abutting");

  tickit_rect_init_bounded(&rect1, 3, 8, 9, 22);

  is_int(rect1.top,    3, "rect1.top from init_bounded");
  is_int(rect1.left,   8, "rect1.left from init_bounded");
  is_int(rect1.lines,  6, "rect1.lines from init_bounded");
  is_int(rect1.cols,  14, "rect1.cols from init_bounded");
  is_int(tickit_rect_bottom(&rect1),  9, "tickit_rect_bottom(rect1) from init_bounded");
  is_int(tickit_rect_right(&rect1),  22, "tickit_rect_right(rect1) from init_bounded");

  // Rectangle addition
  TickitRect rects[4];
  int n_rects;

  rect_init_strp(&rect1, "(10,10)..(20,20)");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(10,10)..(20,20)"));
  is_int(n_rects, 1, "rect_add same");
  is_rect(rects + 0, "(10,10)..(20,20)", "rects[0] for rect_add same");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(5,10)..(10,20)"));
  is_int(n_rects, 1, "rect_add left");
  is_rect(rects + 0, "(5,10)..(20,20)", "rects[0] for rect_add left");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(20,10)..(25,20)"));
  is_int(n_rects, 1, "rect_add right");
  is_rect(rects + 0, "(10,10)..(25,20)", "rects[0] for rect_add right");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(10,5)..(20,10)"));
  is_int(n_rects, 1, "rect_add top");
  is_rect(rects + 0, "(10,5)..(20,20)", "rects[0] for rect_add top");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(10,20)..(20,25)"));
  is_int(n_rects, 1, "rect_add bottom");
  is_rect(rects + 0, "(10,10)..(20,25)", "rects[0] for rect_add bottom");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(12,5)..(18,10)"));
  is_int(n_rects, 2, "rect_add T above");
  is_rect(rects + 0, "(12,5)..(18,10)", "rects[0] for rect_add T above");
  is_rect(rects + 1, "(10,10)..(20,20)", "rects[1] for rect_add T above");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(12,20)..(18,30)"));
  is_int(n_rects, 2, "rect_add T below");
  is_rect(rects + 0, "(10,10)..(20,20)", "rects[0] for rect_add T below");
  is_rect(rects + 1, "(12,20)..(18,30)", "rects[1] for rect_add T below");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(5,12)..(10,18)"));
  is_int(n_rects, 3, "rect_add T left");
  is_rect(rects + 0, "(10,10)..(20,12)", "rects[0] for rect_add T left");
  is_rect(rects + 1, "(5,12)..(20,18)", "rects[1] for rect_add T left");
  is_rect(rects + 2, "(10,18)..(20,20)", "rects[2] for rect_add T left");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(20,12)..(25,18)"));
  is_int(n_rects, 3, "rect_add T right");
  is_rect(rects + 0, "(10,10)..(20,12)", "rects[0] for rect_add T right");
  is_rect(rects + 1, "(10,12)..(25,18)", "rects[1] for rect_add T right");
  is_rect(rects + 2, "(10,18)..(20,20)", "rects[2] for rect_add T right");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(15,15)..(25,25)"));
  is_int(n_rects, 3, "rect_add diagonal");
  is_rect(rects + 0, "(10,10)..(20,15)", "rects[0] for rect_add diagonal");
  is_rect(rects + 1, "(10,15)..(25,20)", "rects[1] for rect_add diagonal");
  is_rect(rects + 2, "(15,20)..(25,25)", "rects[2] for rect_add diagonal");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(12,8)..(18,22)"));
  is_int(n_rects, 3, "rect_add cross");
  is_rect(rects + 0, "(12,8)..(18,10)", "rects[0] for rect_add cross");
  is_rect(rects + 1, "(10,10)..(20,20)", "rects[1] for rect_add cross");
  is_rect(rects + 2, "(12,20)..(18,22)", "rects[2] for rect_add cross");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(10,30)..(20,40)"));
  is_int(n_rects, 2, "rect_add non-overlap horizontal");
  is_rect(rects + 0, "(10,10)..(20,20)", "rects[0] for rect_add non-overlap horizontal");
  is_rect(rects + 1, "(10,30)..(20,40)", "rects[1] for rect_add non-overlap horizontal");

  n_rects = tickit_rect_add(rects, &rect1, rect_init_strp(&rect2, "(30,10)..(40,20)"));
  is_int(n_rects, 2, "rect_add non-overlap vertical");
  is_rect(rects + 0, "(10,10)..(20,20)", "rects[0] for rect_add non-overlap vertical");
  is_rect(rects + 1, "(30,10)..(40,20)", "rects[1] for rect_add non-overlap vertical");

  // Rectangle subtraction

  n_rects = tickit_rect_subtract(rects, &rect1, rect_init_strp(&rect2, "(10,10)..(20,20)"));
  is_int(n_rects, 0, "rect_subtract self");

  n_rects = tickit_rect_subtract(rects, &rect1, rect_init_strp(&rect2, "(5,10)..(15,20)"));
  is_int(n_rects, 1, "rect_subtract truncate left");
  is_rect(rects + 0, "(15,10)..(20,20)", "rects[0] for rect_subtract truncate left");

  n_rects = tickit_rect_subtract(rects, &rect1, rect_init_strp(&rect2, "(15,10)..(25,20)"));
  is_int(n_rects, 1, "rect_subtract truncate right");
  is_rect(rects + 0, "(10,10)..(15,20)", "rects[0] for rect_subtract truncate right");

  n_rects = tickit_rect_subtract(rects, &rect1, rect_init_strp(&rect2, "(10,5)..(20,15)"));
  is_int(n_rects, 1, "rect_subtract truncate top");
  is_rect(rects + 0, "(10,15)..(20,20)", "rects[0] for rect_subtract truncate top");

  n_rects = tickit_rect_subtract(rects, &rect1, rect_init_strp(&rect2, "(10,15)..(20,25)"));
  is_int(n_rects, 1, "rect_subtract truncate bottom");
  is_rect(rects + 0, "(10,10)..(20,15)", "rects[0] for rect_subtract truncate bottom");

  n_rects = tickit_rect_subtract(rects, &rect1, rect_init_strp(&rect2, "(5,12)..(25,18)"));
  is_int(n_rects, 2, "rect_subtract slice horizontal");
  is_rect(rects + 0, "(10,10)..(20,12)", "rects[0] for rect_subtract slice horizontal");
  is_rect(rects + 1, "(10,18)..(20,20)", "rects[1] for rect_subtract slice horizontal");

  n_rects = tickit_rect_subtract(rects, &rect1, rect_init_strp(&rect2, "(12,5)..(18,25)"));
  is_int(n_rects, 2, "rect_subtract slice vertical");
  is_rect(rects + 0, "(10,10)..(12,20)", "rects[0] for rect_subtract slice vertical");
  is_rect(rects + 1, "(18,10)..(20,20)", "rects[1] for rect_subtract slice vertical");

  n_rects = tickit_rect_subtract(rects, &rect1, rect_init_strp(&rect2, "(5,12)..(15,18)"));
  is_int(n_rects, 3, "rect_subtract U left");
  is_rect(rects + 0, "(10,10)..(20,12)", "rects[0] for rect_subtract U left");
  is_rect(rects + 1, "(15,12)..(20,18)", "rects[1] for rect_subtract U left");
  is_rect(rects + 2, "(10,18)..(20,20)", "rects[2] for rect_subtract U left");

  n_rects = tickit_rect_subtract(rects, &rect1, rect_init_strp(&rect2, "(15,12)..(25,18)"));
  is_int(n_rects, 3, "rect_subtract U right");
  is_rect(rects + 0, "(10,10)..(20,12)", "rects[0] for rect_subtract U right");
  is_rect(rects + 1, "(10,12)..(15,18)", "rects[1] for rect_subtract U right");
  is_rect(rects + 2, "(10,18)..(20,20)", "rects[2] for rect_subtract U right");

  n_rects = tickit_rect_subtract(rects, &rect1, rect_init_strp(&rect2, "(12,5)..(18,15)"));
  is_int(n_rects, 3, "rect_subtract U top");
  is_rect(rects + 0, "(10,10)..(12,15)", "rects[0] for rect_subtract U top");
  is_rect(rects + 1, "(18,10)..(20,15)", "rects[1] for rect_subtract U top");
  is_rect(rects + 2, "(10,15)..(20,20)", "rects[2] for rect_subtract U top");

  n_rects = tickit_rect_subtract(rects, &rect1, rect_init_strp(&rect2, "(12,15)..(18,25)"));
  is_int(n_rects, 3, "rect_subtract U bottom");
  is_rect(rects + 0, "(10,10)..(20,15)", "rects[0] for rect_subtract U bottom");
  is_rect(rects + 1, "(10,15)..(12,20)", "rects[1] for rect_subtract U bottom");
  is_rect(rects + 2, "(18,15)..(20,20)", "rects[2] for rect_subtract U bottom");

  n_rects = tickit_rect_subtract(rects, &rect1, rect_init_strp(&rect2, "(12,12)..(18,18)"));
  is_int(n_rects, 4, "rect_subtract hole");
  is_rect(rects + 0, "(10,10)..(20,12)", "rects[0] for rect_subtract hole");
  is_rect(rects + 1, "(10,12)..(12,18)", "rects[1] for rect_subtract hole");
  is_rect(rects + 2, "(18,12)..(20,18)", "rects[2] for rect_subtract hole");
  is_rect(rects + 3, "(10,18)..(20,20)", "rects[3] for rect_subtract hole");

  return exit_status();
}
