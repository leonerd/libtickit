#include "tickit.h"
#include "tickit-window.h"
#include "taplib.h"
#include "taplib-mockterm.h"

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitWindow *root = tickit_window_new_root(tt);

  {
    TickitWindow *win = tickit_window_new_subwindow(root, 3, 10, 4, 30);
    tickit_window_tick(root);

    ok(!tickit_window_scroll(win, 1, 0), "window does not support scrolling");

    tickit_window_destroy(win);
    tickit_window_tick(root);
  }

  return exit_status();
}
