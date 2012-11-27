#include "tickit.h"

typedef struct TickitTermDriver TickitTermDriver;

typedef struct {
  void (*destroy)(TickitTermDriver *ttd);
  void (*start)(TickitTermDriver *ttd);
  void (*stop)(TickitTermDriver *ttd);
  void (*print)(TickitTermDriver *ttd, const char *str);
  void (*goto_abs)(TickitTermDriver *ttd, int line, int col);
  void (*move_rel)(TickitTermDriver *ttd, int downward, int rightward);
  int  (*scrollrect)(TickitTermDriver *ttd, int top, int left, int lines, int cols, int downward, int rightward);
  void (*erasech)(TickitTermDriver *ttd, int count, int moveend);
  void (*clear)(TickitTermDriver *ttd);
  void (*chpen)(TickitTermDriver *ttd, const TickitPen *delta, const TickitPen *final);
  int  (*setctl_int)(TickitTermDriver *ttd, TickitTermCtl ctl, int value);
} TickitTermDriverVTable;

struct TickitTermDriver {
  TickitTerm *tt;
  TickitTermDriverVTable *vtable;
};

void *tickit_termdrv_get_tmpbuffer(TickitTermDriver *ttd, size_t len);
void tickit_termdrv_write_str(TickitTermDriver *ttd, const char *str, size_t len);
void tickit_termdrv_write_strf(TickitTermDriver *ttd, const char *fmt, ...);
TickitPen *tickit_termdrv_current_pen(TickitTermDriver *ttd);

typedef struct {
  TickitTermDriver *(*new)(TickitTerm *tt, const char *termtype);
} TickitTermDriverProbe;

extern TickitTermDriverProbe xterm_probe;
