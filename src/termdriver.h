#include "tickit.h"
#include "tickit-termdrv.h"

typedef struct {
  const char *termtype;
} TickitTermProbeArgs;

typedef struct {
  TickitTermDriver *(*new)(const TickitTermProbeArgs *args);
} TickitTermDriverProbe;

extern TickitTermDriverProbe tickit_termdrv_probe_xterm;
extern TickitTermDriverProbe tickit_termdrv_probe_ti;
