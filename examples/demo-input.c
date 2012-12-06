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

static void render_key(TickitTerm *tt, TickitKeyEventType type, const char *str)
{
  tickit_term_goto(tt, 2, 2);
  tickit_term_print(tt, "Key:");

  tickit_term_goto(tt, 4, 4);
  switch(type) {
    case TICKIT_KEYEV_TEXT: tickit_term_print(tt, "text "); break;
    case TICKIT_KEYEV_KEY:  tickit_term_print(tt, "key  "); break;
    default: return;
  }
  tickit_term_print(tt, str);
  tickit_term_erasech(tt, 16, -1);
}

static void render_mouse(TickitTerm *tt, TickitMouseEventType type, int button, int line, int col)
{
  tickit_term_goto(tt, 8, 2);
  tickit_term_print(tt, "Mouse:");

  tickit_term_goto(tt, 10, 4);
  switch(type) {
    case TICKIT_MOUSEEV_PRESS:   tickit_term_print(tt, "press   "); break;
    case TICKIT_MOUSEEV_DRAG:    tickit_term_print(tt, "drag    "); break;
    case TICKIT_MOUSEEV_RELEASE: tickit_term_print(tt, "release "); break;
    case TICKIT_MOUSEEV_WHEEL:   tickit_term_print(tt, "wheel ");   break;
    default: return;
  }

  /* TODO: want a tickit_term_printf()
   */
  char buffer[64];
  if(type == TICKIT_MOUSEEV_WHEEL) {
    snprintf(buffer, sizeof buffer, "%s at (%d,%d)", button == TICKIT_MOUSEWHEEL_DOWN ? "down" : "up", line, col);
  }
  else {
    snprintf(buffer, sizeof buffer, "button %d at (%d,%d)", button, line, col);
  }
  tickit_term_print(tt, buffer);
  tickit_term_erasech(tt, 8, -1);
}

static void event(TickitTerm *tt, TickitEventType ev, TickitEvent *args, void *data)
{
  switch(ev) {
  case TICKIT_EV_RESIZE:
    break;

  case TICKIT_EV_KEY:
    if(args->type == TICKIT_KEYEV_KEY && strcmp(args->str, "C-c") == 0) {
      still_running = 0;
      return;
    }

    render_key(tt, args->type, args->str);
    break;

  case TICKIT_EV_MOUSE:
    render_mouse(tt, args->type, args->button, args->line, args->col);
    break;
  }
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
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_ALTSCREEN, 1);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_CURSORVIS, 0);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_MOUSE, 1);
  tickit_term_setctl_int(tt, TICKIT_TERMCTL_KEYPAD_APP, 1);
  tickit_term_clear(tt);

  tickit_term_bind_event(tt, TICKIT_EV_KEY|TICKIT_EV_MOUSE, event, NULL);

  render_key(tt, -1, "");
  render_mouse(tt, -1, 0, 0, 0);

  signal(SIGINT, sigint);

  while(still_running)
    tickit_term_input_wait(tt);

  tickit_term_destroy(tt);

  return 0;
}
