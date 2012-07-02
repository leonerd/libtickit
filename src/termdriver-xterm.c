#include "termdriver.h"

#include <stdlib.h>

static void destroy(TickitTermDriver *ttd)
{
  free(ttd);
}

TickitTermDriverVTable xterm_vtable = {
  .destroy = destroy,
};
