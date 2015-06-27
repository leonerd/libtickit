#include "taplib-tickit.h"
#include "taplib.h"

#include <stdio.h>

TickitRect *rect_init_strp(TickitRect *rect, const char *str)
{
  int top, left, bottom, right, lines, cols;
  if(sscanf(str, "%d,%d..%d,%d", &left, &top, &right, &bottom) == 4) {
    tickit_rect_init_bounded(rect, top, left, bottom, right);
    return rect;
  }
  if(sscanf(str, "%d,%d+%d,%d", &left, &top, &cols, &lines) == 4) {
    tickit_rect_init_sized(rect, top, left, lines, cols);
    return rect;
  }

  return NULL;
}

void is_rect(TickitRect *got, const char *expect, char *name)
{
  TickitRect exp;
  rect_init_strp(&exp, expect);

  if(got->top   != exp.top   ||
     got->left  != exp.left  ||
     got->lines != exp.lines ||
     got->cols  != exp.cols) {
    fail(name);
    diag("got %d,%d..%d,%d expected %d,%d..%d,%d",
        got->left, got->top, tickit_rect_right(got),  tickit_rect_bottom(got),
        exp.left,  exp.top,  tickit_rect_right(&exp), tickit_rect_bottom(&exp));
  }
  else {
    pass(name);
  }
}
