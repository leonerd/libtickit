#ifndef __TAPLIB_TICKIT_H__
#define __TAPLIB_TICKIT_H__

#include "tickit.h"

TickitRect *rect_init_strp(TickitRect *rect, const char *str);

void is_rect(TickitRect *got, const char *expect, char *name);

#endif
