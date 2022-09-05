#include "tickit.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int term_lines, term_cols;

static void scroll(TickitTerm *tt, int downward, int rightward)
{
  tickit_term_scrollrect(tt, (TickitRect){ 2, 2, term_lines - 4, term_cols - 4 }, downward, rightward);
}

static void resize(TickitTerm *tt, int lines, int cols)
{
  term_lines = lines;
  term_cols  = cols;

  tickit_term_goto(tt, 0, 2);
  tickit_term_print(tt, "Scroll up");
  tickit_term_goto(tt, 1, 2);
  tickit_term_print(tt, "-----------");

  tickit_term_goto(tt, lines - 2, 2);
  tickit_term_print(tt, "-----------");
  tickit_term_goto(tt, lines - 1, 2);
  tickit_term_print(tt, "Scroll down");

  for(int line = 0; line < 12; line++) {
    char buf[3];

    sprintf(buf, "%c-", "Scroll left "[line]);
    tickit_term_goto(tt, line + 2, 0);
    tickit_term_print(tt, buf);

    sprintf(buf, "-%c", "Scroll right"[line]);
    tickit_term_goto(tt, line + 2, cols - 2);
    tickit_term_print(tt, buf);
  }
}

static int key_event(TickitTerm *tt, TickitEventFlags flags, void *_info, void *data)
{
  TickitKeyEventInfo *info = _info;
  if(info->type != TICKIT_KEYEV_KEY)
    return 1;

  const char *key = info->str;

  if(strcmp(key, "Up") == 0)
    scroll(tt, -1, 0);
  else if(strcmp(key, "Down") == 0)
    scroll(tt, +1, 0);
  else if(strcmp(key, "Left") == 0)
    scroll(tt, 0, -1);
  else if(strcmp(key, "Right") == 0)
    scroll(tt, 0, +1);
  else
    return 1;

  tickit_term_flush(tt);

  return 1;
}

static int mouse_event(TickitTerm *tt, TickitEventFlags flags, void *_info, void *data)
{
  TickitMouseEventInfo *info = _info;
  if(info->button != 1 || info->type != TICKIT_MOUSEEV_PRESS)
    return 1;

  int line = info->line;
  int col  = info->col;

  if(line < 1)
    scroll(tt, -1, 0);
  else if(line > term_lines - 2)
    scroll(tt, +1, 0);
  else if(col < 1)
    scroll(tt, 0, -1);
  else if(col > term_cols - 2)
    scroll(tt, 0, +1);
  else
    return 1;

  tickit_term_flush(tt);

  return 1;
}

static int resize_event(TickitTerm *tt, TickitEventFlags flags, void *_info, void *data)
{
  TickitResizeEventInfo *info = _info;
  resize(tt, info->lines, info->cols);

  return 1;
}

static int render(Tickit *t, TickitEventFlags flags, void *data, void *user)
{
  TickitTerm *tt = tickit_get_term(t);

  int lines, cols;
  tickit_term_get_size(tt, &lines, &cols);
  resize(tt, lines, cols);

  for(int line = 2; line < lines - 2; line++) {
    char buf[64];
    sprintf(buf, "content on line %d here", line);

    tickit_term_goto(tt, line, 5);
    tickit_term_print(tt, buf);
  }

  tickit_term_flush(tt);

  return 0;
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

  tickit_term_bind_event(tt, TICKIT_TERM_ON_KEY, 0, &key_event, NULL);
  tickit_term_bind_event(tt, TICKIT_TERM_ON_MOUSE, 0, &mouse_event, NULL);
  tickit_term_bind_event(tt, TICKIT_TERM_ON_RESIZE, 0, &resize_event, NULL);

  tickit_watch_later(t, 0, &render, NULL);

  tickit_run(t);

  tickit_unref(t);

  return 0;
}
