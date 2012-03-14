#include "tickit.h"

#include <stdlib.h>

struct TickitPen {
  unsigned int fg      : 8, /* 0 - 255 */
               bg      : 8, /* 0 - 255 */
               bold    : 1,
               under   : 1,
               italic  : 1,
               reverse : 1,
               strike  : 1,
               altfont : 4; /* 1 - 10 */

  struct {
    unsigned int fg      : 1,
                 bg      : 1,
                 bold    : 1,
                 under   : 1,
                 italic  : 1,
                 reverse : 1,
                 strike  : 1,
                 altfont : 1;
  } valid;
};

TickitPen *tickit_pen_new(void)
{
  TickitPen *pen = malloc(sizeof(TickitPen));
  if(!pen)
    return NULL;

  for(TickitPenAttr attr = 0; attr < TICKIT_N_PEN_ATTRS; attr++)
    tickit_pen_clear_attr(pen, attr);

  return pen;
}

void tickit_pen_destroy(TickitPen *pen)
{
  free(pen);
}

int tickit_pen_has_attr(TickitPen *pen, TickitPenAttr attr)
{
  switch(attr) {
    case TICKIT_PEN_FG:      return pen->valid.fg;
    case TICKIT_PEN_BG:      return pen->valid.bg;
    case TICKIT_PEN_BOLD:    return pen->valid.bold;
    case TICKIT_PEN_UNDER:   return pen->valid.under;
    case TICKIT_PEN_ITALIC:  return pen->valid.italic;
    case TICKIT_PEN_REVERSE: return pen->valid.reverse;
    case TICKIT_PEN_STRIKE:  return pen->valid.strike;
    case TICKIT_PEN_ALTFONT: return pen->valid.altfont;

    case TICKIT_N_PEN_ATTRS:
      return 0;
  }

  return 0;
}

int tickit_pen_is_nonempty(TickitPen *pen)
{
  for(TickitPenAttr attr = 0; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(tickit_pen_has_attr(pen, attr))
      return 1;
  }
  return 0;
}

int tickit_pen_is_nondefault(TickitPen *pen)
{
  for(TickitPenAttr attr = 0; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(!tickit_pen_has_attr(pen, attr))
      continue;
    switch(tickit_pen_attrtype(attr)) {
    case TICKIT_PENTYPE_BOOL:
      if(tickit_pen_get_bool_attr(pen, attr))
        return 1;
      break;
    case TICKIT_PENTYPE_INT:
      if(tickit_pen_get_int_attr(pen, attr) > -1)
        return 1;
      break;
    }
  }
  return 0;
}

int tickit_pen_get_bool_attr(TickitPen *pen, TickitPenAttr attr)
{
  if(!tickit_pen_has_attr(pen, attr))
    return 0;

  switch(attr) {
    case TICKIT_PEN_BOLD:    return pen->bold;
    case TICKIT_PEN_UNDER:   return pen->under;
    case TICKIT_PEN_ITALIC:  return pen->italic;
    case TICKIT_PEN_REVERSE: return pen->reverse;
    case TICKIT_PEN_STRIKE:  return pen->strike;
    default:
      return 0;
  }
}

void tickit_pen_set_bool_attr(TickitPen *pen, TickitPenAttr attr, int val)
{
  switch(attr) {
    case TICKIT_PEN_BOLD:    pen->bold    = !!val; pen->valid.bold    = 1; break;
    case TICKIT_PEN_UNDER:   pen->under   = !!val; pen->valid.under   = 1; break;
    case TICKIT_PEN_ITALIC:  pen->italic  = !!val; pen->valid.italic  = 1; break;
    case TICKIT_PEN_REVERSE: pen->reverse = !!val; pen->valid.reverse = 1; break;
    case TICKIT_PEN_STRIKE:  pen->strike  = !!val; pen->valid.strike  = 1; break;
    default:
      break;
  }
}

int tickit_pen_get_int_attr(TickitPen *pen, TickitPenAttr attr)
{
  if(!tickit_pen_has_attr(pen, attr))
    return -1;

  switch(attr) {
    case TICKIT_PEN_FG:      return pen->fg;
    case TICKIT_PEN_BG:      return pen->bg;
    case TICKIT_PEN_ALTFONT: return pen->altfont;
    default:
      return 0;
  }
}

void tickit_pen_set_int_attr(TickitPen *pen, TickitPenAttr attr, int val)
{
  switch(attr) {
    case TICKIT_PEN_FG:      pen->fg      = val; pen->valid.fg      = 1; break;
    case TICKIT_PEN_BG:      pen->bg      = val; pen->valid.bg      = 1; break;
    case TICKIT_PEN_ALTFONT: pen->altfont = val; pen->valid.altfont = 1; break;
    default:
      break;
  }
}

void tickit_pen_clear_attr(TickitPen *pen, TickitPenAttr attr)
{
  switch(attr) {
    case TICKIT_PEN_FG:      pen->valid.fg      = 0; break;
    case TICKIT_PEN_BG:      pen->valid.bg      = 0; break;
    case TICKIT_PEN_BOLD:    pen->valid.bold    = 0; break;
    case TICKIT_PEN_UNDER:   pen->valid.under   = 0; break;
    case TICKIT_PEN_ITALIC:  pen->valid.italic  = 0; break;
    case TICKIT_PEN_REVERSE: pen->valid.reverse = 0; break;
    case TICKIT_PEN_STRIKE:  pen->valid.strike  = 0; break;
    case TICKIT_PEN_ALTFONT: pen->valid.altfont = 0; break;

    case TICKIT_N_PEN_ATTRS:
      break;
  }
}

int tickit_pen_equal_attr(TickitPen *a, TickitPen *b, TickitPenAttr attr)
{
  if(!tickit_pen_has_attr(a, attr) && !tickit_pen_has_attr(b, attr))
    return 1;

  if(!tickit_pen_has_attr(a, attr) || !tickit_pen_has_attr(b, attr))
    return 0;

  switch(tickit_pen_attrtype(attr)) {
  case TICKIT_PENTYPE_BOOL:
    return tickit_pen_get_bool_attr(a, attr) == tickit_pen_get_bool_attr(b, attr);
  case TICKIT_PENTYPE_INT:
    return tickit_pen_get_int_attr(a, attr) == tickit_pen_get_int_attr(b, attr);
  }

  return 0;
}

void tickit_pen_copy_attr(TickitPen *dst, TickitPen *src, TickitPenAttr attr)
{
  if(!tickit_pen_has_attr(src, attr)) {
    tickit_pen_clear_attr(dst, attr);
    return;
  }

  switch(tickit_pen_attrtype(attr)) {
  case TICKIT_PENTYPE_BOOL:
    tickit_pen_set_bool_attr(dst, attr, tickit_pen_get_bool_attr(src, attr));
    return;
  case TICKIT_PENTYPE_INT:
    tickit_pen_set_int_attr(dst, attr, tickit_pen_get_int_attr(src, attr));
    return;
  }

  return;
}

TickitPenAttrType tickit_pen_attrtype(TickitPenAttr attr)
{
  switch(attr) {
    case TICKIT_PEN_FG:
    case TICKIT_PEN_BG:
    case TICKIT_PEN_ALTFONT:
      return TICKIT_PENTYPE_INT;

    case TICKIT_PEN_BOLD:
    case TICKIT_PEN_UNDER:
    case TICKIT_PEN_ITALIC:
    case TICKIT_PEN_REVERSE:
    case TICKIT_PEN_STRIKE:
      return TICKIT_PENTYPE_BOOL;

    case TICKIT_N_PEN_ATTRS:
      return -1;
  }

  return -1;
}
