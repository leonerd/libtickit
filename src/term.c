#include "tickit.h"

/* We need C99 vsnprintf() semantics */
#define _ISOC99_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct TickitTerm {
  int                   outfd;
  TickitTermOutputFunc *outfunc;
  void                 *outfunc_user;

  struct {
    unsigned int altscreen:1;
    unsigned int cursorvis:1;
    unsigned int mouse:1;
  } mode;
};

TickitTerm *tickit_term_new(void)
{
  TickitTerm *tt = malloc(sizeof(TickitTerm));

  tt->outfd   = -1;
  tt->outfunc = NULL;

  tt->mode.altscreen = 0;
  tt->mode.cursorvis = 1;
  tt->mode.mouse     = 0;

  return tt;
}

void tickit_term_free(TickitTerm *tt)
{
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
