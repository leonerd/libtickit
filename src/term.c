#include "tickit.h"

/* We need C99 vsnprintf() semantics */
#define _ISOC99_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_UNIBILIUM
# include "unibilium.h"
#endif

struct TickitTerm {
  int                   outfd;
  TickitTermOutputFunc *outfunc;
  void                 *outfunc_user;

  struct {
    unsigned int bce:1;
  } cap;

  struct {
    unsigned int altscreen:1;
    unsigned int cursorvis:1;
    unsigned int mouse:1;
  } mode;

  int lines;
  int cols;

  TickitPen *pen;
};

TickitTerm *tickit_term_new(void)
{
  const char *termtype = getenv("TERM");
  if(!termtype)
    termtype = "xterm";

  return tickit_term_new_for_termtype(termtype);
}

TickitTerm *tickit_term_new_for_termtype(const char *termtype)
{
  TickitTerm *tt = malloc(sizeof(TickitTerm));
  if(!tt)
    return NULL;

  tt->outfd   = -1;
  tt->outfunc = NULL;

  tt->mode.altscreen = 0;
  tt->mode.cursorvis = 1;
  tt->mode.mouse     = 0;

#ifdef HAVE_UNIBILIUM
  {
    unibi_term *ut = unibi_from_term(termtype);
    if(!ut) {
      tickit_term_free(tt);
      return NULL;
    }

    tt->cap.bce = unibi_get_bool(ut, unibi_back_color_erase);

    unibi_destroy(ut);
  }
#else
# error "TODO: implement non-unibilium terminfo lookup"
#endif

  tt->lines = 0;
  tt->cols  = 0;

  /* Initially empty because we don't necessarily know the initial state
   * of the terminal
   */
  tt->pen = tickit_pen_new();

  return tt;
}

void tickit_term_free(TickitTerm *tt)
{
  tickit_pen_destroy(tt->pen);

  free(tt);
}

void tickit_term_destroy(TickitTerm *tt)
{
  if(tt->mode.mouse)
    tickit_term_set_mode_mouse(tt, 0);
  if(!tt->mode.cursorvis)
    tickit_term_set_mode_cursorvis(tt, 1);
  if(tt->mode.altscreen)
    tickit_term_set_mode_altscreen(tt, 0);

  tickit_term_free(tt);
}

void tickit_term_get_size(TickitTerm *tt, int *lines, int *cols)
{
  if(lines)
    *lines = tt->lines;
  if(cols)
    *cols  = tt->cols;
}

void tickit_term_set_size(TickitTerm *tt, int lines, int cols)
{
  tt->lines = lines;
  tt->cols  = cols;
}

void tickit_term_set_output_fd(TickitTerm *tt, int fd)
{
  tt->outfd = fd;

  tt->outfunc = NULL;
}

void tickit_term_set_output_func(TickitTerm *tt, TickitTermOutputFunc *fn, void *user)
{
  tt->outfunc      = fn;
  tt->outfunc_user = user;

  tt->outfd = -1;
}

static void write_str(TickitTerm *tt, const char *str, size_t len)
{
  if(len == 0)
    len = strlen(str);

  if(tt->outfunc) {
    (*tt->outfunc)(tt, str, len, tt->outfunc_user);
  }
  else if(tt->outfd != -1) {
    write(tt->outfd, str, len);
  }
}

static void write_vstrf(TickitTerm *tt, const char *fmt, va_list args)
{
  /* It's likely the output will fit in, say, 64 bytes */
  char buffer[64];
  size_t len = vsnprintf(buffer, sizeof buffer, fmt, args);

  if(len < 64) {
    write_str(tt, buffer, len);
    return;
  }

  char *morebuffer = malloc(len + 1);
  vsnprintf(morebuffer, len + 1, fmt, args);

  write_str(tt, morebuffer, len);

  free(morebuffer);
}

static void write_strf(TickitTerm *tt, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  write_vstrf(tt, fmt, args);
  va_end(args);
}

void tickit_term_print(TickitTerm *tt, const char *str)
{
  write_str(tt, str, strlen(str));
}

void tickit_term_goto(TickitTerm *tt, int line, int col)
{
  if(line == -1)
    write_strf(tt, "\e[%dG", col+1);
  else if(col == -1)
    write_strf(tt, "\e[%dH", line+1);
  else
    write_strf(tt, "\e[%d;%dH", line+1, col+1);
}

void tickit_term_move(TickitTerm *tt, int downward, int rightward)
{
  if(downward > 1)
    write_strf(tt, "\e[%dB", downward);
  else if(downward == 1)
    write_str(tt, "\e[B", 3);
  else if(downward == -1)
    write_str(tt, "\e[A", 3);
  else if(downward < -1)
    write_strf(tt, "\e[%dA", -downward);

  if(rightward > 1)
    write_strf(tt, "\e[%dD", rightward);
  else if(rightward == 1)
    write_str(tt, "\e[D", 3);
  else if(rightward == -1)
    write_str(tt, "\e[C", 3);
  else if(rightward < -1)
    write_strf(tt, "\e[%dC", -rightward);
}

struct SgrOnOff { int on, off; } sgr_onoff[] = {
  { 30, 39 }, /* fg */
  { 40, 49 }, /* bg */
  {  1, 22 }, /* bold */
  {  4, 24 }, /* under */
  {  3, 23 }, /* italic */
  {  7, 27 }, /* reverse */
  {  9, 29 }, /* strike */
  { 10, 10 }, /* altfont */
};

static void do_pen(TickitTerm *tt, TickitPen *pen, int ignoremissing)
{
  /* There can be at most 12 SGR parameters; 3 from each of 2 colours, and
   * 6 single attributes
   */
  int params[12];
  int pindex = 0;

  for(TickitPenAttr attr = 0; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(!tickit_pen_has_attr(pen, attr) && ignoremissing)
      continue;

    if(tickit_pen_has_attr(tt->pen, attr) && tickit_pen_equiv_attr(tt->pen, pen, attr))
      continue;

    tickit_pen_copy_attr(tt->pen, pen, attr);

    struct SgrOnOff *onoff = &sgr_onoff[attr];

    int val;

    switch(attr) {
    case TICKIT_PEN_FG:
    case TICKIT_PEN_BG:
      val = tickit_pen_get_int_attr(pen, attr);
      if(val < 0)
        params[pindex++] = onoff->off;
      else if(val < 8)
        params[pindex++] = onoff->on + val;
      else if(val < 16)
        params[pindex++] = onoff->on+60 + val-8;
      else {
        params[pindex++] = (onoff->on+8) | 0x80000000;
        params[pindex++] = 5 | 0x80000000;
        params[pindex++] = val;
      }
      break;

    case TICKIT_PEN_ALTFONT:
      val = tickit_pen_get_int_attr(pen, attr);
      if(val < 0 || val >= 10)
        params[pindex++] = onoff->off;
      else
        params[pindex++] = onoff->on + val;
      break;

    case TICKIT_PEN_BOLD:
    case TICKIT_PEN_UNDER:
    case TICKIT_PEN_ITALIC:
    case TICKIT_PEN_REVERSE:
    case TICKIT_PEN_STRIKE:
      val = tickit_pen_get_bool_attr(pen, attr);
      params[pindex++] = val ? onoff->on : onoff->off;
      break;

    case TICKIT_N_PEN_ATTRS:
      break;
    }
  }

  if(pindex == 0)
    return;

  /* If we're going to clear all the attributes then empty SGR is neater */
  if(!tickit_pen_is_nondefault(tt->pen))
    pindex = 0;

  /* Render params[] into a CSI string */

  size_t len = 3; /* ESC [ ... m */
  for(int i = 0; i < pindex; i++)
    len += snprintf(NULL, 0, "%d", params[i]&0x7fffffff) + 1;
  if(pindex > 0)
    len--; /* Last one has no final separator */

  char *buffer = malloc(len + 1);
  char *s = buffer;

  s += sprintf(s, "\e[");
  for(int i = 0; i < pindex-1; i++)
    s += sprintf(s, "%d%c", params[i]&0x7fffffff, params[i]&0x80000000 ? ':' : ';');
  if(pindex > 0)
    s += sprintf(s, "%d", params[pindex-1]&0x7fffffff);
  sprintf(s, "m");

  write_str(tt, buffer, len);

  free(buffer);
}

void tickit_term_chpen(TickitTerm *tt, TickitPen *pen)
{
  do_pen(tt, pen, 1);
}

void tickit_term_setpen(TickitTerm *tt, TickitPen *pen)
{
  do_pen(tt, pen, 0);
}

void tickit_term_clear(TickitTerm *tt)
{
  write_strf(tt, "\e[2J", 4);
}

void tickit_term_insertch(TickitTerm *tt, int count)
{
  if(count == 1)
    write_str(tt, "\e[@", 3);
  else if(count > 1)
    write_strf(tt, "\e[%d@", count);
}

void tickit_term_erasech(TickitTerm *tt, int count, int moveend)
{
  if(count < 1)
    return;

  if(tt->cap.bce) {
    if(count == 1)
      write_str(tt, "\e[X", 3);
    else
      write_strf(tt, "\e[%dX", count);

    if(moveend == 1)
      tickit_term_move(tt, 0, count);
  }
  else {
    if(count <= 5)
      write_str(tt, "     ", count);
    else
      write_strf(tt, " \e[%db", count - 1);

    if(moveend == 0)
      tickit_term_move(tt, 0, -count);
  }
}

void tickit_term_deletech(TickitTerm *tt, int count)
{
  if(count == 1)
    write_str(tt, "\e[P", 3);
  else if(count > 1)
    write_strf(tt, "\e[%dP", count);
}

void tickit_term_set_mode_altscreen(TickitTerm *tt, int on)
{
  if(!tt->mode.altscreen == !on)
    return;

  write_str(tt, on ? "\e[?1049h" : "\e[?1049l", 0);
  tt->mode.altscreen = !!on;
}

void tickit_term_set_mode_cursorvis(TickitTerm *tt, int on)
{
  if(!tt->mode.cursorvis == !on)
    return;

  write_str(tt, on ? "\e[?25h" : "\e[?25l", 0);
  tt->mode.cursorvis = !!on;
}

void tickit_term_set_mode_mouse(TickitTerm *tt, int on)
{
  if(!tt->mode.mouse == !on)
    return;

  write_str(tt, on ? "\e[?1002h" : "\e[?1002l", 0);
  tt->mode.mouse = !!on;
}
