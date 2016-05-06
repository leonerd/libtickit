#include "tickit.h"
#include "taplib.h"
#include "taplib-tickit.h"
#include "taplib-mockterm.h"

int next_event = 0;
struct SavedEvent {
  TickitWindow *win;
  int type;
  int line, col;
} events[9];

int on_input_push(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  if(!(ev & TICKIT_EV_MOUSE))
    return 1;

  TickitMouseEventInfo *info = _info;

  events[next_event].win  = win;
  events[next_event].type = info->type;
  events[next_event].line = info->line;
  events[next_event].col  = info->col;
  next_event++;

  return 1;
}

void is_event(struct SavedEvent *ev, int type, int line, int col, char *name)
{
  if(ev->type != type) {
    fail(name);
    diag("got type=%d, expected type=%d", ev->type, type);
    return;
  }

  if(ev->line != line || ev->col != col) {
    fail(name);
    diag("got position=%d,%d, expected position=%d,%d", ev->col, ev->line, col, line);
    return;
  }

  pass(name);
}

void is_event_win(struct SavedEvent *ev, int type, int line, int col, TickitWindow *win, char *name)
{
  if(ev->win != win) {
    fail(name);
    diag("got win=%p, expected win=%p", ev->win, win);
    return;
  }

  is_event(ev, type, line, col, name);
}

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitWindow *root = tickit_window_new_root(tt);


  // dragging mouse within one window
  {
    int event_id = tickit_window_bind_event(root, TICKIT_EV_MOUSE, 0, &on_input_push, NULL);

    next_event = 0;
    press_mouse(TICKIT_MOUSEEV_PRESS, 1, 2, 5, 0);

    is_int(next_event, 1, "pushed 1 event after mouse press");
    is_event(events+0, TICKIT_MOUSEEV_PRESS, 2, 5, "event[0] after mouse press");

    next_event = 0;
    press_mouse(TICKIT_MOUSEEV_DRAG, 1, 3, 5, 0);

    is_int(next_event, 2, "pushed 2 events after drag");
    is_event(events+0, TICKIT_MOUSEEV_DRAG_START, 2, 5, "event[0] after mouse drag");
    is_event(events+1, TICKIT_MOUSEEV_DRAG,       3, 5, "event[1] after mouse drag");

    next_event = 0;
    press_mouse(TICKIT_MOUSEEV_RELEASE, 1, 3, 5, 0);

    is_int(next_event, 3, "pushed 3 events after release");
    is_event(events+0, TICKIT_MOUSEEV_DRAG_DROP, 3, 5, "event[0] after mouse release");
    is_event(events+1, TICKIT_MOUSEEV_DRAG_STOP, 3, 5, "event[1] after mouse release");
    is_event(events+2, TICKIT_MOUSEEV_RELEASE,   3, 5, "event[2] after mouse release");

    tickit_window_unbind_event_id(root, event_id);
  }

  // dragging between windows
  {
    TickitWindow *winA = tickit_window_new(root, (TickitRect){ 0, 0, 10, 80}, 0);
    TickitWindow *winB = tickit_window_new(root, (TickitRect){15, 0, 10, 80}, 0);

    tickit_window_bind_event(winA, TICKIT_EV_MOUSE, 0, &on_input_push, NULL);
    tickit_window_bind_event(winB, TICKIT_EV_MOUSE, 0, &on_input_push, NULL);

    next_event = 0;
    press_mouse(TICKIT_MOUSEEV_PRESS,   1,  5, 20, 0);
    press_mouse(TICKIT_MOUSEEV_DRAG,    1,  8, 20, 0);
    press_mouse(TICKIT_MOUSEEV_DRAG,    1, 12, 20, 0);
    press_mouse(TICKIT_MOUSEEV_DRAG,    1, 18, 20, 0);
    press_mouse(TICKIT_MOUSEEV_RELEASE, 1, 18, 20, 0);

    is_int(next_event, 9, "pushed 9 events");
    // press 5,20
    is_event_win(events+0, TICKIT_MOUSEEV_PRESS,         5, 20, winA, "event[0]");
    // drag 8,20
    is_event_win(events+1, TICKIT_MOUSEEV_DRAG_START,    5, 20, winA, "event[1]");
    is_event_win(events+2, TICKIT_MOUSEEV_DRAG,          8, 20, winA, "event[2]");
    // drag 12,20
    is_event_win(events+3, TICKIT_MOUSEEV_DRAG_OUTSIDE, 12, 20, winA, "event[3]");
    // drag 18,20
    is_event_win(events+4, TICKIT_MOUSEEV_DRAG,          3, 20, winB, "event[4]");
    is_event_win(events+5, TICKIT_MOUSEEV_DRAG_OUTSIDE, 18, 20, winA, "event[5]");
    // release 18,20
    is_event_win(events+6, TICKIT_MOUSEEV_DRAG_DROP,     3, 20, winB, "event[6]");
    is_event_win(events+7, TICKIT_MOUSEEV_DRAG_STOP,    18, 20, winA, "event[7]");
    is_event_win(events+8, TICKIT_MOUSEEV_RELEASE,       3, 20, winB, "event[8]");

    tickit_window_destroy(winA);
    tickit_window_destroy(winB);
  }

  tickit_window_destroy(root);
  tickit_term_destroy(tt);

  return exit_status();
}
