#include "tickit.h"

static inline int minint(int a, int b) { return a < b ? a : b; }
static inline int maxint(int a, int b) { return a > b ? a : b; }

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
  int top    = maxint(a->top, b->top);
  int bottom = minint(tickit_rect_bottom(a), tickit_rect_bottom(b));

  if(top >= bottom)
    return 0;

  int left  = maxint(a->left, b->left);
  int right = minint(tickit_rect_right(a), tickit_rect_right(b));

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
  return (small->top                >= large->top               ) &&
         (tickit_rect_bottom(small) <= tickit_rect_bottom(large)) &&
         (small->left               >= large->left              ) &&
         (tickit_rect_right(small)  <= tickit_rect_right(large) );
}
