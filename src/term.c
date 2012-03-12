#include "tickit.h"

#include <stdlib.h>

struct TickitTerm {
  void *todo;
};

TickitTerm *tickit_term_new(void)
{
  TickitTerm *tt = malloc(sizeof(TickitTerm));

  return tt;
}

void tickit_term_free(TickitTerm *tt)
{
  free(tt);
}
