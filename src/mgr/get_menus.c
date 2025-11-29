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

#include <SDL2/SDL.h>

#include "defs.h"
#include "menu.h"

#include "proto.h"
#include "font_subs.h"
#include "graphics.h"
#include "mouse_get.h"
/* #defines */
#define MAX_LIST        100     /* max number of choices */
#define HOT             4       /* distance to icon hot spot */

/*	The height of each selection area (i.e. word) in the pop-up menu.
        The 2 extra pixels create a 1-pixel boarder above and below each word.
 */
#define BAR_HEIGHT              (font->head.high + 2)

/* put_str -- put a character string into a bitmap - only used for menus */
void put_str(dest, x, y, font, fg_color, bg_color, str)
TEXTURE *dest;
register int x;
int y;
struct font *font;
SDL_Color fg_color;
SDL_Color bg_color;
register char *str;
{
    register unsigned char c;

    SDL_Point char_loc = {.x = x, .y = y - font->head.high};

    while ((c = *str++) != '\0') {
        texture_copy_withbg(dest, char_loc, font->glyph[c], fg_color, bg_color);
        char_loc.x += font->head.wide;
    }
}

void menu_render(struct menu_state *state)
{
    screen_render(1);
    texture_copy(NULL, state->menu_start, state->menu, C_WHITE);

    if (state->current >= 0) {
        SDL_Rect rect_sel = {
            .x = 0,
            .y = state->current * state->bar_sizey,
            .w = state->bar_sizex,
            .h = state->bar_sizey,
        };
        TEXTURE *inverse_sel = texture_create_child(state->inverse_inside, rect_sel);
        SDL_Point p = state->menu_start;
        p.x += MENU_BORDER + rect_sel.x;
        p.y += MENU_BORDER + rect_sel.y;
        texture_copy(NULL, p, inverse_sel, C_WHITE);
        texture_destroy(inverse_sel);
    }

    screen_present();
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
    TEXTURE *menu,              /* menu image */
            *inside;            /* box - border */
    TEXTURE *inverse_inside;
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

    menu = texture_create_empty(box_x, box_y);
    SDL_Rect menu_rect = {.x = MENU_BORDER, .y = MENU_BORDER, .w = size_x, .h = size_y};
    inside = texture_create_child(menu, menu_rect);

    inverse_inside = texture_create_empty(size_x, size_y);

    /* paint text into box */

    texture_clear(menu, C_WHITE);
    texture_clear(inside, C_BLACK);
    texture_clear(inverse_inside, C_WHITE);

    for (i = 0, incr = BAR_HEIGHT - 1; i < count; i++, incr += BAR_HEIGHT) {
        put_str(inside, 1, incr, font, C_WHITE, C_BLACK, list[i]);
        put_str(inverse_inside, 1, incr, font, C_BLACK, C_WHITE, list[i]);
    }

    /* save the menu state */

    if ((state = malloc(sizeof(struct menu_state))) == (struct menu_state *) 0) {
        texture_destroy(inside);
        texture_destroy(menu);
        texture_destroy(inverse_inside);

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
    state->screen = NULL;

    texture_destroy(inside);

    return(state);
}
/* menu_setup -- put the menu on the display */
struct menu_state *menu_setup(state, screen, x, y, start)
struct menu_state *state;       /* existing menu state */
TEXTURE *screen;                 /* where to put the menu */
int x, y;                        /* current offset of mouse on screen */
int start;                      /* preselected item */
{
    /* position the box on the screen */

    SDL_Rect screen_rect = texture_get_rect(screen);
    SDL_Rect menu_rect = texture_get_rect(state->menu);

    if (menu_rect.w > screen_rect.w ||
        menu_rect.h > screen_rect.h) {
        return NULL;
    }

    x = Min(x, screen_rect.w - menu_rect.w);
    y = Min(y, screen_rect.h - menu_rect.h - state->bar_sizey);
    y = Max(y, state->bar_sizey + HOT);

    /* initialize the menu */

    state->screen = screen;
    state->current = start;
    state->menu_start.x = x;
    state->menu_start.y = y;

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
    int x_mouse, y_mouse;
    int push;

    if (state == NULL) {
        return(-1);
    }

    SDL_Rect menu_rect = texture_get_rect(state->menu);

    SETMOUSEICON(mouse_bull);

    state->exit = 0;
    state->current = -1;

    /* track the mouse */
    do {
        push = mouse_get_poll(&x_mouse, &y_mouse);

        if (push == -1) {
            menu_render(state);
            push = mouse_get_wait(&x_mouse, &y_mouse);
        }

        if (x_mouse + HOT <= state->menu_start.x) {
            if (exit & EXIT_LEFT) {
                state->exit = EXIT_LEFT;
                break;
            }

            state->current = -1;
            continue;
        } else if (x_mouse + HOT >= state->menu_start.x + menu_rect.w) {
            if (exit & EXIT_RIGHT) {
                state->exit = EXIT_RIGHT;
                break;
            }

            state->current = -1;
            continue;
        } else if (y_mouse + HOT <= state->menu_start.y) {
            if (exit & EXIT_TOP) {
                state->exit = EXIT_TOP;
                break;
            }

            state->current = -1;
            continue;
        } else if (y_mouse + HOT >= state->menu_start.y + menu_rect.h) {
            if (exit & EXIT_BOTTOM) {
                state->exit = EXIT_BOTTOM;
                break;
            }

            state->current = -1;
            continue;
        }

        state->current = BETWEEN(0, state->count - 1, (y_mouse + HOT - state->menu_start.y - MENU_BORDER) / state->bar_sizey);
    } while (push != button);

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

    if (state->menu != NULL) {
        texture_destroy(state->menu);
    }

    if (state->inverse_inside != NULL) {
        texture_destroy(state->inverse_inside);
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
        tmp->menu = texture_clone(menu->menu);
        if (!tmp->menu) {
            free(tmp);
            return NULL;
        }
        tmp->inverse_inside = texture_clone(menu->inverse_inside);
        if (!tmp->inverse_inside) {
            texture_destroy(tmp->menu);
            free(tmp);
            return NULL;
        }
    }

    /* copy menu values */

    if (menu->action != NULL) {
        tmp->action = (struct menu_action *)
                      malloc(sizeof(struct menu_action) * menu->count);

        if (!tmp->action) {
            texture_destroy(tmp->menu);
            texture_destroy(tmp->inverse_inside);
            free(tmp);
            return NULL;
        }

        for (i = 0; i < menu->count; i++) {
            tmp->action[i].value = strcpy(malloc(strlen(menu->action[i].value) + 1), menu->action[i].value);
            tmp->action[i].next_menu = menu->action[i].next_menu;
        }
    }

    return(tmp);
}
