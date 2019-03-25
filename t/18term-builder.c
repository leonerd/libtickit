#include "tickit.h"
#include "taplib.h"

#include <string.h>

#define streq(a,b) (!strcmp(a,b))

static const char *ti_getstr(const char *name, const char *value, void *user)
{
  if(streq(name, "cursor_address"))
    return "GOTO(%p1%d,%p2%d)";

  return value;
}

static void output(TickitTerm *tt, const char *bytes, size_t len, void *user)
{
  char *buffer = user;
  strncat(buffer, bytes, len);
}

int main(int argc, char *argv[])
{
  // termtype
  {
    struct TickitTermBuilder builder = {
      .termtype = "linux",
    };
    TickitTerm *tt = tickit_term_build(&builder);

    is_str(tickit_term_get_termtype(tt), "linux", "termtype for built term");

    tickit_term_unref(tt);
  }

  // getstr override
  {
    struct TickitTermBuilder builder = {
      .termtype = "linux",
      .ti_hook = &(const struct TickitTerminfoHook){ .getstr = ti_getstr },
    };
    TickitTerm *tt = tickit_term_build(&builder);
    char buffer[1024] = { 0 };

    tickit_term_set_output_func(tt, output, buffer);
    tickit_term_set_output_buffer(tt, 4096);

    buffer[0] = 0;

    tickit_term_goto(tt, 4, 6);
    tickit_term_flush(tt);

    is_str_escape(buffer, "GOTO(4,6)", "buffer after goto for built term");

    tickit_term_unref(tt);
  }

  return exit_status();
}
