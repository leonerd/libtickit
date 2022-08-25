#include "tickit.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void printctl(TickitTerm *tt, const char *name, int ctl)
{
  switch(tickit_termctl_type(ctl)) {
    case TICKIT_TYPE_NONE:
      break;

    case TICKIT_TYPE_BOOL:
    {
      int b;
      tickit_term_getctl_int(tt, ctl, &b);
      printf("  %-26s: %s\n", name, b ? "true" : "false");
      break;
    }

    case TICKIT_TYPE_INT:
    {
      int i;
      tickit_term_getctl_int(tt, ctl, &i);
      printf("  %-26s: %d\n", name, i);
      break;
    }

    // currently there are no controls of type STR or COLOUR
    case TICKIT_TYPE_STR:
    case TICKIT_TYPE_COLOUR:
      break;
  }
}

int main(int argc, char *argv[])
{
  Tickit *t = tickit_new_stdtty();

  TickitTerm *tt = tickit_get_term(t);
  if(!tt) {
    fprintf(stderr, "Cannot create TickitTerm - %s\n", strerror(errno));
    return 1;
  }

  tickit_term_await_started_msec(tt, 100);

  // First the common controls
  printf("Common:\n");
  for(int ctl = 1; ; ctl++) {
    const char *name = tickit_termctl_name(ctl);
    if(!name)
      break;

    printctl(tt, name, ctl);
  }

  // Now the custom ones
  printf("Driver %s\n", tickit_term_get_drivername(tt));
  int range = tickit_term_get_driverctl_range(tt);
  for(int ctl = range+1; range; ctl++) {
    const char *name = tickit_termctl_name(ctl);
    if(!name)
      break;

    printctl(tt, name, ctl);
  }

  tickit_unref(t);

  return 0;
}
