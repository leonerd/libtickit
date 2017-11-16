#include "tickit.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TickitString *s;

  s = tickit_string_new("Hello, world!", 13);
  ok(!!s, "tickit_string_new");

  is_str(tickit_string_get(s), "Hello, world!", "tickit_string_get");
  is_int(tickit_string_len(s), 13, "tickit_string_len");

  {
    is_ptr(tickit_string_ref(s), s, "tickit_string_ref");

    tickit_string_unref(s);
    is_str(tickit_string_get(s), "Hello, world!", "tickit_string_get after tickit_string_unref");
  }

  tickit_string_unref(s);

  return exit_status();
}
