#include "tickit.h"
#include "taplib.h"

int new_lines, new_cols;
int unbound = 0;

int on_resize(TickitTerm *tt, TickitEventFlags flags, void *_info, void *data)
{
  if(flags & TICKIT_EV_UNBIND) {
    unbound = 1;
    return 1;
  }

  TickitResizeEventInfo *info = _info;

  is_int(flags, TICKIT_EV_FIRE, "flags to on_resize()");

  new_lines = info->lines;
  new_cols  = info->cols;
  return 1;
}

int new_lines2, new_cols2;

int on_resize2(TickitTerm *tt, TickitEventFlags flags, void *_info, void *data)
{
  TickitResizeEventInfo *info = _info;

  is_int(flags, TICKIT_EV_FIRE, "flags to on_resize2()");

  new_lines2 = info->lines;
  new_cols2  = info->cols;

  return 1;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;

  tt = tickit_term_build(&(struct TickitTermBuilder){
    .termtype  = "xterm",
  });
  tickit_term_set_size(tt, 25, 80);

  int bindid = tickit_term_bind_event(tt, TICKIT_TERM_ON_RESIZE, TICKIT_BIND_UNBIND, on_resize, NULL);

  tickit_term_set_size(tt, 30, 100);

  is_int(new_lines,  30, "new_lines from event handler after set_size");
  is_int(new_cols,  100, "new_cols from event handler after set_size");

  tickit_term_bind_event(tt, TICKIT_TERM_ON_RESIZE, 0, on_resize2, NULL);

  tickit_term_set_size(tt, 35, 110);

  is_int(new_lines,   35, "new_lines from event handler after set_size");
  is_int(new_cols,   110, "new_cols from event handler after set_size");
  is_int(new_lines2,  35, "new_lines from event handler 2 after set_size");
  is_int(new_cols2,  110, "new_cols from event handler 2 after set_size");

  tickit_term_unbind_event_id(tt, bindid);

  tickit_term_set_size(tt, 40, 120);

  is_int(new_lines, 35, "new_lines still 35 after unbind event");

  tickit_term_unref(tt);

  is_int(unbound, 1, "on_resize unbound after tickit_term_unref");

  return exit_status();
}
