#include "tickit.h"

typedef struct TickitTermDriver TickitTermDriver;

typedef struct {
  void (*destroy)(TickitTermDriver *ttd);
} TickitTermDriverVTable;

struct TickitTermDriver {
  TickitTerm *tt;
  TickitTermDriverVTable *vtable;
};

extern TickitTermDriverVTable xterm_vtable;
