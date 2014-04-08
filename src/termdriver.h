#include "tickit.h"
#include "tickit-termdrv.h"

typedef struct {
  TickitTermDriver *(*new)(const char *termtype);
} TickitTermDriverProbe;

extern TickitTermDriverProbe tickit_termdrv_probe_xterm;
extern TickitTermDriverProbe tickit_termdrv_probe_ti;
