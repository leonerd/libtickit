#include "tickit.h"
#include "taplib.h"
#include "taplib-tickit.h"

#include <stdio.h>
#include <string.h>

int rects_init_strp(TickitRect *rects, int n, const char *str)
{
  int i = 0;
  while(str && *str && i < n) {
    rect_init_strp(rects + i, str);
    i++;

    str = strchr(str, ' ');
    if(!str) break;

    while(*str == ' ')
      str++;
  }

  return i;
}

void test_add(const char *input, const char *output)
{
  TickitRectSet *trs = tickit_rectset_new();

  TickitRect inputs[4];
  int n_inputs = rects_init_strp(inputs, 4, input);

  TickitRect exp_outputs[4];
  int n_exp_outputs = rects_init_strp(exp_outputs, 4, output);

  int reverse;
  for(reverse = 0; reverse < 2; reverse++) {
    tickit_rectset_clear(trs);

    int i;
    for(i = 0; i < n_inputs; i++)
      tickit_rectset_add(trs, inputs + (reverse ? n_inputs - i - 1 : i));

    int n_got_outputs = tickit_rectset_rects(trs);
    TickitRect got_outputs[n_got_outputs];
    tickit_rectset_get_rects(trs, got_outputs, n_got_outputs);

    for(i = 0; i < n_exp_outputs; i++) {
      if(i >= n_got_outputs) {
        diag("Received too many rects");
        goto fail_dir;
      }

      TickitRect *got = got_outputs + i;
      TickitRect *exp = exp_outputs + i;

      if(got->top   != exp->top   ||
         got->left  != exp->left  ||
         got->lines != exp->lines ||
         got->cols  != exp->cols) {
        diag("got %d,%d..%d,%d expected %d,%d..%d,%d",
            got->left, got->top, tickit_rect_right(got), tickit_rect_bottom(got),
            exp->left, exp->top, tickit_rect_right(exp), tickit_rect_bottom(exp));
        goto fail_dir;
      }
    }

    if(i < n_got_outputs) {
      diag("Received insufficient rects");
      goto fail_dir;
    }

    pass(reverse ? "tickit_rectset_add reversed" : "tickit_rectset_add");
    continue;

fail_dir:
    fail(reverse ? "tickit_rectset_add reversed" : "tickit_rectset_add");
  }

  tickit_rectset_destroy(trs);
}

void test_subtract(const char *input, const char *output)
{
  TickitRectSet *trs = tickit_rectset_new();

  TickitRect inputs[4];
  int n_inputs = rects_init_strp(inputs, 4, input);

  tickit_rectset_add(trs, inputs + 0);

  int i;
  for(i = 1; i < n_inputs; i++)
    tickit_rectset_subtract(trs, inputs + i);

  TickitRect exp_outputs[4];
  int n_exp_outputs = rects_init_strp(exp_outputs, 4, output);

  int n_got_outputs = tickit_rectset_rects(trs);
  TickitRect got_outputs[n_got_outputs];
  tickit_rectset_get_rects(trs, got_outputs, n_got_outputs);

  for(i = 0; i < n_exp_outputs; i++) {
    if(i >= n_got_outputs) {
      diag("Received too many rects");
      goto fail;
    }

    TickitRect *got = got_outputs + i;
    TickitRect *exp = exp_outputs + i;

    if(got->top   != exp->top   ||
       got->left  != exp->left  ||
       got->lines != exp->lines ||
       got->cols  != exp->cols) {
      diag("got %d,%d..%d,%d expected %d,%d..%d,%d",
          got->left, got->top, tickit_rect_right(got), tickit_rect_bottom(got),
          exp->left, exp->top, tickit_rect_right(exp), tickit_rect_bottom(exp));
      goto fail;
    }
  }

  if(i < n_got_outputs) {
    diag("Received insufficient rects");
    goto fail;
  }

  pass("tickit_rectset_subtract");
  tickit_rectset_destroy(trs);
  return;

fail:
  fail("tickit_rectset_subtract");
  tickit_rectset_destroy(trs);
}

int main(int argc, char *argv[])
{
  {
    TickitRectSet *trs = tickit_rectset_new();
    TickitRect tmp;
    TickitRect rects[4];

    ok(!!trs, "tickit_rectset_new");

    is_int(tickit_rectset_rects(trs), 0, "tickit_rectset_rects initially");

    is_int(tickit_rectset_get_rects(trs, rects, 4), 0, "tickit_rectset_get_rects initially");

    // Distinct regions
    tickit_rectset_add(trs, rect_init_strp(&tmp, "10,10+20,5"));

    is_int(tickit_rectset_get_rect(trs, 0, &tmp), 1, "tickit_rectset_get_rect after tickit_rectset_Add");
    is_rect(&tmp, "10,10+20,5", "tmp after tickit_rectset_get_rect");

    is_int(tickit_rectset_get_rects(trs, rects, 4), 1, "tickit_rectset_get_rects after tickit_rectset_add");
    is_rect(rects + 0, "10,10+20,5", "rects[0] after tickit_rectset_add");

    tickit_rectset_add(trs, rect_init_strp(&tmp, "10,20+20,2"));

    is_int(tickit_rectset_get_rects(trs, rects, 4), 2, "tickit_rectset_get_rects after second tickit_rectset_add");
    is_rect(rects + 0, "10,10+20,5", "rects[0] after second tickit_rectset_add");
    is_rect(rects + 1, "10,20+20,2", "rects[1] after second tickit_rectset_add");

    // Translation
    tickit_rectset_translate(trs, 2, 3);

    is_int(tickit_rectset_get_rects(trs, rects, 4), 2, "tickit_rectset_get_rects after second tickit_rectset_translate");
    is_rect(rects + 0, "13,12+20,5", "rects[0] after second tickit_rectset_translate");
    is_rect(rects + 1, "13,22+20,2", "rects[1] after second tickit_rectset_translate");

    tickit_rectset_clear(trs);

    is_int(tickit_rectset_rects(trs), 0, "tickit_rectset_rects after clear");

    // Intersect and containment
    tickit_rectset_add(trs, rect_init_strp(&tmp, "1,1..20,5"));
    tickit_rectset_add(trs, rect_init_strp(&tmp, "1,5..10,10"));

    ok( tickit_rectset_intersects(trs, rect_init_strp(&tmp, "0,0..5,5")), "intersects overlap");
    ok(!tickit_rectset_intersects(trs, rect_init_strp(&tmp, "15,6..25,9")), "doesn't intersect no overlap");

    ok( tickit_rectset_contains(trs, rect_init_strp(&tmp, "5,1..15,4")), "contains simple");
    ok( tickit_rectset_contains(trs, rect_init_strp(&tmp, "5,2..8,9")), "contains split");
    ok(!tickit_rectset_contains(trs, rect_init_strp(&tmp, "5,2..12,9")), "doesn't contain split");
    ok(!tickit_rectset_contains(trs, rect_init_strp(&tmp, "15,6..25,9")), "doesn't contain non-intersect");

    tickit_rectset_destroy(trs);
  }

  // Distinct regions
  test_add("10,10..30,15 40,10..60,15", "10,10..30,15 40,10..60,15");
  test_add("10,10..30,15 10,20..30,25", "10,10..30,15 10,20..30,25");

  // Ignorable regions
  test_add("10,10..30,15 10,10..30,15", "10,10..30,15");
  test_add("10,10..30,15 10,10..20,12", "10,10..30,15");
  test_add("10,10..30,15 20,13..30,15", "10,10..30,15");
  test_add("10,10..30,15 15,11..25,14", "10,10..30,15");

  // Overlapping extension top
  test_add("10,10..30,15 10,8..30,12", "10,8..30,15");
  test_add("10,10..30,15 10,8..30,10", "10,8..30,15");
  test_add("10,10..30,12 10,15..30,17 10,12..30,15", "10,10..30,17");

  // Overlapping extension bottom
  test_add("10,10..30,15 10,12..30,17", "10,10..30,17");
  test_add("10,10..30,15 10,15..30,17", "10,10..30,17");

  // Overlapping extension left
  test_add("10,10..30,15 5,10..25,15", "5,10..30,15");
  test_add("10,10..30,15 5,10..10,15", "5,10..30,15");

  // Overlapping extension right
  test_add("10,10..30,15 20,10..35,15", "10,10..35,15");
  test_add("10,10..30,15 30,10..35,15", "10,10..35,15");

  // L/T shape top abutting
  test_add("10,10..30,15 10,8..20,10", "10,8..20,10 10,10..30,15");
  test_add("10,10..30,15 15,8..25,10", "15,8..25,10 10,10..30,15");
  test_add("10,10..30,15 20,8..30,10", "20,8..30,10 10,10..30,15");

  // L/T shape top overlapping
  test_add("10,10..30,15 10,8..20,12", "10,8..20,10 10,10..30,15");
  test_add("10,10..30,15 15,8..25,12", "15,8..25,10 10,10..30,15");
  test_add("10,10..30,15 20,8..30,12", "20,8..30,10 10,10..30,15");

  // L/T shape bottom abutting
  test_add("10,10..30,15 10,15..20,17", "10,10..30,15 10,15..20,17");
  test_add("10,10..30,15 15,15..25,17", "10,10..30,15 15,15..25,17");
  test_add("10,10..30,15 20,15..30,17", "10,10..30,15 20,15..30,17");

  // L/T shape bottom overlapping
  test_add("10,10..30,15 10,13..20,17", "10,10..30,15 10,15..20,17");
  test_add("10,10..30,15 15,13..25,17", "10,10..30,15 15,15..25,17");
  test_add("10,10..30,15 20,13..30,17", "10,10..30,15 20,15..30,17");

  // L/T shape left abutting
  test_add("10,10..30,15 5,10..10,12", "5,10..30,12 10,12..30,15");
  test_add("10,10..30,15 5,11..10,14", "10,10..30,11 5,11..30,14 10,14..30,15");
  test_add("10,10..30,15 5,13..10,15", "10,10..30,13 5,13..30,15");

  // L/T shape left overlapping
  test_add("10,10..30,15 5,10..15,12", "5,10..30,12 10,12..30,15");
  test_add("10,10..30,15 5,11..15,14", "10,10..30,11 5,11..30,14 10,14..30,15");
  test_add("10,10..30,15 5,13..15,15", "10,10..30,13 5,13..30,15");

  // L/T shape right abutting
  test_add("10,10..30,15 30,10..35,12", "10,10..35,12 10,12..30,15");
  test_add("10,10..30,15 30,11..35,14", "10,10..30,11 10,11..35,14 10,14..30,15");
  test_add("10,10..30,15 30,13..35,15", "10,10..30,13 10,13..35,15");

  // L/T shape right overlapping
  test_add("10,10..30,15 20,10..35,12", "10,10..35,12 10,12..30,15");
  test_add("10,10..30,15 20,11..35,14", "10,10..30,11 10,11..35,14 10,14..30,15");
  test_add("10,10..30,15 20,13..35,15", "10,10..30,13 10,13..35,15");

  // Cross shape
  test_add("10,10..30,15 15,5..25,20", "15,5..25,10 10,10..30,15 15,15..25,20");

  // Diagonal overlap
  test_add("10,10..30,15 20,12..40,20", "10,10..30,12 10,12..40,15 20,15..40,20");
  test_add("10,10..30,15  0,12..20,20", "10,10..30,12  0,12..30,15  0,15..20,20");

  // Distinct regions
  test_subtract("10,10..30,15 10,20..30,22", "10,10..30,15");

  // Overlapping truncate left
  test_subtract("10,10..30,15 5,10..20,15", "20,10..30,15");

  // Overlapping truncate right
  test_subtract("10,10..30,15 20,10..35,15", "10,10..20,15");

  // Overlapping truncate top
  test_subtract("10,10..30,15 10,8..30,12", "10,12..30,15");

  // Overlapping truncate bottom
  test_subtract("10,10..30,15 10,13..30,18", "10,10..30,13");

  // Overlapping U left
  test_subtract("10,10..30,15 5,11..20,14", "10,10..30,11 20,11..30,14 10,14..30,15");

  // Overlapping U right
  test_subtract("10,10..30,15 20,11..35,14", "10,10..30,11 10,11..20,14 10,14..30,15");

  // Overlapping U top
  test_subtract("10,10..30,15 15,8..25,12", "10,10..15,12 25,10..30,12 10,12..30,15");

  // Overlapping U bottom
  test_subtract("10,10..30,15 15,13..25,18", "10,10..30,13 10,13..15,15 25,13..30,15");

  // Remove entirely
  test_subtract("10,10..30,15 8,8..32,17", "");

  return exit_status();
}
