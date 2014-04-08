#include "tickit.h"
#include "tickit-termdrv.h"

typedef struct {
  TickitTermDriver *(*new)(TickitTerm *tt, const char *termtype);
} TickitTermDriverProbe;

extern TickitTermDriverProbe tickit_termdrv_probe_xterm;
extern TickitTermDriverProbe tickit_termdrv_probe_ti;
