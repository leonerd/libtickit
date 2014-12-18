#include "tickit.h"
#include "tickit-termdrv.h"
#include "taplib.h"

#include <string.h>

static void print(TickitTermDriver *ttd, const char *str, size_t len)
{
  tickit_termdrv_write_strf(ttd, "PRINT(%.*s)", len, str);
}

static int getctl_int(TickitTermDriver *ttd, TickitTermCtl ctl, int *value)
{
  switch(ctl) {
    case TICKIT_TERMCTL_COLORS:
      *value = 8; return 1;
  }

  return 0;
}

static bool setctl_int(TickitTermDriver *ttd, TickitTermCtl ctl, int value)
{
  return false;
}

static bool setctl_str(TickitTermDriver *ttd, TickitTermCtl ctl, const char *value)
{
  return false;
}

static TickitTermDriverVTable vtable = {
  .destroy    = (void (*)(TickitTermDriver *))free,
  .print      = print,
  /* Technically these are not optional but the test doesn't use them
  .goto_abs   = goto_abs,
  .move_rel   = move_rel,
  .scrollrect = scrollrect,
  .erasech    = erasech,
  .clear      = clear,
  .chpen      = chpen,
  */

  .getctl_int = getctl_int,
  .setctl_int = setctl_int,
  .setctl_str = setctl_str,
};

void output(TickitTerm *tt, const char *bytes, size_t len, void *user)
{
  char *buffer = user;
  strncat(buffer, bytes, len);
}

int main(int argc, char *argv[])
{
  TickitTerm *tt;
  char   buffer[1024] = { 0 };

  TickitTermDriver *ttd = malloc(sizeof(TickitTermDriver));
  ttd->vtable = &vtable;

  tt = tickit_term_new_for_driver(ttd);

  ok(!!tt, "tickit_term_new_for_driver");

  tickit_term_set_output_func(tt, output, buffer);
  tickit_term_set_output_buffer(tt, 4096);

  buffer[0] = 0;

  tickit_term_print(tt, "Hello");
  tickit_term_flush(tt);

  is_str(buffer, "PRINT(Hello)", "buffer after print");

  return exit_status();
}
