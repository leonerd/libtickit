#include "tickit.h"
#include "hooklists.h"

#include <stdio.h>   /* sscanf */
#include <stdlib.h>
#include <string.h>

#define streq(a,b) (!strcmp(a,b))

struct TickitPen {
  signed   int fg      : 9, /* 0 - 255 or -1 */
               bg      : 9; /* 0 - 255 or -1 */

  unsigned int bold    : 1,
               under   : 1,
               italic  : 1,
               reverse : 1,
               strike  : 1;

  signed   int altfont : 5; /* 1 - 10 or -1 */

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

  struct TickitEventHook *hooks;
};

DEFINE_HOOKLIST_FUNCS(pen,TickitPen,TickitPenEventFn)

TickitPen *tickit_pen_new(void)
{
  TickitPen *pen = malloc(sizeof(TickitPen));
  if(!pen)
    return NULL;

  pen->hooks = NULL;

  for(TickitPenAttr attr = 0; attr < TICKIT_N_PEN_ATTRS; attr++)
    tickit_pen_clear_attr(pen, attr);

  return pen;
}

TickitPen *tickit_pen_clone(TickitPen *orig)
{
  TickitPen *pen = tickit_pen_new();
  tickit_pen_copy(pen, orig, 1);
  return pen;
}

void tickit_pen_destroy(TickitPen *pen)
{
  tickit_hooklist_unbind_and_destroy(pen->hooks, pen);
  free(pen);
}

int tickit_pen_has_attr(const TickitPen *pen, TickitPenAttr attr)
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

int tickit_pen_is_nonempty(const TickitPen *pen)
{
  for(TickitPenAttr attr = 0; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(tickit_pen_has_attr(pen, attr))
      return 1;
  }
  return 0;
}

int tickit_pen_is_nondefault(const TickitPen *pen)
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
    case TICKIT_PENTYPE_COLOUR:
      if(tickit_pen_get_colour_attr(pen, attr) > -1)
        return 1;
      break;
    }
  }
  return 0;
}

int tickit_pen_get_bool_attr(const TickitPen *pen, TickitPenAttr attr)
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
      return;
  }
  run_events(pen, TICKIT_EV_CHANGE, NULL);
}

int tickit_pen_get_int_attr(const TickitPen *pen, TickitPenAttr attr)
{
  if(!tickit_pen_has_attr(pen, attr))
    return -1;

  switch(attr) {
    case TICKIT_PEN_ALTFONT: return pen->altfont;
    default:
      return 0;
  }
}

void tickit_pen_set_int_attr(TickitPen *pen, TickitPenAttr attr, int val)
{
  switch(attr) {
    case TICKIT_PEN_ALTFONT: pen->altfont = val; pen->valid.altfont = 1; break;
    default:
      return;
  }
  run_events(pen, TICKIT_EV_CHANGE, NULL);
}

/* Cheat and pretend the index of a colour attribute is a number attribute */
int tickit_pen_get_colour_attr(const TickitPen *pen, TickitPenAttr attr)
{
  if(!tickit_pen_has_attr(pen, attr))
    return -1;

  switch(attr) {
    case TICKIT_PEN_FG: return pen->fg;
    case TICKIT_PEN_BG: return pen->bg;
    default:
      return 0;
  }
}

void tickit_pen_set_colour_attr(TickitPen *pen, TickitPenAttr attr, int val)
{
  switch(attr) {
    case TICKIT_PEN_FG: pen->fg = val; pen->valid.fg = 1; break;
    case TICKIT_PEN_BG: pen->bg = val; pen->valid.bg = 1; break;
    default:
      return;
  }
  run_events(pen, TICKIT_EV_CHANGE, NULL);
}

static struct { const char *name; int colour; } colournames[] = {
  { "black",   0 },
  { "red",     1 },
  { "green",   2 },
  { "yellow",  3 },
  { "blue",    4 },
  { "magenta", 5 },
  { "cyan",    6 },
  { "white",   7 },
  // 256-colour palette
  { "grey",     8 },
  { "brown",   94 },
  { "orange", 208 },
  { "pink",   212 },
  { "purple", 128 },
};

int tickit_pen_set_colour_attr_desc(TickitPen *pen, TickitPenAttr attr, const char *desc)
{
  int hi = 0;
  int val;
  if(strncmp(desc, "hi-", 3) == 0) {
    desc += 3;
    hi   = 8;
  }

  if(sscanf(desc, "%d", &val) == 1) {
    if(hi && val > 7)
      return 0;

    tickit_pen_set_colour_attr(pen, attr, val + hi);
    return 1;
  }

  for(int i = 0; i < sizeof(colournames)/sizeof(colournames[0]); i++) {
    if(strcmp(desc, colournames[i].name) != 0)
      continue;

    val = colournames[i].colour;
    if(val < 8 && hi)
      val += hi;

    tickit_pen_set_colour_attr(pen, attr, val);
    return 1;
  }

  return 0;
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
      return;
  }
  run_events(pen, TICKIT_EV_CHANGE, NULL);
}

int tickit_pen_equiv_attr(const TickitPen *a, const TickitPen *b, TickitPenAttr attr)
{
  switch(tickit_pen_attrtype(attr)) {
  case TICKIT_PENTYPE_BOOL:
    return tickit_pen_get_bool_attr(a, attr) == tickit_pen_get_bool_attr(b, attr);
  case TICKIT_PENTYPE_INT:
    return tickit_pen_get_int_attr(a, attr) == tickit_pen_get_int_attr(b, attr);
  case TICKIT_PENTYPE_COLOUR:
    return tickit_pen_get_colour_attr(a, attr) == tickit_pen_get_colour_attr(b, attr);
  }

  return 0;
}

int tickit_pen_equiv(const TickitPen *a, const TickitPen *b)
{
  if(a == b)
    return 1;

  for(TickitPenAttr attr = 0; attr < TICKIT_N_PEN_ATTRS; attr++)
    if(!tickit_pen_equiv_attr(a, b, attr))
      return 0;

  return 1;
}

void tickit_pen_copy_attr(TickitPen *dst, const TickitPen *src, TickitPenAttr attr)
{
  switch(tickit_pen_attrtype(attr)) {
  case TICKIT_PENTYPE_BOOL:
    tickit_pen_set_bool_attr(dst, attr, tickit_pen_get_bool_attr(src, attr));
    return;
  case TICKIT_PENTYPE_INT:
    tickit_pen_set_int_attr(dst, attr, tickit_pen_get_int_attr(src, attr));
    return;
  case TICKIT_PENTYPE_COLOUR:
    tickit_pen_set_colour_attr(dst, attr, tickit_pen_get_colour_attr(src, attr));
    return;
  }

  return;
}

void tickit_pen_copy(TickitPen *dst, const TickitPen *src, int overwrite)
{
  int changed = 0;
  for(TickitPenAttr attr = 0; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(!tickit_pen_has_attr(src, attr))
      continue;
    if(tickit_pen_has_attr(dst, attr) &&
       (!overwrite || tickit_pen_equiv_attr(src, dst, attr)))
      continue;

    /* Avoid using copy_attr so it doesn't invoke change events yet */
    switch(attr) {
    case TICKIT_PEN_FG:
      dst->fg = src->fg;
      dst->valid.fg = 1;
      break;
    case TICKIT_PEN_BG:
      dst->bg = src->bg;
      dst->valid.bg = 1;
      break;
    case TICKIT_PEN_BOLD:
      dst->bold = src->bold;
      dst->valid.bold = 1;
      break;
    case TICKIT_PEN_ITALIC:
      dst->italic = src->italic;
      dst->valid.italic = 1;
      break;
    case TICKIT_PEN_UNDER:
      dst->under = src->under;
      dst->valid.under = 1;
      break;
    case TICKIT_PEN_REVERSE:
      dst->reverse = src->reverse;
      dst->valid.reverse = 1;
      break;
    case TICKIT_PEN_STRIKE:
      dst->strike = src->strike;
      dst->valid.strike = 1;
      break;
    case TICKIT_PEN_ALTFONT:
      dst->altfont = src->altfont;
      dst->valid.altfont = 1;
      break;
    case TICKIT_N_PEN_ATTRS:
      continue;
    }

    changed++;
  }

  if(changed)
    run_events(dst, TICKIT_EV_CHANGE, NULL);
}

TickitPenAttrType tickit_pen_attrtype(TickitPenAttr attr)
{
  switch(attr) {
    case TICKIT_PEN_FG:
    case TICKIT_PEN_BG:
      return TICKIT_PENTYPE_COLOUR;

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

const char *tickit_pen_attrname(TickitPenAttr attr)
{
  switch(attr) {
    case TICKIT_PEN_FG:      return "fg";
    case TICKIT_PEN_BG:      return "bg";
    case TICKIT_PEN_BOLD:    return "b";
    case TICKIT_PEN_UNDER:   return "u";
    case TICKIT_PEN_ITALIC:  return "i";
    case TICKIT_PEN_REVERSE: return "rv";
    case TICKIT_PEN_STRIKE:  return "strike";
    case TICKIT_PEN_ALTFONT: return "af";

    case TICKIT_N_PEN_ATTRS: ;
  }
  return NULL;
}

TickitPenAttr tickit_pen_lookup_attr(const char *name)
{
  switch(name[0]) {
    case 'a':
      return streq(name+1,"f") ? TICKIT_PEN_ALTFONT
                               : -1;
    case 'b':
      return name[1] == 0      ? TICKIT_PEN_BOLD
           : streq(name+1,"g") ? TICKIT_PEN_BG
                               : -1;
    case 'f':
      return streq(name+1,"g") ? TICKIT_PEN_FG
                               : -1;
    case 'i':
      return name[1] == 0      ? TICKIT_PEN_ITALIC
                               : -1;
    case 'r':
      return streq(name+1,"v") ? TICKIT_PEN_REVERSE
                               : -1;
    case 's':
      return streq(name+1,"trike") ? TICKIT_PEN_STRIKE
                               : -1;
    case 'u':
      return name[1] == 0      ? TICKIT_PEN_UNDER
                               : -1;
  }
  return -1;
}
