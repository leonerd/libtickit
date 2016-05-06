#include "tickit.h"
#include "taplib.h"
#include "taplib-tickit.h"
#include "taplib-mockterm.h"

static int next_rect = 0;
static TickitRect exposed_rects[16];

int on_expose_pushrect(TickitWindow *win, TickitEventType ev, void *_info, void *data)
{
  TickitExposeEventInfo *info = _info;

  if(next_rect >= sizeof(exposed_rects)/sizeof(exposed_rects[0]))
    return 0;

  exposed_rects[next_rect++] = info->rect;
  return 1;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitWindow *root = tickit_window_new_root(tt);

  {
    TickitWindow *win = tickit_window_new(root, (TickitRect){3, 10, 4, 30}, 0);
    tickit_window_flush(root);

    ok(!tickit_window_scroll(win, 1, 0), "window does not support scrolling");
    drain_termlog();

    tickit_window_destroy(win);
    tickit_window_flush(root);
  }

  // Scrollable window probably needs to be fullwidth
  TickitWindow *win = tickit_window_new(root, (TickitRect){5, 0, 10, 80}, 0);
  tickit_window_flush(root);

  int bind_id = tickit_window_bind_event(win, TICKIT_EV_EXPOSE, 0, &on_expose_pushrect, NULL);

  // scroll down
  {
    next_rect = 0;
    ok(tickit_window_scroll(win, 1, 0), "fullwidth window supports vertical scrolling");
    tickit_window_flush(root);

    is_termlog("Termlog after fullwidth scroll downward",
        SETPEN(),
        SCROLLRECT(5,0,10,80, 1,0),
        NULL);

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects+0, "0,9+80,1", "exposed_rects[0]");
  }

  // scroll up
  {
    next_rect = 0;
    tickit_window_scroll(win, -1, 0);
    tickit_window_flush(root);

    is_termlog("Termlog after fullwidth scroll upward",
        SETPEN(),
        SCROLLRECT(5,0,10,80, -1,0),
        NULL);

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects+0, "0,0+80,1", "exposed_rects[0]");
  }

  // scroll right
  {
    next_rect = 0;
    ok(tickit_window_scroll(win, 0, 1), "fullwidth window supports horizontal scrolling");
    tickit_window_flush(root);

    is_termlog("Termlog after fullwidth scroll rightward",
        SETPEN(),
        SCROLLRECT(5,0,10,80, 0,1),
        NULL);

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects+0, "79,0+1,10", "exposed_rects[0]");
  }

  // scroll left
  {
    next_rect = 0;
    tickit_window_scroll(win, 0, -1);
    tickit_window_flush(root);

    is_termlog("Termlog after fullwidth scroll leftward",
        SETPEN(),
        SCROLLRECT(5,0,10,80, 0,-1),
        NULL);

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects+0, "0,0+1,10", "exposed_rects[0]");
  }

  // scrollrect up
  {
    next_rect = 0;
    ok(tickit_window_scrollrect(win, &(TickitRect){ .top = 2, .left = 0, .lines = 3, .cols = 80 },
          -1, 0, NULL),
        "Fullwidth window supports scrolling a region");
    tickit_window_flush(root);

    is_termlog("Termlog after fullwidth scrollrect downward",
        SETPEN(),
        SCROLLRECT(7,0,3,80, -1,0),
        NULL);

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects+0, "0,2+80,1", "exposed_rects[0]");
  }

  // scrollrect down
  {
    next_rect = 0;
    tickit_window_scrollrect(win, &(TickitRect){ .top = 2, .left = 0, .lines = 3, .cols = 80 },
        1, 0, NULL);
    tickit_window_flush(root);

    is_termlog("Termlog after fullwidth scrollrect upward",
        SETPEN(),
        SCROLLRECT(7,0,3,80, 1,0),
        NULL);

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects+0, "0,4+80,1", "exposed_rects[0]");
  }

  // scrollrect further than area just exposes
  {
    next_rect = 0;
    tickit_window_scrollrect(win, &(TickitRect){ .top = 2, .left = 0, .lines = 3, .cols = 80 },
        5, 0, NULL);
    tickit_window_flush(root);

    is_termlog("Termlog empty after scrollrect further than area",
        NULL);

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects+0, "0,2+80,3", "exposed_rects[0]");
  }

  // scrollrect with pending damage
  {
    // outside
    tickit_window_expose(win, &(TickitRect){ .top = 1, .left =  4, .lines = 2, .cols = 10 });

    // split
    tickit_window_expose(win, &(TickitRect){ .top = 3, .left = 10, .lines = 3, .cols = 5 });

    // inside
    tickit_window_expose(win, &(TickitRect){ .top = 5, .left = 20, .lines = 2, .cols = 10 });

    next_rect = 0;
    tickit_window_scrollrect(win, &(TickitRect){ .top = 4, .left = 0, .lines = 6, .cols = 80 },
        2, 0, NULL);
    tickit_window_flush(root);

    is_termlog("Termlog after scrollrect with impending damage",
        SETPEN(),
        SCROLLRECT(9,0,6,80, 2,0),
        NULL);

    is_int(next_rect, 4, "pushed 4 exposed rects");
    // outside
    is_rect(exposed_rects+0, "4,1+10,2", "exposed_rects[0]");
    // split part outside
    is_rect(exposed_rects+1, "10,3+5,1", "exposed_rects[1]");
    // translated+truncated inside
    is_rect(exposed_rects+2, "20,4+10,1", "exposed_rects[2]");
    // exposed
    is_rect(exposed_rects+3, "0,8+80,2", "exposed_rects[3]");
  }

  // Child to obscure part of it
  {
    TickitWindow *child = tickit_window_new(win, (TickitRect){0, 0, 3, 10}, 0);
    tickit_window_flush(root);

    next_rect = 0;
    tickit_window_scroll(win, 0, 4);
    tickit_window_flush(root);

    is_termlog("Termlog after scroll with obscuring child",
        SETPEN(),
        SCROLLRECT(5,10,3,70, 0,4),
        SCROLLRECT(8, 0,7,80, 0,4),
        NULL);

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects+0, "76,0+4,10", "exposed_rects[0]");

    tickit_window_hide(child);
    tickit_window_flush(root);

    next_rect = 0;
    tickit_window_scroll(win, 0, 4);
    tickit_window_flush(root);

    is_termlog("Termlog after scroll with hidden obscuring child",
        SETPEN(),
        SCROLLRECT(5,0,10,80, 0,4),
        NULL);

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects+0, "76,0+4,10", "exposed_rects[0]");

    tickit_window_destroy(child);
    tickit_window_flush(root);
  }

  // scroll_with_children
  {
    TickitWindow *child = tickit_window_new(win, (TickitRect){0, 70, 1, 10}, 0);
    tickit_window_flush(root);

    next_rect = 0;
    tickit_window_scroll_with_children(win, -2, 0);
    tickit_window_flush(root);

    is_termlog("Termlog after scroll_with_children",
        SETPEN(),
        SCROLLRECT(5,0,10,80, -2,0),
        NULL);

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects+0, "0,0+80,2", "exposed_rects[0]");

    tickit_window_destroy(child);
    tickit_window_flush(root);
  }

  // sibling to obscure part of it
  {
    TickitWindow *sibling = tickit_window_new(root, (TickitRect){0, 0, 10, 20}, 0);
    tickit_window_flush(root);

    tickit_window_raise(sibling);
    tickit_window_flush(root);

    next_rect = 0;
    tickit_window_scroll_with_children(win, -2, 0);
    tickit_window_flush(root);

    is_termlog("Termlog after scroll_with_children with sibling",
        SETPEN(),
        SCROLLRECT(10,0,5,80, -2,0),
        NULL);

    is_int(next_rect, 2, "pushed 2 exposed rects");
    is_rect(exposed_rects+0, "20,0+60,5", "exposed_rects[0]");
    is_rect(exposed_rects+1, "0,5+80,2", "exposed_rects[1]");

    tickit_window_hide(sibling);
    tickit_window_flush(root);

    next_rect = 0;
    tickit_window_scroll_with_children(win, -2, 0);
    tickit_window_flush(root);

    is_termlog("Termlog after scroll_with_children with hidden sibling",
        SETPEN(),
        SCROLLRECT(5,0,10,80, -2,0),
        NULL);

    is_int(next_rect, 1, "pushed 1 exposed rect");
    is_rect(exposed_rects+0, "0,0+80,2", "exposed_rects[0]");

    tickit_window_destroy(sibling);
    tickit_window_flush(root);
  }

  // Hidden windows should be ignored
  {
    next_rect = 0;
    tickit_window_hide(win);
    tickit_window_flush(root);

    tickit_window_scroll(win, 2, 0);
    tickit_window_flush(root);

    is_termlog("Termlog empty after scroll on hidden window",
        NULL);
  }

  tickit_window_destroy(root);
  tickit_term_destroy(tt);

  return exit_status();
}
