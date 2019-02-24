#include "termdriver.h"

// This entire driver requires unibilium
#ifdef HAVE_UNIBILIUM
#include "unibilium.h"

#include <string.h>
#include <stdarg.h>

/* I would love if terminfo gave us these extra strings, but it does not. At a
 * minor risk of non-portability, define them here.
 */
struct TermInfoExtraStrings {
  const char *enter_altscreen_mode;
  const char *exit_altscreen_mode;

  const char *enter_mouse_mode;
  const char *exit_mouse_mode;
};

static const struct TermInfoExtraStrings extra_strings_default = {
  // Alternate screen + cursor save mode
  .enter_altscreen_mode = "\e[?1049h",
  .exit_altscreen_mode  = "\e[?1049l",
};

static const struct TermInfoExtraStrings extra_strings_vt200_mouse = {
  // Alternate screen + cursor save mode
  .enter_altscreen_mode = "\e[?1049h",
  .exit_altscreen_mode  = "\e[?1049l",

  // Mouse click/drag reporting
  // Also speculatively enable SGR protocol
  .enter_mouse_mode = "\e[?1002h\e[?1006h",
  .exit_mouse_mode  = "\e[?1002l\e[?1006l",
};

/* Also, some terminfo databases are incomplete. Lets provide some fallbacks
 */
struct TermInfoFallback {
  enum unibi_string cap;
  const char *value;
};

const static struct TermInfoFallback terminfo_fallback[] = {
  { unibi_erase_chars, "\e[%dX" },
  { unibi_parm_dch,    "\e[%dP" },
  { 0 },
};

struct TIDriver {
  TickitTermDriver driver;

  unibi_term *ut;

  struct {
    unsigned int altscreen:1;
    unsigned int cursorvis:1;
    unsigned int mouse:1;
  } mode;

  struct {
    unsigned int bce:1;
    int colours;
  } cap;

  struct {
    // Positioning
    const char *cup;  // cursor_address
    const char *vpa;  // row_address == vertical position absolute
    const char *hpa;  // column_address = horizontal position absolute

    // Moving
    const char *cuu; const char *cuu1; // Cursor Up
    const char *cud; const char *cud1; // Cursor Down
    const char *cuf; const char *cuf1; // Cursor Forward == Right
    const char *cub; const char *cub1; // Cursor Backward == Left

    // Editing
    const char *ich; const char *ich1; // Insert Character
    const char *dch; const char *dch1; // Delete Character
    const char *il;  const char *il1;  // Insert Line
    const char *dl;  const char *dl1;  // Delete Line
    const char *ech;                   // Erase Character
    const char *ed2;                   // Erase Data 2 == Clear screen
    const char *stbm;                  // Set Top/Bottom Margins

    // Formatting
    const char *sgr;    // Select Graphic Rendition
    const char *sgr0;   // Exit Attribute Mode
    const char *sgr_i0, *sgr_i1; // SGR italic off/on
    const char *sgr_fg; // SGR foreground colour
    const char *sgr_bg; // SGR background colour

    // Mode setting/clearing
    const char *sm_csr; const char *rm_csr; // Set/reset mode: Cursor visible
  } str;

  const struct TermInfoExtraStrings *extra;
};

static const char *lookup_ti_string(struct TIDriver *td, const TickitTermProbeArgs *args, enum unibi_string s)
{
  const char *value = unibi_get_str(td->ut, s);

  for(const struct TermInfoFallback *fb = terminfo_fallback; !value && fb->cap; fb++)
    if(fb->cap == s)
      value = fb->value;

  if(args->ti_hook && args->ti_hook->getstr)
    value = (*args->ti_hook->getstr)(unibi_name_str(s), value, args->ti_hook->data);

  return value;
}

static const char *require_ti_string(struct TIDriver *td, const TickitTermProbeArgs *args, enum unibi_string s)
{
  const char *ret = lookup_ti_string(td, args, s);
  if(ret)
    return ret;

  fprintf(stderr, "Required TI string '%s' is missing\n", unibi_name_str(s));
  abort();
}

static bool print(TickitTermDriver *ttd, const char *str, size_t len)
{
  tickit_termdrv_write_str(ttd, str, len);
  return true;
}

static void run_ti(TickitTermDriver *ttd, const char *str, int n_params, ...)
{
  unibi_var_t params[9];
  va_list args;

  if(!str) {
    fprintf(stderr, "Abort on attempt to use NULL TI string\n");
    abort();
  }

  va_start(args, n_params);
  for(int i = 0; i < 10 && i < n_params; i++)
    params[i] = unibi_var_from_num(va_arg(args, int));

  char tmp[64];
  char *buf = tmp;
  size_t len = unibi_run(str, params, buf, sizeof(tmp));

  if(len > sizeof(tmp)) {
    buf = tickit_termdrv_get_tmpbuffer(ttd, len);
    unibi_run(str, params, buf, len);
  }

  tickit_termdrv_write_str(ttd, buf, len);
}

static bool goto_abs(TickitTermDriver *ttd, int line, int col)
{
  struct TIDriver *td = (struct TIDriver*)ttd;

  if(line != -1 && col != -1)
    run_ti(ttd, td->str.cup, 2, line, col);
  else if(line != -1) {
    if(!td->str.vpa)
      return false;

    run_ti(ttd, td->str.vpa, 1, line);
  }
  else if(col != -1) {
    if(col == 0) {
      tickit_termdrv_write_str(ttd, "\r", 1);
      return true;
    }

    if(td->str.hpa)
      run_ti(ttd, td->str.hpa, 1, col);
    else if(td->str.cuf) {
      // Emulate HPA by CR + CUF
      tickit_termdrv_write_str(ttd, "\r", 1);
      run_ti(ttd, td->str.cuf, 1, col);
    }
    else
      return false;
  }

  return true;
}

static bool move_rel(TickitTermDriver *ttd, int downward, int rightward)
{
  struct TIDriver *td = (struct TIDriver*)ttd;

  if(downward == 1 && td->str.cud1)
    run_ti(ttd, td->str.cud1, 0);
  else if(downward == -1 && td->str.cuu1)
    run_ti(ttd, td->str.cuu1, 0);
  else if(downward > 0)
    run_ti(ttd, td->str.cud, 1, downward);
  else if(downward < 0)
    run_ti(ttd, td->str.cuu, 1, -downward);

  if(rightward == 1 && td->str.cuf1)
    run_ti(ttd, td->str.cuf1, 0);
  else if(rightward == -1 && td->str.cub1)
    run_ti(ttd, td->str.cub1, 0);
  else if(rightward > 0)
    run_ti(ttd, td->str.cuf, 1, rightward);
  else if(rightward < 0)
    run_ti(ttd, td->str.cub, 1, -rightward);

  return true;
}

static bool scrollrect(TickitTermDriver *ttd, const TickitRect *rect, int downward, int rightward)
{
  struct TIDriver *td = (struct TIDriver*)ttd;

  if(!downward && !rightward)
    return true;

  int term_lines, term_cols;
  tickit_term_get_size(ttd->tt, &term_lines, &term_cols);

  if((tickit_rect_right(rect) == term_cols) && downward == 0) {
    for(int line = rect->top; line < tickit_rect_bottom(rect); line++) {
      goto_abs(ttd, line, rect->left);

      if(rightward == 1 && td->str.dch1)
        run_ti(ttd, td->str.dch1, 0);
      else if(rightward == -1 && td->str.ich1)
        run_ti(ttd, td->str.ich1, 0);
      else if(rightward > 0)
        run_ti(ttd, td->str.dch, 1, rightward);
      else if(rightward < 0)
        run_ti(ttd, td->str.ich, 1, -rightward);
    }

    return true;
  }

  if(rect->left == 0 && rect->cols == term_cols && rightward == 0) {
    run_ti(ttd, td->str.stbm, 2, rect->top, tickit_rect_bottom(rect) - 1);

    goto_abs(ttd, rect->top, 0);

    if(downward == 1 && td->str.dl1)
      run_ti(ttd, td->str.dl1, 0);
    else if(downward == -1 && td->str.il1)
      run_ti(ttd, td->str.il1, 0);
    else if(downward > 0)
      run_ti(ttd, td->str.dl, 1, downward);
    else if(downward < 0)
      run_ti(ttd, td->str.il, 1, -downward);

    run_ti(ttd, td->str.stbm, 2, 0, term_lines - 1);
    return true;
  }

  return false;
}

static bool erasech(TickitTermDriver *ttd, int count, TickitMaybeBool moveend)
{
  struct TIDriver *td = (struct TIDriver *)ttd;

  if(count < 1)
    return true;

  /* Even if the terminal can do bce, only use ECH if we're not in
   * reverse-video mode. Most terminals don't do rv+ECH properly
   */
  if(td->cap.bce && !tickit_pen_get_bool_attr(tickit_termdrv_current_pen(ttd), TICKIT_PEN_REVERSE)) {
    run_ti(ttd, td->str.ech, 1, count);

    if(moveend == TICKIT_YES)
      move_rel(ttd, 0, count);
  }
  else {
     /* TODO: consider tickit_termdrv_write_chrfill(ttd, c, n)
     */
    char *spaces = tickit_termdrv_get_tmpbuffer(ttd, 64);
    memset(spaces, ' ', 64);
    while(count > 64) {
      tickit_termdrv_write_str(ttd, spaces, 64);
      count -= 64;
    }
    tickit_termdrv_write_str(ttd, spaces, count);

    if(moveend == TICKIT_NO)
      move_rel(ttd, 0, -count);
  }

  return true;
}

static bool clear(TickitTermDriver *ttd)
{
  struct TIDriver *td = (struct TIDriver *)ttd;

  run_ti(ttd, td->str.ed2, 0);

  return true;
}

static bool chpen(TickitTermDriver *ttd, const TickitPen *delta, const TickitPen *final)
{
  struct TIDriver *td = (struct TIDriver *)ttd;

  /* TODO: This is all a bit of a mess
   * Would be nicer to detect if fg/bg colour are changed, and if not, use
   * the individual enter/exit modes from the delta pen
   */

  run_ti(ttd, td->str.sgr, 9,
      0, // standout
      tickit_pen_get_bool_attr(final, TICKIT_PEN_UNDER),
      tickit_pen_get_bool_attr(final, TICKIT_PEN_REVERSE),
      tickit_pen_get_bool_attr(final, TICKIT_PEN_BLINK),
      0, // dim
      tickit_pen_get_bool_attr(final, TICKIT_PEN_BOLD),
      0, // invisible
      0, // protect
      0); // alt charset

  if(tickit_pen_has_attr(delta, TICKIT_PEN_ITALIC)) {
    if(td->str.sgr_i1 && tickit_pen_get_bool_attr(delta, TICKIT_PEN_ITALIC))
      run_ti(ttd, td->str.sgr_i1, 0);
    else if(td->str.sgr_i0)
      run_ti(ttd, td->str.sgr_i0, 0);
  }

  int c;
  if((c = tickit_pen_get_colour_attr(final, TICKIT_PEN_FG)) > -1 &&
      c < td->cap.colours)
    run_ti(ttd, td->str.sgr_fg, 1, c);

  if((c = tickit_pen_get_colour_attr(final, TICKIT_PEN_BG)) > -1 &&
      c < td->cap.colours)
    run_ti(ttd, td->str.sgr_bg, 1, c);

  return true;
}

static bool getctl_int(TickitTermDriver *ttd, TickitTermCtl ctl, int *value)
{
  struct TIDriver *td = (struct TIDriver *)ttd;

  switch(ctl) {
    case TICKIT_TERMCTL_ALTSCREEN:
      *value = td->mode.altscreen;
      return true;

    case TICKIT_TERMCTL_CURSORVIS:
      *value = td->mode.cursorvis;
      return true;

    case TICKIT_TERMCTL_MOUSE:
      *value = td->mode.mouse;
      return true;

    case TICKIT_TERMCTL_COLORS:
      *value = td->cap.colours;
      return true;

    default:
      return false;
  }
}

static bool setctl_int(TickitTermDriver *ttd, TickitTermCtl ctl, int value)
{
  struct TIDriver *td = (struct TIDriver *)ttd;

  switch(ctl) {
    case TICKIT_TERMCTL_ALTSCREEN:
      if(!td->extra->enter_altscreen_mode)
        return false;

      if(!td->mode.altscreen == !value)
        return true;

      tickit_termdrv_write_str(ttd, value ? td->extra->enter_altscreen_mode : td->extra->exit_altscreen_mode, 0);
      td->mode.altscreen = !!value;
      return true;

    case TICKIT_TERMCTL_CURSORVIS:
      if(!td->mode.cursorvis == !value)
        return true;

      run_ti(ttd, value ? td->str.sm_csr : td->str.rm_csr, 0);
      td->mode.cursorvis = !!value;
      return true;

    case TICKIT_TERMCTL_MOUSE:
      if(!td->extra->enter_mouse_mode)
        return false;

      if(!td->mode.mouse == !value)
        return true;

      tickit_termdrv_write_str(ttd, value ? td->extra->enter_mouse_mode : td->extra->exit_mouse_mode, 0);
      td->mode.mouse = !!value;
      return true;

    default:
      return false;
  }
}

static bool setctl_str(TickitTermDriver *ttd, TickitTermCtl ctl, const char *value)
{
  return false;
}

static void attach(TickitTermDriver *ttd, TickitTerm *tt)
{
  struct TIDriver *td = (struct TIDriver *)ttd;
  unibi_term *ut = td->ut;

  tickit_term_set_size(tt, unibi_get_num(ut, unibi_lines), unibi_get_num(ut, unibi_columns));
}

static void start(TickitTermDriver *ttd)
{
  // Nothing needed
}

static void shutdown(TickitTermDriver *ttd)
{
  struct TIDriver *td = (struct TIDriver *)ttd;

  if(td->mode.mouse)
    tickit_termdrv_write_str(ttd, td->extra->exit_mouse_mode, 0);
  if(!td->mode.cursorvis)
    run_ti(ttd, td->str.sm_csr, 0);
  if(td->mode.altscreen)
    tickit_termdrv_write_str(ttd, td->extra->exit_altscreen_mode, 0);

  run_ti(ttd, td->str.sgr0, 0);
}

static void resume(TickitTermDriver *ttd)
{
  struct TIDriver *td = (struct TIDriver *)ttd;

  if(td->mode.altscreen)
    tickit_termdrv_write_str(ttd, td->extra->enter_altscreen_mode, 0);
  if(!td->mode.cursorvis)
    run_ti(ttd, td->str.rm_csr, 0);
  if(td->mode.mouse)
    tickit_termdrv_write_str(ttd, td->extra->enter_mouse_mode, 0);
}

static void destroy(TickitTermDriver *ttd)
{
  struct TIDriver *td = (struct TIDriver *)ttd;

  unibi_destroy(td->ut);

  free(td);
}

static TickitTermDriverVTable ti_vtable = {
  .attach     = attach,
  .destroy    = destroy,
  .start      = start,
  .stop       = shutdown,
  .pause      = shutdown,
  .resume     = resume,
  .print      = print,
  .goto_abs   = goto_abs,
  .move_rel   = move_rel,
  .scrollrect = scrollrect,
  .erasech    = erasech,
  .clear      = clear,
  .chpen      = chpen,
  .getctl_int = getctl_int,
  .setctl_int = setctl_int,
  .setctl_str = setctl_str,
};

static TickitTermDriver *new(const TickitTermProbeArgs *args)
{
  unibi_term *ut = unibi_from_term(args->termtype);
  if(!ut)
    return NULL;

  struct TIDriver *td = malloc(sizeof(struct TIDriver));
  td->driver.vtable = &ti_vtable;

  td->ut = ut;

  td->mode.mouse = 0;
  td->mode.cursorvis = 1;
  td->mode.altscreen = 0;

  td->cap.bce = unibi_get_bool(ut, unibi_back_color_erase);
  td->cap.colours = unibi_get_num(ut, unibi_max_colors);

  td->str.cup    = require_ti_string(td, args, unibi_cursor_address);
  td->str.vpa    = lookup_ti_string (td, args, unibi_row_address);
  td->str.hpa    = lookup_ti_string (td, args, unibi_column_address);
  td->str.cuu    = require_ti_string(td, args, unibi_parm_up_cursor);
  td->str.cuu1   = lookup_ti_string (td, args, unibi_cursor_up);
  td->str.cud    = require_ti_string(td, args, unibi_parm_down_cursor);
  td->str.cud1   = lookup_ti_string (td, args, unibi_cursor_down);
  td->str.cuf    = require_ti_string(td, args, unibi_parm_right_cursor);
  td->str.cuf1   = lookup_ti_string (td, args, unibi_cursor_right);
  td->str.cub    = require_ti_string(td, args, unibi_parm_left_cursor);
  td->str.cub1   = lookup_ti_string (td, args, unibi_cursor_left);
  td->str.ich    = require_ti_string(td, args, unibi_parm_ich);
  td->str.ich1   = lookup_ti_string (td, args, unibi_insert_character);
  td->str.dch    = require_ti_string(td, args, unibi_parm_dch);
  td->str.dch1   = lookup_ti_string (td, args, unibi_delete_character);
  td->str.il     = require_ti_string(td, args, unibi_parm_insert_line);
  td->str.il1    = lookup_ti_string (td, args, unibi_insert_line);
  td->str.dl     = require_ti_string(td, args, unibi_parm_delete_line);
  td->str.dl1    = lookup_ti_string (td, args, unibi_delete_line);
  td->str.ech    = require_ti_string(td, args, unibi_erase_chars);
  td->str.ed2    = require_ti_string(td, args, unibi_clear_screen);
  td->str.stbm   = require_ti_string(td, args, unibi_change_scroll_region);
  td->str.sgr    = require_ti_string(td, args, unibi_set_attributes);
  td->str.sgr0   = require_ti_string(td, args, unibi_exit_attribute_mode);
  td->str.sgr_i0 = lookup_ti_string (td, args, unibi_exit_italics_mode);
  td->str.sgr_i1 = lookup_ti_string (td, args, unibi_enter_italics_mode);
  td->str.sgr_fg = require_ti_string(td, args, unibi_set_a_foreground);
  td->str.sgr_bg = require_ti_string(td, args, unibi_set_a_background);

  td->str.sm_csr = require_ti_string(td, args, unibi_cursor_normal);
  td->str.rm_csr = require_ti_string(td, args, unibi_cursor_invisible);

  const char *key_mouse = lookup_ti_string(td, args, unibi_key_mouse);
  if(key_mouse && strcmp(key_mouse, "\e[M") == 0)
    td->extra = &extra_strings_vt200_mouse;
  else
    td->extra = &extra_strings_default;

  return (TickitTermDriver*)td;
}

#else /* not HAVE_UNIBILIUM */

static TickitTermDriver *new(const TickitTermProbeArgs *args)
{
  return NULL;
}

#endif

TickitTermDriverProbe tickit_termdrv_probe_ti = {
  .new = new,
};
