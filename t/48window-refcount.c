#include "tickit.h"
#include "taplib.h"
#include "taplib-mockterm.h"

/*
 * This test file mostly can't detect its own errors. It might segfault if the
 * library can't cope. Alternatively, run it via valgrind
 *
 *   $ LD_LIBRARY_PATH=.libs valgrind t/.libs/48window-refcount.t
 */

int on_event_close(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  TickitWindow *target = data;

  tickit_window_close(target);
  tickit_window_unref(target);

  return 0;
}

int on_event_incr(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
{
  (*((int *)data))++;
  return 0;
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

    tickit_window_bind_event(win, TICKIT_WINDOW_ON_KEY, 0, &on_event_close, win);

    press_key(TICKIT_KEYEV_TEXT, "A", 0);
    tickit_window_flush(root);

    // We can't really tell here if it worked, but it hasn't crashed.
    pass("Window can close itself in event handler");
  }

  // Next window sibling unaffected by destruction
  {
    TickitWindow *win1 = tickit_window_new(root, (TickitRect){3, 10, 4, 20}, 0);
    TickitWindow *win2 = tickit_window_new(root, (TickitRect){7, 10, 4, 20}, 0);

    tickit_window_flush(root);

    tickit_window_bind_event(win1, TICKIT_WINDOW_ON_KEY, 0, &on_event_close, win1);
    int count = 0;
    tickit_window_bind_event(win2, TICKIT_WINDOW_ON_KEY, 0, &on_event_incr, &count);

    press_key(TICKIT_KEYEV_TEXT, "A", 0);
    tickit_window_flush(root);

    is_int(count, 1, "Event handler in sibling window is invoked");

    tickit_window_unref(win2);
  }

  // Orphan windows can safely outlive their parent
  {
    TickitWindow *orphanwin = tickit_window_new(root, (TickitRect){10, 1, 2, 2}, 0);
    tickit_window_ref(orphanwin);

    tickit_window_flush(root);

    // DESTROY ROOT
    tickit_window_unref(root);

    tickit_window_unref(orphanwin);
  }

  tickit_term_unref(tt);

  return exit_status();
}
