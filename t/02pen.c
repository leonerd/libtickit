#include "tickit.h"
#include "taplib.h"

static int on_event_incr(TickitPen *pen, TickitEventFlags flags, void *_info, void *data) {
  ((int *)data)[0]++;
  return 1;
}

static int arr[2];
static int next_arr = 0;

static int on_event_push(TickitPen *pen, TickitEventFlags flags, void *_info, void *data) {
  arr[next_arr++] = *(int *)data;
  return 1;
}

int main(int argc, char *argv[])
{
  TickitPen *pen, *pen2;

  pen = tickit_pen_new();

  ok(!!pen, "tickit_pen_new");

  int changed = 0;
  tickit_pen_bind_event(pen, TICKIT_PEN_ON_CHANGE, 0, on_event_incr, &changed);

  is_int(tickit_penattr_type(TICKIT_PEN_BOLD), TICKIT_PENTYPE_BOOL, "bold is a boolean attribute");

  is_int(tickit_penattr_lookup("b"), TICKIT_PEN_BOLD, "lookup_attr \"b\" gives bold");
  is_str(tickit_penattr_name(TICKIT_PEN_BOLD), "b", "penattr_name bold gives \"b\"");

  is_int(changed, 0, "change counter 0 initially");

  ok(!tickit_pen_has_attr(pen, TICKIT_PEN_BOLD), "pen lacks bold initially");
  ok(!tickit_pen_nondefault_attr(pen, TICKIT_PEN_BOLD), "pen bold attr is default initially");
  is_int(tickit_pen_get_bool_attr(pen, TICKIT_PEN_BOLD), 0, "bold 0 initially");

  ok(!tickit_pen_is_nonempty(pen), "pen initially empty");
  ok(!tickit_pen_is_nondefault(pen), "pen initially default");

  tickit_pen_set_bool_attr(pen, TICKIT_PEN_BOLD, 1);

  ok(tickit_pen_has_attr(pen, TICKIT_PEN_BOLD), "pen has bold after set");
  ok(tickit_pen_nondefault_attr(pen, TICKIT_PEN_BOLD), "pen bold attr is nondefault after set");
  is_int(tickit_pen_get_bool_attr(pen, TICKIT_PEN_BOLD), 1, "bold 1 after set");

  ok(tickit_pen_is_nonempty(pen), "pen non-empty after set bold on");
  ok(tickit_pen_is_nondefault(pen), "pen non-default after set bold on");

  is_int(changed, 1, "change counter 1 after set bold on");

  tickit_pen_set_bool_attr(pen, TICKIT_PEN_BOLD, 0);

  ok(!tickit_pen_nondefault_attr(pen, TICKIT_PEN_BOLD), "pen bold attr is default after set bold off");

  ok(tickit_pen_is_nonempty(pen), "pen non-empty after set bold off");
  ok(!tickit_pen_is_nondefault(pen), "pen default after set bold off");

  is_int(changed, 2, "change counter 2 after set bold off");

  tickit_pen_clear_attr(pen, TICKIT_PEN_BOLD);

  ok(!tickit_pen_has_attr(pen, TICKIT_PEN_BOLD), "pen lacks bold after clear");
  is_int(tickit_pen_get_bool_attr(pen, TICKIT_PEN_BOLD), 0, "bold 0 after clear");

  is_int(changed, 3, "change counter 3 after clear bold");

  is_int(tickit_penattr_type(TICKIT_PEN_FG), TICKIT_PENTYPE_COLOUR, "foreground is a colour attribute");

  ok(!tickit_pen_has_attr(pen, TICKIT_PEN_FG), "pen lacks foreground initially");
  is_int(tickit_pen_get_colour_attr(pen, TICKIT_PEN_FG), -1, "foreground -1 initially");

  tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, 4);

  ok(tickit_pen_has_attr(pen, TICKIT_PEN_FG), "pen has foreground after set");
  is_int(tickit_pen_get_colour_attr(pen, TICKIT_PEN_FG), 4, "foreground 4 after set");

  ok(tickit_pen_set_colour_attr_desc(pen, TICKIT_PEN_FG, "12"), "pen set foreground '12'");
  is_int(tickit_pen_get_colour_attr(pen, TICKIT_PEN_FG), 12, "foreground 12 after set '12'");

  ok(tickit_pen_set_colour_attr_desc(pen, TICKIT_PEN_FG, "green"), "pen set foreground 'green'");
  is_int(tickit_pen_get_colour_attr(pen, TICKIT_PEN_FG), 2, "foreground 2 after set 'green'");

  ok(tickit_pen_set_colour_attr_desc(pen, TICKIT_PEN_FG, "hi-red"), "pen set foreground 'hi-red'");
  is_int(tickit_pen_get_colour_attr(pen, TICKIT_PEN_FG), 8+1, "foreground 8+1 after set 'hi-red'");

  tickit_pen_clear_attr(pen, TICKIT_PEN_FG);

  ok(!tickit_pen_has_attr(pen, TICKIT_PEN_FG), "pen lacks foreground after clear");
  is_int(tickit_pen_get_colour_attr(pen, TICKIT_PEN_FG), -1, "foreground -1 after clear");

  pen2 = tickit_pen_new();

  ok(tickit_pen_equiv_attr(pen, pen2, TICKIT_PEN_BOLD), "pens have equiv bold attribute initially");

  tickit_pen_set_bool_attr(pen, TICKIT_PEN_BOLD, 1);

  ok(!tickit_pen_equiv_attr(pen, pen2, TICKIT_PEN_BOLD), "pens have unequiv bold attribute after set");

  ok(tickit_pen_equiv_attr(pen, pen2, TICKIT_PEN_ITALIC), "pens have equiv italic attribute");

  tickit_pen_set_bool_attr(pen, TICKIT_PEN_ITALIC, 0);
  ok(tickit_pen_equiv_attr(pen, pen2, TICKIT_PEN_ITALIC), "pens have equiv italic attribute after set 0");

  tickit_pen_copy_attr(pen2, pen, TICKIT_PEN_BOLD);
  ok(tickit_pen_equiv_attr(pen, pen2, TICKIT_PEN_BOLD), "pens have equiv bold attribute after copy attr");

  tickit_pen_clear_attr(pen2, TICKIT_PEN_BOLD);
  tickit_pen_copy(pen2, pen, 1);

  ok(tickit_pen_equiv_attr(pen, pen2, TICKIT_PEN_BOLD), "pens have equiv bold attribute after copy");

  tickit_pen_set_bool_attr(pen2, TICKIT_PEN_BOLD, 0);
  tickit_pen_copy(pen2, pen, 0);

  ok(!tickit_pen_equiv_attr(pen, pen2, TICKIT_PEN_BOLD), "pens have non-equiv bold attribute after copy no overwrite");

  tickit_pen_set_int_attr(pen, TICKIT_PEN_UNDER, TICKIT_PEN_UNDER_NONE);
  tickit_pen_clear_attr(pen2, TICKIT_PEN_UNDER);
  tickit_pen_copy(pen2, pen, 1);

  ok(tickit_pen_has_attr(pen2, TICKIT_PEN_UNDER), "pen copy still copies present but default-value attributes");

  is_ptr(tickit_pen_ref(pen), pen, "tickit_pen_ref() returns same pen");

  tickit_pen_bind_event(pen, TICKIT_PEN_ON_DESTROY, 0, on_event_push, (int []){1});
  tickit_pen_bind_event(pen, TICKIT_PEN_ON_DESTROY, 0, on_event_push, (int []){2});

  tickit_pen_unref(pen);
  ok(!next_arr, "pen not destroyed after first unref");

  tickit_pen_unref(pen);
  is_int(next_arr, 2, "pen destroyed after second unref");

  is_int(arr[0]*10 + arr[1], 21, "TICKIT_EV_DESTROY runs in reverse order");

  tickit_pen_unref(pen2);

  pen = tickit_pen_new_attrs(TICKIT_PEN_BOLD, 1, TICKIT_PEN_FG, 3, 0);
  is_int(tickit_pen_get_bool_attr(pen, TICKIT_PEN_BOLD), 1, "pen bold attr for new_attrs");
  is_int(tickit_pen_get_colour_attr(pen, TICKIT_PEN_FG), 3, "pen fg attr for new_attrs");

  tickit_pen_unref(pen);

  // UNDER bool back-compat
  {
    TickitPen *pen = tickit_pen_new();

    ok(!tickit_pen_has_attr(pen, TICKIT_PEN_UNDER), "default pen has no UNDER");
    is_int(tickit_pen_get_int_attr(pen, TICKIT_PEN_UNDER), TICKIT_PEN_UNDER_NONE,
        "default pen UNDER is 0");
    ok(!tickit_pen_get_bool_attr(pen, TICKIT_PEN_UNDER), "default pen has no UNDER bool");

    tickit_pen_set_int_attr(pen, TICKIT_PEN_UNDER, TICKIT_PEN_UNDER_DOUBLE);

    ok(tickit_pen_has_attr(pen, TICKIT_PEN_UNDER), "pen has UNDER");
    is_int(tickit_pen_get_int_attr(pen, TICKIT_PEN_UNDER), TICKIT_PEN_UNDER_DOUBLE,
        "pen UNDER is DOUBLE");
    ok(tickit_pen_get_bool_attr(pen, TICKIT_PEN_UNDER), "pen has UNDER bool");

    tickit_pen_unref(pen);
  }

  // RGB8 secondary colour attributes
  {
    TickitPen *pen = tickit_pen_new();
    TickitPenRGB8 val;

    ok(!tickit_pen_has_colour_attr_rgb8(pen, TICKIT_PEN_FG),
        "new pen does not have FG RGB8");

    tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, 15);
    ok(!tickit_pen_has_colour_attr_rgb8(pen, TICKIT_PEN_FG),
        "pen still does not have FG RGB after set index");

    tickit_pen_set_colour_attr_rgb8(pen, TICKIT_PEN_FG, (TickitPenRGB8){230, 240, 250});
    ok(tickit_pen_has_colour_attr_rgb8(pen, TICKIT_PEN_FG),
        "pen now has FG RGB8 after set RGB8");

    val = tickit_pen_get_colour_attr_rgb8(pen, TICKIT_PEN_FG);
    is_int(val.r, 230, "val.r from get RGB8");
    is_int(val.g, 240, "val.g from get RGB8");
    is_int(val.b, 250, "val.b from get RGB8");

    {
      TickitPen *pen2 = tickit_pen_clone(pen);
      ok(tickit_pen_has_colour_attr_rgb8(pen2, TICKIT_PEN_FG),
          "tickit_pen_clone preserves RGB8 attributes");

      ok(tickit_pen_equiv(pen, pen2), "cloned pen is equivalent");

      tickit_pen_clear_attr(pen2, TICKIT_PEN_FG);
      tickit_pen_set_colour_attr(pen2, TICKIT_PEN_FG, 15);
      ok(!tickit_pen_equiv(pen, pen2), "pen not equivalent after clearing RGB8");

      tickit_pen_set_colour_attr(pen, TICKIT_PEN_FG, 20);
      ok(!tickit_pen_has_colour_attr_rgb8(pen, TICKIT_PEN_FG),
          "pen does not have FG RGB after set new index");

      tickit_pen_set_colour_attr_rgb8(pen, TICKIT_PEN_BG, (TickitPenRGB8){10, 20, 30});
      ok(!tickit_pen_has_colour_attr_rgb8(pen, TICKIT_PEN_BG),
          "pen does not have BG RGB8 with no index");

      tickit_pen_unref(pen2);
    }

    tickit_pen_set_colour_attr_desc(pen, TICKIT_PEN_FG, "red #F01010");
    is_int(tickit_pen_get_colour_attr(pen, TICKIT_PEN_FG), 1, "index from set_desc with RGB");
    val = tickit_pen_get_colour_attr_rgb8(pen, TICKIT_PEN_FG);
    is_int(val.r, 240, "val.r from get RGB8 for desc RGB");
    is_int(val.g,  16, "val.g from get RGB8 for desc RGB");
    is_int(val.b,  16, "val.b from get RGB8 for desc RGB");

    tickit_pen_unref(pen);

    pen = tickit_pen_new_attrs(TICKIT_PEN_FG_DESC, "green#20FF20", 0);
    is_int(tickit_pen_get_colour_attr(pen, TICKIT_PEN_FG), 2, "index from new FG_DESC");
    val = tickit_pen_get_colour_attr_rgb8(pen, TICKIT_PEN_FG);
    is_int(val.r,  32, "val.r from get RGB8 for new FG_DESC");
    is_int(val.g, 255, "val.g from get RGB8 for new FG_DESC");
    is_int(val.b,  32, "val.b from get RGB8 for new FG_DESC");
  }

  return exit_status();
}
