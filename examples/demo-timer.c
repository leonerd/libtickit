#include "tickit.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int on_expose(TickitWindow *win, TickitEventType ev, void *_info, void *user)
{
  int *counterp = user;
  TickitExposeEventInfo *info = _info;
  TickitRenderBuffer *rb = info->rb;

  tickit_renderbuffer_eraserect(rb, &info->rect);

  tickit_renderbuffer_goto(rb, 5, 5);
  tickit_renderbuffer_textf(rb, "Counter %d", *counterp);

  return 1;
}

static void on_timer(Tickit *t, void *user);
static void on_timer(Tickit *t, void *user)
{
  int *counterp = user;

  (*counterp)++;
  tickit_window_expose(tickit_get_rootwin(t), NULL);

  tickit_timer_after_msec(t, 1000, &on_timer, user);
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new();

  TickitWindow *root = tickit_get_rootwin(t);
  if(!root) {
    fprintf(stderr, "Cannot create TickitTerm - %s\n", strerror(errno));
    return 1;
  }

  int counter = 0;

  tickit_window_bind_event(root, TICKIT_EV_EXPOSE, 0, &on_expose, &counter);

  tickit_timer_after_msec(t, 1000, &on_timer, &counter);

  tickit_run(t);

  tickit_window_close(root);

  tickit_unref(t);

  return 0;
}
