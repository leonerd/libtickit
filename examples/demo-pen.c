#include "tickit.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct {
  char *name;
  int  val;
} colours[] = {
  { "red   ", 1 },
  { "blue  ", 4 },
  { "green ", 2 },
  { "yellow", 3 },
};

struct {
  char *name;
  TickitPenAttr attr;
} attrs[] = {
  { "bold",          TICKIT_PEN_BOLD },
  { "underline",     TICKIT_PEN_UNDER },
  { "italic",        TICKIT_PEN_ITALIC },
  { "strikethrough", TICKIT_PEN_STRIKE },
  { "reverse video", TICKIT_PEN_REVERSE },
  { "blink",         TICKIT_PEN_BLINK },
};

int still_running = 1;

static void sigint(int sig)
{
  still_running = 0;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;
  TickitPen *default_pen, *pen;

  tt = tickit_term_open_stdio();
  if(!tt) {
    fprintf(stderr, "Cannot create TickitTerm - %s\n", strerror(errno));
    return 1;
  }
  tickit_term_await_started_msec(tt, 50);

  tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, 0);
  tickit_term_clear(tt);

  default_pen = tickit_pen_new();

  pen = tickit_pen_new();

  /* ANSI colours foreground */
  tickit_term_goto(tt, 0, 0);

  for(int i = 0; i < 4; i++) {
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, colours[i].val);
    tickit_term_setpen(tt, pen);
    tickit_term_printf(tt, "fg %s", colours[i].name);

    tickit_term_setpen(tt, default_pen);
    tickit_term_print(tt, "     ");
  }

  tickit_term_goto(tt, 2, 0);

  /* ANSI high-brightness colours foreground */
  for(int i = 0; i < 4; i++) {
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, colours[i].val+8);
    tickit_term_setpen(tt, pen);
    tickit_term_printf(tt, "fg hi-%s", colours[i].name);

    tickit_term_setpen(tt, default_pen);
    tickit_term_print(tt, "  ");
  }

  /* ANSI colours background */
  tickit_term_goto(tt, 4, 0);

  tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, 0);

  for(int i = 0; i < 4; i++) {
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, colours[i].val);
    tickit_term_setpen(tt, pen);
    tickit_term_printf(tt, "bg %s", colours[i].name);

    tickit_term_setpen(tt, default_pen);
    tickit_term_print(tt, "     ");
  }

  tickit_term_goto(tt, 6, 0);

  /* ANSI high-brightness colours background */
  for(int i = 0; i < 4; i++) {
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, colours[i].val+8);
    tickit_term_setpen(tt, pen);
    tickit_term_printf(tt, "bg hi-%s", colours[i].name);

    tickit_term_setpen(tt, default_pen);
    tickit_term_print(tt, "  ");
  }

  tickit_pen_clear_attr(pen, TICKIT_PEN_FG);
  tickit_pen_clear_attr(pen, TICKIT_PEN_BG);

  /* Some interesting rendering attributes */
  for(int i = 0; i < 6; i++) {
    tickit_term_goto(tt, 8 + 2*i, 0);

    tickit_pen_set_bool_attr(pen, attrs[i].attr, 1);
    tickit_term_setpen(tt, pen);
    tickit_term_print(tt, attrs[i].name);

    tickit_pen_clear_attr(pen, attrs[i].attr);
  }

  tickit_term_goto(tt, 20, 0);

  tickit_pen_set_int_attr(pen, TICKIT_PEN_ALTFONT, 1);
  tickit_term_setpen(tt, pen);
  tickit_term_print(tt, "alternate font");

  signal(SIGINT, sigint);

  while(still_running)
    tickit_term_input_wait_msec(tt, -1);

  tickit_term_destroy(tt);

  return 0;
}
