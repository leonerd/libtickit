#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TICKIT_TERMDRV_H__
#define __TICKIT_TERMDRV_H__

/*
 * The contents of this file should be considered entirely experimental, and
 * subject to any change at any time. We make no API or ABI stability
 * guarantees at this time.
 */

#include "tickit.h"

typedef struct {
  void (*attach)(TickitTermDriver *ttd, TickitTerm *tt); /* optional */
  void (*destroy)(TickitTermDriver *ttd);
  void (*start)(TickitTermDriver *ttd); /* optional */
  bool (*started)(TickitTermDriver *ttd); /* optional */
  void (*stop)(TickitTermDriver *ttd); /* optional */
  void (*pause)(TickitTermDriver *ttd); /* optional */
  void (*resume)(TickitTermDriver *ttd); /* optional */
  bool (*print)(TickitTermDriver *ttd, const char *str, size_t len);
  bool (*goto_abs)(TickitTermDriver *ttd, int line, int col);
  bool (*move_rel)(TickitTermDriver *ttd, int downward, int rightward);
  bool (*scrollrect)(TickitTermDriver *ttd, const TickitRect *rect, int downward, int rightward);
  bool (*erasech)(TickitTermDriver *ttd, int count, TickitMaybeBool moveend);
  bool (*clear)(TickitTermDriver *ttd);
  bool (*chpen)(TickitTermDriver *ttd, const TickitPen *delta, const TickitPen *final);
  bool (*getctl_int)(TickitTermDriver *ttd, TickitTermCtl ctl, int *value);
  bool (*setctl_int)(TickitTermDriver *ttd, TickitTermCtl ctl, int value);
  bool (*setctl_str)(TickitTermDriver *ttd, TickitTermCtl ctl, const char *value);

  /* optional */
  int  (*on_modereport)(TickitTermDriver *ttd, int initial, int mode, int value);
  int  (*on_decrqss)(TickitTermDriver *ttd, const char *args, size_t arglen);
} TickitTermDriverVTable;

struct TickitTermDriver {
  TickitTerm *tt;
  TickitTermDriverVTable *vtable;
  const char *name;
};

void *tickit_termdrv_get_tmpbuffer(TickitTermDriver *ttd, size_t len);
void tickit_termdrv_write_str(TickitTermDriver *ttd, const char *str, size_t len);
void tickit_termdrv_write_strf(TickitTermDriver *ttd, const char *fmt, ...);
TickitPen *tickit_termdrv_current_pen(TickitTermDriver *ttd);

/*
 * Intended for "subclass" terminal methods, to obtain their custom driver
 * when given a TT instance
 */
TickitTermDriver *tickit_term_get_driver(TickitTerm *tt);

#endif

#ifdef __cplusplus
}
#endif
