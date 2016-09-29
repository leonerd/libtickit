#include "tickit.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int still_running = 1;

static void sigint(int sig)
{
  still_running = 0;
}

static int on_expose(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;
  TickitRenderBuffer *rb = info->rb;

  tickit_renderbuffer_eraserect(rb, &info->rect);

  tickit_renderbuffer_goto(rb, 5, 5);
  tickit_renderbuffer_textf(rb, "Counter %d", *((int *)data));

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
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, 0);
  tickit_term_clear(tt);

  int counter = 0;

  TickitWindow *root = tickit_window_new_root(tt);
  tickit_window_bind_event(root, TICKIT_EV_EXPOSE, 0, &on_expose, &counter);

  signal(SIGINT, sigint);

  while(still_running) {
    tickit_window_flush(root);
    tickit_term_input_wait_msec(tt, 1000);

    counter++;
    tickit_window_expose(root, NULL);
  }

  tickit_term_destroy(tt);

  return 0;
}
