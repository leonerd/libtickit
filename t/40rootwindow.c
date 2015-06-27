#include "tickit.h"
#include "tickit-window.h"
#include "taplib.h"
#include "taplib-mockterm.h"

int on_geom_changed(TickitWindow *window, TickitEventType ev, void *_info, void *data)
{
  (*(int*)data)++;
  return 1;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitWindow *root = tickit_window_new_root(tt);

  // Basics
  {
    is_int(tickit_window_top(root), 0, "root tickit_window_top");
    is_int(tickit_window_left(root), 0, "root tickit_window_left");

    is_int(tickit_window_abs_top(root), 0, "root tickit_window_abs_top");
    is_int(tickit_window_abs_left(root), 0, "root tickit_window_abs_left");

    is_int(tickit_window_lines(root), 25, "root tickit_window_lines");
    is_int(tickit_window_cols(root), 80, "root tickit_window_cols");

    is_int(tickit_window_bottom(root), 25, "root tickit_window_bottom");
    is_int(tickit_window_right(root), 80, "root tickit_window_right");

    ok(tickit_window_parent(root) == NULL, "root tickit_window_parent");
    ok(tickit_window_root(root) == root, "root tickit_window_root");
  }

  // Window pen
  {
    TickitPen *pen = tickit_window_get_pen(root);

    ok(!!pen, "window pen");
    ok(!tickit_pen_is_nonempty(pen), "pen has no attrs set");

    // TODO: effective pen?
  }

  // Scrolling
  {
    ok(tickit_window_scroll(root, 1, 0), "window can scroll");
    tickit_window_tick(root);

    is_termlog("Termlog scrolled",
        SETPEN(),
        SCROLLRECT(0,0,25,80, 1,0),
        NULL);

    tickit_window_scrollrect(root, &(TickitRect){ .top = 5, .left = 0, .lines = 10, .cols = 80 },
        3, 0, NULL);
    tickit_window_tick(root);

    is_termlog("Termlog after scrollrect",
        SETPEN(),
        SCROLLRECT(5,0,10,80, 3,0),
        NULL);

    tickit_window_scrollrect(root, &(TickitRect){ .top = 20, .left = 0, .lines = 1, .cols = 80 },
        0, 1, NULL);
    tickit_window_tick(root);

    is_termlog("Termlog after scrollrect rightward",
        SETPEN(),
        SCROLLRECT(20,0,1,80, 0,1),
        NULL);

    tickit_window_scrollrect(root, &(TickitRect){ .top = 21, .left = 10, .lines = 1, .cols = 70 },
        0, -1, NULL);
    tickit_window_tick(root);

    is_termlog("Termlog after scrollrect leftward not fullwidth",
        SETPEN(),
        SCROLLRECT(21,10,1,70, 0,-1),
        NULL);
  }

  // Geometry change event
  {
    int geom_changed = 0;
    tickit_window_bind_event(root, TICKIT_EV_GEOMCHANGE, &on_geom_changed, &geom_changed);
    is_int(geom_changed, 0, "geometry not yet changed");

    tickit_mockterm_resize(tt, 30, 100);

    is_int(tickit_window_lines(root), 30, "root tickit_window_lines after term resize");
    is_int(tickit_window_cols(root), 100, "root tickit_window_cols after term resize");

    is_int(geom_changed, 1, "geometry changed after term resize");
  }

  return exit_status();
}
