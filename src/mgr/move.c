/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/* move a window */
/* #includes */
#include <mgr/bitblit.h>
#include <stdio.h>

#include "defs.h"
#include "event.h"

#include "border.h"
#include "do_event.h"
#include "mouse_get.h"
#include "window_box.h"
#include "shape.h"

/* move_window */
void move_window()
{
    int button;
    int dx = BIT_WIDE(ACTIVE(border));
    int dy = BIT_HIGH(ACTIVE(border));
    int sx = ACTIVE(x0);
    int sy = ACTIVE(y0);

    move_box(mouse, &sx, &sy, dx, dy, 0);

    /* adjust window state */

    mousex += sx - ACTIVE(x0);
    mousey += sy - ACTIVE(y0);

    shape(sx, sy, dx, dy);
    bit_present(screen);
    do_event(EVENT_MOVE, active, E_MAIN);

    /* wait till button is released */
    do {
        button = mouse_get_wait(&sx, &sy);
    }

    while (button != 0);
}
