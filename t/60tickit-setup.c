#include "tickit.h"
#include "taplib.h"

#include <string.h>

void output(TickitTerm *tt, const char *bytes, size_t len, void *user)
{
  char *buffer = user;
  strncat(buffer, bytes, len);
}

int main(int argc, char *argv[])
{
  char buffer[1024] = { 0 };

  /* Regular setup */
  {
    TickitTerm *tt = tickit_term_build(&(struct TickitTermBuilder){
      .termtype  = "xterm",
      .output_func      = output,
      .output_func_user = buffer,
    });
    Tickit *t = tickit_new_for_term(tt);

    buffer[0] = 0;
    tickit_tick(t, TICKIT_RUN_NOHANG);

    /* This test is somewhat fragile, but there's not a lot we can do about
     * that. It has to be
     */
    is_str_escape(buffer,
        "\e[?1049h"          // ALTSCREEN on
        "\e[?25l"            // CURSORVIS off
        "\e[?1002h\e[?1006h" // MOUSE
        "\e="                // KEYPAD_APP
        "\e[2J",             // clear
        "buffer after setup");

    tickit_unref(t);
  }

  {
    TickitTerm *tt = tickit_term_build(&(struct TickitTermBuilder){
      .termtype  = "xterm",
      .output_func      = output,
      .output_func_user = buffer,
    });
    Tickit *t = tickit_new_for_term(tt);

    tickit_setctl_int(t, TICKIT_CTL_USE_ALTSCREEN, 0);

    buffer[0] = 0;
    tickit_tick(t, TICKIT_RUN_NOHANG);

    /* This test is somewhat fragile, but there's not a lot we can do about
     * that. It has to be
     */
    is_str_escape(buffer,
        // no ALTSCREEN
        "\e[?25l"            // CURSORVIS off
        "\e[?1002h\e[?1006h" // MOUSE
        "\e="                // KEYPAD_APP
        "\e[2J",             // clear
        "buffer after setup");

    tickit_unref(t);
  }

  return exit_status();
}
