/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/* re-shape a window */
/* #includes */
#include <mgr/font.h>
#include <stdio.h>

#include "defs.h"
#include "event.h"

#include "border.h"
#include "do_button.h"
#include "do_event.h"
#include "erase_win.h"
#include "font_subs.h"
#include "graphics.h"
#include "intersect.h"
#include "put_window.h"
#include "subs.h"
#include "window_box.h"

/* shape -- reshape a window to specified dimensions */
int shape(int x, int y, int dx, int dy)
{
    int sx, sy, w, h;
    SDL_Rect screen_rect = texture_get_rect(screen);

    if (dx > 0) {
        sx = x; w = dx;
    } else {
        sx = x + dx; w = -dx;
    }

    if (dy > 0) {
        sy = y; h = dy;
    } else {
        sy = y + dy; h = -dy;
    }

    if (sx < 0) {
        sx = 0;
    }

    if (sx + w >= screen_rect.w) {
        w = screen_rect.w - sx;
    }

    if (sy + h >= screen_rect.h) {
        h = screen_rect.h - sy;
    }

    if (w < 2 * ACTIVE(borderwid) + ACTIVE(font)->head.wide * MIN_X ||
        h < 2 * ACTIVE(borderwid) + ACTIVE(font)->head.high * MIN_Y) {
        return -1;
    }

    /* adjust window state */
    ACTIVE(x0) = sx;
    ACTIVE(y0) = sy;
    TEXTURE *old_border = ACTIVE(border);
    TEXTURE *old_window = ACTIVE(window);
    ACTIVE(border) = texture_create_empty(w, h);
    SDL_Rect window_rect = {.x = ACTIVE(borderwid), .y = ACTIVE(borderwid), .w = w - ACTIVE(borderwid) * 2, .h = h - ACTIVE(borderwid) * 2};
    ACTIVE(window) = texture_create_child(ACTIVE(border), window_rect);
    texture_clear(ACTIVE(window), ACTIVE(bg_color));
    border(active, BORDER_THIN);
    SDL_Point p = {0};
    texture_copy(ACTIVE(window), p, old_window, C_WHITE);

    /* make sure character cursor is in a good spot */
    if (ACTIVE(x) > window_rect.w) {
        ACTIVE(x) = 0;
        ACTIVE(y) += ((int)(ACTIVE(font)->head.high));
    }

    if (ACTIVE(y) > window_rect.h) {
        ACTIVE(y) = window_rect.h - ((int)(ACTIVE(font)->head.high));
    }

    texture_destroy(old_window);
    texture_destroy(old_border);

    un_covered();
    set_size(active);

    return(0);
}
/* shape_window -- reshape a window with the mouse */
void shape_window(void)
{
    int dx = 16, dy = 16;

    SETMOUSEICON(mouse_box);
    move_mouse(screen, mouse, &mousex, &mousey, 0);
    SETMOUSEICON(DEFAULT_MOUSE_CURSOR);
    get_rect(mouse, mousex, mousey, &dx, &dy, 0);
    do_button(0);

    /* look for shape event here */
    do_event(EVENT_SHAPE, active, E_MAIN);

    (void) shape(mousex, mousey, dx, dy);
}
/* stretch_window -- stretch a window with the mouse */
#ifdef STRETCH
void stretch_window(void)
{
    int dx, dy;
    int x0, x1, y0, y1;
    SDL_Rect br = texture_get_rect(ACTIVE(border));

    SETMOUSEICON(mouse_box);
    move_mouse(screen, mouse, &mousex, &mousey, 0);
    SETMOUSEICON( DEFAULT_MOUSE_CURSOR);

    x0 = ACTIVE(x0);
    y0 = ACTIVE(y0);
    x1 = x0 + br.w;
    y1 = y0 + br.h;

    if (2 * (mousex - x0) < x1 - x0) {
        x0 = x1;
    }

    dx = mousex - x0;

    if (2 * (mousey - y0) < y1 - y0) {
        y0 = y1;
    }

    dy = mousey - y0;
    /* x0,y0 is corner farthest from mouse. x0+dx,y0+dx is mouse position */

    get_rect(mouse, x0, y0, &dx, &dy, 0);
    do_button(0);

    /* look for shape event here */
    do_event(EVENT_SHAPE, active, E_MAIN);

    (void) shape(x0, y0, dx, dy);
}
#endif /* STRETCH */
