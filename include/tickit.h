#ifndef __TICKIT_H__
#define __TICKIT_H__

#include <stdlib.h>

typedef struct TickitTerm TickitTerm;
typedef void TickitTermOutputFunc(TickitTerm *tt, const char *bytes, size_t len, void *user);

TickitTerm *tickit_term_new(void);
void tickit_term_free(TickitTerm *tt);

void tickit_term_set_output_func(TickitTerm *tt, TickitTermOutputFunc *fn, void *user);

void tickit_term_print(TickitTerm *tt, const char *str);

#endif
