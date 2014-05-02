#include "tickit.h"
#include "taplib.h"
#include "taplib-mockterm.h"

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitRenderBuffer *rb;
  char buffer[256];

  rb = tickit_renderbuffer_new(10, 20);

  // Absolute characters
  {
    TickitPen *fg_pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 4, -1);

    tickit_renderbuffer_char_at(rb, 5, 5, 0x41, fg_pen);
    tickit_renderbuffer_char_at(rb, 5, 6, 0x42, fg_pen);
    tickit_renderbuffer_char_at(rb, 5, 7, 0x43, fg_pen);

    is_int(tickit_renderbuffer_get_cell_text(rb, 5, 5, buffer, sizeof buffer), 1, "get_cell_text CHAR at 5,5");
    is_str(buffer, "A", "buffer text at 5,5");

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders char_at to terminal",
        GOTO(5,5), SETPEN(.fg=4), PRINT("A"),
                   SETPEN(.fg=4), PRINT("B"),
                   SETPEN(.fg=4), PRINT("C"),
        NULL);

    tickit_pen_destroy(fg_pen);
  }

  // Characters setpen
  {
    TickitPen *bg_pen = tickit_pen_new_attrs(TICKIT_PEN_BG, 5, -1);
    tickit_renderbuffer_setpen(rb, bg_pen);

    tickit_renderbuffer_char_at(rb, 5, 5, 0x44, NULL);
    tickit_renderbuffer_char_at(rb, 5, 6, 0x45, NULL);
    tickit_renderbuffer_char_at(rb, 5, 7, 0x46, NULL);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders char_at with stored pen",
        GOTO(5,5), SETPEN(.bg=5), PRINT("D"),
                   SETPEN(.bg=5), PRINT("E"),
                   SETPEN(.bg=5), PRINT("F"),
        NULL);

    tickit_pen_destroy(bg_pen);
  }

  // VC characters
  {
    tickit_renderbuffer_goto(rb, 0, 4);

    // Direct pen
    TickitPen *fg_pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 5, -1);
    tickit_renderbuffer_char(rb, 0x47, fg_pen);

    // Stored pen
    TickitPen *bg_pen = tickit_pen_new_attrs(TICKIT_PEN_BG, 6, -1);
    tickit_renderbuffer_setpen(rb, bg_pen);
    tickit_renderbuffer_char(rb, 0x48, NULL);

    // Combined pens
    tickit_renderbuffer_char(rb, 0x49, fg_pen);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders chars at VC",
        GOTO(0,4), SETPEN(.fg=5), PRINT("G"),
                   SETPEN(.bg=6), PRINT("H"),
                   SETPEN(.fg=5,.bg=6), PRINT("I"),
        NULL);

    tickit_pen_destroy(fg_pen);
    tickit_pen_destroy(bg_pen);
  }

  // Characters with translation
  {
    tickit_renderbuffer_translate(rb, 3, 5);

    tickit_renderbuffer_char_at(rb, 1, 1, 0x31, NULL);
    tickit_renderbuffer_char_at(rb, 1, 2, 0x32, NULL);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders char_at with translation",
        GOTO(4,6), SETPEN(), PRINT("1"),
                   SETPEN(), PRINT("2"),
        NULL);
  }

  tickit_renderbuffer_destroy(rb);

  return exit_status();
}
