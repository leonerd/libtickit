#include "tickit.h"
#include "bindings.h"

#include <stdarg.h>
#include <stdio.h>   /* sscanf */
#include <stdlib.h>
#include <string.h>

#define streq(a,b) (!strcmp(a,b))

#define COLOUR_DEFAULT -1

struct TickitPen {
  signed   int fgindex : 9, /* 0 - 255 or COLOUR_DEFAULT */
               bgindex : 9; /* 0 - 255 or COLOUR_DEFAULT */
  TickitPenRGB8 fg_rgb8,
                bg_rgb8;

  unsigned int bold    : 1,
               italic  : 1,
               reverse : 1,
               strike  : 1,
               blink   : 1,
               sizepos : 2;

  signed   int under   : 3; /* 0 to 3 or -1 */
  signed   int altfont : 5; /* 1 - 10 or -1 */

  struct {
    unsigned int fgindex : 1,
                 bgindex : 1,
                 fg_rgb8 : 1,
                 bg_rgb8 : 1,
                 bold    : 1,
                 under   : 1,
                 italic  : 1,
                 reverse : 1,
                 strike  : 1,
                 altfont : 1,
                 blink   : 1,
                 sizepos : 2;
  } valid;

  int refcount;
  struct TickitBindings bindings;
  int freezecount;
  bool changed;
};

DEFINE_BINDINGS_FUNCS(pen,TickitPen,TickitPenEventFn)

TickitPen *tickit_pen_new(void)
{
  TickitPen *pen = malloc(sizeof(TickitPen));
  if(!pen)
    return NULL;

  pen->refcount = 1;
  pen->bindings = (struct TickitBindings){ NULL };
  pen->freezecount = 0;
  pen->changed = false;

  tickit_pen_clear(pen);

  return pen;
}

TickitPen *tickit_pen_new_attrs(TickitPenAttr attr, ...)
{
  TickitPen *pen = tickit_pen_new();
  if(!pen)
    return NULL;

  va_list args;
  va_start(args, attr);
  int first = 1;
  int val;

  while(1) {
    int a = first ? attr : va_arg(args, TickitPenAttr);
    first = 0;
    if(a < 1)
      break;

    if(a == TICKIT_PEN_FG_DESC || a == TICKIT_PEN_BG_DESC) {
      const char *str = va_arg(args, const char *);
      tickit_pen_set_colour_attr_desc(pen, a - 0x100, str);
      continue;
    }

    // TODO: accept colour rgb8?
    switch(tickit_penattr_type(a)) {
    case TICKIT_PENTYPE_BOOL:
      val = va_arg(args, int);
      tickit_pen_set_bool_attr(pen, a, val);
      break;
    case TICKIT_PENTYPE_INT:
      val = va_arg(args, int);
      tickit_pen_set_int_attr(pen, a, val);
      break;
    case TICKIT_PENTYPE_COLOUR:
      val = va_arg(args, int);
      tickit_pen_set_colour_attr(pen, a, val);
      break;
    }
  }

  va_end(args);

  return pen;
}

TickitPen *tickit_pen_clone(const TickitPen *orig)
{
  TickitPen *pen = tickit_pen_new();
  tickit_pen_copy(pen, orig, true);
  return pen;
}

static void destroy(TickitPen *pen)
{
  tickit_bindings_unbind_and_destroy(&pen->bindings, pen);
  free(pen);
}

static void changed(TickitPen *pen)
{
  if(!pen->freezecount)
    run_events(pen, TICKIT_PEN_ON_CHANGE, NULL);
  else
    pen->changed = true;
}

static void freeze(TickitPen *pen)
{
  pen->freezecount++;
}

static void thaw(TickitPen *pen)
{
  pen->freezecount--;
  if(!pen->freezecount && pen->changed) {
    run_events(pen, TICKIT_PEN_ON_CHANGE, NULL);
    pen->changed = false;
  }
}

TickitPen *tickit_pen_ref(TickitPen *pen)
{
  pen->refcount++;
  return pen;
}

void tickit_pen_unref(TickitPen *pen)
{
  if(pen->refcount < 1) {
    fprintf(stderr, "tickit_pen_unref: invalid refcount %d\n", pen->refcount);
    abort();
  }
  pen->refcount--;
  if(!pen->refcount)
    destroy(pen);
}

bool tickit_pen_has_attr(const TickitPen *pen, TickitPenAttr attr)
{
  switch(attr) {
    case TICKIT_PEN_FG:      return pen->valid.fgindex;
    case TICKIT_PEN_BG:      return pen->valid.bgindex;
    case TICKIT_PEN_BOLD:    return pen->valid.bold;
    case TICKIT_PEN_UNDER:   return pen->valid.under;
    case TICKIT_PEN_ITALIC:  return pen->valid.italic;
    case TICKIT_PEN_REVERSE: return pen->valid.reverse;
    case TICKIT_PEN_STRIKE:  return pen->valid.strike;
    case TICKIT_PEN_ALTFONT: return pen->valid.altfont;
    case TICKIT_PEN_BLINK:   return pen->valid.blink;
    case TICKIT_PEN_SIZEPOS: return pen->valid.sizepos;

    case TICKIT_N_PEN_ATTRS:
      return false;
  }

  return false;
}

bool tickit_pen_nondefault_attr(const TickitPen *pen, TickitPenAttr attr)
{
  if(!tickit_pen_has_attr(pen, attr))
    return false;

  switch(tickit_penattr_type(attr)) {
  case TICKIT_PENTYPE_BOOL:
    if(tickit_pen_get_bool_attr(pen, attr))
      return true;
    break;
  case TICKIT_PENTYPE_INT:
    if(tickit_pen_get_int_attr(pen, attr) > 0)
      return true;
    break;
  case TICKIT_PENTYPE_COLOUR:
    if(tickit_pen_get_colour_attr(pen, attr) != COLOUR_DEFAULT)
      return true;
    break;
  }

  return false;
}

bool tickit_pen_is_nonempty(const TickitPen *pen)
{
  for(TickitPenAttr attr = 1; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(tickit_pen_has_attr(pen, attr))
      return true;
  }
  return false;
}

bool tickit_pen_is_nondefault(const TickitPen *pen)
{
  for(TickitPenAttr attr = 1; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(tickit_pen_nondefault_attr(pen, attr))
      return true;
  }
  return false;
}

bool tickit_pen_get_bool_attr(const TickitPen *pen, TickitPenAttr attr)
{
  if(!tickit_pen_has_attr(pen, attr))
    return false;

  switch(attr) {
    case TICKIT_PEN_BOLD:    return pen->bold;
    case TICKIT_PEN_ITALIC:  return pen->italic;
    case TICKIT_PEN_REVERSE: return pen->reverse;
    case TICKIT_PEN_STRIKE:  return pen->strike;
    case TICKIT_PEN_BLINK:   return pen->blink;

    /* back-compat */
    case TICKIT_PEN_UNDER:
      return pen->under > 0;
    default:
      return false;
  }
}

void tickit_pen_set_bool_attr(TickitPen *pen, TickitPenAttr attr, bool val)
{
  switch(attr) {
    case TICKIT_PEN_BOLD:    pen->bold    = !!val; pen->valid.bold    = 1; break;
    case TICKIT_PEN_ITALIC:  pen->italic  = !!val; pen->valid.italic  = 1; break;
    case TICKIT_PEN_REVERSE: pen->reverse = !!val; pen->valid.reverse = 1; break;
    case TICKIT_PEN_STRIKE:  pen->strike  = !!val; pen->valid.strike  = 1; break;
    case TICKIT_PEN_BLINK:   pen->blink   = !!val; pen->valid.blink   = 1; break;

    /* back-compat */
    case TICKIT_PEN_UNDER:
      pen->under       = val ? TICKIT_PEN_UNDER_SINGLE : TICKIT_PEN_UNDER_NONE;
      pen->valid.under = 1;
      break;
    default:
      return;
  }
  changed(pen);
}

int tickit_pen_get_int_attr(const TickitPen *pen, TickitPenAttr attr)
{
  if(!tickit_pen_has_attr(pen, attr))
    return 0;

  switch(attr) {
    case TICKIT_PEN_UNDER:   return pen->under;
    case TICKIT_PEN_ALTFONT: return pen->altfont;
    case TICKIT_PEN_SIZEPOS: return pen->sizepos;
    default:
      return 0;
  }
}

void tickit_pen_set_int_attr(TickitPen *pen, TickitPenAttr attr, int val)
{
  switch(attr) {
    case TICKIT_PEN_UNDER:   pen->under   = val; pen->valid.under   = 1; break;
    case TICKIT_PEN_ALTFONT: pen->altfont = val; pen->valid.altfont = 1; break;
    case TICKIT_PEN_SIZEPOS: pen->sizepos = val; pen->valid.sizepos = 1; break;
    default:
      return;
  }
  changed(pen);
}

int tickit_pen_get_colour_attr(const TickitPen *pen, TickitPenAttr attr)
{
  if(!tickit_pen_has_attr(pen, attr))
    return COLOUR_DEFAULT;

  switch(attr) {
    case TICKIT_PEN_FG: return pen->fgindex;
    case TICKIT_PEN_BG: return pen->bgindex;
    default:
      return 0;
  }
}

void tickit_pen_set_colour_attr(TickitPen *pen, TickitPenAttr attr, int val)
{
  switch(attr) {
    case TICKIT_PEN_FG:
      pen->fgindex = val; pen->valid.fgindex = 1;
      pen->valid.fg_rgb8 = 0;
      break;
    case TICKIT_PEN_BG:
      pen->bgindex = val; pen->valid.bgindex = 1;
      pen->valid.bg_rgb8 = 0;
      break;
    default:
      return;
  }
  run_events(pen, TICKIT_PEN_ON_CHANGE, NULL);
}

bool tickit_pen_has_colour_attr_rgb8(const TickitPen *pen, TickitPenAttr attr)
{
  switch(attr) {
    case TICKIT_PEN_FG: return pen->valid.fgindex && pen->valid.fg_rgb8;
    case TICKIT_PEN_BG: return pen->valid.bgindex && pen->valid.bg_rgb8;
    default:
      return 0;
  }
}

TickitPenRGB8 tickit_pen_get_colour_attr_rgb8(const TickitPen *pen, TickitPenAttr attr)
{
  if(tickit_pen_has_colour_attr_rgb8(pen, attr))
    switch(attr) {
      case TICKIT_PEN_FG: return pen->fg_rgb8;
      case TICKIT_PEN_BG: return pen->bg_rgb8;
      default:
        break;
    }

  return (TickitPenRGB8){0, 0, 0};
}

void tickit_pen_set_colour_attr_rgb8(TickitPen *pen, TickitPenAttr attr, TickitPenRGB8 val)
{
  /* can only set an RGB8 version if the regular index version is already set */
  if(!tickit_pen_has_attr(pen, attr))
    return;

  switch(attr) {
    case TICKIT_PEN_FG: pen->fg_rgb8 = val; pen->valid.fg_rgb8 = 1; break;
    case TICKIT_PEN_BG: pen->bg_rgb8 = val; pen->valid.bg_rgb8 = 1; break;
    default:
      return;
  }
  changed(pen);
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

bool tickit_pen_set_colour_attr_desc(TickitPen *pen, TickitPenAttr attr, const char *desc)
{
  int hi = 0;
  int val;
  if(strncmp(desc, "hi-", 3) == 0) {
    desc += 3;
    hi   = 8;
  }

  const char *hashp = strchr(desc, '#');
  size_t len;
  if(hashp) {
    len = hashp - desc;
    // Trim spaces
    while(len > 0 && desc[len-1] == ' ')
      len--;
  }
  else
    len = strlen(desc);

  if(sscanf(desc, "%d", &val) == 1) {
    if(hi && val > 7)
      return false;

    freeze(pen);
    tickit_pen_set_colour_attr(pen, attr, val + hi);
    goto parse_rgb8;
  }

  for(int i = 0; i < sizeof(colournames)/sizeof(colournames[0]); i++) {
    if(strncmp(desc, colournames[i].name, len) != 0)
      continue;

    val = colournames[i].colour;
    if(val < 8 && hi)
      val += hi;

    freeze(pen);
    tickit_pen_set_colour_attr(pen, attr, val);
    goto parse_rgb8;
  }

  return false;

parse_rgb8:
  if (hashp) {
    TickitPenRGB8 rgb;
    if(sscanf(hashp+1, "%2hhx%2hhx%2hhx", &rgb.r, &rgb.g, &rgb.b) == 3)
      tickit_pen_set_colour_attr_rgb8(pen, attr, rgb);
  }

  thaw(pen);
  return true;
}

void tickit_pen_clear_attr(TickitPen *pen, TickitPenAttr attr)
{
  switch(attr) {
    case TICKIT_PEN_FG:      pen->valid.fgindex = 0; break;
    case TICKIT_PEN_BG:      pen->valid.bgindex = 0; break;
    case TICKIT_PEN_BOLD:    pen->valid.bold    = 0; break;
    case TICKIT_PEN_UNDER:   pen->valid.under   = 0; break;
    case TICKIT_PEN_ITALIC:  pen->valid.italic  = 0; break;
    case TICKIT_PEN_REVERSE: pen->valid.reverse = 0; break;
    case TICKIT_PEN_STRIKE:  pen->valid.strike  = 0; break;
    case TICKIT_PEN_ALTFONT: pen->valid.altfont = 0; break;
    case TICKIT_PEN_BLINK:   pen->valid.blink   = 0; break;
    case TICKIT_PEN_SIZEPOS: pen->valid.sizepos = 0; break;

    case TICKIT_N_PEN_ATTRS:
      return;
  }
  changed(pen);
}

void tickit_pen_clear(TickitPen *pen)
{
  for(TickitPenAttr attr = 1; attr < TICKIT_N_PEN_ATTRS; attr++)
    tickit_pen_clear_attr(pen, attr);
}

bool tickit_pen_equiv_attr(const TickitPen *a, const TickitPen *b, TickitPenAttr attr)
{
  switch(tickit_penattr_type(attr)) {
  case TICKIT_PENTYPE_BOOL:
    return tickit_pen_get_bool_attr(a, attr) == tickit_pen_get_bool_attr(b, attr);
  case TICKIT_PENTYPE_INT:
    return tickit_pen_get_int_attr(a, attr) == tickit_pen_get_int_attr(b, attr);
  case TICKIT_PENTYPE_COLOUR:
    if(tickit_pen_get_colour_attr(a, attr) != tickit_pen_get_colour_attr(b, attr))
      return false;
    /* indexes are equal; now compare RGB8s */
    if(!tickit_pen_has_colour_attr_rgb8(a, attr) && !tickit_pen_has_colour_attr_rgb8(b, attr))
      return true;
    if(!tickit_pen_has_colour_attr_rgb8(a, attr) || !tickit_pen_has_colour_attr_rgb8(b, attr))
      return false;
    TickitPenRGB8 acol = tickit_pen_get_colour_attr_rgb8(a, attr),
                  bcol = tickit_pen_get_colour_attr_rgb8(b, attr);
    return (acol.r == bcol.r) && (acol.g == bcol.g) && (acol.b == bcol.b);
  }

  return false;
}

bool tickit_pen_equiv(const TickitPen *a, const TickitPen *b)
{
  if(a == b)
    return true;

  for(TickitPenAttr attr = 1; attr < TICKIT_N_PEN_ATTRS; attr++)
    if(!tickit_pen_equiv_attr(a, b, attr))
      return false;

  return true;
}

void tickit_pen_copy_attr(TickitPen *dst, const TickitPen *src, TickitPenAttr attr)
{
  switch(tickit_penattr_type(attr)) {
  case TICKIT_PENTYPE_BOOL:
    tickit_pen_set_bool_attr(dst, attr, tickit_pen_get_bool_attr(src, attr));
    return;
  case TICKIT_PENTYPE_INT:
    tickit_pen_set_int_attr(dst, attr, tickit_pen_get_int_attr(src, attr));
    return;
  case TICKIT_PENTYPE_COLOUR:
    freeze(dst);
    tickit_pen_set_colour_attr(dst, attr, tickit_pen_get_colour_attr(src, attr));
    if(tickit_pen_has_colour_attr_rgb8(src, attr))
      tickit_pen_set_colour_attr_rgb8(dst, attr, tickit_pen_get_colour_attr_rgb8(src, attr));
    thaw(dst);
    return;
  }

  return;
}

void tickit_pen_copy(TickitPen *dst, const TickitPen *src, bool overwrite)
{
  freeze(dst);

  for(TickitPenAttr attr = 1; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(!tickit_pen_has_attr(src, attr))
      continue;
    if(tickit_pen_has_attr(dst, attr) &&
       (!overwrite || tickit_pen_equiv_attr(src, dst, attr)))
      continue;

    tickit_pen_copy_attr(dst, src, attr);
  }

  thaw(dst);
}

TickitPenAttrType tickit_penattr_type(TickitPenAttr attr)
{
  switch(attr) {
    case TICKIT_PEN_FG:
    case TICKIT_PEN_BG:
      return TICKIT_PENTYPE_COLOUR;

    case TICKIT_PEN_ALTFONT:
    case TICKIT_PEN_UNDER:
    case TICKIT_PEN_SIZEPOS:
      return TICKIT_PENTYPE_INT;

    case TICKIT_PEN_BOLD:
    case TICKIT_PEN_ITALIC:
    case TICKIT_PEN_REVERSE:
    case TICKIT_PEN_STRIKE:
    case TICKIT_PEN_BLINK:
      return TICKIT_PENTYPE_BOOL;

    case TICKIT_N_PEN_ATTRS:
      return -1;
  }

  return -1;
}

const char *tickit_penattr_name(TickitPenAttr attr)
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
    case TICKIT_PEN_BLINK:   return "blink";
    case TICKIT_PEN_SIZEPOS: return "sizepos";

    case TICKIT_N_PEN_ATTRS: ;
  }
  return NULL;
}

TickitPenAttr tickit_penattr_lookup(const char *name)
{
  switch(name[0]) {
    case 'a':
      return streq(name+1,"f") ? TICKIT_PEN_ALTFONT
                               : -1;
    case 'b':
      return name[1] == 0      ? TICKIT_PEN_BOLD
           : streq(name+1,"g") ? TICKIT_PEN_BG
           : streq(name+1,"link") ? TICKIT_PEN_BLINK
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
           : streq(name+1,"izepos") ? TICKIT_PEN_SIZEPOS
                               : -1;
    case 'u':
      return name[1] == 0      ? TICKIT_PEN_UNDER
                               : -1;
  }
  return -1;
}

const char *tickit_pen_attrname(TickitPenAttr attr) { return tickit_penattr_name(attr); }
TickitPenAttr tickit_pen_lookup_attr(const char *name) { return tickit_penattr_lookup(name); }
TickitPenAttrType tickit_pen_attrtype(TickitPenAttr attr) { return tickit_penattr_type(attr); }
