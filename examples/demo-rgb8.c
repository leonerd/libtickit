#include "tickit.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define streq(a,b) (!strcmp(a,b))

#define COL_RED   1
#define COL_GREEN 2
#define COL_BLUE  4

static int on_geomchange(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  /* re-expose the entire window if it changes shape */
  tickit_window_expose(win, NULL);

  return 1;
}

static int on_expose(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;
  TickitRenderBuffer *rb = info->rb;

  int cols = tickit_window_cols(win);
  TickitPen *pen = tickit_pen_new();

  tickit_renderbuffer_eraserect(rb, &info->rect);

  /* Red */
  tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, COL_RED);
  tickit_renderbuffer_setpen(rb, pen);
  tickit_renderbuffer_text_at(rb, 1, 0, "Red:");

  tickit_renderbuffer_goto(rb, 2, 0);
  for(int x = 0; x < cols; x++) {
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, x > cols/2 ? COL_RED : 0);
    tickit_pen_set_colour_attr_rgb8(pen, TICKIT_PEN_BG,
        (TickitPenRGB8){ .r = 255 * x / (cols-1), .g = 0, .b = 0 });

    tickit_renderbuffer_setpen(rb, pen);
    tickit_renderbuffer_text(rb, " ");
  }
  tickit_pen_clear_attr(pen, TICKIT_PEN_BG);

  /* Green */
  tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, COL_GREEN);
  tickit_renderbuffer_setpen(rb, pen);
  tickit_renderbuffer_text_at(rb, 5, 0, "Green:");

  tickit_renderbuffer_goto(rb, 6, 0);
  for(int x = 0; x < cols; x++) {
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, x > cols/2 ? COL_GREEN : 0);
    tickit_pen_set_colour_attr_rgb8(pen, TICKIT_PEN_BG,
        (TickitPenRGB8){ .r = 0, .g = 255 * x / (cols-1), .b = 0 });

    tickit_renderbuffer_setpen(rb, pen);
    tickit_renderbuffer_text(rb, " ");
  }
  tickit_pen_clear_attr(pen, TICKIT_PEN_BG);

  /* Blue */
  tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, COL_BLUE);
  tickit_renderbuffer_setpen(rb, pen);
  tickit_renderbuffer_text_at(rb, 9, 0, "Blue:");

  tickit_renderbuffer_goto(rb, 10, 0);
  for(int x = 0; x < cols; x++) {
    tickit_pen_set_colour_attr(pen, TICKIT_PEN_BG, x > cols/2 ? COL_BLUE : 0);
    tickit_pen_set_colour_attr_rgb8(pen, TICKIT_PEN_BG,
        (TickitPenRGB8){ .r = 0, .g = 0, .b = 255 * x / (cols-1) });

    tickit_renderbuffer_setpen(rb, pen);
    tickit_renderbuffer_text(rb, " ");
  }
  tickit_pen_clear_attr(pen, TICKIT_PEN_BG);

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
    TICKIT_TERMCTL_TITLE_TEXT, "XTerm RGB8 colour demo");

  tickit_window_bind_event(root, TICKIT_WINDOW_ON_GEOMCHANGE, 0, &on_geomchange, NULL);
  tickit_window_bind_event(root, TICKIT_WINDOW_ON_EXPOSE, 0, &on_expose, NULL);

  if(argc > 1 && argv[1]) {
    TickitTerm *tt = tickit_get_term(t);
    tickit_term_await_started_msec(tt, 100);

    if(streq(argv[1], "force-on"))
      tickit_term_setctl_int(tt, tickit_termctl_lookup("xterm.cap_rgb8"), 1);
    else if(streq(argv[1], "force-off"))
      tickit_term_setctl_int(tt, tickit_termctl_lookup("xterm.cap_rgb8"), 0);
  }

  tickit_run(t);

  tickit_window_close(root);

  tickit_unref(t);

  return 0;
}
