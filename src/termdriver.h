#include "tickit.h"
#include "tickit-termdrv.h"

/* TiciktTermCtl also has "driver-private" ranges, implemented by the various
 * drivers. To avoid too much tight coupling between here and the drivers we
 * allocate private range numbers to the specific drivers
 */
enum {
  TICKIT_TERMCTL_PRIVATE_TERMINFO = 0x1000,
  TICKIT_TERMCTL_PRIVATE_XTERM    = 0x2000,

  TICKIT_TERMCTL_PRIVATEMASK = 0xF000,
};

typedef struct {
  const char *termtype;
  const struct TickitTerminfoHook *ti_hook;
} TickitTermProbeArgs;

typedef struct {
  const char *name;
  TickitTermDriver *(*new)(const TickitTermProbeArgs *args);
  int privatectl;
  const char *(*ctlname)(TickitTermCtl ctl);
  TickitType (*ctltype)(TickitTermCtl ctl);
} TickitTermDriverInfo;

extern TickitTermDriverInfo tickit_termdrv_info_xterm;
extern TickitTermDriverInfo tickit_termdrv_info_ti;
