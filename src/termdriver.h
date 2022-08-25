#include "tickit.h"
#include "tickit-termdrv.h"

typedef struct {
  const char *termtype;
  const struct TickitTerminfoHook *ti_hook;
} TickitTermProbeArgs;

typedef struct {
  TickitTermDriver *(*new)(const TickitTermProbeArgs *args);
} TickitTermDriverInfo;

extern TickitTermDriverInfo tickit_termdrv_info_xterm;
extern TickitTermDriverInfo tickit_termdrv_info_ti;
