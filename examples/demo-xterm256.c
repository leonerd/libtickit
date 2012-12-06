#include "tickit.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int still_running = 1;

static void sigint(int sig)
{
  still_running = 0;
}

/* TODO: Consider if this would be useful in Tickit itself */
void tickit_term_printf(TickitTerm *tt, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  size_t len = vsnprintf(NULL, 0, fmt, args);

  char *buffer = malloc(len + 1);

  va_start(args, fmt);
  vsnprintf(buffer, len + 1, fmt, args);

  tickit_term_print(tt, buffer);

  free(buffer);
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;
  TickitPen *default_pen, *pen;

  tt = tickit_term_new();
  if(!tt) {
    fprintf(stderr, "Cannot create TickitTerm - %s\n", strerror(errno));
    return 1;
  }

  tickit_term_set_input_fd(tt, STDIN_FILENO);
  tickit_term_set_output_fd(tt, STDOUT_FILENO);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, 0);
  tickit_term_clear(tt);

  default_pen = tickit_pen_new();

  pen = tickit_pen_new();

  tickit_term_goto(tt, 0, 0);
  tickit_term_setpen(tt, default_pen);
  tickit_term_print(tt, "ANSI");

  tickit_term_goto(tt, 2, 0);
  for(int i = 0; i < 16; i++) {
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, i);
    tickit_term_setpen(tt, pen);
    tickit_term_printf(tt, "[%02d]", i);
  }

  tickit_term_goto(tt, 4, 0);
  tickit_term_setpen(tt, default_pen);
  tickit_term_print(tt, "216 RGB cube");

  for(int y = 0; y < 6; y++) {
    tickit_term_goto(tt, 6+y, 0);
    for(int x = 0; x < 36; x++) {
      tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, y*36 + x + 16);
      tickit_term_setpen(tt, pen);
      tickit_term_print(tt, "  ");
    }
  }

  tickit_term_goto(tt, 13, 0);
  tickit_term_setpen(tt, default_pen);
  tickit_term_print(tt, "24 Greyscale ramp");

  tickit_term_goto(tt, 15, 0);
  for(int i = 0; i < 24; i++) {
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, 232 + i);
    if(i > 12)
      tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, 0);
    tickit_term_setpen(tt, pen);
    tickit_term_printf(tt, "g%02d", i);
  }

  signal(SIGINT, sigint);

  while(still_running)
    tickit_term_input_wait(tt);

  tickit_term_destroy(tt);

  return 0;
}
