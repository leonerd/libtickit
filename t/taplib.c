#include "taplib.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static int nexttest = 1;
static int _exit_status = 0;

void plan_tests(int n)
{
  printf("1..%d\n", n);
}

void ok(int cmp, char *name)
{
  printf("%s %d - %s\n", cmp ? "ok" : "not ok", nexttest++, name);
  if(!cmp)
    _exit_status = 1;
}

void diag(char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  fprintf(stderr, "# ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");

  va_end(args);
}

void is_int(int got, int expect, char *name)
{
  if(got == expect)
    ok(1, name);
  else {
    ok(0, name);
    diag("got %d expected %d", got, expect);
  }
}

void is_str(const char *got, const char *expect, char *name)
{
  if(strcmp(got, expect) == 0)
    ok(1, name);
  else {
    ok(0, name);
    diag("got '%s' expected '%s'", got, expect);
  }
}

static const char *strescape(const char *s)
{
  size_t len = 0;
  char *ret;
  char *q;

  for(const char *p = s; p[0]; p++)
    switch(p[0]) {
      case '\n':
      case '\r':
      case '\x1b':
      case '\\':
        len += 2;
        break;
      default:
        len += 1;
    }

  ret = malloc(len + 1);

  q = ret;
  for(const char *p = s; p[0]; p++)
    switch(p[0]) {
      case '\n':
        q[0] = '\\'; q[1] = 'n'; q += 2; break;
      case '\r':
        q[0] = '\\'; q[1] = 'r'; q += 2; break;
      case '\x1b':
        q[0] = '\\'; q[1] = 'e'; q += 2; break;
      case '\\':
        q[0] = '\\'; q[1] = '\\'; q += 2; break;
      default:
        q[0] = p[0]; q += 1; break;
    }

  q[0] = 0;

  return ret;
}

void is_str_escape(const char *got, const char *expect, char *name)
{
  if(strcmp(got, expect) == 0)
    ok(1, name);
  else {
    const char *got_e    = strescape(got);
    const char *expect_e = strescape(expect);
    ok(0, name);
    diag("got \"%s\" expected \"%s\"", got_e, expect_e);
    free(got_e);
    free(expect_e);
  }
}

int exit_status(void)
{
  return _exit_status;
}
