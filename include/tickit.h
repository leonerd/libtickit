#ifndef __TICKIT_H__
#define __TICKIT_H__

#include <stdlib.h>

typedef struct TickitTerm TickitTerm;

TickitTerm *tickit_term_new(void);
void tickit_term_free(TickitTerm *tt);

#endif
