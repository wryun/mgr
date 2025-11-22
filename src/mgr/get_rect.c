/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/* sweep out a rectangle */

/* #includes */
#include <mgr/bitblit.h>

#include "defs.h"

#include "graphics.h"
#include "mouse_get.h"

/* box -- draw a box */
void box(x1, y1, dx, dy)
int x1, y1, dx, dy;
{
    if (dx < 0) {
        x1 += dx, dx = -dx;
    }

    if (dy < 0) {
        y1 += dy, dy = -dy;
    }

    if (dx < 3) {
        dx = 3;
    }

    if (dy < 3) {
        dy = 3;
    }

    SDL_Rect rect = {
        .x = x1,
        .y = y1,
        .w = dx,
        .h = dy,
    };
    texture_rect(NULL, rect, C_GREY_ALPHA, 2);
}
/* get_rect */
void get_rect(mouse, x, y, dx, dy, type)
TEXTURE *screen;         /* where to sweep out the box */
int mouse;                      /* file to get mouse coords from */
int x, y;                        /* starting position */
register int *dx, *dy;           /* box width,height */
int type;                       /* rectangle or line */
{
    int x_mouse = x, y_mouse = y;
    register int button;

    for (;;) {
        *dx = x_mouse - x;
        *dy = y_mouse - y;
        screen_render();
        if (type == 1) {
            SDL_Point start = {.x = x, .y = y};
            SDL_Point end = {.x = x + *dx, .y = y + *dy};
            texture_line(NULL, start, end, C_GREY_ALPHA);
        } else {
            box(x, y, *dx, *dy);
        }
        screen_present();
        screen_flush();
        button = mouse_get_wait(&x_mouse, &y_mouse);

        do {
            if (button == 0) {
                return;
            }
        } while ((button = mouse_get_poll(&x_mouse, &y_mouse)) != -1);
    }
}
