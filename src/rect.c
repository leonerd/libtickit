#include "tickit.h"

void tickit_rect_init_sized(TickitRect *rect, int top, int left, int lines, int cols)
{
  rect->top   = top;
  rect->left  = left;
  rect->lines = lines;
  rect->cols  = cols;
}

void tickit_rect_init_bounded(TickitRect *rect, int top, int left, int bottom, int right)
{
  rect->top   = top;
  rect->left  = left;
  rect->lines = bottom - top;
  rect->cols  = right - left;
}

int tickit_rect_intersect(TickitRect *dst, const TickitRect *a, const TickitRect *b)
{
  int top, bottom, left, right;

  top = a->top;
  if(b->top > top) top = b->top;

  bottom = tickit_rect_bottom(a);
  if(tickit_rect_bottom(b) < bottom) bottom = tickit_rect_bottom(b);

  if(top >= bottom)
    return 0;

  left = a->left;
  if(b->left > left) left = b->left;

  right = tickit_rect_right(a);
  if(tickit_rect_right(b) < right) right = tickit_rect_right(b);

  if(left >= right)
    return 0;

  tickit_rect_init_bounded(dst, top, left, bottom, right);
  return 1;
}

int tickit_rect_intersects(const TickitRect *a, const TickitRect *b)
{
  return a->top < tickit_rect_bottom(b) &&
         b->top < tickit_rect_bottom(a) &&
         a->left < tickit_rect_right(b) &&
         b->left < tickit_rect_right(a);
}

int tickit_rect_contains(const TickitRect *large, const TickitRect *small)
{
  return small->top                >= large->top                &&
         tickit_rect_bottom(small) <= tickit_rect_bottom(large) &&
         small->left               >= large->top                &&
         tickit_rect_right(small)  <= tickit_rect_right(large);
}
