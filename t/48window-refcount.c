#include "tickit.h"
#include "taplib.h"
#include "taplib-mockterm.h"

/*
 * This test file mostly can't detect its own errors. It might segfault if the
 * library can't cope. Alternatively, run it via valgrind
 *
 *   $ LD_LIBRARY_PATH=.libs valgrind t/.libs/48window-refcount.t
 */

int on_event_close(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitWindow *target = data;

  tickit_window_close(target);
  tickit_window_unref(target);
}

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitWindow *root = tickit_window_new_root(tt);

  // Windows can close themeselves in event handler without crashing
  {
    TickitWindow *win = tickit_window_new(root, (TickitRect){3, 10, 4, 20}, 0);
    tickit_window_take_focus(win);
    tickit_window_flush(root);

    tickit_window_bind_event(win, TICKIT_EV_KEY, 0, &on_event_close, win);

    press_key(TICKIT_KEYEV_TEXT, "A", 0);
    tickit_window_flush(root);

    // We can't really tell here if it worked, but it hasn't crashed.
    pass("Window can close itself in event handler");
  }

  tickit_window_unref(root);
  tickit_term_unref(tt);

  return exit_status();
}
