#include "tickit.h"
#include "tickit-window.h"
#include "taplib.h"
#include "taplib-tickit.h"
#include "taplib-mockterm.h"

int on_expose_fillchr(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;

  for(int line = info->rect.top; line < info->rect.top + info->rect.lines; line++) {
    char buffer[90];
    for(int i = 0; i < info->rect.cols; i++)
      buffer[i] = *(char *)data;
    buffer[info->rect.cols] = 0;
    tickit_renderbuffer_text_at(info->rb, line, info->rect.left, buffer, NULL);
  }

  return 1;
}

int on_expose_textat(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;

  tickit_renderbuffer_text_at(info->rb, 0, 0, data, NULL);
  return 1;
}

static int next_rect = 0;
static TickitRect exposed_rects[16];

int on_expose_pushrect(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;

  if(next_rect >= sizeof(exposed_rects)/sizeof(exposed_rects[0]))
    return 0;

  exposed_rects[next_rect++] = info->rect;
  return 1;
}

int on_input_capture(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  if(ev & TICKIT_EV_KEY)
    *((TickitKeyEventInfo *)data) = *(TickitKeyEventInfo *)_info;
  if(ev & TICKIT_EV_MOUSE)
    *((TickitMouseEventInfo *)data) = *(TickitMouseEventInfo *)_info;
  return 1;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitWindow *root = tickit_window_new_root(tt);

  TickitWindow *rootfloat = tickit_window_new_float(root, 10, 10, 5, 30);
  tickit_window_tick(root);

  // Basics
  {
    int bind_id = tickit_window_bind_event(root, TICKIT_EV_EXPOSE, &on_expose_fillchr, "X");

    tickit_window_expose(root, &(TickitRect){ .top = 10, .lines = 1, .left = 0, .cols = 80 });
    tickit_window_tick(root);

    is_termlog("Termlog for print under floating window",
        GOTO(10,0), SETPEN(), PRINT("XXXXXXXXXX"),
        GOTO(10,40), SETPEN(), PRINT("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"),
        NULL);

    TickitWindow *win = tickit_window_new_subwindow(root, 10, 20, 1, 50);

    tickit_window_bind_event(win, TICKIT_EV_EXPOSE, &on_expose_fillchr, "Y");

    tickit_window_expose(win, NULL);
    tickit_window_tick(root);

    is_termlog("Termlog for print sibling under floating window",
        GOTO(10,40), SETPEN(), PRINT("YYYYYYYYYYYYYYYYYYYYYYYYYYYYYY"),
        NULL);

    TickitWindow *popupwin = tickit_window_new_popup(win, 2, 2, 10, 10);
    tickit_window_tick(root);

    is_int(tickit_window_abs_top(popupwin),  12, "popupwin abs_top");
    is_int(tickit_window_abs_left(popupwin), 22, "popupwin abs_left");

    TickitKeyEventInfo keyinfo;
    tickit_window_bind_event(popupwin, TICKIT_EV_KEY, &on_input_capture, &keyinfo);

    press_key(TICKIT_KEYEV_TEXT, "G", 0);

    is_int(keyinfo.type, TICKIT_KEYEV_TEXT, "key type after press_key on popupwin");

    TickitMouseEventInfo mouseinfo;
    tickit_window_bind_event(popupwin, TICKIT_EV_MOUSE, &on_input_capture, &mouseinfo);

    press_mouse(TICKIT_MOUSEEV_PRESS, 1, 5, 12, 0);

    is_int(mouseinfo.type, TICKIT_MOUSEEV_PRESS, "mouse type after press_mouse on popupwin");
    is_int(mouseinfo.line,  -7, "mouse line after press_mouse on popupwin");
    is_int(mouseinfo.col,  -10, "mouse column after press_mouse on popupwin");

    tickit_window_destroy(popupwin);
    tickit_window_destroy(win);

    tickit_window_unbind_event_id(root, bind_id);
  }

  // Drawing on floats
  {
    int bind_id = tickit_window_bind_event(rootfloat, TICKIT_EV_EXPOSE, &on_expose_textat, "|-- Yipee --|");
    tickit_window_expose(rootfloat, NULL);
    tickit_window_tick(root);

    is_termlog("Termlog for print to floating window",
        GOTO(10,10), SETPEN(), PRINT("|-- Yipee --|"),
        NULL);

    TickitWindow *subwin = tickit_window_new_subwindow(rootfloat, 0, 4, 1, 6);

    tickit_window_bind_event(subwin, TICKIT_EV_EXPOSE, &on_expose_textat, "Byenow");
    tickit_window_expose(subwin, NULL);
    tickit_window_tick(root);

    is_termlog("Termlog for print to child of floating window",
        GOTO(10,14), SETPEN(), PRINT("Byenow"),
        NULL);

    tickit_window_destroy(subwin);
    tickit_window_unbind_event_id(rootfloat, bind_id);
  }

  // Scrolling with float obscurations
  {
    int bind_id = tickit_window_bind_event(root, TICKIT_EV_EXPOSE, &on_expose_pushrect, NULL);
    tickit_window_tick(root);
    drain_termlog();

    next_rect = 0;

    tickit_window_scroll(root, 3, 0);
    tickit_window_tick(root);

    is_termlog("Termlog after scroll with floats",
        SETPEN(),
        SCROLLRECT( 0,0,10,80, 3,0),
        SCROLLRECT(15,0,10,80, 3,0),
        NULL);

    is_int(next_rect, 4, "");
    is_rect(exposed_rects+0, "0,7+80,3", "exposed_rects[0]");
    is_rect(exposed_rects+1, "0,10+10,5", "exposed_rects[1]");
    is_rect(exposed_rects+2, "40,10+40,5", "exposed_rects[2]");
    is_rect(exposed_rects+3, "0,22+80,3", "exposed_rects[3]");
  }

  return exit_status();
}
