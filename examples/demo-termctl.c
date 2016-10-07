#include "tickit.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int still_running = 1;

static struct {
  int vis : 1;
  int blink : 1;
  unsigned int shape : 2;
} modes;

static void sigint(int sig)
{
  still_running = 0;
}

static void render_modes(TickitTerm *tt)
{
  tickit_term_goto(tt, 5, 3);
  tickit_term_print(tt, "Cursor visible:  ");
  tickit_term_print(tt, modes.vis   ? "| >On< |  Off  |" : "|  On  | >Off< |");

  tickit_term_goto(tt, 7, 3);
  tickit_term_print(tt, "Cursor blink:    ");
  tickit_term_print(tt, modes.blink ? "| >On< |  Off  |" : "|  On  | >Off< |");

  tickit_term_goto(tt, 9, 3);
  tickit_term_print(tt, "Cursor shape:    ");
  tickit_term_print(tt, modes.shape == TICKIT_CURSORSHAPE_BLOCK ? "| >Block< |  Under  |  Bar  |" :
                        modes.shape == TICKIT_CURSORSHAPE_UNDER ? "|  Block  | >Under< |  Bar  |" :
                                                                       "|  Block  |  Under  | >Bar< |");

  tickit_term_goto(tt, 20, 10);
  tickit_term_print(tt, "Cursor  >   <");
  tickit_term_goto(tt, 20, 20);
}

static int event(TickitTerm *tt, TickitEventType ev, void *_info, void *data)
{
  TickitMouseEventInfo *info = _info;

  fprintf(stderr, "mouse event %d type=%d at %d,%d\n", ev, info->type, info->col, info->line);

  if(info->type != TICKIT_MOUSEEV_PRESS || info->button != 1)
    return 0;

  if(info->line == 5) {
    if(info->col >= 21 && info->col <= 26)
      modes.vis = 1;
    else if(info->col >= 28 && info->col <= 34)
      modes.vis = 0;
    else
      return 0;

    tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, modes.vis);
  }

  if(info->line == 7) {
    if(info->col >= 21 && info->col <= 26)
      modes.blink = 1;
    else if(info->col >= 28 && info->col <= 34)
      modes.blink = 0;
    else
      return 0;

    tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORBLINK, modes.blink);
  }

  if(info->line == 9) {
    if(info->col >= 21 && info->col <= 29)
      modes.shape = TICKIT_CURSORSHAPE_BLOCK;
    else if(info->col >= 31 && info->col <= 39)
      modes.shape = TICKIT_CURSORSHAPE_UNDER;
    else if(info->col >= 40 && info->col <= 47)
      modes.shape = TICKIT_CURSORSHAPE_LEFT_BAR;
    else
      return 0;

    tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORSHAPE, modes.shape);
  }

  render_modes(tt);

  return 1;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;

  tt = tickit_term_open_stdio();
  if(!tt) {
    fprintf(stderr, "Cannot create TickitTerm - %s\n", strerror(errno));
    return 1;
  }
  tickit_term_await_started_msec(tt, 50);

  tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_MOUSE, TICKIT_TERM_MOUSEMODE_CLICK);
  tickit_term_clear(tt);

  tickit_term_bind_event(tt, TICKIT_EV_MOUSE, 0, event, NULL);

  modes.vis = 1;
  modes.blink = 1;
  modes.shape = TICKIT_CURSORSHAPE_BLOCK;

  render_modes(tt);

  signal(SIGINT, sigint);

  while(still_running)
    tickit_term_input_wait_msec(tt, -1);

  tickit_term_unref(tt);

  return 0;
}
