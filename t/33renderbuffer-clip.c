#include "tickit.h"
#include "taplib.h"
#include "taplib-mockterm.h"

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitRenderBuffer *rb;

  rb = tickit_renderbuffer_new(10, 20);

  // Clipping to edge
  {
    tickit_renderbuffer_text_at(rb, -1, 5, "TTTTTTTTTT", NULL);
    tickit_renderbuffer_text_at(rb, 11, 5, "BBBBBBBBBB", NULL);
    tickit_renderbuffer_text_at(rb, 4, -3, "[LLLLLLLL]", NULL);
    tickit_renderbuffer_text_at(rb, 5, 15, "[RRRRRRRR]", NULL);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer text rendering with clipping",
        GOTO(4,0), SETPEN(), PRINT("LLLLLL]"),
        GOTO(5,15), SETPEN(), PRINT("[RRRR"),
        NULL);

    TickitPen *pen;

    tickit_renderbuffer_erase_at(rb, -1, 5, 10, pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 1, -1)),
      tickit_pen_destroy(pen);
    tickit_renderbuffer_erase_at(rb, 11, 5, 10, pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 2, -1)),
      tickit_pen_destroy(pen);
    tickit_renderbuffer_erase_at(rb, 4, -3, 10, pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 3, -1)),
      tickit_pen_destroy(pen);
    tickit_renderbuffer_erase_at(rb, 5, 15, 10, pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 4, -1)),
      tickit_pen_destroy(pen);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer erasech rendering with clipping",
        GOTO(4,0), SETPEN(.fg=3), ERASECH(7,-1),
        GOTO(5,15), SETPEN(.fg=4), ERASECH(5,-1),
        NULL);

    tickit_renderbuffer_goto(rb, 2, 18);
    tickit_renderbuffer_text(rb, "A", NULL);
    tickit_renderbuffer_text(rb, "B", NULL);
    tickit_renderbuffer_text(rb, "C", NULL);
    tickit_renderbuffer_text(rb, "D", NULL);
    tickit_renderbuffer_text(rb, "E", NULL);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer text at VC with clipping",
        GOTO(2,18), SETPEN(), PRINT("A"),
                    SETPEN(), PRINT("B"),
        NULL);
  }

  // Clipping to rect
  {
    tickit_renderbuffer_clip(rb, &(TickitRect){.top = 2, .left=2, .lines=6, .cols=16});

    tickit_renderbuffer_text_at(rb, 1, 5, "TTTTTTTTTT", NULL);
    tickit_renderbuffer_text_at(rb, 9, 5, "BBBBBBBBBB", NULL);
    tickit_renderbuffer_text_at(rb, 4, -3, "[LLLLLLLL]", NULL);
    tickit_renderbuffer_text_at(rb, 5, 15, "[RRRRRRRR]", NULL);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders text rendering with rect clipping",
        GOTO(4,2), SETPEN(), PRINT("LLLL]"),
        GOTO(5,15), SETPEN(), PRINT("[RR"),
        NULL);

    tickit_renderbuffer_clip(rb, &(TickitRect){.top = 2, .left=2, .lines=6, .cols=16});

    TickitPen *pen;

    tickit_renderbuffer_erase_at(rb, 1, 5, 10, pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 1, -1)),
      tickit_pen_destroy(pen);
    tickit_renderbuffer_erase_at(rb, 9, 5, 10, pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 2, -1)),
      tickit_pen_destroy(pen);
    tickit_renderbuffer_erase_at(rb, 4, -3, 10, pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 3, -1)),
      tickit_pen_destroy(pen);
    tickit_renderbuffer_erase_at(rb, 5, 15, 10, pen = tickit_pen_new_attrs(TICKIT_PEN_FG, 4, -1)),
      tickit_pen_destroy(pen);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer renders erasech with rect clipping",
        GOTO(4,2), SETPEN(.fg=3), ERASECH(5,-1),
        GOTO(5,15), SETPEN(.fg=4), ERASECH(3,-1),
        NULL);
  }

  // Clipping with translation
  {
    tickit_renderbuffer_translate(rb, 3, 5);

    tickit_renderbuffer_clip(rb, &(TickitRect){.top = 2, .left = 2, .lines = 3, .cols = 5});

    tickit_renderbuffer_text_at(rb, 1, 0, "1111111111", NULL);
    tickit_renderbuffer_text_at(rb, 2, 0, "2222222222", NULL);
    tickit_renderbuffer_text_at(rb, 3, 0, "3333333333", NULL);
    tickit_renderbuffer_text_at(rb, 4, 0, "4444444444", NULL);
    tickit_renderbuffer_text_at(rb, 5, 0, "5555555555", NULL);

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer clipping rectangle translated",
        GOTO(5,7), SETPEN(), PRINT("22222"),
        GOTO(6,7), SETPEN(), PRINT("33333"),
        GOTO(7,7), SETPEN(), PRINT("44444"),
        NULL);
  }

  tickit_renderbuffer_destroy(rb);

  return exit_status();
}
