#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TICKIT_MOCKTERM_H__
#define __TICKIT_MOCKTERM_H__

/*
 * The contents of this file should be considered entirely experimental, and
 * subject to any change at any time. We make no API or ABI stability
 * guarantees at this time.
 */

#include "tickit.h"

typedef struct
{
  enum {
    LOG_GOTO = 1,
    LOG_PRINT,
    LOG_ERASECH,
    LOG_CLEAR,
    LOG_SCROLLRECT,
    LOG_SETPEN,
  } type;
  int val1, val2;  // GOTO(line, col); ERASECH(count, moveend); SCROLLRECT(downward,rightward)
  const char *str; // PRINT
  TickitRect rect; // SCROLLRECT
  TickitPen *pen;  // SETPEN
} TickitMockTermLogEntry;

/* A TickitMockTerm really is a TickitTerm */
typedef TickitTerm TickitMockTerm;

TickitMockTerm *tickit_mockterm_new(int lines, int cols);
void tickit_mockterm_destroy(TickitMockTerm *mt);

void tickit_mockterm_resize(TickitMockTerm *mt, int newlines, int newcols);

size_t tickit_mockterm_get_display_text(TickitMockTerm *mt, char *buffer, size_t len, int line, int col, int width);
TickitPen *tickit_mockterm_get_display_pen(TickitMockTerm *mt, int line, int col);

int tickit_mockterm_loglen(TickitMockTerm *mt);
TickitMockTermLogEntry *tickit_mockterm_peeklog(TickitMockTerm *mt, int i);
void tickit_mockterm_clearlog(TickitMockTerm *mt);

void tickit_mockterm_get_position(TickitMockTerm *mt, int *line, int *col);

#endif

#ifdef __cplusplus
}
#endif
