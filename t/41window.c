#include "tickit.h"
#include "tickit-window.h"
#include "taplib.h"
#include "taplib-mockterm.h"

void on_geom_changed(TickitWindow *window, TickitEventType ev, TickitEventInfo *args, void *data)
{
  (*(int*)data)++;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitWindow *root = tickit_window_new_root(tt);

  TickitWindow *win = tickit_window_new_subwindow(root, 3, 10, 4, 20);
  tickit_window_tick(root);

  // Basics
  {
    is_int(tickit_window_top(win), 3, "tickit_window_top");
    is_int(tickit_window_left(win), 10, "tickit_window_left");

    is_int(tickit_window_abs_top(win), 3, "tickit_window_abs_top");
    is_int(tickit_window_abs_left(win), 10, "tickit_window_abs_left");

    is_int(tickit_window_lines(win), 4, "tickit_window_lines");
    is_int(tickit_window_cols(win), 20, "tickit_window_cols");

    is_int(tickit_window_bottom(win), 7, "tickit_window_bottom");
    is_int(tickit_window_right(win), 30, "tickit_window_right");

    ok(tickit_window_parent(win) == root, "tickit_window_parent");
    ok(tickit_window_root(win) == root, "tickit_window_root");
  }

  // Geometry change event
  {
    int geom_changed = 0;
    tickit_window_bind_event(win, TICKIT_EV_GEOMCHANGE, on_geom_changed, &geom_changed);
    is_int(geom_changed, 0, "geometry not yet changed");

    tickit_window_resize(win, 4, 15);

    is_int(tickit_window_lines(win), 4, "resize tickit_window_lines after resize");
    is_int(tickit_window_cols(win), 15, "resize tickit_window_cols after resize");

    is_int(geom_changed, 1, "geometry changed after resize");

    tickit_window_reposition(win, 5, 15);

    is_int(tickit_window_top(win), 5, "tickit_window_top after reposition");
    is_int(tickit_window_left(win), 15, "tickit_window_left after reposition");

    is_int(tickit_window_abs_top(win), 5, "tickit_window_abs_top after reposition");
    is_int(tickit_window_abs_left(win), 15, "tickit_window_abs_left after reposition");

    is_int(geom_changed, 2, "geometry changed after reposition");
  }

  // sub-window nesting
  {
    TickitWindow *subwin = tickit_window_new_subwindow(win, 2, 2, 1, 10);
    tickit_window_tick(root);

    is_int(tickit_window_top(subwin), 2, "nested tickit_window_top");
    is_int(tickit_window_left(subwin), 2, "nested tickit_window_left");

    is_int(tickit_window_abs_top(subwin), 7, "nested tickit_window_abs_top");
    is_int(tickit_window_abs_left(subwin), 17, "nested tickit_window_abs_left");

    is_int(tickit_window_lines(subwin), 1, "nested tickit_window_lines");
    is_int(tickit_window_cols(subwin), 10, "nested tickit_window_cols");

    ok(tickit_window_parent(subwin) == win, "nested tickit_window_parent");
    ok(tickit_window_root(subwin) == root, "nested tickit_window_root");
  }

  return exit_status();
}
