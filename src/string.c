#include "tickit.h"

#include <string.h>

struct TickitString {
  int refcount;
  size_t len;
  char str[0];
};

TickitString *tickit_string_new(const char *str, size_t len)
{
  TickitString *s = malloc(sizeof(TickitString) + len + 1);

  s->refcount = 1;
  s->len = len;
  memcpy(s->str, str, len);
  s->str[len] = '\0';

  return s;
}

TickitString *tickit_string_ref(TickitString *s)
{
  s->refcount++;
  return s;
}

void tickit_string_unref(TickitString *s)
{
  if(s->refcount > 1) {
    s->refcount--;
    return;
  }

  s->refcount = 0;
  s->str[0] = '\0';
  free(s);
}

const char *tickit_string_get(const TickitString *s)
{
  return s->str;
}

size_t tickit_string_len(const TickitString *s)
{
  return s->len;
}
