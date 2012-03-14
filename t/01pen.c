#include "tickit.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TickitPen *pen;
  TickitPenAttr attr;

  plan_tests(14);

  pen = tickit_pen_new();

  ok(!!pen, "tickit_pen_new");

  ok(!tickit_pen_has_attr(pen, TICKIT_PEN_BOLD), "pen lacks bold initially");
  is_int(tickit_pen_get_bool_attr(pen, TICKIT_PEN_BOLD), 0, "bold 0 initially");

  tickit_pen_set_bool_attr(pen, TICKIT_PEN_BOLD, 1);

  ok(tickit_pen_has_attr(pen, TICKIT_PEN_BOLD), "pen has bold after set");
  is_int(tickit_pen_get_bool_attr(pen, TICKIT_PEN_BOLD), 1, "bold 1 after set");

  tickit_pen_clear_attr(pen, TICKIT_PEN_BOLD);

  ok(!tickit_pen_has_attr(pen, TICKIT_PEN_BOLD), "pen lacks bold after clear");
  is_int(tickit_pen_get_bool_attr(pen, TICKIT_PEN_BOLD), 0, "bold 0 after clear");

  ok(!tickit_pen_has_attr(pen, TICKIT_PEN_FG), "pen lacks foreground initially");
  is_int(tickit_pen_get_int_attr(pen, TICKIT_PEN_FG), -1, "foreground -1 initially");

  tickit_pen_set_int_attr(pen, TICKIT_PEN_FG, 4);

  ok(tickit_pen_has_attr(pen, TICKIT_PEN_FG), "pen has foreground after set");
  is_int(tickit_pen_get_int_attr(pen, TICKIT_PEN_FG), 4, "foreground 4 after set");

  tickit_pen_clear_attr(pen, TICKIT_PEN_FG);

  ok(!tickit_pen_has_attr(pen, TICKIT_PEN_FG), "pen lacks foreground after clear");
  is_int(tickit_pen_get_int_attr(pen, TICKIT_PEN_FG), -1, "foreground -1 after clear");

  tickit_pen_destroy(pen);

  ok(1, "tickit_pen_destroy");

  return exit_status();
}
