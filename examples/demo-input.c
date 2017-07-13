#define _XOPEN_SOURCE 500  /* strdup */

#include "tickit.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int still_running = 1;

TickitKeyEventInfo lastkey;
TickitWindow *keywin;

TickitMouseEventInfo lastmouse;
TickitWindow *mousewin;

static void sigint(int sig)
{
  still_running = 0;
}

static void render_modifier(TickitRenderBuffer *rb, int mod)
{
  if(!mod)
    return;

  int pipe = 0;

  tickit_renderbuffer_text(rb, "<");

  if(mod & TICKIT_MOD_SHIFT)
    tickit_renderbuffer_text(rb, pipe++ ? "|SHIFT" : "SHIFT");
  if(mod & TICKIT_MOD_ALT)
    tickit_renderbuffer_text(rb, pipe++ ? "|ALT" : "ALT");
  if(mod & TICKIT_MOD_CTRL)
    tickit_renderbuffer_text(rb, pipe++ ? "|CTRL" : "CTRL");

  tickit_renderbuffer_text(rb, ">");
}

static int render_key(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;
  TickitRenderBuffer *rb = info->rb;

  tickit_renderbuffer_eraserect(rb, &info->rect);

  tickit_renderbuffer_goto(rb, 0, 0);
  tickit_renderbuffer_text(rb, "Key:");

  tickit_renderbuffer_goto(rb, 2, 2);
  switch(lastkey.type) {
    case TICKIT_KEYEV_TEXT: tickit_renderbuffer_text(rb, "text "); break;
    case TICKIT_KEYEV_KEY:  tickit_renderbuffer_text(rb, "key  "); break;
    default: return 0;
  }
  tickit_renderbuffer_text(rb, lastkey.str);

  render_modifier(rb, lastkey.mod);

  return 1;
}

static int event_key(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitKeyEventInfo *info = _info;
  if(info->type == TICKIT_KEYEV_KEY && strcmp(info->str, "C-c") == 0) {
    still_running = 0;
    return 0;
  }

  if(lastkey.str)
    free((void *)lastkey.str);

  lastkey = *info;
  lastkey.str = strdup(info->str);

  tickit_window_expose(keywin, NULL);

  return 1;
}

static int render_mouse(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;
  TickitRenderBuffer *rb = info->rb;

  tickit_renderbuffer_eraserect(rb, &info->rect);

  tickit_renderbuffer_goto(rb, 0, 0);
  tickit_renderbuffer_text(rb, "Mouse:");

  tickit_renderbuffer_goto(rb, 2, 2);
  switch(lastmouse.type) {
    case TICKIT_MOUSEEV_PRESS:   tickit_renderbuffer_text(rb, "press   "); break;
    case TICKIT_MOUSEEV_DRAG:    tickit_renderbuffer_text(rb, "drag    "); break;
    case TICKIT_MOUSEEV_RELEASE: tickit_renderbuffer_text(rb, "release "); break;
    case TICKIT_MOUSEEV_WHEEL:   tickit_renderbuffer_text(rb, "wheel ");   break;
    default: return 0;
  }

  if(lastmouse.type == TICKIT_MOUSEEV_WHEEL) {
    tickit_renderbuffer_text(rb, lastmouse.button == TICKIT_MOUSEWHEEL_DOWN ? "down" : "up");
  }
  else {
    tickit_renderbuffer_textf(rb, "button %d", lastmouse.button);
  }

  tickit_renderbuffer_textf(rb, " at (%d,%d)", lastmouse.line, lastmouse.col);

  render_modifier(rb, lastmouse.mod);

  return 1;
}

static int event_mouse(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitMouseEventInfo *info = _info;

  lastmouse = *info;

  tickit_window_expose(mousewin, NULL);

  return 1;
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new();

  TickitWindow *root = tickit_get_rootwin(t);
  if(!root) {
    fprintf(stderr, "Cannot create TickitTerm - %s\n", strerror(errno));
    return 1;
  }

  keywin = tickit_window_new(root, (TickitRect){
      .top = 2, .left = 2, .lines = 3, .cols = tickit_window_cols(root) - 4
    }, 0);

  tickit_window_bind_event(keywin, TICKIT_EV_EXPOSE, 0, &render_key, NULL);
  tickit_window_bind_event(root, TICKIT_EV_KEY, 0, &event_key, NULL);

  mousewin = tickit_window_new(root, (TickitRect){
      .top = 8, .left = 2, .lines = 3, .cols = tickit_window_cols(root) - 4
    }, 0);

  tickit_window_bind_event(mousewin, TICKIT_EV_EXPOSE, 0, &render_mouse, NULL);
  tickit_window_bind_event(root, TICKIT_EV_MOUSE, 0, &event_mouse, NULL);

  tickit_window_take_focus(root);
  tickit_window_set_cursor_visible(root, false);

  signal(SIGINT, sigint);

  while(still_running) {
    tickit_tick(t);
  }

  tickit_window_close(root);

  tickit_unref(t);

  return 0;
}
