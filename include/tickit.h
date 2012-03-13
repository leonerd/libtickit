#ifndef __TICKIT_H__
#define __TICKIT_H__

#include <stdlib.h>

typedef struct TickitTerm TickitTerm;
typedef void TickitTermOutputFunc(TickitTerm *tt, const char *bytes, size_t len, void *user);

TickitTerm *tickit_term_new(void);
void tickit_term_free(TickitTerm *tt);
void tickit_term_destroy(TickitTerm *tt);

void tickit_term_set_output_fd(TickitTerm *tt, int fd);
void tickit_term_set_output_func(TickitTerm *tt, TickitTermOutputFunc *fn, void *user);

void tickit_term_print(TickitTerm *tt, const char *str);
void tickit_term_goto(TickitTerm *tt, int line, int col);
void tickit_term_move(TickitTerm *tt, int downward, int rightward);

void tickit_term_clear(TickitTerm *tt);
void tickit_term_insertch(TickitTerm *tt, int count);
void tickit_term_deletech(TickitTerm *tt, int count);

void tickit_term_set_mode_altscreen(TickitTerm *tt, int on);
void tickit_term_set_mode_cursorvis(TickitTerm *tt, int on);
void tickit_term_set_mode_mouse(TickitTerm *tt, int on);

#endif
