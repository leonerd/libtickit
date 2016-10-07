#include "tickit.h"
#include "taplib.h"
#include "taplib-mockterm.h"

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitRenderBuffer *rb;

  rb = tickit_renderbuffer_new(10, 20);

  // Position
  {
    int line, col;

    tickit_renderbuffer_goto(rb, 2, 2);

    {
      tickit_renderbuffer_save(rb);

      tickit_renderbuffer_goto(rb, 4, 4);
      tickit_renderbuffer_get_cursorpos(rb, &line, &col);
      is_int(line, 4, "line before restore");
      is_int(col,  4, "col before restore");

      tickit_renderbuffer_restore(rb);
    }

    tickit_renderbuffer_get_cursorpos(rb, &line, &col);
    is_int(line, 2, "line after restore");
    is_int(col,  2, "col after restore");

    tickit_renderbuffer_text(rb, "some text");

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("Stack saves/restores virtual cursor position",
        GOTO(2,2), SETPEN(), PRINT("some text"),
        NULL);
  }

  // Clipping
  {
    tickit_renderbuffer_text_at(rb, 0, 0, "0000000000");

    {
      tickit_renderbuffer_save(rb);
      tickit_renderbuffer_clip(rb, &(TickitRect){.top = 0, .left = 2, .lines = 10, .cols = 16});

      tickit_renderbuffer_text_at(rb, 1, 0, "1111111111");

      tickit_renderbuffer_restore(rb);
    }

    tickit_renderbuffer_text_at(rb, 2, 0, "2222222222");

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("Stack saves/restores clipping region",
        GOTO(0,0), SETPEN(), PRINT("0000000000"),
        GOTO(1,2), SETPEN(), PRINT("11111111"),
        GOTO(2,0), SETPEN(), PRINT("2222222222"),
        NULL);
  }

  // Pen
  {
    TickitPen *pen;

    tickit_renderbuffer_goto(rb, 3, 0);

    tickit_renderbuffer_setpen(rb, pen = tickit_pen_new_attrs(TICKIT_PEN_BG, 1, -1));
    tickit_pen_unref(pen);

    tickit_renderbuffer_text(rb, "123");

    {
      tickit_renderbuffer_savepen(rb);

      tickit_renderbuffer_setpen(rb, pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 4, -1));
      tickit_pen_unref(pen);

      tickit_renderbuffer_text(rb, "456");

      tickit_renderbuffer_setpen(rb, pen = tickit_pen_new_attrs(TICKIT_PEN_BG, -1, -1));
      tickit_pen_unref(pen);

      tickit_renderbuffer_text(rb, "789");

      tickit_renderbuffer_restore(rb);
    }

    tickit_renderbuffer_text(rb, "ABC");

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("Stack saves/restores render pen",
        GOTO(3,0), SETPEN(.bg=1), PRINT("123"),
                   SETPEN(.bg=1,.fg=4), PRINT("456"),
                   SETPEN(), PRINT("789"),
                   SETPEN(.bg=1), PRINT("ABC"),
        NULL);

    tickit_renderbuffer_goto(rb, 4, 0);

    tickit_renderbuffer_setpen(rb, pen = tickit_pen_new_attrs(TICKIT_PEN_REVERSE, 1, -1));
    tickit_pen_unref(pen);

    tickit_renderbuffer_text(rb, "123");

    {
      tickit_renderbuffer_savepen(rb);

      tickit_renderbuffer_setpen(rb, pen = tickit_pen_new_attrs(TICKIT_PEN_REVERSE, 0, -1));
      tickit_pen_unref(pen);

      tickit_renderbuffer_text(rb, "456");

      tickit_renderbuffer_restore(rb);
    }

    tickit_renderbuffer_text(rb, "789");

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("Stack saves/restores allow zeroing pen attributes",
        GOTO(4,0), SETPEN(.rv=1), PRINT("123"),
                   SETPEN(), PRINT("456"),
                   SETPEN(.rv=1), PRINT("789"),
        NULL);
  }

  // Translation
  {
    tickit_renderbuffer_text_at(rb, 0, 0, "A");

    tickit_renderbuffer_save(rb);
    {
      tickit_renderbuffer_translate(rb, 2, 2);

      tickit_renderbuffer_text_at(rb, 1, 1, "B");
    }
    tickit_renderbuffer_restore(rb);

    tickit_renderbuffer_text_at(rb, 2, 2, "C");

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("Stack saves/restores translation offset",
        GOTO(0,0), SETPEN(), PRINT("A"),
        GOTO(2,2), SETPEN(), PRINT("C"),
        GOTO(3,3), SETPEN(), PRINT("B"),
        NULL);
  }

  tickit_renderbuffer_unref(rb);
  tickit_term_unref(tt);

  return exit_status();
}
