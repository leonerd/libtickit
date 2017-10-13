#include "tickit.h"
#include "taplib.h"
#include "taplib-mockterm.h"

#include <string.h>

struct LastEvent {
  int type;
  int mod;
  int line, col, button;
  char str[16];

  int ret;
};

int on_key_event_capture(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  struct LastEvent *last_event = data;
  TickitKeyEventInfo *info = _info;

  last_event->type = info->type;
  last_event->mod = info->mod;
  strcpy(last_event->str, info->str);

  return last_event->ret;
}

int on_mouse_event_capture(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  struct LastEvent *last_event = data;
  TickitMouseEventInfo *info = _info;

  last_event->type = info->type;
  last_event->mod = info->mod;
  last_event->line = info->line;
  last_event->col = info->col;
  last_event->button = info->button;

  return last_event->ret;
}

int next_idx = 0;
char *ids[3];

int on_event_push(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  ids[next_idx++] = data;
  return 0;
}

int on_event_incr_int(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  ((int *)data)[0]++;
  return 0;
}

int on_termevent_incr_int(TickitTerm *tt, TickitEventFlags flags, void *_info, void *data)
{
  ((int *)data)[0]++;
  return 0;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitWindow *root = tickit_window_new_root(tt);

  TickitWindow *win = tickit_window_new(root, (TickitRect){3, 10, 4, 20}, 0);

  tickit_window_take_focus(win);
  tickit_window_flush(root);

  struct LastEvent win_last = { .ret = 1 };
  int key_bind_id = tickit_window_bind_event(win, TICKIT_WINDOW_ON_KEY, 0,
      &on_key_event_capture, &win_last);
  int mouse_bind_id = tickit_window_bind_event(win, TICKIT_WINDOW_ON_MOUSE, 0,
      &on_mouse_event_capture, &win_last);

  // Key events
  {
    press_key(TICKIT_KEYEV_TEXT, "A", 0);

    is_int(win_last.type, TICKIT_KEYEV_TEXT, "win_last.type for A");
    is_str(win_last.str,  "A",               "win_last.str for A");

    press_key(TICKIT_KEYEV_KEY, "C-a", TICKIT_MOD_CTRL);

    is_int(win_last.type, TICKIT_KEYEV_KEY, "win_last.type for C-a");
    is_str(win_last.str,  "C-a",            "win_last.str for C-a");
    is_int(win_last.mod,  TICKIT_MOD_CTRL,  "win_last.mod for C-a");
  }

  // Mouse events
  {
    win_last.type = 0;
    press_mouse(TICKIT_MOUSEEV_PRESS, 1, 5, 15, 0);

    is_int(win_last.type, TICKIT_MOUSEEV_PRESS, "win_last.type for press 1@15,5");
    is_int(win_last.line, 2,                    "win_last.line for press 1@15,5");
    is_int(win_last.col,  5,                    "win_last.col for press 1@15,5");

    win_last.type = 0;
    press_mouse(TICKIT_MOUSEEV_PRESS, 1, 1, 2, 0);

    is_int(win_last.type, 0, "win_last.type still 0 after press @1,2");
  }

  // Unhandled events are not consumed
  {
    int term_invoked = 0;
    int bind_id = tickit_term_bind_event(tt, TICKIT_TERM_ON_KEY, 0, &on_termevent_incr_int, &term_invoked);

    win_last.ret = 0;

    press_key(TICKIT_KEYEV_KEY, "A", 0);

    ok(term_invoked, "term event handler invoked after unhandled window event");

    tickit_term_unbind_event_id(tt, bind_id);
  }

  TickitWindow *subwin = tickit_window_new(win, (TickitRect){2, 2, 1, 10}, 0);

  // Subwindow
  {
    tickit_window_take_focus(subwin);
    tickit_window_flush(root);

    struct LastEvent subwin_last = { .ret = 1 };
    int sub_key_bind_id = tickit_window_bind_event(subwin, TICKIT_WINDOW_ON_KEY, 0,
        &on_key_event_capture, &subwin_last);
    int sub_mouse_bind_id = tickit_window_bind_event(subwin, TICKIT_WINDOW_ON_MOUSE, 0,
        &on_mouse_event_capture, &subwin_last);

    win_last.type = 0;

    press_key(TICKIT_KEYEV_TEXT, "B", 0);

    is_int(subwin_last.type, TICKIT_KEYEV_TEXT, "subwin_last.type for B");
    is_str(subwin_last.str,  "B",               "subwin_last.str for B");

    is_int(win_last.type, 0, "win_last.type for B");

    subwin_last.type = 0;

    press_mouse(TICKIT_MOUSEEV_PRESS, 1, 5, 15, 0);

    is_int(subwin_last.type, TICKIT_MOUSEEV_PRESS, "subwin_last.type for press 1@15,5");
    is_int(subwin_last.line, 0,                    "subwin_last.line for press 1@15,5");
    is_int(subwin_last.col,  3,                    "subwin_last.col for press 1@15,5");

    subwin_last.ret = 0;

    subwin_last.type = 0;
    win_last.type = 0;

    press_key(TICKIT_KEYEV_TEXT, "C", 0);

    is_int(subwin_last.type, TICKIT_KEYEV_TEXT, "subwin_last.type for C");
    is_str(subwin_last.str,  "C",               "subwin_last.str for C");

    is_int(win_last.type, TICKIT_KEYEV_TEXT, "win_last.type for C");
    is_str(win_last.str,  "C",               "win_last.str for C");

    tickit_window_unbind_event_id(subwin, sub_key_bind_id);
    tickit_window_unbind_event_id(subwin, sub_mouse_bind_id);
  }

  tickit_window_unbind_event_id(win, key_bind_id);
  tickit_window_unbind_event_id(win, mouse_bind_id);

  // Event ordering
  {
    TickitWindow *otherwin = tickit_window_new(root, (TickitRect){10, 10, 4, 20}, 0);
    tickit_window_flush(root);

    int bind_ids[] = {
      tickit_window_bind_event(win,      TICKIT_WINDOW_ON_KEY, 0, &on_event_push, "win"),
      tickit_window_bind_event(subwin,   TICKIT_WINDOW_ON_KEY, 0, &on_event_push, "subwin"),
      tickit_window_bind_event(otherwin, TICKIT_WINDOW_ON_KEY, 0, &on_event_push, "otherwin"),
    };

    press_key(TICKIT_KEYEV_TEXT, "D", 0);

    is_int(next_idx, 3, "press_key pushes 3 strings for D");
    is_str(ids[0], "subwin",   "ids[0] for D");
    is_str(ids[1], "win",      "ids[1] for D");
    is_str(ids[2], "otherwin", "ids[2] for D");

    tickit_window_hide(subwin);

    next_idx = 0;

    press_key(TICKIT_KEYEV_TEXT, "E", 0);

    is_int(next_idx, 2, "press_key pushes 2 strings for E");
    is_str(ids[0], "win",      "ids[0] for E");
    is_str(ids[1], "otherwin", "ids[1] for E");

    tickit_window_unref(otherwin);
    tickit_window_flush(root);
  }

  tickit_window_unref(subwin);

  // Windows created in input events handlers don't receive events
  //   child windows
  {
    TickitWindow *childwin = NULL;
    int childmouse = 0;

    int win_on_mouse(TickitWindow *win, TickitEventFlags flags, void *_info, void *data) {
      if(childwin)
        return 0;

      childwin = tickit_window_new(win, (TickitRect){0, 0, 2, 2}, 0);
      tickit_window_bind_event(childwin, TICKIT_WINDOW_ON_MOUSE, 0, &on_event_incr_int, &childmouse);
      return 0;
    }

    int id = tickit_window_bind_event(win, TICKIT_WINDOW_ON_MOUSE, 0, &win_on_mouse, NULL);

    press_mouse(TICKIT_MOUSEEV_PRESS, 1, 3, 10, 0);

    ok(!!childwin, "child window created");
    is_int(childmouse, 0, "child window has not yet received a mouse event");

    tickit_window_flush(root);

    press_mouse(TICKIT_MOUSEEV_PRESS, 1, 3, 10, 0);

    is_int(childmouse, 1, "child window has now received an event after flush");

    tickit_window_unbind_event_id(win, id);
    tickit_window_unref(childwin);
    tickit_window_flush(root);
  }

  //   sibling windows
  {
    TickitWindow *siblingwin = NULL;
    int siblingmouse = 0;

    int win_on_mouse(TickitWindow *win, TickitEventFlags flags, void *_info, void *data) {
      if(siblingwin)
        return 0;

      siblingwin = tickit_window_new(win, (TickitRect){0, 0, 2, 2}, 0);
      tickit_window_bind_event(siblingwin, TICKIT_WINDOW_ON_MOUSE, 0, &on_event_incr_int, &siblingmouse);
      return 0;
    }

    int id = tickit_window_bind_event(win, TICKIT_WINDOW_ON_MOUSE, 0, &win_on_mouse, NULL);

    press_mouse(TICKIT_MOUSEEV_PRESS, 1, 3, 10, 0);

    ok(!!siblingwin, "sibling window created");
    is_int(siblingmouse, 0, "sibling window has not yet received a mouse event");

    tickit_window_flush(root);

    press_mouse(TICKIT_MOUSEEV_PRESS, 1, 3, 10, 0);

    is_int(siblingmouse, 1, "sibling window has now received an event after flush");

    tickit_window_unbind_event_id(win, id);
    tickit_window_unref(siblingwin);
    tickit_window_flush(root);
  }

  // STEAL_INPUT
  {
    TickitWindow *thief = tickit_window_new(root, (TickitRect){2, 5, 3, 3},
        TICKIT_WINDOW_STEAL_INPUT);

    struct LastEvent thief_last = { .ret = 1 };
    tickit_window_bind_event(thief, TICKIT_WINDOW_ON_KEY, 0, &on_key_event_capture, &thief_last);
    tickit_window_bind_event(thief, TICKIT_WINDOW_ON_MOUSE, 0, &on_mouse_event_capture, &thief_last);

    ok(tickit_window_is_steal_input(thief), "tickit_window_is_steal_input() returns true");

    press_key(TICKIT_KEYEV_TEXT, "D", 0);

    is_int(thief_last.type, TICKIT_KEYEV_TEXT, "thief_last.type for D");
    is_str(thief_last.str, "D",                "thief_last.str for D");

    press_mouse(TICKIT_MOUSEEV_PRESS, 1, 1, 1, 0);

    is_int(thief_last.type, TICKIT_MOUSEEV_PRESS, "thief_last.type for press 1@1,1");
    is_int(thief_last.line, -1,                   "thief_last.line for press 1@1,1");
    is_int(thief_last.col,  -4,                   "thief_last.col for press 1@1,1");

    thief_last.type = 0;
    win_last.type = 0;

    tickit_window_set_steal_input(thief, false);
    ok(!tickit_window_is_steal_input(thief), "tickit_window_is_steal_input() returns false");

    press_mouse(TICKIT_MOUSEEV_PRESS, 1, 1, 1, 0);

    is_int(thief_last.type, 0, "thief does not see event after disabling STEAL_INPUT");

    tickit_window_unref(thief);
  }

  tickit_window_unref(root);
  tickit_term_unref(tt);

  return exit_status();
}
