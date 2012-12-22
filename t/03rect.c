#include "tickit.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TickitRect rect1, rect2, rectOut;

  plan_tests(30);

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

  return exit_status();
}
