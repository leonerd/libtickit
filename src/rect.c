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

int tickit_rect_add(TickitRect ret[3], const TickitRect *a, const TickitRect *b)
{
  int a_bottom = tickit_rect_bottom(a);
  int a_right  = tickit_rect_right(a);
  int b_bottom = tickit_rect_bottom(b);
  int b_right  = tickit_rect_right(b);

  if(a->left > b_right  || b->left > a_right ||
      a->top  > b_bottom || b->top  > a_bottom) {
    ret[0] = *a;
    ret[1] = *b;
    return 2;
  }

  int rows[4];
  rows[0] = a->top;
  rows[1] = b->top;
  rows[2] = a_bottom;
  rows[3] = b_bottom;

  /* Since we know top < bottom for each rect individually we can do this
   * better than a plain sort
   */
  if(rows[0] > rows[1]) { int tmp = rows[0]; rows[0] = rows[1]; rows[1] = tmp; } // tops now in order
  if(rows[2] > rows[3]) { int tmp = rows[2]; rows[2] = rows[3]; rows[3] = tmp; } // bottoms now in order
  if(rows[1] > rows[2]) { int tmp = rows[1]; rows[1] = rows[2]; rows[2] = tmp; } // all now in order

  int rects = 0;
  for(int i = 0; i < 3; i++ ) {
    int this_top    = rows[i];
    int this_bottom = rows[i+1];

    // Skip non-unique
    if(this_top == this_bottom)
      continue;

    int has_a = this_top >= a->top && this_bottom <= a_bottom;
    int has_b = this_top >= b->top && this_bottom <= b_bottom;

    int this_left =  (has_a && has_b) ? minint(a->left, b->left) :
                     (has_a)          ? a->left                  :
                                        b->left;

    int this_right = (has_a && has_b) ? maxint(a_right, b_right) :
                     (has_a)          ? a_right                  :
                                        b_right;

    if(rects && ret[rects-1].left == this_left && ret[rects-1].cols == this_right - this_left) {
      ret[rects-1].lines = this_bottom - ret[rects-1].top;
    }
    else {
      tickit_rect_init_bounded(ret + rects, this_top, this_left, this_bottom, this_right);
      rects++;
    }
  }

  return rects;
}
