#define _XOPEN_SOURCE  // fdopen

#include <tickit.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define streq(a,b) (!strcmp(a,b))

static void (*debug_func)(const char *str, void *data);
static void *debug_func_data;

static FILE *debug_fh;

struct Flag {
  struct Flag *next;
  char *name;
};

static struct Flag *enabled_flags;

static bool init_done = false;
void tickit_debug_init(void)
{
  if(init_done)
    return;

  const char *flags_str = getenv("TICKIT_DEBUG_FLAGS");

  while(flags_str) {
    const char *endp = strchr(flags_str, ',');
    if(!endp)
      endp = flags_str + strlen(flags_str);

    struct Flag *newflag = malloc(sizeof *newflag);
    newflag->name = malloc(endp - flags_str + 1);
    strncpy(newflag->name, flags_str, endp - flags_str);

    newflag->next = enabled_flags;
    enabled_flags = newflag;

    flags_str = endp;
    if(flags_str[0] == ',')
      flags_str++;
    else
      break;
  }

  const char *val;
  if((val = getenv("TICKIT_DEBUG_FD")) && val[0]) {
    int fd;
    if(sscanf(val, "%d", &fd))
      tickit_debug_set_fh(fdopen(fd, "a"));
  }
  else if((val = getenv("TICKIT_DEBUG_FILE")) && val[0]) {
    tickit_debug_open(val);
  }
  // TODO: else if(enabled_flags) open autogen filename

  init_done = true;
}

static bool flag_enabled(const char *name)
{
  for(struct Flag *f = enabled_flags; f; f = f->next) {
    if(f->name[0] == '*')
      return true;

    if(name[0] != f->name[0])
      continue;

    if(!f->name[1])
      return true;
    if(streq(name+1, f->name+1))
      return true;
  }

  return false;
}

void tickit_debug_set_func(void (*func)(const char *str, void *data), void *data)
{
  debug_func      = func;
  debug_func_data = data;

  if(debug_fh)
    fclose(debug_fh);
}

void tickit_debug_set_fh(FILE *fh)
{
  if(debug_fh)
    fclose(debug_fh);

  debug_fh = fh;

  if(debug_func)
    debug_func = NULL;
}

bool tickit_debug_open(const char *path)
{
  FILE *fh = fopen(path, "a");
  if(!fh)
    return false;

  tickit_debug_set_fh(fh);
  return true;
}

void tickit_debug_logf(const char *flag, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  tickit_debug_vlogf(flag, fmt, args);

  va_end(args);
}

void tickit_debug_vlogf(const char *flag, const char *fmt, va_list args)
{
  if(!init_done)
    tickit_debug_init();

  if(!debug_fh && !debug_func)
    return;
  if(!flag_enabled(flag))
    return;

  struct timeval now;
  gettimeofday(&now, NULL);

  char timestamp[9];
  strftime(timestamp, sizeof timestamp, "%H:%M:%S", localtime(&now.tv_sec));

#define LINE_PREFIX "%s.%03d [%-3s]: "

  if(debug_func) {
    size_t len;
    va_list args_copy;
    va_copy(args_copy, args);
    len = snprintf(NULL, 0, LINE_PREFIX, timestamp, 0, flag) +
      vsnprintf(NULL, 0, fmt, args_copy) +
      1;
    va_end(args_copy);

    char *buf = malloc(len + 1);
    {
      char *s = buf;

      s += sprintf(s, LINE_PREFIX, timestamp, (int)(now.tv_usec / 1000), flag);
      s += vsprintf(s, fmt, args);
      s += sprintf(s, "\n");
    }

    (*debug_func)(buf, debug_func_data);

    free(buf);
  }
  else if(debug_fh) {
    fprintf(debug_fh, LINE_PREFIX, timestamp, (int)(now.tv_usec / 1000), flag);
    vfprintf(debug_fh, fmt, args);
    fprintf(debug_fh, "\n");
  }

#undef LINE_PREFIX
}
