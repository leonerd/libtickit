#include "tickit.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int on_expose(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
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

  tickit_pen_unref(pen);

  return 1;
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_stdtty();
  if(!t) {
    fprintf(stderr, "Cannot create Tickit - %s\n", strerror(errno));
    return 1;
  }

  TickitWindow *root = tickit_get_rootwin(t);
  if(!root) {
    fprintf(stderr, "Cannot create root window - %s\n", strerror(errno));
    return 1;
  }

  tickit_term_setctl_str(tickit_get_term(t),
    TICKIT_TERMCTL_TITLE_TEXT, "XTerm256 colour demo");

  tickit_window_bind_event(root, TICKIT_WINDOW_ON_EXPOSE, 0, &on_expose, NULL);

  tickit_run(t);

  tickit_window_close(root);

  tickit_unref(t);

  return 0;
}
