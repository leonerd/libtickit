#include "tickit.h"
#include "taplib.h"
#include "taplib-tickit.h"
#include "taplib-mockterm.h"

int on_event_incr_int(TickitWindow *window, TickitEventFlags flags, void *_info, void *data)
{
  (*(int*)data)++;
  return 1;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitWindow *root = tickit_window_new_root(tt);

  TickitWindow *win = tickit_window_new(root, (TickitRect){3, 10, 4, 20}, 0);
  tickit_window_flush(root);

  // Basics
  {
    TickitRect geom = tickit_window_get_geometry(win);
    is_rect(&geom, "10,3+20,4", "tickit_window_get_geometry");

    is_int(tickit_window_top(win), 3, "tickit_window_top");
    is_int(tickit_window_left(win), 10, "tickit_window_left");

    geom = tickit_window_get_abs_geometry(win);
    is_rect(&geom, "10,3+20,4", "tickit_window_get_abs_geometry");

    is_int(tickit_window_lines(win), 4, "tickit_window_lines");
    is_int(tickit_window_cols(win), 20, "tickit_window_cols");

    is_int(tickit_window_bottom(win), 7, "tickit_window_bottom");
    is_int(tickit_window_right(win), 30, "tickit_window_right");

    ok(tickit_window_parent(win) == root, "tickit_window_parent");
    ok(tickit_window_root(win) == root, "tickit_window_root");

    is_int(tickit_window_children(win), 0, "tickit_window_children");

    is_int(tickit_window_children(root), 1, "root tickit_window_children");
    TickitWindow *children[1] = {0};
    tickit_window_get_children(root, children, 1);
    is_ptr(children[0], win, "root tickit_window_get_children result[0]");

    ok(tickit_window_get_term(win) == tt, "tickit_window_get_term");
  }

  // Geometry change event
  {
    int geom_changed = 0;
    tickit_window_bind_event(win, TICKIT_WINDOW_ON_GEOMCHANGE, 0, on_event_incr_int, &geom_changed);
    is_int(geom_changed, 0, "geometry not yet changed");

    tickit_window_resize(win, 4, 15);

    TickitRect geom = tickit_window_get_geometry(win);
    is_rect(&geom, "10,3+15,4", "tickit_window_get_geometry after resize");

    is_int(tickit_window_lines(win), 4, "resize tickit_window_lines after resize");
    is_int(tickit_window_cols(win), 15, "resize tickit_window_cols after resize");

    is_int(geom_changed, 1, "geometry changed after resize");

    tickit_window_reposition(win, 5, 15);

    geom = tickit_window_get_geometry(win);
    is_rect(&geom, "15,5+15,4", "tickit_window_get_geometry after reposition");

    is_int(tickit_window_top(win), 5, "tickit_window_top after reposition");
    is_int(tickit_window_left(win), 15, "tickit_window_left after reposition");

    geom = tickit_window_get_abs_geometry(win);
    is_rect(&geom, "15,5+15,4", "tickit_window_get_abs_geometry after reposition");

    is_int(geom_changed, 2, "geometry changed after reposition");
  }

  // sub-window nesting
  {
    TickitWindow *subwin = tickit_window_new(win, (TickitRect){2, 2, 1, 10}, 0);
    tickit_window_flush(root);

    TickitRect geom = tickit_window_get_geometry(subwin);
    is_rect(&geom, "2,2+10,1", "nested tickit_window_get_geometry");

    is_int(tickit_window_top(subwin), 2, "nested tickit_window_top");
    is_int(tickit_window_left(subwin), 2, "nested tickit_window_left");

    geom = tickit_window_get_abs_geometry(subwin);
    is_rect(&geom, "17,7+10,1", "nested tickit_window_get_abs_geometry");

    is_int(tickit_window_lines(subwin), 1, "nested tickit_window_lines");
    is_int(tickit_window_cols(subwin), 10, "nested tickit_window_cols");

    ok(tickit_window_parent(subwin) == win, "nested tickit_window_parent");
    ok(tickit_window_root(subwin) == root, "nested tickit_window_root");

    tickit_window_unref(subwin);
    tickit_window_flush(root);
  }

  // initially-hidden
  {
    TickitWindow *subwin = tickit_window_new(win, (TickitRect){4, 4, 2, 2}, TICKIT_WINDOW_HIDDEN);
    tickit_window_flush(root);

    ok(!tickit_window_is_visible(subwin), "initially-hidden window not yet visible");

    tickit_window_show(subwin);
    ok(tickit_window_is_visible(subwin), "initially-hidden window visible after show");

    tickit_window_unref(subwin);
    tickit_window_flush(root);
  }

  // TICKIT_WINDOW_ON_DESTROY
  {
    int destroyed = 0;
    tickit_window_bind_event(win, TICKIT_WINDOW_ON_DESTROY, 0, &on_event_incr_int, &destroyed);

    tickit_window_unref(win);
    ok(destroyed, "TICKIT_WINDOW_ON_DESTROY invoked");
  }

  // explicit close
  {
    TickitWindow *win = tickit_window_new(root, (TickitRect){1, 1, 4, 4}, 0);
    is_int(tickit_window_children(root), 1, "root window has 1 child before close()");

    int destroyed = 0;
    tickit_window_bind_event(win, TICKIT_WINDOW_ON_DESTROY, 0, &on_event_incr_int, &destroyed);

    tickit_window_close(win);

    is_int(tickit_window_children(root), 0, "root window has 1 child after close()");
    ok(!destroyed, "window not destroyed before unref");

    // calling close a second time doesn't crash
    tickit_window_close(win);

    tickit_window_unref(win);
    ok(destroyed, "window not destroyed after unref");
  }

  tickit_window_unref(root);
  tickit_term_unref(tt);

  return exit_status();
}
