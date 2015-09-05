#define _XOPEN_SOURCE 500  // strdup

#include "tickit.h"
#include "taplib.h"

#include <stdlib.h>
#include <string.h>

char *msg = NULL;
void savemsg(const char *str, void *data)
{
  if(msg)
    free(msg);

  msg = strdup(str);
}

int main(int argc, char *argv[])
{
  putenv("TICKIT_DEBUG_FLAGS=T,X");
  tickit_debug_set_func(savemsg, NULL);

  tickit_debug_logf("T", "Test message %s", "here");

  ok(!!msg, "tickit_debug_logf saves a message");
  // First 12 characters are timestamp
  is_str(msg+12, " [T  ]: Test message here\n", "message string");

  free(msg); msg = NULL;
  tickit_debug_logf("A", "Another message");

  ok(!msg, "tickit_debug_logf does not save a message with nonmatching flag");

  return exit_status();
}
