/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/* low level popup menu management routines */
/* #includes */
#include <mgr/bitblit.h>
#include <mgr/font.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "defs.h"
#include "menu.h"

#include "proto.h"
#include "font_subs.h"
#include "icon_server.h"
#include "mouse_get.h"
/* #defines */
#define MAX_LIST        100     /* max number of choices */
#define HOT             4       /* distance to icon hot spot */

#define Pr_ropall(S, color)  bit_rect(S, 0, 0, BIT_WIDE(S), BIT_HIGH(S), &color)

/*	The height of each selection area (i.e. word) in the pop-up menu.
        The 2 extra pixels create a 1-pixel boarder above and below each word.
 */
#define BAR_HEIGHT              (font->head.high + 2)

/* put_str -- put a character string into a bitmap - only used for menus */
void put_str(map, x, y, font, color, str)
BITMAP *map;
register int x;
int y;
struct font *font;
COLOR *color;
register char *str;
{
    register unsigned char c;
    register int wide = font->head.wide;
    register int high = font->head.high;

    while ((c = *str++) != '\0') {
        bit_blit_color(map, x, y - high, wide, high, color, NULL, font->glyph[c], 0, 0);
        x += wide;
    }
}
void menu_render(struct menu_state *state)
{
    bit_blit_color(screen, state->menu_startx, state->menu_starty, BIT_WIDE(state->menu), BIT_HIGH(state->menu),
                   &C_WHITE, NULL, state->menu, 0, 0);

    if (state->current >= 0) {
        int y_bar_start = state->current * state->bar_sizey;
        bit_blit_color(screen, state->menu_startx + MENU_BORDER, state->menu_starty + MENU_BORDER + y_bar_start,
                       state->bar_sizex, state->bar_sizey,
                       &C_WHITE, NULL, state->inverse_inside, 0, y_bar_start);
    }

    bit_present(screen);
}

/* menu_define -- allocate space for and initialize menu */
struct menu_state *menu_define(font, list, values, max, color)
struct font *font;              /* which font to use for menu */
char *list[];                   /* list of menu items */
char *values[];                 /* list of return values */
int max;                        /* max number of menu items */
int color;              /* raster op function containing the colors to use for the menus */
{
    register int i, incr, count; /* counters */
    int size_x = 0, size_y = 0;
    struct menu_state *state;   /* menu state */
    BITMAP *menu,                       /* menu image */
           *inside;             /* box - border */
    BITMAP *inverse_inside;
    int box_x, box_y;           /* dimensions of menu box */

    /* find size of box */

    for (count = 0; list[count] != (char *) 0 && count < (max > 0?max:MAX_LIST); count++) {
        size_x = Max(size_x, strlen(list[count]));
    }

    /*	The 2 extra pixels are to allow a 1-pixel border to the left and right
         of each word.
     */
    size_x = size_x * font->head.wide + 2;
    size_y = count * BAR_HEIGHT;
    box_x = size_x + 2 * MENU_BORDER;
    box_y = size_y + 2 * MENU_BORDER;

    /* build box */

    /* menus are DEPTH bits deep, even though they are just text.
        This is because their colors are fixed at creation time, and
        we'd have to cache the DEPTH version anyway
     */

    menu = bit_alloc(box_x, box_y, NULL_DATA, BIT_DEPTH(screen));
    inside = bit_create(menu, MENU_BORDER, MENU_BORDER, size_x, size_y);

    inverse_inside = bit_alloc(size_x, size_y, NULL_DATA, BIT_DEPTH(screen));

    /* paint text into box */

    Pr_ropall(menu, C_WHITE);
    Pr_ropall(inside, C_BLACK);
    Pr_ropall(inverse_inside, C_WHITE);

    for (i = 0, incr = BAR_HEIGHT - 1; i < count; i++, incr += BAR_HEIGHT) {
        put_str(inside, 1, incr, font, &C_WHITE, list[i]);
        put_str(inverse_inside, 1, incr, font, &C_BLACK, list[i]);
    }

    /* save the menu state */

    if ((state = malloc(sizeof(struct menu_state))) == (struct menu_state *) 0) {
        bit_destroy(inside);
        bit_destroy(menu);
        bit_destroy(inverse_inside);

        return(state);
    }

    /* get the values */

    if (values != (char **) 0) {
        state->action = malloc(count * sizeof(struct menu_action));

        if (state->action) {
            for (i = 0; i < count; i++) {
                state->action[i].value = strcpy(malloc(strlen(values[i]) + 1), values[i]);
                state->action[i].next_menu = -1;
            }
        }
    } else {
        state->action = (struct menu_action *) 0;
    }

    state->menu = menu;
    state->inverse_inside = inverse_inside;
    state->bar_sizex = size_x;
    state->bar_sizey = BAR_HEIGHT;
    state->count = count;
    state->current = 0;
    state->next = -1;
    state->flags = 0;
    state->screen = (BITMAP *) 0;

    bit_destroy(inside);

    return(state);
}
/* menu_setup -- put the menu on the display */
struct menu_state *menu_setup(state, screen, x, y, start)
struct menu_state *state;       /* existing menu state */
BITMAP *screen;                 /* where to put the menu */
int x, y;                        /* current offset of mouse on screen */
int start;                      /* preselected item */
{
    /* position the box on the screen */

    if (BIT_WIDE(state->menu) > BIT_WIDE(screen) ||
        BIT_HIGH(state->menu) > BIT_HIGH(screen)) {
        return((struct menu_state *) 0);
    }

    x = Min(x, BIT_WIDE(screen) - BIT_WIDE(state->menu));
    y = Min(y, BIT_HIGH(screen) -
            BIT_HIGH(state->menu) - state->bar_sizey);
    y = Max(y, state->bar_sizey + HOT);

    /* initialize the menu */

    state->screen = screen;
    state->current = start;
    state->menu_startx = x;
    state->menu_starty = y;

    //menu_render(state);

    return(state);
}
/* menu_get -- allow user to select an item */
int menu_get(state, mouse, button, exit)
struct menu_state *state;
int mouse;                      /* fd to read mouse data from */
int button;                     /* button termination condition (not yet)*/
int exit;                       /* off-menu exit codes */
{
    register BITMAP *inside;    /* the menu */
    int x_mouse, y_mouse;
    int push;

    if (state == (struct menu_state *) 0) {
        return(-1);
    }

    SETMOUSEICON(&mouse_bull);

    state->exit = 0;
    state->current = -1;

    /* set up text region */

    inside = bit_create(state->screen, state->menu_startx + MENU_BORDER,
                        state->menu_starty + MENU_BORDER - state->bar_sizey,
                        state->bar_sizex, state->bar_sizey * (state->count + 2));

    /* track the mouse */
    do {
        push = mouse_get_poll(&x_mouse, &y_mouse);

        if (push == -1) {
            menu_render(state);
            push = mouse_get_wait(&x_mouse, &y_mouse);
        }

        if (x_mouse + HOT <= state->menu_startx) {
            if (exit & EXIT_LEFT) {
                state->exit = EXIT_LEFT;
                break;
            }

            state->current = -1;
            continue;
        } else if (x_mouse + HOT >= state->menu_startx + BIT_WIDE(state->menu)) {
            if (exit & EXIT_RIGHT) {
                state->exit = EXIT_RIGHT;
                break;
            }

            state->current = -1;
            continue;
        } else if (y_mouse + HOT <= state->menu_starty) {
            if (exit & EXIT_TOP) {
                state->exit = EXIT_TOP;
                break;
            }

            state->current = -1;
            continue;
        } else if (y_mouse + HOT >= state->menu_starty + BIT_HIGH(state->menu)) {
            if (exit & EXIT_BOTTOM) {
                state->exit = EXIT_BOTTOM;
                break;
            }

            state->current = -1;
            continue;
        }

        state->current = BETWEEN(0, state->count - 1, (state->count + 2) * (y_mouse + HOT - state->menu_starty - MENU_BORDER) / BIT_HIGH(inside));
    } while (push != button);

    bit_destroy(inside);
    SETMOUSEICON(DEFAULT_MOUSE_CURSOR);

    return(0);
}
/* menu_remove -- remove the menu drom the screen, restore previous screen contents */
struct menu_state *menu_remove(state)
struct menu_state *state;
{
    return(state);
}
/* menu_destroy -- free space associated with a menu */
int menu_destroy(state)
struct menu_state *state;
{
    register int i;

    menu_remove(state);

    if (state->menu != (BITMAP *)0) {
        bit_destroy(state->menu);
    }

    if (state->action != (struct menu_action *) 0) {
        for (i = 0; i < state->count; i++) {
            if (state->action[i].value) {
                free(state->action[i].value);
            }
        }

        free(state->action);
    }

    free(state);

    return(0);
}
/* menu_copy -- copy a menu  - for environment stacks */
struct menu_state *menu_copy(menu)
register struct menu_state *menu;
{
    register struct menu_state *tmp;
    register int i;

    if (menu == (struct menu_state *) 0) {
        return(menu);
    }

    if ((tmp = (struct menu_state *) malloc(sizeof(struct menu_state)))
        == (struct menu_state *) 0) {
        return((struct menu_state *) 0);
    }

    (void) memcpy(tmp, menu, sizeof(struct menu_state));

    /* copy menu image */

    if (menu->menu) {
        tmp->menu = bit_alloc(BIT_WIDE(menu->menu),
                              BIT_HIGH(menu->menu), NULL_DATA, BIT_DEPTH(menu->menu));
        bit_blit_color(tmp->menu, 0, 0, BIT_WIDE(tmp->menu), BIT_HIGH(tmp->menu),
                       &C_WHITE, NULL, menu->menu, 0, 0);
        tmp->inverse_inside = bit_alloc(BIT_WIDE(menu->inverse_inside),
                                        BIT_HIGH(menu->inverse_inside), NULL_DATA, BIT_DEPTH(menu->inverse_inside));
        bit_blit_color(tmp->inverse_inside, 0, 0, BIT_WIDE(tmp->inverse_inside), BIT_HIGH(tmp->inverse_inside),
                       &C_WHITE, NULL, menu->inverse_inside, 0, 0);
    }

    /* copy menu values */

    if (menu->action != (struct menu_action *) 0) {
        tmp->action = (struct menu_action *)
                      malloc(sizeof(struct menu_action) * menu->count);

        if (tmp->action) {
            for (i = 0; i < menu->count; i++) {
                tmp->action[i].value = strcpy(malloc(strlen(menu->action[i].value) + 1), menu->action[i].value);
                tmp->action[i].next_menu = menu->action[i].next_menu;
            }
        }
    }

    return(tmp);
}
