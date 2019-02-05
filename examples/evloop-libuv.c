#define _XOPEN_SOURCE 500  /* strdup */

#include "tickit.h"
#include "tickit-evloop.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <uv.h>

TickitKeyEventInfo lastkey;
TickitWindow *keywin;

TickitMouseEventInfo lastmouse;
TickitWindow *mousewin;

int counter = 0;
TickitWindow *timerwin;

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

static int render_key(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
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

static int event_key(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  TickitKeyEventInfo *info = _info;

  if(lastkey.str)
    free((void *)lastkey.str);

  lastkey = *info;
  lastkey.str = strdup(info->str);

  tickit_window_expose(keywin, NULL);

  return 1;
}

static int render_mouse(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
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

static int event_mouse(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  TickitMouseEventInfo *info = _info;

  lastmouse = *info;

  tickit_window_expose(mousewin, NULL);

  return 1;
}

static int render_timer(TickitWindow *win, TickitEventFlags flags, void *_info, void *user)
{
  int *counterp = user;
  TickitExposeEventInfo *info = _info;
  TickitRenderBuffer *rb = info->rb;

  tickit_renderbuffer_eraserect(rb, &info->rect);

  tickit_renderbuffer_goto(rb, 0, 0);
  tickit_renderbuffer_textf(rb, "Counter %d", *counterp);

  return 1;
}

static int on_timer(Tickit *t, TickitEventFlags flags, void *user);
static int on_timer(Tickit *t, TickitEventFlags flags, void *user)
{
  int *counterp = user;

  (*counterp)++;
  tickit_window_expose(timerwin, NULL);

  tickit_watch_timer_after_msec(t, 1000, 0, &on_timer, user);

  return 0;
}

TickitEventHooks libuv_evhooks;

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_with_evloop(NULL, &libuv_evhooks, NULL);

  TickitWindow *root = tickit_get_rootwin(t);
  if(!root) {
    fprintf(stderr, "Cannot create TickitTerm - %s\n", strerror(errno));
    return 1;
  }

  keywin = tickit_window_new(root, (TickitRect){
      .top = 2, .left = 2, .lines = 3, .cols = tickit_window_cols(root) - 4
    }, 0);

  tickit_window_bind_event(keywin, TICKIT_WINDOW_ON_EXPOSE, 0, &render_key, NULL);
  tickit_window_bind_event(root, TICKIT_WINDOW_ON_KEY, 0, &event_key, NULL);

  mousewin = tickit_window_new(root, (TickitRect){
      .top = 8, .left = 2, .lines = 3, .cols = tickit_window_cols(root) - 4
    }, 0);

  tickit_window_bind_event(mousewin, TICKIT_WINDOW_ON_EXPOSE, 0, &render_mouse, NULL);
  tickit_window_bind_event(root, TICKIT_WINDOW_ON_MOUSE, 0, &event_mouse, NULL);

  timerwin = tickit_window_new(root, (TickitRect){
      .top = 15, .left = 2, .lines = 1, .cols = tickit_window_cols(root) - 4
    }, 0);

  tickit_window_bind_event(timerwin, TICKIT_WINDOW_ON_EXPOSE, 0, &render_timer, &counter);

  tickit_watch_timer_after_msec(t, 1000, 0, &on_timer, &counter);

  tickit_window_take_focus(root);
  tickit_window_set_cursor_visible(root, false);

  tickit_run(t);

  tickit_window_close(root);

  tickit_unref(t);

  return 0;
}

/*
 * UV event loop integration
 */

static void destroy_handle(uv_handle_t *handle)
{
  free(handle);
}

typedef struct {
  uv_loop_t *loop;
  uv_async_t *stop;
} EventLoopData;

static void *el_init(Tickit *t, void *initdata)
{
  EventLoopData *evdata = malloc(sizeof(EventLoopData));
  if(!evdata)
    return NULL;

  if(initdata)
    evdata->loop = initdata;
  else
    evdata->loop = uv_default_loop();

  if(!evdata->loop)
    goto abort;

  // No callback; we're just using this to wake the event loop up when we want to stop
  // See also  https://github.com/libuv/libuv/issues/2173
  evdata->stop = malloc(sizeof(uv_async_t));
  uv_async_init(evdata->loop, evdata->stop, NULL);

  return evdata;

abort:
  free(evdata);
  return NULL;
}

static void el_destroy(void *data)
{
}

static void el_run(void *data)
{
  EventLoopData *evdata = data;

  uv_run(evdata->loop, UV_RUN_DEFAULT);
}

static void el_stop(void *data)
{
  EventLoopData *evdata = data;

  uv_stop(evdata->loop);
  uv_async_send(evdata->stop);
}

static void fire_io_event(uv_poll_t *handle, int status, int events)
{
  TickitWatch *watch = handle->data;

  if(events & UV_READABLE) {
    tickit_evloop_invoke_watch(watch, TICKIT_EV_FIRE);
  }
}

static bool el_io_read(void *data, int fd, TickitBindFlags flags, TickitWatch *watch)
{
  EventLoopData *evdata = data;

  uv_poll_t *handle = malloc(sizeof(uv_poll_t));
  if(!handle)
    return false;

  uv_poll_init(evdata->loop, handle, fd);
  uv_poll_start(handle, UV_READABLE, &fire_io_event);

  handle->data = watch;
  tickit_evloop_set_watch_data(watch, handle);

  return true;
}

static void el_cancel_io(void *data, TickitWatch *watch)
{
  uv_poll_t *handle = tickit_evloop_get_watch_data(watch);

  uv_poll_stop(handle);
  uv_close((uv_handle_t *)handle, &destroy_handle);
}

static void fire_timer_event(uv_timer_t *handle)
{
  TickitWatch *watch = handle->data;

  tickit_evloop_invoke_watch(watch, TICKIT_EV_FIRE);

  uv_timer_stop(handle);
  uv_close((uv_handle_t *)handle, &destroy_handle);
}

static bool el_timer(void *data, const struct timeval *at, TickitBindFlags flags, TickitWatch *watch)
{
  EventLoopData *evdata = data;

  struct timeval now;
  gettimeofday(&now, NULL);

  long msec = (at->tv_sec - now.tv_sec) * 1000 + (at->tv_usec - now.tv_usec) / 1000;

  uv_timer_t *handle = malloc(sizeof(uv_timer_t));
  if(!handle)
    return false;

  uv_timer_init(evdata->loop, handle);
  uv_timer_start(handle, &fire_timer_event, msec, 0);

  handle->data = watch;
  tickit_evloop_set_watch_data(watch, handle);

  return true;
}

static void el_cancel_timer(void *data, TickitWatch *watch)
{
  uv_timer_t *handle = tickit_evloop_get_watch_data(watch);

  uv_timer_stop(handle);
  uv_close((uv_handle_t *)handle, &destroy_handle);
}

static void fire_later_event(uv_idle_t *handle)
{
  TickitWatch *watch = handle->data;

  tickit_evloop_invoke_watch(watch, TICKIT_EV_FIRE);

  uv_idle_stop(handle);
  uv_close((uv_handle_t *)handle, &destroy_handle);
}

static bool el_later(void *data, TickitBindFlags flags, TickitWatch *watch)
{
  EventLoopData *evdata = data;

  uv_idle_t *handle = malloc(sizeof(uv_idle_t));
  if(!handle)
    return false;

  uv_idle_init(evdata->loop, handle);
  uv_idle_start(handle, &fire_later_event);

  handle->data = watch;
  tickit_evloop_set_watch_data(watch, handle);

  return true;
}

static void el_cancel_later(void *data, TickitWatch *watch)
{
  uv_idle_t *handle = tickit_evloop_get_watch_data(watch);

  uv_idle_stop(handle);
  uv_close((uv_handle_t *)handle, &destroy_handle);
}

TickitEventHooks libuv_evhooks = {
  .init         = el_init,
  .destroy      = el_destroy,
  .run          = el_run,
  .stop         = el_stop,
  .io_read      = el_io_read,
  .cancel_io    = el_cancel_io,
  .timer        = el_timer,
  .cancel_timer = el_cancel_timer,
  .later        = el_later,
  .cancel_later = el_cancel_later,
};
