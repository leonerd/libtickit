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

  // Basics
  {
    TickitRect geom = tickit_window_get_geometry(root);
    is_rect(&geom, "0,0+80,25", "root tickit_window_get_geometry");

    is_int(tickit_window_top(root), 0, "root tickit_window_top");
    is_int(tickit_window_left(root), 0, "root tickit_window_left");

    geom = tickit_window_get_abs_geometry(root);
    is_rect(&geom, "0,0+80,25", "root tickit_window_get_abs_geometry");

    is_int(tickit_window_lines(root), 25, "root tickit_window_lines");
    is_int(tickit_window_cols(root), 80, "root tickit_window_cols");

    is_int(tickit_window_bottom(root), 25, "root tickit_window_bottom");
    is_int(tickit_window_right(root), 80, "root tickit_window_right");

    ok(tickit_window_parent(root) == NULL, "root tickit_window_parent");
    ok(tickit_window_root(root) == root, "root tickit_window_root");

    is_int(tickit_window_children(root), 0, "root tickit_window_children");

    ok(tickit_window_get_term(root) == tt, "root tickit_window_get_term");
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
    tickit_window_flush(root);

    is_termlog("Termlog scrolled",
        SETPEN(),
        SCROLLRECT(0,0,25,80, 1,0),
        NULL);

    tickit_window_scrollrect(root, &(TickitRect){ .top = 5, .left = 0, .lines = 10, .cols = 80 },
        3, 0, NULL);
    tickit_window_flush(root);

    is_termlog("Termlog after scrollrect",
        SETPEN(),
        SCROLLRECT(5,0,10,80, 3,0),
        NULL);

    tickit_window_scrollrect(root, &(TickitRect){ .top = 20, .left = 0, .lines = 1, .cols = 80 },
        0, 1, NULL);
    tickit_window_flush(root);

    is_termlog("Termlog after scrollrect rightward",
        SETPEN(),
        SCROLLRECT(20,0,1,80, 0,1),
        NULL);

    tickit_window_scrollrect(root, &(TickitRect){ .top = 21, .left = 10, .lines = 1, .cols = 70 },
        0, -1, NULL);
    tickit_window_flush(root);

    is_termlog("Termlog after scrollrect leftward not fullwidth",
        SETPEN(),
        SCROLLRECT(21,10,1,70, 0,-1),
        NULL);
  }

  // Resize events
  {
    int geom_changed = 0;
    tickit_window_bind_event(root, TICKIT_WINDOW_ON_GEOMCHANGE, 0, &on_event_incr_int, &geom_changed);
    is_int(geom_changed, 0, "geometry not yet changed");

    int nextrect = 0;
    TickitRect rects[4];
    int push_on_expose(TickitWindow *win, TickitEventFlags flags, void *_info, void *data)
    {
      TickitExposeEventInfo *info = _info;
      rects[nextrect++] = info->rect;
      return 1;
    }
    tickit_window_bind_event(root, TICKIT_WINDOW_ON_EXPOSE, 0, &push_on_expose, NULL);

    tickit_mockterm_resize(tt, 30, 100);
    tickit_window_flush(root);

    is_int(tickit_window_lines(root), 30, "root tickit_window_lines after term resize");
    is_int(tickit_window_cols(root), 100, "root tickit_window_cols after term resize");

    is_int(geom_changed, 1, "geometry changed after term resize");

    is_int(nextrect, 2, "two exposed rects after term resize");
    is_rect(rects+0, "80,0..100,25", "exposed rects[0]");
    is_rect(rects+1, "0,25..100,30", "exposed rects[1]");
  }

  // DESTROY
  {
    int destroyed = 0;
    tickit_window_bind_event(root, TICKIT_WINDOW_ON_DESTROY, 0, &on_event_incr_int, &destroyed);

    tickit_window_unref(root);
    ok(destroyed, "TICKIT_WINDOW_ON_DESTROY invoked");
  }

  tickit_term_unref(tt);

  return exit_status();
}
