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
#include <termkey.h>

typedef struct TickitTermDriver TickitTermDriver;

typedef struct {
  void (*attach)(TickitTermDriver *ttd, TickitTerm *tt); /* optional */
  void (*destroy)(TickitTermDriver *ttd);
  void (*start)(TickitTermDriver *ttd); /* optional */
  int  (*started)(TickitTermDriver *ttd); /* optional */
  void (*stop)(TickitTermDriver *ttd); /* optional */
  void (*print)(TickitTermDriver *ttd, const char *str, size_t len);
  int  (*goto_abs)(TickitTermDriver *ttd, int line, int col);
  void (*move_rel)(TickitTermDriver *ttd, int downward, int rightward);
  int  (*scrollrect)(TickitTermDriver *ttd, const TickitRect *rect, int downward, int rightward);
  void (*erasech)(TickitTermDriver *ttd, int count, int moveend);
  void (*clear)(TickitTermDriver *ttd);
  void (*chpen)(TickitTermDriver *ttd, const TickitPen *delta, const TickitPen *final);
  int  (*getctl_int)(TickitTermDriver *ttd, TickitTermCtl ctl, int *value);
  int  (*setctl_int)(TickitTermDriver *ttd, TickitTermCtl ctl, int value);
  int  (*setctl_str)(TickitTermDriver *ttd, TickitTermCtl ctl, const char *value);
  int  (*gotkey)(TickitTermDriver *ttd, TermKey *tk, const TermKeyKey *key); /* optional */
} TickitTermDriverVTable;

struct TickitTermDriver {
  TickitTerm *tt;
  TickitTermDriverVTable *vtable;
};

void *tickit_termdrv_get_tmpbuffer(TickitTermDriver *ttd, size_t len);
void tickit_termdrv_write_str(TickitTermDriver *ttd, const char *str, size_t len);
void tickit_termdrv_write_strf(TickitTermDriver *ttd, const char *fmt, ...);
TickitPen *tickit_termdrv_current_pen(TickitTermDriver *ttd);

/*
 * Function to construct a new TickitTerm directly from a TickitTermDriver
 */
TickitTerm *tickit_term_new_for_driver(TickitTermDriver *ttd);

/*
 * Intended for "subclass" terminal methods, to obtain their custom driver
 * when given a TT instance
 */
TickitTermDriver *tickit_term_get_driver(TickitTerm *tt);

#endif

#ifdef __cplusplus
}
#endif
