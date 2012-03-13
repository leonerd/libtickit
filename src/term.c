#include "tickit.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct TickitTerm {
  int                   outfd;
  TickitTermOutputFunc *outfunc;
  void                 *outfunc_user;
};

TickitTerm *tickit_term_new(void)
{
  TickitTerm *tt = malloc(sizeof(TickitTerm));

  tt->outfd   = -1;
  tt->outfunc = NULL;

  return tt;
}

void tickit_term_free(TickitTerm *tt)
{
  free(tt);
}

void tickit_term_set_output_fd(TickitTerm *tt, int fd)
{
  tt->outfd = fd;

  tt->outfunc = NULL;
}

void tickit_term_set_output_func(TickitTerm *tt, TickitTermOutputFunc *fn, void *user)
{
  tt->outfunc      = fn;
  tt->outfunc_user = user;

  tt->outfd = -1;
}

static void write_str(TickitTerm *tt, const char *str)
{
  if(tt->outfunc) {
    (*tt->outfunc)(tt, str, strlen(str), tt->outfunc_user);
  }
  else if(tt->outfd != -1) {
    write(tt->outfd, str, strlen(str));
  }
}

void tickit_term_print(TickitTerm *tt, const char *str)
{
  write_str(tt, str);
}
