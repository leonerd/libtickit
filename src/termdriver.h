#include "tickit.h"
#include "tickit-termdrv.h"

typedef struct {
  const char *termtype;
  const char *(*ti_getstr_hook)(const char *name, const char *value, void *data);
  void *ti_getstr_data;
} TickitTermProbeArgs;

typedef struct {
  TickitTermDriver *(*new)(const TickitTermProbeArgs *args);
} TickitTermDriverProbe;

extern TickitTermDriverProbe tickit_termdrv_probe_xterm;
extern TickitTermDriverProbe tickit_termdrv_probe_ti;
