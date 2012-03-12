#include "tickit.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  TickitTerm *tt;

  plan_tests(2);

  tt = tickit_term_new();

  ok(!!tt, "tickit_term_new");

  tickit_term_free(tt);

  ok(1, "tickit_term_free");

  return exit_status();
}
