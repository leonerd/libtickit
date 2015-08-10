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

static int on_expose(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;
  TickitRenderBuffer *rb = info->rb;

  tickit_renderbuffer_eraserect(rb, &info->rect);

  TickitPen *pen = tickit_pen_new();

  tickit_renderbuffer_goto(rb, 0, 0);
  tickit_renderbuffer_text(rb, "ANSI");

  {
    tickit_renderbuffer_save(rb);

    tickit_renderbuffer_goto(rb, 2, 0);
    for(int i = 0; i < 16; i++) {
      tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, i);
      tickit_renderbuffer_setpen(rb, pen);
      tickit_renderbuffer_textf(rb, "[%02d]", i);
    }

    tickit_renderbuffer_restore(rb);
  }

  tickit_renderbuffer_goto(rb, 4, 0);
  tickit_renderbuffer_text(rb, "216 RGB cube");

  {
    tickit_renderbuffer_save(rb);

    for(int y = 0; y < 6; y++) {
      tickit_renderbuffer_goto(rb, 6+y, 0);
      for(int x = 0; x < 36; x++) {
        tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, y*36 + x + 16);
        tickit_renderbuffer_setpen(rb, pen);
        tickit_renderbuffer_text(rb, "  ");
      }
    }

    tickit_renderbuffer_restore(rb);
  }

  tickit_renderbuffer_goto(rb, 13, 0);
  tickit_renderbuffer_text(rb, "24 Greyscale ramp");

  {
    tickit_renderbuffer_save(rb);

    tickit_renderbuffer_goto(rb, 15, 0);
    for(int i = 0; i < 24; i++) {
      tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, 232 + i);
      if(i > 12)
        tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, 0);
      tickit_renderbuffer_setpen(rb, pen);
      tickit_renderbuffer_textf(rb, "g%02d", i);
    }

    tickit_renderbuffer_restore(rb);
  }

  tickit_pen_destroy(pen);

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
  tickit_term_setctl_str(tt, TICKIT_TERMCTL_TITLE_TEXT, "XTerm256 colour demo");
  tickit_term_clear(tt);

  TickitWindow *root = tickit_window_new_root(tt);
  tickit_window_bind_event(root, TICKIT_EV_GEOMCHANGE, &tickit_window_on_geomchange_expose, NULL);
  tickit_window_bind_event(root, TICKIT_EV_EXPOSE, &on_expose, NULL);

  // Initial expose
  tickit_window_expose(root, NULL);
  tickit_window_tick(root);

  signal(SIGINT, sigint);

  while(still_running) {
    tickit_term_input_wait_msec(tt, -1);
    tickit_window_tick(root);
  }

  tickit_term_destroy(tt);

  return 0;
}
