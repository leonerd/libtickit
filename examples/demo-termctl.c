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
  tickit_term_print(tt, modes.shape == TICKIT_TERM_CURSORSHAPE_BLOCK ? "| >Block< |  Under  |  Bar  |" :
                        modes.shape == TICKIT_TERM_CURSORSHAPE_UNDER ? "|  Block  | >Under< |  Bar  |" :
                                                                       "|  Block  |  Under  | >Bar< |");

  tickit_term_goto(tt, 20, 10);
  tickit_term_print(tt, "Cursor  >   <");
  tickit_term_goto(tt, 20, 20);
}

static void event(TickitTerm *tt, TickitEventType ev, TickitEvent *args, void *data)
{
  if(ev != TICKIT_EV_MOUSE)
    return;

  if(args->type != TICKIT_MOUSEEV_PRESS || args->button != 1)
    return;

  if(args->line == 5) {
    if(args->col >= 21 && args->col <= 26)
      modes.vis = 1;
    else if(args->col >= 28 && args->col <= 34)
      modes.vis = 0;
    else
      return;

    tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, modes.vis);
  }

  if(args->line == 7) {
    if(args->col >= 21 && args->col <= 26)
      modes.blink = 1;
    else if(args->col >= 28 && args->col <= 34)
      modes.blink = 0;
    else
      return;

    tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORBLINK, modes.blink);
  }

  if(args->line == 9) {
    if(args->col >= 21 && args->col <= 29)
      modes.shape = TICKIT_TERM_CURSORSHAPE_BLOCK;
    else if(args->col >= 31 && args->col <= 39)
      modes.shape = TICKIT_TERM_CURSORSHAPE_UNDER;
    else if(args->col >= 40 && args->col <= 47)
      modes.shape = TICKIT_TERM_CURSORSHAPE_LEFT_BAR;
    else
      return;

    tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORSHAPE, modes.shape);
  }

  render_modes(tt);
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;

  tt = tickit_term_new();
  if(!tt) {
    fprintf(stderr, "Cannot create TickitTerm - %s\n", strerror(errno));
    return 1;
  }

  tickit_term_set_input_fd(tt, STDIN_FILENO);
  tickit_term_set_output_fd(tt, STDOUT_FILENO);
  tickit_term_await_started(tt, &(const struct timeval){ .tv_sec = 0, .tv_usec = 50000 });

  tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_MOUSE, TICKIT_TERM_MOUSEMODE_CLICK);
  tickit_term_clear(tt);

  tickit_term_bind_event(tt, TICKIT_EV_MOUSE, event, NULL);

  modes.vis = 1;
  modes.blink = 1;
  modes.shape = TICKIT_TERM_CURSORSHAPE_BLOCK;

  render_modes(tt);

  signal(SIGINT, sigint);

  while(still_running)
    tickit_term_input_wait_msec(tt, -1);

  tickit_term_destroy(tt);

  return 0;
}
