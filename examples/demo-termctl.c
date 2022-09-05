#include "tickit.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static struct {
  int vis : 1;
  int blink : 1;
  unsigned int shape : 2;
} modes;

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

  tickit_term_flush(tt);
}

static int event(TickitTerm *tt, TickitEventFlags flags, void *_info, void *data)
{
  TickitMouseEventInfo *info = _info;

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

static int later(Tickit *t, TickitEventFlags flags, void *_info, void *data)
{
  TickitTerm *tt = tickit_get_term(t);

  render_modes(tt);

  tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, modes.vis);

  return 1;
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_stdtty();
  if(!t) {
    fprintf(stderr, "Cannot create Tickit - %s\n", strerror(errno));
    return 1;
  }

  TickitTerm *tt = tickit_get_term(t);
  if(!tt) {
    fprintf(stderr, "Cannot create TickitTerm - %s\n", strerror(errno));
    return 1;
  }

  tickit_term_setctl_int(tt, TICKIT_TERMCTL_MOUSE, TICKIT_TERM_MOUSEMODE_CLICK);

  tickit_term_bind_event(tt, TICKIT_TERM_ON_MOUSE, 0, event, NULL);

  tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS,   (modes.vis = 1));
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORBLINK, (modes.blink = 1));
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORSHAPE, (modes.shape = TICKIT_CURSORSHAPE_BLOCK));

  tickit_watch_later(t, 0, &later, NULL);

  tickit_run(t);

  tickit_unref(t);

  return 0;
}
