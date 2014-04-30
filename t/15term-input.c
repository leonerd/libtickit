#define _XOPEN_SOURCE 500 // usleep

#include "tickit.h"
#include "taplib.h"

#include <string.h>
#include <unistd.h>

TickitKeyEventType keytype;
char               keystr[16];
int                keymod;

void on_key(TickitTerm *tt, TickitEventType ev, TickitEvent *args, void *data)
{
  keytype = args->type;
  strncpy(keystr, args->str, sizeof(keystr)-1); keystr[sizeof(keystr)-1] = 0;
  keymod = args->mod;
}

TickitMouseEventType mousetype;
int mousebutton, mouseline, mousecol;
int mousemod;

void on_mouse(TickitTerm *tt, TickitEventType ev, TickitEvent *args, void *data)
{
  mousetype   = args->type;
  mousebutton = args->button;
  mouseline   = args->line;
  mousecol    = args->col;
  mousemod    = args->mod;
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;

  tt = tickit_term_new_for_termtype("xterm");
  tickit_term_set_utf8(tt, 1);

  ok(tickit_term_get_utf8(tt), "get_utf8 true");

  tickit_term_bind_event(tt, TICKIT_EV_KEY,   on_key,   NULL);
  tickit_term_bind_event(tt, TICKIT_EV_MOUSE, on_mouse, NULL);

  tickit_term_input_push_bytes(tt, "A", 1);

  is_int(keytype, TICKIT_KEYEV_TEXT, "keytype after push_bytes A");
  is_str(keystr,  "A",               "keystr after push_bytes A");
  is_int(keymod,  0,                 "keymod after push_bytes A");

  is_int(tickit_term_input_check_timeout(tt), -1, "term has no timeout after A");

  /* U+0109 - LATIN SMALL LETTER C WITH CIRCUMFLEX
   * UTF-8: 0xc4 0x89
   */
  tickit_term_input_push_bytes(tt, "\xc4\x89", 2);

  is_int(keytype, TICKIT_KEYEV_TEXT, "keytype after push_bytes U+0109");
  is_str(keystr,  "\xc4\x89",        "keystr after push_bytes U+0109");
  is_int(keymod,  0,                 "keymod after push_bytes U+0109");

  tickit_term_input_push_bytes(tt, "\e[A", 3);

  is_int(keytype, TICKIT_KEYEV_KEY, "keytype after push_bytes Up");
  is_str(keystr,  "Up",             "keystr after push_bytes Up");
  is_int(keymod,  0,                 "keymod after push_bytes Up");

  tickit_term_input_push_bytes(tt, "\x01", 1);

  is_int(keytype, TICKIT_KEYEV_KEY, "keytype after push_bytes C-a");
  is_str(keystr,  "C-a",            "keystr after push_bytes C-a");
  is_int(keymod,  TICKIT_MOD_CTRL,  "keymod after push_bytes C-a");

  is_int(tickit_term_input_check_timeout(tt), -1, "term has no timeout after Up");

  tickit_term_input_push_bytes(tt, "\e[M !!", 6);

  is_int(mousetype,   TICKIT_MOUSEEV_PRESS, "mousetype after mouse button press");
  is_int(mousebutton, 1,                    "mousebutton after mouse button press");
  is_int(mouseline,   0,                    "mouseline after mouse button press");
  is_int(mousecol,    0,                    "mousecol after mouse button press");
  is_int(mousemod,    0,                    "mousemod after mouse button press");

  tickit_term_input_push_bytes(tt, "\e[M`!!", 6);

  is_int(mousetype,   TICKIT_MOUSEEV_WHEEL, "mousetype after mouse wheel up");
  is_int(mousebutton, TICKIT_MOUSEWHEEL_UP, "mousebutton after mouse wheel up");
  is_int(mouseline,   0,                    "mouseline after mouse wheel up");
  is_int(mousecol,    0,                    "mousecol after mouse wheel up");
  is_int(mousemod,    0,                    "mousemod after mouse wheel up");

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
