#include "tickit.h"
#include "taplib.h"

#include <string.h>

TickitKeyEventType keytype;
char               keystr[16];

void on_key(TickitTerm *tt, TickitEventType ev, TickitEvent *args, void *data)
{
  keytype = args->keytype;
  strncpy(keystr, args->str, sizeof(keystr)-1); keystr[sizeof(keystr)-1] = 0;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;

  plan_tests(16);

  tt = tickit_term_new_for_termtype("xterm");

  tickit_term_bind_event(tt, TICKIT_EV_KEY, on_key, NULL);

  tickit_term_input_push_bytes(tt, "A", 1);

  is_int(keytype, TICKIT_KEYEV_TEXT, "keytype after push_bytes A");
  is_str(keystr,  "A",               "keystr after push_bytes A");

  is_int(tickit_term_input_check_timeout(tt), -1, "term has no timeout after A");

  tickit_term_input_push_bytes(tt, "\e[A", 3);

  is_int(keytype, TICKIT_KEYEV_KEY, "keytype after push_bytes Up");
  is_str(keystr,  "Up",             "keystr after push_bytes Up");

  is_int(tickit_term_input_check_timeout(tt), -1, "term has no timeout after Up");

  keytype = -1; keystr[0] = 0;
  tickit_term_input_push_bytes(tt, "\e[", 2);

  is_int(keytype, -1, "keytype not set after push_bytes partial Down");

  ok(tickit_term_input_check_timeout(tt) > 0, "term has timeout after partial Down");

  tickit_term_input_push_bytes(tt, "B", 1);

  is_int(keytype, TICKIT_KEYEV_KEY, "keytype after push_bytes completed Down");
  is_str(keystr,  "Down",           "keystr after push_bytes completed Down");

  is_int(tickit_term_input_check_timeout(tt), -1, "term has no timeout after completed Down");

  keytype = -1; keystr[0] = 0;
  tickit_term_input_push_bytes(tt, "\e", 1);

  is_int(keytype, -1, "keytype not set after push_bytes Escape");

  int timeout_msec = tickit_term_input_check_timeout(tt);
  ok(timeout_msec > 0, "term has timeout after Escape");

  /* Add an extra milisecond timing grace */
  usleep((timeout_msec+1) * 1000);

  tickit_term_input_check_timeout(tt);

  is_int(keytype, TICKIT_KEYEV_KEY, "keytype after push_bytes completed Escape");
  is_str(keystr,  "Escape",         "keystr after push_bytes completed Escape");

  is_int(tickit_term_input_check_timeout(tt), -1, "term has no timeout after completed Escape");

  tickit_term_destroy(tt);

  return exit_status();
}
