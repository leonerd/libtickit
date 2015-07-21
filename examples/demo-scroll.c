#include "tickit.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int still_running = 1;
int term_lines, term_cols;

static void sigint(int sig)
{
  still_running = 0;
}

static void scroll(TickitTerm *tt, int downward, int rightward)
{
  tickit_term_scrollrect(tt, (TickitRect){ 2, 2, term_lines - 4, term_cols - 4 }, downward, rightward);
}

static void key_event(TickitTerm *tt, const char *key)
{
  if(strcmp(key, "Up") == 0)
    scroll(tt, -1, 0);
  else if(strcmp(key, "Down") == 0)
    scroll(tt, +1, 0);
  else if(strcmp(key, "Left") == 0)
    scroll(tt, 0, -1);
  else if(strcmp(key, "Right") == 0)
    scroll(tt, 0, +1);
}

static void mouse_event(TickitTerm *tt, int line, int col)
{
  if(line < 1) {
    scroll(tt, -1, 0);
    return;
  }

  if(line > term_lines - 2) {
    scroll(tt, +1, 0);
    return;
  }

  if(col < 1) {
    scroll(tt, 0, -1);
    return;
  }

  if(col > term_cols - 2) {
    scroll(tt, 0, +1);
    return;
  }
}

static void resize_event(TickitTerm *tt, int lines, int cols)
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

static int event(TickitTerm *tt, TickitEventType ev, void *_info, void *data)
{
  if(ev & TICKIT_EV_MOUSE) {
    TickitMouseEventInfo *info = _info;
    if(info->button == 1 && info->type == TICKIT_MOUSEEV_PRESS)
      mouse_event(tt, info->line, info->col);
  }

  if(ev & TICKIT_EV_KEY) {
    TickitKeyEventInfo *info = _info;
    if(info->type == TICKIT_KEYEV_KEY)
      key_event(tt, info->str);
  }

  if(ev & TICKIT_EV_RESIZE) {
    TickitResizeEventInfo *info = _info;
    resize_event(tt, info->lines, info->cols);
  }

  return 1;
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
  tickit_term_await_started_msec(tt, 50);

  tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, 0);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_MOUSE, TICKIT_TERM_MOUSEMODE_CLICK);
  tickit_term_clear(tt);

  tickit_term_bind_event(tt, TICKIT_EV_KEY|TICKIT_EV_MOUSE|TICKIT_EV_RESIZE, event, NULL);

  signal(SIGINT, sigint);

  int lines, cols;
  tickit_term_get_size(tt, &lines, &cols);
  resize_event(tt, lines, cols);

  for(int line = 2; line < lines - 2; line++) {
    char buf[64];
    sprintf(buf, "content on line %d here", line);

    tickit_term_goto(tt, line, 5);
    tickit_term_print(tt, buf);
  }

  while(still_running)
    tickit_term_input_wait_msec(tt, -1);

  tickit_term_destroy(tt);

  return 0;
}
