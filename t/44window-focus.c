#include "tickit.h"
#include "tickit-window.h"
#include "taplib.h"
#include "taplib-mockterm.h"

int on_focus(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  *(int *)data = ((TickitFocusEventInfo *)_info)->type == TICKIT_FOCUSEV_IN ? 1 : -1;
  return 1;
}

int next_event = 0;
static struct {
  int type;
  TickitWindow *win;
  TickitWindow *focuswin;
} focus_events[16];

int on_focus_push(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitFocusEventInfo *info = _info;

  if(next_event > sizeof(focus_events)/sizeof(focus_events[0]))
    return 0;

  focus_events[next_event].type = info->type;
  focus_events[next_event].win = win;
  focus_events[next_event].focuswin = info->win;
  next_event++;

  return 1;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitWindow *root = tickit_window_new_root(tt);

  TickitWindow *win = tickit_window_new_subwindow(root, 3, 10, 4, 20);

  int focused;
  tickit_window_bind_event(win, TICKIT_EV_FOCUS, &on_focus, &focused);

  int value;

  // Basics
  {
    tickit_term_getctl_int(tt, TICKIT_TERMCTL_CURSORVIS, &value);
    ok(!value, "cursor not yet visible initially");

    tickit_window_tick(root);

    ok(!tickit_window_is_focused(win), "window does not yet have focus");

    tickit_window_cursor_at(win, 0, 0);
    tickit_window_tick(root);

    ok(!tickit_window_is_focused(win), "window still unfocused after cursor_at");

    tickit_window_take_focus(win);
    // no flush

    ok(tickit_window_is_focused(win), "window immediately has focus after take_focus");
    is_int(focused, 1, "window receives FOCUS_IN event");
    focused = 0;

    tickit_window_tick(root);

    is_termlog("Termlog after focus",
        GOTO(3,10),
        NULL);

    tickit_term_getctl_int(tt, TICKIT_TERMCTL_CURSORVIS, &value);
    ok(value, "Terminal cursor visible after window focus");

    tickit_window_reposition(win, 5, 15);
    tickit_window_tick(root);

    is_termlog("Termlog after window reposition",
        GOTO(5,15),
        NULL);

    tickit_window_cursor_at(win, 2, 2);
    tickit_window_tick(root);

    is_termlog("Termlog after cursor_at",
        GOTO(7,17),
        NULL);

    tickit_window_cursor_shape(win, TICKIT_TERM_CURSORSHAPE_UNDER);
    tickit_window_tick(root);

    is_termlog("Termlog after cursor_shape",
        GOTO(7,17),
        NULL);
    tickit_term_getctl_int(tt, TICKIT_TERMCTL_CURSORSHAPE, &value);
    is_int(value, TICKIT_TERM_CURSORSHAPE_UNDER, "Cursor shape after cursor_shape");

    tickit_window_cursor_visible(win, false);
    tickit_window_tick(root);

    is_termlog("Termlog empty after cursor_visible false",
        NULL);

    tickit_term_getctl_int(tt, TICKIT_TERMCTL_CURSORVIS, &value);
    ok(!value, "Cursor is invisible after cursor_visible false");

    tickit_window_cursor_visible(win, true);

    tickit_window_hide(win);
    tickit_window_tick(root);

    tickit_term_getctl_int(tt, TICKIT_TERMCTL_CURSORVIS, &value);
    ok(!value, "Cursor is invisible after focus window hide");

    tickit_window_show(win);
    tickit_window_tick(root);

    tickit_term_getctl_int(tt, TICKIT_TERMCTL_CURSORVIS, &value);
    ok(value, "Cursor is visible after focus window show");

    is_termlog("Termlog after focus window show",
        GOTO(7,17),
        NULL);
  }

  {
    TickitWindow *winA = tickit_window_new_subwindow(root, 5, 0, 1, 80);
    TickitWindow *winB = tickit_window_new_subwindow(root, 6, 0, 1, 80);
    tickit_window_cursor_at(winA, 0, 0);
    tickit_window_cursor_at(winB, 0, 0);

    int focusA = 0;
    int focusB = 0;
    tickit_window_bind_event(winA, TICKIT_EV_FOCUS, &on_focus, &focusA);
    tickit_window_bind_event(winB, TICKIT_EV_FOCUS, &on_focus, &focusB);

    tickit_window_take_focus(winA);
    tickit_window_tick(root);

    is_int(focusA, 1, "focusA after winA takes focus");
    is_int(focusB, 0, "focusB undef after winA takes focus");
    is_termlog("Termlog after winA takes focus",
        GOTO(5,0),
        NULL);

    tickit_window_take_focus(winB);
    tickit_window_tick(root);

    is_int(focusA, -1, "focusA lost after winB takes focus");
    is_int(focusB,  1, "focusB after winB takes focus");
    is_termlog("Termlog after winB takes focus",
        GOTO(6,0),
        NULL);

    tickit_window_hide(winB);
    tickit_window_take_focus(winA);
    tickit_window_tick(root);

    is_termlog("Termlog after winB hidden",
        GOTO(5,0),
        NULL);

    tickit_window_hide(winA);
    tickit_window_show(winB);
    tickit_window_tick(root);

    is_termlog("Termlog after winA hidden / winB shown",
        GOTO(6,0),
        NULL);

    tickit_window_take_focus(winA);
    tickit_window_tick(root);

    is_termlog("Termlog empty after winA take focus while hidden", NULL);
    ok(tickit_window_is_focused(winB), "winB still has focus after take focus while hidden");

    tickit_window_destroy(winA);
    tickit_window_destroy(winB);
    tickit_window_tick(root);
  }

  // Child notifications
  {
    TickitWindow *subwin = tickit_window_new_subwindow(win, 1, 1, 2, 2);

    int bind_id = tickit_window_bind_event(win, TICKIT_EV_FOCUS, &on_focus_push, NULL);
    tickit_window_bind_event(subwin, TICKIT_EV_FOCUS, &on_focus_push, NULL);

    tickit_window_set_focus_child_notify(win, true);
    tickit_window_tick(root);

    tickit_window_cursor_at(subwin, 0, 0);
    tickit_window_take_focus(subwin);

    is_int(next_event, 2, "take_focus pushes two events");
    is_int(focus_events[0].type,     TICKIT_FOCUSEV_IN, "focus_events[0].type");
    is_ptr(focus_events[0].win,      win,               "focus_events[0].win");
    is_ptr(focus_events[0].focuswin, subwin,            "focus_events[0].focuswin");
    is_int(focus_events[1].type,     TICKIT_FOCUSEV_IN, "focus_events[1].type");
    is_ptr(focus_events[1].win,      subwin,            "focus_events[1].win");
    is_ptr(focus_events[1].focuswin, subwin,            "focus_events[1].focuswin");

    TickitWindow *otherwin = tickit_window_new_subwindow(root, 0, 0, 1, 1);
    tickit_window_tick(root);

    next_event = 0;

    tickit_window_take_focus(otherwin);

    is_int(next_event, 2, "losing focus pushes two events");
    is_int(focus_events[0].type,     TICKIT_FOCUSEV_OUT, "focus_events[0].type");
    is_ptr(focus_events[0].win,      subwin,             "focus_events[0].win");
    is_ptr(focus_events[0].focuswin, subwin,             "focus_events[0].focuswin");
    is_int(focus_events[1].type,     TICKIT_FOCUSEV_OUT, "focus_events[1].type");
    is_ptr(focus_events[1].win,      win,                "focus_events[1].win");
    is_ptr(focus_events[1].focuswin, subwin,             "focus_events[1].focuswin");

    tickit_window_destroy(otherwin);
    tickit_window_destroy(subwin);
  }

  tickit_window_destroy(win);

  tickit_window_destroy(root);

  return exit_status();
}
