#include "tickit.h"

#include <string.h> // memcpy, memmove

struct TickitRectSet {
  TickitRect *rects;
  size_t      count;  /* How many we consider valid */
  size_t      size;   /* How many can fit in the allocated array */
};

TickitRectSet *tickit_rectset_new(void)
{
  TickitRectSet *ret = malloc(sizeof(TickitRectSet));
  if(!ret)
    return NULL;

  ret->size = 4;
  ret->rects = malloc(ret->size * sizeof(ret->rects[0]));
  if(!ret->rects)
    goto abort_free;

  ret->count = 0;

  return ret;

abort_free:
  free(ret);
  return NULL;
}

void tickit_rectset_destroy(TickitRectSet *trs)
{
  free(trs->rects);
  free(trs);
}

void tickit_rectset_clear(TickitRectSet *trs)
{
  trs->count = 0;
}

size_t tickit_rectset_rects(const TickitRectSet *trs)
{
  return trs->count;
}

size_t tickit_rectset_get_rects(const TickitRectSet *trs, TickitRect rects[], size_t n)
{
  if(n > trs->count)
    n = trs->count;

  memcpy(rects, trs->rects, n * sizeof(trs->rects[0]));
  return n;
}

static int cmprect(const TickitRect *a, const TickitRect *b)
{
  if(a->top != b->top)
    return a->top - b->top;
  return a->left - b->left;
}

static int insert_rect(TickitRectSet *trs, const TickitRect *r)
{
  if(trs->count + 1 > trs->size) {
    TickitRect *newrects = realloc(trs->rects, trs->size * 2 * sizeof(trs->rects[0]));
    if(!newrects)
      return 0;

    trs->rects = newrects;
    trs->size *= 2;
  }

  /* TODO: binary search */
  int idx;
  for(idx = 0; idx < trs->count; idx++)
    if(cmprect(trs->rects + idx, r) > 0)
      break;

  memmove(trs->rects + idx + 1, trs->rects + idx, (trs->count - idx) * sizeof(trs->rects[0]));
  trs->rects[idx] = *r;
  trs->count++;
  return 1;
}

static void delete_rect(TickitRectSet *trs, int idx)
{
  memmove(trs->rects + idx, trs->rects + idx + 1, (trs->count - idx - 1) * sizeof(trs->rects[0]));
  trs->count--;
}

void tickit_rectset_add(TickitRectSet *trs, const TickitRect *rect)
{
  int top    = rect->top;
  int bottom = tickit_rect_bottom(rect);
  int left   = rect->left;
  int right  = tickit_rect_right(rect);

restart:
  for(int i = 0; i < trs->count; i++) {
    TickitRect *r = trs->rects + i;
    int r_bottom = tickit_rect_bottom(r);
    int r_right  = tickit_rect_right(r);

    // Because the rects are ordered, there is no point continuing if we're
    // already past it
    if(bottom < r->top)
      break;

    if(top > r_bottom || left > r_right || right < r->left)
      continue;

    if(tickit_rect_contains(r, rect))
      // Already entirely covered, just return
      return;

    int top_eq    = top    == r->top;
    int bottom_eq = bottom == r_bottom;
    int left_eq   = left   == r->left;
    int right_eq  = right  == r_right;

    if((top_eq && bottom_eq) || (left_eq && right_eq)) {
      // Stretch an existing rectangle horizontally or vertically
      // Tempting to think we can just do this in-place but needs to account
      // for being able to merge multiple at once
      if(r->top   < top)    top    = r->top;
      if(r_bottom > bottom) bottom = r_bottom;
      if(r->left  < left)   left   = r->left;
      if(r_right  > right)  right  = r_right;

      delete_rect(trs, i);
      goto restart;
    }

    if(top == r_bottom || bottom == r->top)
      // No actual interaction, just add it. This case handles the recursion
      // implied at the end of this loop
      continue;

    // Non-simple interaction. Split r and rect into the 2 or 3 separate rects
    // it now must be composed of, delete r, then recurse on those to-be-added
    // rects instead.
    TickitRect to_add[3];
    int n = tickit_rect_add(to_add, r, rect); // TODO: top/left/bottom/right ?

    delete_rect(trs, i);

    for(i = 0; i < n; i++)
      tickit_rectset_add(trs, to_add + i);
    return;
  }

  // If we got this far then we need to add it
  TickitRect new;
  tickit_rect_init_bounded(&new, top, left, bottom, right);

  // TODO: error handling
  insert_rect(trs, &new);
}

void tickit_rectset_subtract(TickitRectSet *trs, const TickitRect *rect)
{
  for(int i = 0; i < trs->count; i++) {
    TickitRect *r = trs->rects + i;
    if(!tickit_rect_intersects(r, rect))
      continue;

    TickitRect remains[4];
    int n = tickit_rect_subtract(remains, r, rect);

    delete_rect(trs, i);
    i--;

    // It doesn't matter if insert starts putting these before i; the worst
    // that will happen is that a few rects that we inspected once just move
    // underneath us and we have to inspect them a second time
    int j;
    for(j = 0; j < n; j++)
      tickit_rectset_add(trs, remains + j);
  }
}

int tickit_rectset_intersects(const TickitRectSet *trs, const TickitRect *rect)
{
  for(int i = 0; i < trs->count; i++)
    if(tickit_rect_intersects(trs->rects + i, rect))
      return 1;

  return 0;
}

int tickit_rectset_contains(const TickitRectSet *trs, const TickitRect *rectptr)
{
  // We might want to modify it
  TickitRect rect = *rectptr;

  for(int i = 0; i < trs->count; i++) {
    TickitRect *r = trs->rects + i;
    if(!tickit_rect_intersects(r, &rect))
      continue;

    // Because rects are in order, if there's any part of rect above or to
    // the left of here we know we didn't match it
    if(rect.top  < r->top ||
       rect.left < r->left)
      return 0;

    int r_bottom = tickit_rect_bottom(r);
    int rect_bottom = tickit_rect_bottom(&rect);

    if(rect.top < r_bottom && r_bottom < rect_bottom) {
      int rect_right = tickit_rect_right(&rect);

      TickitRect lower;
      tickit_rect_init_bounded(&lower, r_bottom, rect.left, rect_bottom, rect_right);

      if(!tickit_rectset_contains(trs, &lower))
        return 0;

      // tickit_rect_init_bounded(&rect, rect.top, rect.left, r_bottom, r_right)
      rect.lines = r_bottom - rect.top;
    }

    return tickit_rect_contains(r, &rect);
  }

  return 0;
}
