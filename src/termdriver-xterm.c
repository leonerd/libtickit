#include "termdriver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print(TickitTermDriver *ttd, const char *str)
{
  tickit_termdrv_write_str(ttd, str, strlen(str));
}

static void goto_abs(TickitTermDriver *ttd, int line, int col)
{
  if(line != -1 && col > 0)
    tickit_termdrv_write_strf(ttd, "\e[%d;%dH", line+1, col+1);
  else if(line != -1 && col == 0)
    tickit_termdrv_write_strf(ttd, "\e[%dH", line+1);
  else if(line != -1)
    tickit_termdrv_write_strf(ttd, "\e[%dd", line+1);
  else if(col > 0)
    tickit_termdrv_write_strf(ttd, "\e[%dG", col+1);
  else if(col != -1)
    tickit_termdrv_write_str(ttd, "\e[G", 3);
}

static void move_rel(TickitTermDriver *ttd, int downward, int rightward)
{
  if(downward > 1)
    tickit_termdrv_write_strf(ttd, "\e[%dB", downward);
  else if(downward == 1)
    tickit_termdrv_write_str(ttd, "\e[B", 3);
  else if(downward == -1)
    tickit_termdrv_write_str(ttd, "\e[A", 3);
  else if(downward < -1)
    tickit_termdrv_write_strf(ttd, "\e[%dA", -downward);

  if(rightward > 1)
    tickit_termdrv_write_strf(ttd, "\e[%dC", rightward);
  else if(rightward == 1)
    tickit_termdrv_write_str(ttd, "\e[C", 3);
  else if(rightward == -1)
    tickit_termdrv_write_str(ttd, "\e[D", 3);
  else if(rightward < -1)
    tickit_termdrv_write_strf(ttd, "\e[%dD", -rightward);
}

static void insertch(TickitTermDriver *ttd, int count)
{
  if(count == 1)
    tickit_termdrv_write_str(ttd, "\e[@", 3);
  else if(count > 1)
    tickit_termdrv_write_strf(ttd, "\e[%d@", count);
}

static void deletech(TickitTermDriver *ttd, int count)
{
  if(count == 1)
    tickit_termdrv_write_str(ttd, "\e[P", 3);
  else if(count > 1)
    tickit_termdrv_write_strf(ttd, "\e[%dP", count);
}

static int scrollrect(TickitTermDriver *ttd, int top, int left, int lines, int cols, int downward, int rightward)
{
  if(!downward && !rightward)
    return 1;

  int term_cols;
  tickit_term_get_size(ttd->tt, NULL, &term_cols);

  if(left == 0 && cols == term_cols && rightward == 0) {
    tickit_termdrv_write_strf(ttd, "\e[%d;%dr", top + 1, top + lines);
    goto_abs(ttd, top, left);
    if(downward > 0) {
      if(downward > 1)
        tickit_termdrv_write_strf(ttd, "\e[%dM", downward); /* DL */
      else
        tickit_termdrv_write_str(ttd, "\e[M", 3);
    }
    else {
      if(downward < -1)
        tickit_termdrv_write_strf(ttd, "\e[%dL", -downward); /* IL */
      else
        tickit_termdrv_write_str(ttd, "\e[L", 3);
    }
    tickit_termdrv_write_str(ttd, "\e[r", 3);
    return 1;
  }

  if(left + cols == term_cols && downward == 0) {
    for(int line = top; line < top + lines; line++) {
      goto_abs(ttd, line, left);
      if(rightward > 0)
        insertch(ttd,  rightward);
      else
        deletech(ttd, -rightward);
    }
  }

  return 0;
}

static void erasech(TickitTermDriver *ttd, int count, int moveend)
{
  if(count < 1)
    return;

  /* Even if the terminal can do bce, only use ECH if we're not in
   * reverse-video mode. Most terminals don't do rv+ECH properly
   */
  if(ttd->cap.bce && !tickit_pen_get_bool_attr(tickit_termdrv_current_pen(ttd), TICKIT_PEN_REVERSE)) {
    if(count == 1)
      tickit_termdrv_write_str(ttd, "\e[X", 3);
    else
      tickit_termdrv_write_strf(ttd, "\e[%dX", count);

    if(moveend == 1)
      move_rel(ttd, 0, count);
  }
  else {
     /* TODO: benchmark it and find out a suitable number for 20
     */
    if(count > 20)
      tickit_termdrv_write_strf(ttd, "%*s", count, "");
    else 
      tickit_termdrv_write_str(ttd, "                    ", count);

    if(moveend == 0)
      move_rel(ttd, 0, -count);
  }
}

static void clear(TickitTermDriver *ttd)
{
  tickit_termdrv_write_strf(ttd, "\e[2J", 4);
}

static struct SgrOnOff { int on, off; } sgr_onoff[] = {
  { 30, 39 }, /* fg */
  { 40, 49 }, /* bg */
  {  1, 22 }, /* bold */
  {  4, 24 }, /* under */
  {  3, 23 }, /* italic */
  {  7, 27 }, /* reverse */
  {  9, 29 }, /* strike */
  { 10, 10 }, /* altfont */
};

static void chpen(TickitTermDriver *ttd, const TickitPen *delta, const TickitPen *final)
{
  /* There can be at most 12 SGR parameters; 3 from each of 2 colours, and
   * 6 single attributes
   */
  int params[12];
  int pindex = 0;

  for(TickitPenAttr attr = 0; attr < TICKIT_N_PEN_ATTRS; attr++) {
    if(!tickit_pen_has_attr(delta, attr))
      continue;

    struct SgrOnOff *onoff = &sgr_onoff[attr];

    int val;

    switch(attr) {
    case TICKIT_PEN_FG:
    case TICKIT_PEN_BG:
      val = tickit_pen_get_colour_attr(delta, attr);
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
      val = tickit_pen_get_int_attr(delta, attr);
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
      val = tickit_pen_get_bool_attr(delta, attr);
      params[pindex++] = val ? onoff->on : onoff->off;
      break;

    case TICKIT_N_PEN_ATTRS:
      break;
    }
  }

  if(pindex == 0)
    return;

  /* If we're going to clear all the attributes then empty SGR is neater */
  if(!tickit_pen_is_nondefault(final))
    pindex = 0;

  /* Render params[] into a CSI string */

  size_t len = 3; /* ESC [ ... m */
  for(int i = 0; i < pindex; i++)
    len += snprintf(NULL, 0, "%d", params[i]&0x7fffffff) + 1;
  if(pindex > 0)
    len--; /* Last one has no final separator */

  char *buffer = tickit_termdrv_get_tmpbuffer(ttd, len + 1);
  char *s = buffer;

  s += sprintf(s, "\e[");
  for(int i = 0; i < pindex-1; i++)
    /* TODO: Work out what terminals support :s */
    s += sprintf(s, "%d%c", params[i]&0x7fffffff, ';');
  if(pindex > 0)
    s += sprintf(s, "%d", params[pindex-1]&0x7fffffff);
  sprintf(s, "m");

  tickit_termdrv_write_str(ttd, buffer, len);
}

void set_mode(TickitTermDriver *ttd, TickitTermDriverMode mode, int value)
{
  switch(mode) {
    case TICKIT_TERMMODE_ALTSCREEN:
      if(!ttd->mode.altscreen == !value)
        return;

      tickit_termdrv_write_str(ttd, value ? "\e[?1049h" : "\e[?1049l", 0);
      ttd->mode.altscreen = !!value;
      break;
    case TICKIT_TERMMODE_CURSORVIS:
      if(!ttd->mode.cursorvis == !value)
        return;

      tickit_termdrv_write_str(ttd, value ? "\e[?25h" : "\e[?25l", 0);
      ttd->mode.cursorvis = !!value;
      break;
    case TICKIT_TERMMODE_MOUSE:
      if(!ttd->mode.mouse == !value)
        return;

      tickit_termdrv_write_str(ttd, value ? "\e[?1002h" : "\e[?1002l", 0);
      ttd->mode.mouse = !!value;
  }
}

static void destroy(TickitTermDriver *ttd)
{
  if(ttd->mode.mouse)
    set_mode(ttd, TICKIT_TERMMODE_MOUSE, 0);
  if(!ttd->mode.cursorvis)
    set_mode(ttd, TICKIT_TERMMODE_CURSORVIS, 1);
  if(ttd->mode.altscreen)
    set_mode(ttd, TICKIT_TERMMODE_ALTSCREEN, 0);

  free(ttd);
}

TickitTermDriverVTable xterm_vtable = {
  .destroy    = destroy,
  .print      = print,
  .goto_abs   = goto_abs,
  .move_rel   = move_rel,
  .scrollrect = scrollrect,
  .erasech    = erasech,
  .clear      = clear,
  .chpen      = chpen,
  .set_mode   = set_mode,
};
