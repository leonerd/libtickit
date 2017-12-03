#include "tickit.h"
#include "taplib.h"
#include "taplib-mockterm.h"

int main(int argc, char *argv[])
{
  TickitTerm *tt = make_term(25, 80);
  TickitRenderBuffer *rb;

  rb = tickit_renderbuffer_new(10, 20);

  // Basic copy
  {
    tickit_renderbuffer_text_at(rb, 0, 0, "Hello");
    tickit_renderbuffer_char_at(rb, 1, 1, 'A');
    tickit_renderbuffer_erase_at(rb, 1, 2, 3);

    tickit_renderbuffer_copyrect(rb,
        &(TickitRect){ .top = 4, .left = 2, .lines = 2, .cols = 10 },
        &(TickitRect){ .top = 0, .left = 0, .lines = 2, .cols = 10 });

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer contents duplicated after copyrect",
        /* orig */
        GOTO(0,0), SETPEN(), PRINT("Hello"),
        GOTO(1,1), SETPEN(), PRINT("A"),
                   SETPEN(), ERASECH(3,-1),
        /* copy */
        GOTO(4,2), SETPEN(), PRINT("Hello"),
        GOTO(5,3), SETPEN(), PRINT("A"),
                   SETPEN(), ERASECH(3,-1),
        NULL);
  }

  // Truncate right
  {
    tickit_renderbuffer_text_at(rb, 0, 0, "ABCDE");
    tickit_renderbuffer_erase_at(rb, 1, 0, 6);

    tickit_renderbuffer_copyrect(rb,
        &(TickitRect){ .top = 4, .left = 0, .lines = 2, .cols = 3 },
        &(TickitRect){ .top = 0, .left = 0, .lines = 2, .cols = 3 });

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer copyrect can truncate to right",
        /* orig */
        GOTO(0,0), SETPEN(), PRINT("ABCDE"),
        GOTO(1,0), SETPEN(), ERASECH(6,-1),
        /* copy */
        GOTO(4,0), SETPEN(), PRINT("ABC"),
        GOTO(5,0), SETPEN(), ERASECH(3,-1),
        NULL);
  }

  // Truncate left
  {
    tickit_renderbuffer_text_at(rb, 0, 0, "ABCDE");
    tickit_renderbuffer_erase_at(rb, 1, 0, 6);

    tickit_renderbuffer_copyrect(rb,
        &(TickitRect){ .top = 4, .left = 2, .lines = 2, .cols = 3 },
        &(TickitRect){ .top = 0, .left = 2, .lines = 2, .cols = 3 });

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer copyrect can truncate to left",
        /* orig */
        GOTO(0,0), SETPEN(), PRINT("ABCDE"),
        GOTO(1,0), SETPEN(), ERASECH(6,-1),
        /* copy */
        GOTO(4,2), SETPEN(), PRINT("CDE"),
        GOTO(5,2), SETPEN(), ERASECH(3,-1),
        NULL);
  }

  // Overlap upwards
  {
    tickit_renderbuffer_text_at(rb, 0, 0, "Abcde");
    tickit_renderbuffer_text_at(rb, 1, 0, "Fghij");
    tickit_renderbuffer_text_at(rb, 2, 0, "Klmno");

    tickit_renderbuffer_copyrect(rb,
        &(TickitRect){ .top = 0, .left = 0, .lines = 2, .cols = 10 },
        &(TickitRect){ .top = 1, .left = 0, .lines = 2, .cols = 10 });

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer copyrect can copy upwards",
        GOTO(0,0), SETPEN(), PRINT("Fghij"),
        GOTO(1,0), SETPEN(), PRINT("Klmno"),
        GOTO(2,0), SETPEN(), PRINT("Klmno"),
        NULL);
  }

  // Overlap downwards
  {
    tickit_renderbuffer_text_at(rb, 0, 0, "aBcde");
    tickit_renderbuffer_text_at(rb, 1, 0, "fGhij");
    tickit_renderbuffer_text_at(rb, 2, 0, "kLmno");

    tickit_renderbuffer_copyrect(rb,
        &(TickitRect){ .top = 1, .left = 0, .lines = 2, .cols = 10 },
        &(TickitRect){ .top = 0, .left = 0, .lines = 2, .cols = 10 });

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer copyrect can copy downwards",
        GOTO(0,0), SETPEN(), PRINT("aBcde"),
        GOTO(1,0), SETPEN(), PRINT("aBcde"),
        GOTO(2,0), SETPEN(), PRINT("fGhij"),
        NULL);
  }

  // Overlap leftwards
  {
    tickit_renderbuffer_text_at(rb, 0, 0, "Abc");
    tickit_renderbuffer_text_at(rb, 0, 3, "Def");
    tickit_renderbuffer_text_at(rb, 0, 6, "Ghi");

    tickit_renderbuffer_copyrect(rb,
        &(TickitRect){ .top = 0, .left = 0, .lines = 1, .cols = 6 },
        &(TickitRect){ .top = 0, .left = 3, .lines = 1, .cols = 6 });

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer copyrect can copy leftwards",
        GOTO(0,0), SETPEN(), PRINT("Def"),
                   SETPEN(), PRINT("Ghi"),
                   SETPEN(), PRINT("Ghi"),
        NULL);
  }

  // Overlap rightwards
  {
    tickit_renderbuffer_text_at(rb, 0, 0, "aBc");
    tickit_renderbuffer_text_at(rb, 0, 3, "dEf");
    tickit_renderbuffer_text_at(rb, 0, 6, "gHi");

    tickit_renderbuffer_copyrect(rb,
        &(TickitRect){ .top = 0, .left = 3, .lines = 1, .cols = 6 },
        &(TickitRect){ .top = 0, .left = 0, .lines = 1, .cols = 6 });

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer copyrect can copy rightwards",
        GOTO(0,0), SETPEN(), PRINT("aBc"),
                   SETPEN(), PRINT("aBc"),
                   SETPEN(), PRINT("dEf"),
        NULL);
  }

  // Move
  {
    tickit_renderbuffer_text_at(rb, 0, 0, "Hello");
    tickit_renderbuffer_char_at(rb, 1, 1, 'A');
    tickit_renderbuffer_erase_at(rb, 1, 2, 3);

    tickit_renderbuffer_moverect(rb,
        &(TickitRect){ .top = 4, .left = 2, .lines = 2, .cols = 10 },
        &(TickitRect){ .top = 0, .left = 0, .lines = 2, .cols = 10 });

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer contents moved after move",
        GOTO(4,2), SETPEN(), PRINT("Hello"),
        GOTO(5,3), SETPEN(), PRINT("A"),
                   SETPEN(), ERASECH(3,-1),
        NULL);
  }

  // Move with overlap
  {
    tickit_renderbuffer_text_at(rb, 0, 0, "Abcde");

    tickit_renderbuffer_moverect(rb,
        &(TickitRect){ .top = 0, .left = 0, .lines = 1, .cols = 10 },
        &(TickitRect){ .top = 0, .left = 3, .lines = 1, .cols = 10 });

    tickit_renderbuffer_flush_to_term(rb, tt);
    is_termlog("RenderBuffer contents moved after move with overlap",
        GOTO(0,0), SETPEN(), PRINT("de"),
        NULL);
  }

  return exit_status();
}
