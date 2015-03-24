#include "tickit.h"
#include "tickit-window.h"
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

int on_input_capture(TickitWindow *win, TickitEventType ev, TickitEventInfo *args, void *data)
{
  struct LastEvent *last_event = data;

  last_event->type = args->type;
  last_event->mod = args->mod;
  if(ev & TICKIT_EV_KEY)
    strcpy(last_event->str, args->str);
  else if(ev & TICKIT_EV_MOUSE) {
    last_event->line = args->line;
    last_event->col = args->col;
    last_event->button = args->button;
  }

  return last_event->ret;
}

int next_idx = 0;
char *ids[3];

int on_input_push(TickitWindow *win, TickitEventType ev, TickitEventInfo *args, void *data)
{
  ids[next_idx++] = data;
  return 0;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitWindow *root = tickit_window_new_root(tt);

  TickitWindow *win = tickit_window_new_subwindow(root, 3, 10, 4, 20);

  tickit_window_take_focus(win);
  tickit_window_tick(root);

  struct LastEvent win_last = { .ret = 1 };
  int bind_id = tickit_window_bind_event(win, TICKIT_EV_KEY|TICKIT_EV_MOUSE, &on_input_capture, &win_last);

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

  TickitWindow *subwin = tickit_window_new_subwindow(win, 2, 2, 1, 10);

  // Subwindow
  {
    tickit_window_take_focus(subwin);
    tickit_window_tick(root);

    struct LastEvent subwin_last = { .ret = 1 };
    int sub_bind_id = tickit_window_bind_event(subwin, TICKIT_EV_KEY|TICKIT_EV_MOUSE, &on_input_capture, &subwin_last);

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

    tickit_window_unbind_event_id(subwin, sub_bind_id);
  }

  tickit_window_unbind_event_id(win, bind_id);

  // Event ordering
  {
    TickitWindow *otherwin = tickit_window_new_subwindow(root, 10, 10, 4, 20);
    tickit_window_tick(root);

    int bind_ids[] = {
      tickit_window_bind_event(win,      TICKIT_EV_KEY, &on_input_push, "win"),
      tickit_window_bind_event(subwin,   TICKIT_EV_KEY, &on_input_push, "subwin"),
      tickit_window_bind_event(otherwin, TICKIT_EV_KEY, &on_input_push, "otherwin"),
    };

    press_key(TICKIT_EV_KEY, "D", 0);

    is_int(next_idx, 3, "press_key pushes 3 strings for D");
    is_str(ids[0], "subwin",   "ids[0] for D");
    is_str(ids[1], "win",      "ids[1] for D");
    is_str(ids[2], "otherwin", "ids[2] for D");

    tickit_window_hide(subwin);

    next_idx = 0;

    press_key(TICKIT_EV_KEY, "E", 0);

    is_int(next_idx, 2, "press_key pushes 2 strings for E");
    is_str(ids[0], "win",      "ids[0] for E");
    is_str(ids[1], "otherwin", "ids[1] for E");
  }

  return exit_status();
}
