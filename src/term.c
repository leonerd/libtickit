#include "tickit.h"

#include <stdlib.h>
#include <string.h>

struct TickitTerm {
  TickitTermOutputFunc *outfunc;
  void                 *outfunc_user;
};

TickitTerm *tickit_term_new(void)
{
  TickitTerm *tt = malloc(sizeof(TickitTerm));

  tt->outfunc = NULL;

  return tt;
}

void tickit_term_free(TickitTerm *tt)
{
  free(tt);
}

void tickit_term_set_output_func(TickitTerm *tt, TickitTermOutputFunc *fn, void *user)
{
  tt->outfunc      = fn;
  tt->outfunc_user = user;
}

static void write_str(TickitTerm *tt, const char *str)
{
  if(tt->outfunc) {
    (*tt->outfunc)(tt, str, strlen(str), tt->outfunc_user);
  }
}

void tickit_term_print(TickitTerm *tt, const char *str)
{
  write_str(tt, str);
}
