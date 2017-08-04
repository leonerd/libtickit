#include "tickit.h"
#include "taplib.h"

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new();

  ok(!!t, "tickit_new()");

  /* TODO: add some real tests
   * There isn't a lot that can be checked in a unit-test script yet, until
   * more accessors, mutators, and maybe some construction options or a
   * builder happens
   */

  tickit_unref(t);

  return exit_status();
}
