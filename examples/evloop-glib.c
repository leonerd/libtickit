#ifdef __GLIBC__
#  define _XOPEN_SOURCE 500  /* strdup */
#endif

#include "tickit.h"
#include "tickit-evloop.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <glib.h>
#include <glib-unix.h>

TickitKeyEventInfo lastkey;
TickitWindow *keywin;

TickitMouseEventInfo lastmouse;
TickitWindow *mousewin;

int counter = 0;
TickitWindow *timerwin;

static TickitPen *mkpen_highlight(void)
{
  static TickitPen *pen;
  if(!pen)
    pen = tickit_pen_new_attrs(
      TICKIT_PEN_FG,   3,
      TICKIT_PEN_BOLD, 1,
      0);

  return pen;
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

static int render_key(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;
  TickitRenderBuffer *rb = info->rb;

  tickit_renderbuffer_eraserect(rb, &info->rect);

  tickit_renderbuffer_goto(rb, 0, 0);
  {
    tickit_renderbuffer_savepen(rb);

    tickit_renderbuffer_setpen(rb, mkpen_highlight());
    tickit_renderbuffer_text(rb, "Key:");

    tickit_renderbuffer_restore(rb);
  }

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
  {
    tickit_renderbuffer_savepen(rb);

    tickit_renderbuffer_setpen(rb, mkpen_highlight());
    tickit_renderbuffer_text(rb, "Mouse:");

    tickit_renderbuffer_restore(rb);
  }

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
  {
    tickit_renderbuffer_savepen(rb);

    tickit_renderbuffer_setpen(rb, mkpen_highlight());
    tickit_renderbuffer_text(rb, "Counter:");

    tickit_renderbuffer_restore(rb);
  }

  tickit_renderbuffer_goto(rb, 2, 2);
  tickit_renderbuffer_textf(rb, "%d", *counterp);

  return 1;
}

static int on_timer(Tickit *t, TickitEventFlags flags, void *_info, void *user);
static int on_timer(Tickit *t, TickitEventFlags flags, void *_info, void *user)
{
  int *counterp = user;

  (*counterp)++;
  tickit_window_expose(timerwin, NULL);

  tickit_watch_timer_after_msec(t, 1000, 0, &on_timer, user);

  return 0;
}

static int render_root(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;
  TickitRenderBuffer *rb = info->rb;

  int right = tickit_window_cols(win) - 1;
  int bottom = tickit_window_lines(win) - 1;

  tickit_renderbuffer_eraserect(rb, &(TickitRect){
      .top = 0, .left = 0, .lines = bottom+1, .cols = right+1,
  });

  static TickitPen *pen_blue;
  if(!pen_blue)
    pen_blue = tickit_pen_new_attrs(
      TICKIT_PEN_FG, 4+8,
      0);

  static TickitPen *pen_white;
  if(!pen_white)
    pen_white = tickit_pen_new_attrs(
        TICKIT_PEN_FG, 7+8,
        0);

  // Draw a horizontal size marker bar
  {
    tickit_renderbuffer_setpen(rb, pen_blue);
    tickit_renderbuffer_hline_at(rb, 1, 0, right, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_vline_at(rb, 0, 2, 0, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_vline_at(rb, 0, 2, right, TICKIT_LINE_SINGLE, 0);

    tickit_renderbuffer_setpen(rb, pen_white);
    tickit_renderbuffer_goto(rb, 1, (right / 2) - 2);
    tickit_renderbuffer_textf(rb, " %d ", right + 1); // cols
  }

  int left = right - 4;

  // Draw a vertical size marker bar
  {
    tickit_renderbuffer_setpen(rb, pen_blue);
    tickit_renderbuffer_vline_at(rb, 0, bottom, left + 2, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_hline_at(rb, 0, left, right, TICKIT_LINE_SINGLE, 0);
    tickit_renderbuffer_hline_at(rb, bottom, left, right, TICKIT_LINE_SINGLE, 0);

    tickit_renderbuffer_setpen(rb, pen_white);
    tickit_renderbuffer_goto(rb, (bottom / 2) - 1, left);
    tickit_renderbuffer_textf(rb, "%d", bottom + 1); // lines
  }

  return 1;
}

static int event_resize(TickitWindow *root, TickitEventFlags flags, void *_info, void *data)
{
  int cols = tickit_window_cols(root);

  tickit_window_set_geometry(keywin, (TickitRect){
      .top = 2, .left = 2, .lines = 3, .cols = cols - 7
    });

  tickit_window_set_geometry(mousewin, (TickitRect){
      .top = 8, .left = 2, .lines = 3, .cols = cols - 7
    });

  tickit_window_set_geometry(timerwin, (TickitRect){
      .top = 15, .left = 2, .lines = 3, .cols = cols - 7
    });

  tickit_window_expose(root, NULL);

  return 1;
}

TickitEventHooks glib_evhooks;

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_with_evloop(NULL, &glib_evhooks, NULL);

  TickitWindow *root = tickit_get_rootwin(t);
  if(!root) {
    fprintf(stderr, "Cannot create TickitTerm - %s\n", strerror(errno));
    return 1;
  }

  keywin = tickit_window_new(root, (TickitRect){
      .top = 2, .left = 2, .lines = 3, .cols = tickit_window_cols(root) - 7
    }, 0);

  tickit_window_bind_event(keywin, TICKIT_WINDOW_ON_EXPOSE, 0, &render_key, NULL);
  tickit_window_bind_event(root, TICKIT_WINDOW_ON_KEY, 0, &event_key, NULL);

  mousewin = tickit_window_new(root, (TickitRect){
      .top = 8, .left = 2, .lines = 3, .cols = tickit_window_cols(root) - 7
    }, 0);

  tickit_window_bind_event(mousewin, TICKIT_WINDOW_ON_EXPOSE, 0, &render_mouse, NULL);
  tickit_window_bind_event(root, TICKIT_WINDOW_ON_MOUSE, 0, &event_mouse, NULL);

  timerwin = tickit_window_new(root, (TickitRect){
      .top = 15, .left = 2, .lines = 3, .cols = tickit_window_cols(root) - 7
    }, 0);

  tickit_window_bind_event(timerwin, TICKIT_WINDOW_ON_EXPOSE, 0, &render_timer, &counter);

  tickit_window_bind_event(root, TICKIT_WINDOW_ON_EXPOSE, 0, &render_root, NULL);
  tickit_window_bind_event(root, TICKIT_WINDOW_ON_GEOMCHANGE, 0, &event_resize, NULL);

  tickit_watch_timer_after_msec(t, 1000, 0, &on_timer, &counter);

  tickit_window_take_focus(root);
  tickit_window_set_cursor_visible(root, false);

  tickit_run(t);

  tickit_window_close(root);

  tickit_unref(t);

  return 0;
}

/*
 * GLib event loop integration
 */

typedef struct {
  GMainLoop *loop;
} EventLoopData;

static gboolean el_sighandler(gpointer t)
{
  tickit_evloop_invoke_watch(t, TICKIT_EV_FIRE);

  return TRUE;
}

static void *el_init(Tickit *t, void *initdata)
{
  EventLoopData *evdata = malloc(sizeof(EventLoopData));
  if(!evdata)
    return NULL;

  if(initdata)
    evdata->loop = initdata;
  else
    evdata->loop = g_main_loop_new(NULL, FALSE);

  return evdata;
}

static void el_destroy(void *data)
{
  EventLoopData *evdata = data;

  if(evdata->loop)
    g_main_loop_unref(evdata->loop);

  free(evdata);
}

static void el_run(void *data, TickitRunFlags flags)
{
  EventLoopData *evdata = data;

  if(flags & (TICKIT_RUN_ONCE|TICKIT_RUN_NOHANG)) {
    GMainContext *context = g_main_loop_get_context(evdata->loop);
    g_main_context_iteration(context, !(flags & TICKIT_RUN_NOHANG));
  }
  else
    g_main_loop_run(evdata->loop);
}

static void el_stop(void *data)
{
  EventLoopData *evdata = data;

  g_main_loop_quit(evdata->loop);
}

static gboolean fire_io_event(gint fd, GIOCondition gcond, gpointer watch)
{
  TickitIOCondition cond = 0;
  if(gcond & G_IO_IN)
    cond |= TICKIT_IO_IN;
  if(gcond & G_IO_OUT)
    cond |= TICKIT_IO_OUT;
  if(gcond & G_IO_HUP)
    cond |= TICKIT_IO_HUP;
  if(gcond & G_IO_ERR)
    cond |= TICKIT_IO_ERR;
  if(gcond & G_IO_NVAL)
    cond |= TICKIT_IO_INVAL;

  if(cond) {
    tickit_evloop_invoke_iowatch(watch, TICKIT_EV_FIRE, cond);
  }

  return TRUE;
}

static bool el_io(void *data, int fd, TickitIOCondition cond, TickitBindFlags flags, TickitWatch *watch)
{
  GIOCondition gcond = 0;
  if(cond & TICKIT_IO_IN)
    gcond |= G_IO_IN;
  if(cond & TICKIT_IO_OUT)
    gcond |= G_IO_OUT;
  if(cond & TICKIT_IO_HUP)
    gcond |= G_IO_HUP;

  int id = g_unix_fd_add(fd, gcond, fire_io_event, watch);
  tickit_evloop_set_watch_data_int(watch, id);

  return true;
}

static void el_cancel_io(void *data, TickitWatch *watch)
{
  g_source_remove(tickit_evloop_get_watch_data_int(watch));
}

static gboolean fire_event(gpointer watch)
{
  tickit_evloop_invoke_watch(watch, TICKIT_EV_FIRE);

  return FALSE; // All non-IO events are oneshot
}

static bool el_timer(void *data, const struct timeval *at, TickitBindFlags flags, TickitWatch *watch)
{
  struct timeval now;
  gettimeofday(&now, NULL);

  long msec = (at->tv_sec - now.tv_sec) * 1000 + (at->tv_usec - now.tv_usec) / 1000;
  int id = g_timeout_add(msec, fire_event, watch);
  tickit_evloop_set_watch_data_int(watch, id);

  return true;
}

static void el_cancel_timer(void *data, TickitWatch *watch)
{
  g_source_remove(tickit_evloop_get_watch_data_int(watch));
}

static bool el_later(void *data, TickitBindFlags flags, TickitWatch *watch)
{
  int id = g_idle_add(fire_event, watch);
  tickit_evloop_set_watch_data_int(watch, id);

  return true;
}

static void el_cancel_later(void *data, TickitWatch *watch)
{
  g_source_remove(tickit_evloop_get_watch_data_int(watch));
}

static bool el_signal(void *data, int signum, TickitBindFlags flags, TickitWatch *watch)
{
  int id = g_unix_signal_add(signum, el_sighandler, watch);
  tickit_evloop_set_watch_data_int(watch, id);

  return true;
}

static void el_cancel_signal(void *data, TickitWatch *watch)
{
  g_source_remove(tickit_evloop_get_watch_data_int(watch));
}

TickitEventHooks glib_evhooks = {
  .init          = el_init,
  .destroy       = el_destroy,
  .run           = el_run,
  .stop          = el_stop,
  .io            = el_io,
  .cancel_io     = el_cancel_io,
  .timer         = el_timer,
  .cancel_timer  = el_cancel_timer,
  .later         = el_later,
  .cancel_later  = el_cancel_later,
  .signal        = el_signal,
  .cancel_signal = el_cancel_signal,
};
