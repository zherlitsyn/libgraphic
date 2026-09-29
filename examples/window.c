#include <stdio.h>

#include "graphic.h"

/*
 * Minimal exercise of the window layer: open a window and run the
 * frame loop until it is closed. It grows along with the library.
 */

int main(void)
{
    graphic_window_flags_set(GRAPHIC_WINDOW_RESIZABLE |
                             GRAPHIC_WINDOW_VSYNC     |
                             GRAPHIC_WINDOW_VALIDATION);

    if (!graphic_window(960, 540, "graphic - window test"))
        return 1;

    while (!graphic_window_should_close()) {
        graphic_drawing_begin();
        graphic_drawing_end();
    }

    graphic_window_close();

    return 0;
}
