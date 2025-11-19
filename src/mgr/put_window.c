/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/* Terminal emulator */
#include <mgr/font.h>
#include <mgr/window.h>
#include <sys/time.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>

#include "defs.h"
#include "event.h"
#include "menu.h"

#include "proto.h"
#include "Write.h"
#include "border.h"
#include "do_event.h"
#include "do_menu.h"
#include "down_load.h"
#include "font_subs.h"
#include "get_font.h"
#include "get_info.h"
#include "get_menus.h"
#include "graphics.h"
#include "intersect.h"
#include "new_window.h"
#include "shape.h"
#include "subs.h"
#include "win_make.h"
#include "win_stack.h"
#include "win_subs.h"

/* fix the border color */

#define BORDER(win) \
    ((win == active) ? border(win, BORDER_FAT) : border(win, BORDER_THIN))

/* Scroll text from current line onwards */
#define TEXT_SCROLL_Y(delta_y) \
    { \
        SDL_Rect region = text_rect; \
        region.y = W(y) - fsizehigh; \
        region.h -= region.y; \
        texture_scroll(text, region, 0, (delta_y), W(bg_color)); \
    }

/* Scroll text within current line */
#define TEXT_SCROLL_X(delta_x) \
    { \
        SDL_Rect region = text_rect; \
        region.x = W(x); \
        region.w -= region.x; \
        region.y = W(y) - fsizehigh; \
        region.h = fsizehigh; \
        texture_scroll(text, region, (delta_x), 0, W(bg_color)); \
    }

/* fsleep is experimental */

#define fsleep() \
    { \
        struct timeval time; \
        time.tv_sec = 0; \
        time.tv_usec = 330000; \
        select(0, 0, 0, 0, &time); \
    }

#define B_SIZE8(w, h, d)  ((h) * ((((w * d) + 7L) & ~7L) >> 3))

/* set_winsize */
static void set_winsize(fd, rows, cols, ypixel, xpixel) int fd, rows, cols, ypixel, xpixel;
{
    struct winsize size;

    size.ws_row = rows;
    size.ws_col = cols;
    size.ws_xpixel = xpixel;
    size.ws_ypixel = ypixel;
    ioctl(fd, TIOCSWINSZ, &size);
    dbgprintf('t', (stderr, "SWINSZ ioctl %dx%d\n", rows, cols));
}

static void standout(WINDOW *win)
{
    if (W(flags) & W_STANDOUT) {
        return;
    }

#ifdef COLORSTANDOUT

    if (BIT_DEPTH(W(window)) > 1 && GETFCOLOR(~W(style)) != GETBCOLOR(W(style))) {
        W(style) = PUTFCOLOR(W(style), GETFCOLOR(~W(style)));
    } else
#endif /* COLORSTANDOUT */
    W(style) = PUTOP(BIT_NOT(W(style)), W(style));
    W(flags) |= W_STANDOUT;
}

static void standend(WINDOW *win)
{
    if (!(W(flags) & W_STANDOUT)) {
        return;
    }

#ifdef COLORSTANDOUT

    if (BIT_DEPTH(W(window)) > 1 && GETFCOLOR(~W(style)) != GETBCOLOR(W(style))) {
        W(style) = PUTFCOLOR(W(style), GETFCOLOR(~W(style)));
    } else
#endif /* COLORSTANDOUT */
    W(style) = PUTOP(BIT_NOT(W(style)), W(style));
    W(flags) &= ~W_STANDOUT;
}

/* set_size -- set the kernel's idea of the screen size */
void set_size(WINDOW *win)
{
    if (win == (WINDOW *)0) {
        return;
    }

    if (W(flags) & W_NOREPORT) {
        return;                         /* just return if user requested */
    }

    if (W(text.w) > 0) {
        set_winsize(ACTIVE(to_fd), W(text.h) / FSIZE(high), W(text.w) / FSIZE(wide));
    } else {
        SDL_Rect region = texture_get_rect(W(window));
        set_winsize(W(to_fd), region.h / FSIZE(high), region.w / FSIZE(wide));
    }
}

/* put_window -- send a string to a window, interpret ESCs, return # of processed character */
int put_window(WINDOW *win, unsigned char *buff, int buff_count)
{
    /* variables */
    register TEXTURE *window = W(window);      /* bitmap to update */
    register TEXTURE *text = NULL;             /* current text region */
    register int indx;                         /* index into buff */
    register int cnt;                          /* # of esc. numbers */
    register unsigned char c;                  /* current char */
    register int done = 0;                     /* set to 1 to exit */
    int bell = 0;                              /* 1 if screen flashed once */
    int fsizehigh, fsizewide;                  /* variables to save deref. */
    int offset = 0;                            /* font glyph offset */
    char tbuff[40];                            /* tmp space for replies */


    /* avoid repeated dereferencing of pointers */

    fsizehigh = FSIZE(high);
    fsizewide = FSIZE(wide);


    if (W(flags) & W_SPECIAL) {
        if (W(flags) & W_UNDER) {
            offset = MAXGLYPHS;
        }

        if (W(flags) & W_BOLD) {
            offset += 2 * MAXGLYPHS;
        }
    }

    if (W(text.w)) {
        text = texture_create_child(window, W(text));
    }

    if (text == NULL) {
        text = window;
    }

    if (W(flags) & W_ACTIVE && mousein(mousex, mousey, win, 0)) {
    }

    if (win == active) {
        cursor_off();
    }

    SDL_Rect text_rect = texture_get_rect(text);
    SDL_Rect screen_rect = texture_get_rect(screen);
    SDL_Rect window_rect = texture_get_rect(window);

    /* do each character */
    for (indx = 0; c = *buff++, indx < buff_count && !done; indx++) {
        switch (W(flags) & W_STATE) {
        /* W_TEXT -- down load a text string */
        case W_TEXT:
            cnt = W(esc_cnt);
            W(snarf[W(esc)[TEXT_COUNT]++]) = c;

            if (W(esc)[TEXT_COUNT] >= W(esc)[cnt]) {
                W(flags) &= ~W_TEXT;

                if (W(snarf) && W(code) != T_BITMAP && W(code) != T_GRUNCH) {
                    W(snarf)[W(esc)[TEXT_COUNT]] = '\0';
                    trans(W(snarf));
                }

                down_load(win, window, text);
                done++;
            }

            break;
        /* W_ANSI -- process an ANSI escape code */
        case W_ANSI:
            W(flags) &= ~W_ANSI;
            cnt = W(esc_cnt);

            switch (c) {
            /* ESC          -- turn on escape mode */
            case ESC:
                W(flags) |= W_ESCAPE;
                W(esc_cnt) = 0;
                W(esc[0]) = 0;
                break;
            /* 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 */
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
            {
                int n = W(esc)[W(esc_cnt)];

                if (n >= 0) {
                    n = n * 10 + (c - '0');
                } else {
                    n = n * 10 - (c - '0');
                }

                W(flags) |= W_ANSI;

                if (W(flags) & W_MINUS && n != 0) {
                    n = -n;
                    W(flags) &= ~(W_MINUS);
                }

                W(esc)[W(esc_cnt)] = n;
            }
            break;
            /* private sequence parameter bytes - ignore so we swallow */
            /* TODO - not properly swallowing, since we don't invalidate
             * the current command... */
            case '?': case '>': case '=': case '<':
                W(flags) |= W_ANSI;
                break;
            case EA_SEP:
                if (W(esc_cnt) + 1 < MAXESC) {
                    W(esc_cnt)++;
                }

                W(esc)[W(esc_cnt)] = 0;
                W(flags) &= ~(W_MINUS);
                W(flags) |= W_ANSI;
                break;
            /* E_MINUS      -- set the MINUS flag */
            case E_MINUS:
                W(flags) |= (W_ANSI | W_MINUS);
                break;
            /* E_NULL       -- do nothing */
            case E_NULL:
                done++;
                break;
            case EA_COLOR:
                for (int i = 0; i <= W(esc_cnt); ++i) {
                    int code = W(esc)[i];

                    switch (code) {
                    case 0:
                        W(fg_color) = C_BLACK;
                        W(bg_color) = C_WHITE;
                        break;
                    // Foreground colour
                    case 30: case 31: case 32: case 33:
                    case 34: case 35: case 36: case 37:
                        W(fg_color) = fg_colors[code - 30];
                        break;
                    // Background colour
                    case 40: case 41: case 42: case 43:
                    case 44: case 45: case 46: case 47:
                        W(bg_color) = bg_colors[code - 40];
                        break;
                    // Foreground bright colour
                    case 90: case 91: case 92: case 93:
                    case 94: case 95: case 96: case 97:
                        W(fg_color) = fg_bright_colors[code - 90];
                        break;
                    // Background bright colour
                    case 100: case 101: case 102: case 103:
                    case 104: case 105: case 106: case 107:
                        W(bg_color) = bg_bright_colors[code - 100];
                        break;
                    default:
                        break;
                    }
                }
                break;
            default:
                break;
            }
            break;
        /* W_ESCAPE -- process an escape code */
        case W_ESCAPE:
            W(flags) &= ~(W_ESCAPE);
            cnt = W(esc_cnt);

            switch (c) {
            /* ESC          -- turn on escape mode */
            case ESC:
                W(flags) |= W_ESCAPE;
                W(esc_cnt) = 0;
                W(esc[0]) = 0;
                break;
            /* 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 */
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
            {
                int n = W(esc)[W(esc_cnt)];

                if (n >= 0) {
                    n = n * 10 + (c - '0');
                } else {
                    n = n * 10 - (c - '0');
                }

                W(flags) |= W_ESCAPE;

                if (W(flags) & W_MINUS && n != 0) {
                    n = -n;
                    W(flags) &= ~(W_MINUS);
                }

                W(esc)[W(esc_cnt)] = n;
            }
                break;
            /* E_SEP1, E_SEP2 field seperators */
            case E_SEP1:
            case E_SEP2:

                if (W(esc_cnt) + 1 < MAXESC) {
                    W(esc_cnt)++;
                }

                W(esc)[W(esc_cnt)] = 0;
                W(flags) &= ~(W_MINUS);
                W(flags) |= W_ESCAPE;
                break;
            /* E_MINUS      -- set the MINUS flag */
            case E_MINUS:
                W(flags) |= (W_ESCAPE | W_MINUS);
                break;
            /* E_NULL       -- do nothing */
            case E_NULL:
                done++;
                break;
            /* E_ADDLINE    -- add a new line */
            case E_ADDLINE:
                TEXT_SCROLL_Y((*W(esc) || 1) * fsizehigh);
                done++;
                break;
            /* E_ADDCHAR    -- insert a character */
            case E_ADDCHAR:
                TEXT_SCROLL_X((*W(esc) || 1) * fsizewide);
                break;
            break;
            /* E_DELETELINE -- delete a line */
            case E_DELETELINE:         /* delete a line */
                TEXT_SCROLL_Y(-(*W(esc) || 1) * fsizehigh);
                done++;
            break;
            /* E_DELETECHAR -- delete a character */
            case E_DELETECHAR:
                TEXT_SCROLL_X(-(*W(esc) || 1) * fsizewide);
                break;
            /* E_UPLINE     -- up 1 line */
            case E_UPLINE:             /* up 1 line */
#ifdef FRACCHAR

                if (cnt > 0) { /* move up fractions of a character line */
                    int div = W(esc)[1] == 0 ? 1 : W(esc)[1];
                    int n = W(esc)[0] * fsizehigh / div;

                    if (W(y) > n) {
                        W(y) -= n;
                        break;
                    }

#endif

                if (W(y) > fsizehigh) {
                    W(y) -= fsizehigh;
                }

                break;
            /* E_RIGHT      -- right 1 line */
            case E_RIGHT:
#ifdef FRACCHAR

                if (cnt > 0) { /* move right/left a fraction of a character */
                    int div = W(esc)[1] == 0 ? 1 : W(esc)[1];
                    int n = W(esc)[0] * fsizewide / div;
                    W(x) += n;

                    if (W(x) < 0) {
                        W(x) = 0;
                    }

                    break;
                }

#endif
                W(x) += fsizewide;
                break;
            /* E_DOWN       -- down 1 line */
            case E_DOWN:               /* down 1 line */
#ifdef FRACCHAR

                if (cnt > 0) { /* move down a fraction of a character */
                    int div = W(esc)[1] == 0 ? 1 : W(esc)[1];
                    int n = W(esc)[0] * fsizehigh / div;

                    if (W(y) + n > text_rect.h) {
                        texture_scroll(text, text_rect, 0, n, W(bg_color));
                        done++;
                    } else {
                        W(y) += n;
                    }

                    break;
                }

#endif

                if (W(y) + fsizehigh > text_rect.h) {
                    texture_scroll(text, text_rect, 0, fsizehigh, W(bg_color));
                    done++;
                } else {
                    W(y) += fsizehigh;
                }

                break;
            /* E_FCOLOR     -- set forground color */
            case E_FCOLOR:

                if (W(flags) & W_STANDOUT) {
                    standend(win);
                    W(style) = W(flags) & W_REVERSE ?
                               PUTBCOLOR(W(style), *W(esc)):
                               PUTFCOLOR(W(style), *W(esc));
                    standout(win);
                } else {
                    W(style) = W(flags) & W_REVERSE ?
                               PUTBCOLOR(W(style), *W(esc)):
                               PUTFCOLOR(W(style), *W(esc));
                }

                BORDER(win);
                break;
            /* E_BCOLOR     -- set background golor */
            case E_BCOLOR:
                W(style) = W(flags) & W_REVERSE ?
                           PUTFCOLOR(W(style), *W(esc)):
                           PUTBCOLOR(W(style), *W(esc));
                BORDER(win);
                break;
            /* E_ANSI  -- accept an ANSI sequence */
            case E_ANSI:
                /* This is a hack since most programs these days like
                 * ANSI color sequences, and it's probably (at a minimum)
                 * worth swallowing these.
                 */
                W(flags) |= W_ANSI;
                break;
            /* E_STANDOUT   -- inverse video (characters) */
            case E_STANDOUT:
                standout(win);
                break;
            /* E_STANDEND   -- normal video (characters) */
            /* standend also sets character attributes */
            case E_STANDEND:
            {
                int mode = *W(esc);

                if (mode) {
                    //enhance_font(W(font));
                    done++;
                }

                offset = 0;

                if (mode & 1) {          /* standout */
                    standout(win);
                    offset = 1;
                } else {
                    standend(win);
                }

#if 0

                if (mode & 2) { /* bold */
                    W(flags) |= W_BOLD;
                    offset |= 2;
                } else {
                    W(flags) &= ~W_BOLD;
                }

                if (mode & 4) { /* underline */
                    W(flags) |= W_UNDER;
                    offset |= 4;
                } else {
                    W(flags) &= ~W_UNDER;
                }

#endif
                offset *= MAXGLYPHS;

                break;
            }
            /* E_CLEAREOL   -- clear to end of line */
            case E_CLEAREOL:           /* clear to end of line */
            {
                SDL_Rect eol_rect = {.x = W(x), .y = W(y) - fsizehigh, .w = text_rect.w - W(x), .h = fsizehigh};
                texture_fill_rect(text, eol_rect, W(bg_color));
                break;
            }
            /* E_CLEAREOS   -- clear to end of window */
            case E_CLEAREOS:           /* clear to end of window */
                SDL_Rect eol_rect = {.x = W(x), .y = W(y) - fsizehigh, .w = text_rect.w - W(x), .h = fsizehigh};
                texture_fill_rect(text, eol_rect, W(bg_color));
                SDL_Rect eos_rect = {.x = 0, .y = W(y), .w = text_rect.w, .h = text_rect.h - W(y)};
                texture_fill_rect(text, eos_rect, W(bg_color));
                break;
            /* E_SETCURSOR  -- set the character cursor */
            case E_SETCURSOR:
                W(curs_type) = *W(esc);
                break;
            /* E_BLEEP      -- highlight a section of the screen */
            case E_BLEEP:

#if 0
                if (cnt > 2) {
                    register int *p = W(esc);

                    if (p[0] < 0 || p[1] < 0) {
                        break;
                    }

                    p[2] = BETWEEN(1, p[2], screen_rect.w - 1);
                    p[3] = BETWEEN(1, p[3], screen_rect.w - 1);
                    bit_blit(screen, p[0], p[1], p[2], p[3], BIT_NOT(BIT_DST), 0, 0, 0);
                    fsleep();
                    bit_blit(screen, p[0], p[1], p[2], p[3], BIT_NOT(BIT_DST), 0, 0, 0);
                    done++;
                }
#endif

                break;
            /* E_FONT       -- pick a new font */
            case E_FONT:
                W(flags) &= ~W_SNARFABLE;
                W(flags) &= ~W_SPECIAL;       /* reset bold or underline */
                offset = 0;

                if (cnt > 0) {
                    W(esc)[TEXT_COUNT] = 0;

                    if (W(esc)[cnt] > 0 && (W(snarf) = malloc(W(esc)[cnt] + 1)) != 0) {
                        W(flags) |= W_TEXT;
                    }

                    W(code) = T_FONT;
                    break;
                }

                {
                    int font_count = W(esc)[cnt];
                    int baseline = FSIZE(baseline);

                    W(font) = Get_font(font_count);
                    fsizehigh = FSIZE(high);
                    fsizewide = FSIZE(wide);
                    W(y) += FSIZE(baseline) - baseline;

                    if (W(y) < fsizehigh) {
                        TEXT_SCROLL_Y(fsizehigh);
                        W(y) = fsizehigh;
                        done++;
                    }
                }
                set_size(win);
                break;
            /* E_MOUSE      -- move the mouse or change cursor shape  */
            case E_MOUSE:

#if 0
                if (cnt == 0 || (cnt == 1 && win == active)) {

                    if (cnt == 0) {
                        /* change mouse cursor shape */
                        int bn = W(esc)[0];

                        bit_destroy(W(cursor));

                        if (bn > 0 && bn <= MAXBITMAPS
                            && cursor_ok(W(bitmaps)[bn - 1])
                            && (W(cursor) = bit_alloc(16, 32, 0, 1)) != NULL) {
                            bit_blit(W(cursor), 0, 0, 16, 32, BIT_SRC,
                                     W(bitmaps)[bn - 1], 0, 0);
                        } else {
                            W(cursor) = &mouse_arrow;
                        }

                        SETMOUSEICON( DEFAULT_MOUSE_CURSOR);
                    } else {
                        /* move mouse */
                        mousex = W(esc)[0];
                        mousey = W(esc)[1];
                        mousex = BETWEEN(0, mousex, BIT_WIDE(screen) - 1);
                        mousey = BETWEEN(0, mousey, BIT_HIGH(screen) - 1);
                    }
                }
#endif

                break;
            /* E_SIZE       -- reshape window: cols, rows */
            case E_SIZE:

                if (!W(flags) & W_ACTIVE) {
                    break;
                }

                if (cnt >= 1) {
                    int cols = W(esc)[cnt - 1];
                    int lines = W(esc)[cnt];
                    int x = W(x0), y = W(y0);

                    if (cnt >= 3) {
                        x = W(esc)[0];
                        y = W(esc)[1];
                    }

                    if (win != active) {
                        cursor_off();
                    }

                    ACTIVE_OFF();
                    expose(win);
                    shape(x, y,
                          cols?cols * fsizewide + 2 * W(borderwid):
                          2 * W(borderwid) + window_rect.w,
                          lines?lines * fsizehigh + 2 * W(borderwid):
                          2 * W(borderwid) + window_rect.w);
                    ACTIVE_ON();

                    if (!(W(flags) & W_ACTIVE && mousein(mousex, mousey, win, 0))) {
                        done++;
                    }
                }

                break;
            /* E_PUTSNARF   -- put the snarf buffer */
            case E_PUTSNARF:

                if (snarf) {
                    Write(W(to_fd), snarf, strlen(snarf));
                }

                break;
            /* E_GIMME      -- snarf text into input queue */
            case E_GIMME:
                W(esc)[TEXT_COUNT] = 0;

                if (W(esc)[cnt] > 0 && W(esc)[cnt] < MAXSHELL &&
                    (W(snarf) = malloc(W(esc)[cnt] + 1)) != (char *)0) {
                    W(flags) |= W_TEXT;
                }

                W(code) = T_GIMME;
                break;
            /* E_GMAP       -- read a bitmap from a file */
            case E_GMAP:
                W(esc)[TEXT_COUNT] = 0;

                if (W(esc)[cnt] > 0 && W(esc)[cnt] < MAX_PATH &&
                    (W(snarf) = malloc(W(esc)[cnt] + 1)) != 0) {
                    W(flags) |= W_TEXT;
                }

                W(code) = T_GMAP;
                break;
            /* E_SMAP       -- save a bitmap on a file */
            case E_SMAP:
                W(esc)[TEXT_COUNT] = 0;

                if (W(esc)[cnt] > 0 && W(esc)[cnt] < MAX_PATH &&
                    (W(snarf) = malloc(W(esc)[cnt] + 1)) != 0) {
                    W(flags) |= W_TEXT;
                }

                W(code) = T_SMAP;
                break;
            /* E_SNARF      -- snarf text into the snarf buffer */
            case E_SNARF:
                W(esc)[TEXT_COUNT] = 0;

                if (W(esc)[cnt] >= 0 && /*** was just > */
                    (W(snarf) = malloc(W(esc)[cnt] + 1)) != 0) {
                    W(flags) |= W_TEXT;
                }

                W(code) = T_YANK;
                break;
            /* E_STRING     -- write text into the offscreen bitmap */
            case E_STRING:
                W(esc)[TEXT_COUNT] = 0;

                if (W(esc)[cnt] > 0 && (W(snarf) = malloc(W(esc)[cnt] + 1)) != 0) {
                    W(flags) |= W_TEXT;
                }

                W(code) = T_STRING;
                break;
            /* E_GRUNCH     -- graphics scrunch mode  (experimental) */
            case E_GRUNCH:             /* graphics scrunch mode  (experimental) */
                W(esc)[TEXT_COUNT] = 0;

                if (W(esc)[cnt] >= 0 && /*** was just > */
                    (W(snarf) = malloc(W(esc)[cnt] + 1)) != 0) {
                    W(flags) |= W_TEXT;
                }

                W(code) = T_GRUNCH;
                break;
#if 0
#ifdef XMENU
            /* E_XMENU      -- extended menu stuff */
            case E_XMENU:
                /* ^[3X remove menu 3 from window */
                /* ^[3,4X       select item 4 of menu 3 */
                /* ^[1,2,3X     display menu 3 at 1,2 */
                /* ^[1,2,3,4Xhighlight menu 3 item 4 at 1,2 */
            {
                register int *p = W(esc);
                register struct menu_state *menu;

                switch (cnt) {
                case 0:                        /* remove menu from display */

                    if (p[0] >= 0 && p[0] < MAXMENU && (menu = W(menus[p[0]]))) {
                        menu_remove(menu);
                    }

                    break;
                case 1:                        /* select active item */

                    if (p[0] >= 0 && p[0] < MAXMENU && (menu = W(menus[p[0]]))) {
                        menu->current = p[1];
                    }

                    break;
                case 2:                        /* display menu  on window */

                    if (p[2] >= 0 && p[2] < MAXMENU && (menu = W(menus[p[2]]))) {
                        menu_setup(menu, window, Scalex(p[0]), Scaley(p[1]), -1);
                    }

                    break;
                case 3:                        /* highlight menu item on window */

                    if (p[2] >= 0 && p[2] < MAXMENU &&
                        (menu = W(menus[p[2]])) && menu->menu) {
                        bit_blit(window, Scalex(p[0]) + MENU_BORDER,
                                 Scaley(p[1]) + (p[3] - 1) * menu->bar_sizey +
                                 MENU_BORDER,
                                 menu->bar_sizex, menu->bar_sizey,
                                 BIT_NOT(BIT_DST), 0, 0, 0);
                    }

                    break;
                }
            }
            break;
#endif
#endif
            /* E_MENU       -- get a menu */
            case E_MENU:               /* get a menu */
            {                          /* should be split into several cases */
                register int b = (W(esc)[0] < 0);      /* which button */
                register int n = ABS(W(esc)[0]);       /* menu number */

                /* setup menu pointer */

                if (cnt > 2) {
                    int parent = n;            /* parent menu # */
                    int child = W(esc[2]);     /* child menu number */
                    int item = W(esc[1]);      /* item # of parent */
                    int flags = W(esc[3]);     /* menu flags */

                    if (parent < 0 || parent >= MAXMENU || child >= MAXMENU ||
                        W(menus[parent]) == (struct menu_state *)0) {
                        break;
                    }

                    dbgprintf('M', (stderr, "Linking menu %d to parent %d at item %d\n",
                                    child, parent, item));

                    if (item < 0) {                            /* page link */
                        W(menus[parent])->next = child;
                    } else if (item < W(menus[parent])->count) { /* slide lnk */
                        menu_setnext(W(menus[parent]), item) = child;
                    }

                    /* menu flags */

                    if (flags > 0) {
                        W(menus[parent])->flags = flags;
                    }

                    break;
                }

                /* download a menu */

                if (cnt > 0) {
                    W(esc)[TEXT_COUNT] = 0;

                    if (W(menus)[n]) {
                        menu_destroy(W(menus)[n]);
                        W(menus)[n] = (struct menu_state *) 0;

                        if (W(menu[0]) == n) {
                            W(menu[0]) = -1;
                        }

                        if (W(menu[1]) == n) {
                            W(menu[1]) = -1;
                        }
                    }

                    if (W(esc)[cnt] > 0 && (W(snarf) = malloc(W(esc)[cnt] + 1)) != 0) {
                        W(flags) |= W_TEXT;
                        W(code) = T_MENU;
                    }

                    dbgprintf('M', (stderr, "downloading menu %d\n", n));
                }
                /* select menu number */
                else if (n < MAXMENU && W(menus)[n]) {
                    int last_menu = W(menu[b]);

                    dbgprintf('M', (stderr, "selecting menu %d on button %d\n", n, b));
                    W(menu[b]) = n;

                    if (last_menu < 0 && button_state == (b?BUTTON_1:BUTTON_2)) {
                        go_menu(b);
                    }
                } else {
                    W(menu[b]) = -1;
                }
            }
            break;
            /* E_EVENT      -- get an event */
            case E_EVENT:

                switch (cnt) {
                case 2:        /* append to an event */
                case 1:        /* set an event */
                    W(esc)[TEXT_COUNT] = 0;

                    if (W(esc)[cnt] > 0 && (W(snarf) = malloc(W(esc)[cnt] + 1))
                        != 0) {
                        W(flags) |= W_TEXT;
                        W(code) = T_EVENT;
                    }

                    break;
                case 0:
                    cnt = W(esc)[0];

                    if (!CHK_EVENT(cnt)) {
                        break;
                    }

                    EVENT_CLEAR_MASK(win, cnt);

                    if (W(events)[GET_EVENT(cnt)]) {
                        free (W(events)[GET_EVENT(cnt)]);
                        W(events)[GET_EVENT(cnt)] = NULL;
                    }

                    break;
                }

                break;
            /* E_SEND       -- send a message */
            case E_SEND:               /* send a message */
                W(esc)[TEXT_COUNT] = 0;

                if (W(esc)[cnt] > 0 && (W(snarf) = malloc(W(esc)[cnt] + 1)) != 0) {
                    W(flags) |= W_TEXT;
                    W(code) = T_SEND;
                }

                break;
            /* E_COLORPALETTE   -- set/get color palette entries  */
            case E_COLORPALETTE:
            {
                int i = W(esc)[0];
                unsigned int ind, r, g, b, maxi;

#if 0
                if (cnt == 0) {         /* read or allocate palette entry */
                    if (i >= 0) {
                        getpalette( screen, (unsigned int)i, &r, &g, &b, &maxi);
                        sprintf(tbuff, "COLOR %d %u %u %u %u\n", i, r, g, b, maxi);
                    } else {
                        /* TODO i = allocate_color( win); */
                        i = 0; /* TODO */

                        if (i >= 0) {
                            sprintf(tbuff, "YOURCOLOR %d\n", i);
                        } else {
                            sprintf(tbuff, "\n"); /* none available */
                        }
                    }

                    write(W(to_fd), tbuff, strlen(tbuff));
                } else if (cnt == 1) { /* free some colors */
                    /* TODO */
                    /* free_colors( win, (unsigned int)i, (unsigned int)W(esc)[1]); */
                } else if (cnt == 3) { /* find a color */
                    r = (unsigned int)i;
                    g = W(esc)[1];
                    b = W(esc)[2];
                    maxi = W(esc)[3];
                    /* TODO findcolor( screen, &ind, &r, &g, &b, &maxi); */
                    sprintf(tbuff, "COLOR %u %u %u %u %u\n", ind, r, g, b, maxi);
                    write(W(to_fd), tbuff, strlen(tbuff));
                } else if (cnt == 4) {  /* set palette entry */
                    r = W(esc)[1];
                    g = W(esc)[2];
                    b = W(esc)[3];
                    maxi = W(esc)[4];
                    setpalette( screen, (unsigned int)i, r, g, b, maxi);
                }

#endif
                break;
            }
            /* E_BITGET     -- transfer a bitmap from server to client */
            case E_BITGET:
            {
                fprintf(stderr, "MGR: E_PUSH disabled during libbitblit removal\n");

#if 0
                int offset = W(esc)[2];
                int which = *W(esc);
                int size = W(esc)[1];
                unsigned char *data;

                if (cnt <= 1 || which <= 0 || which > MAXBITMAPS) {
                    break;
                }

                TEXTURE *m = W(bitmaps)[which - 1];
                if (m == NULL) {
                    break;
                }

                SDL_Rect m_rect = texture_get_rect(m);

                if (size + offset <= B_SIZE8(BIT_WIDE(m), BIT_HIGH(m), BIT_DEPTH(m))) {
                    && (m = W(bitmaps)[which - 1]) != (BITMAP *)0
                    && size + offset <= 

                    data = bit_save(m);
                    write(W(to_fd), data + offset, size);
                    free(data);
                    /* write(W(to_fd),BIT_DATA(m)+offset,size); */
                }
#endif

                break;
            }
            /* E_BITVALUE   -- set/get pixel value */
            case E_BITVALUE:
            {
                fprintf(stderr, "MGR: E_BITVALUE disabled during libbitblit removal\n");

#if 0
                int m = W(esc)[0];
                int x = W(esc)[1];
                int y = W(esc)[2];
                int v = W(esc)[3];
                BITMAP *map;

                if ((cnt == 2 || cnt == 3) && m > 0 && m <= MAXBITMAPS) {
                    map = W(bitmaps)[m - 1];

                    if (map) {
                        if (cnt == 2) {       /* get pixel value */
                            sprintf(tbuff, "%d\n", bit_on(map, x, y));
                            write(W(to_fd), tbuff, strlen(tbuff));
                        } else {
                            v &= ~(~0 << BIT_DEPTH(map));
                            bit_point(map, x, y, PUTFCOLOR(BIT_SET, v));
                        }
                    }
                }
#endif

                break;
            }
            /* E_BITCRT     -- create/destroy a bitmap */
            case E_BITCRT:             /* create/destroy a bitmap */
            {
                fprintf(stderr, "MGR: E_BITVALUE disabled during libbitblit removal\n");

#if 0
                int bmnbr = W(esc)[0];

                switch (cnt) {
                case 0:                /* destroy a bitmap */

                    if (bmnbr > 0 && bmnbr <= MAXBITMAPS && W(bitmaps)[bmnbr - 1]) {
                        bit_destroy(W(bitmaps)[bmnbr - 1]);
                        W(bitmaps)[bmnbr - 1] = (BITMAP *) 0;
                    }

                    break;
                case 2:                /* create new bitmap - same depth as window */
                case 3:                /* " - specify depth, 1->1 bit, otherwise DEPTH */

                    if (bmnbr > 0 && bmnbr <= MAXBITMAPS && W(bitmaps)[bmnbr - 1]) {
                        int w = W(esc)[1];
                        int h = W(esc)[2];

                        W(bitmaps)[bmnbr - 1] =
                            bit_alloc( Scalex(w),
                                       Scaley(h),
                                       0,
                                       (cnt == 3 && W(esc)[3] == 1)
                                           ? 1: BIT_DEPTH(W(window)));
                        dbgprintf('B', (stderr, "%s: created bitmap %d (%d,%d)\r\n",
                                        W(tty), bmnbr, w, h));
                    }

                    break;
                }
#endif
            }
            break;
            /* E_BITLOAD    -- transfer a bitmap from client to server */
            case E_BITLOAD:

                if (cnt >= 4) {
                    if ((W(snarf) = malloc(W(esc[W(esc_cnt)]))) != 0) {
                        W(esc)[TEXT_COUNT] = 0;
                        W(code) = T_BITMAP;
                        W(flags) |= W_TEXT;
                    } else {
                        fprintf(stderr, "MGR: Can't allocate bitmap!\n");
                    }
                }

                break;
            /* E_SHAPE      -- reshape window, make it active */
            case E_SHAPE:


#if 0
                ACTIVE_OFF();

                if (win != active) {
                    cursor_off();
                    expose(win);
                }

                if (cnt >= 3) {
                    shape(W(esc)[cnt - 3], W(esc)[cnt - 2],
                          W(esc)[cnt - 1], W(esc)[cnt]);
                } else if (cnt == 1) {
                    shape(W(esc)[cnt - 1], W(esc)[cnt],
                          BIT_WIDE(W(border)),
                          BIT_HIGH(W(border)));
                }

                ACTIVE_ON();

                if (!(W(flags) & W_ACTIVE && mousein(mousex, mousey, win, 0))) {

                    done++;
                }
#endif

                break;
            /* E_BITBLT     -- do a bit blit */
            case E_BITBLT:             /* do a bit blit */
                win_rop(win, window);
                done++;
                break;
            /* E_CIRCLE     -- Plot a circle (or ellipse) */
            case E_CIRCLE:             /* Plot a circle (or ellipse) */
                circle_plot(win, window);
                break;
            /* E_LINE       -- Plot a line */
            case E_LINE:
                win_plot(win, window);
                break;
            /* E_GO         -- move graphics pointer */
            case E_GO:
                win_go(win);
                break;
            /* E_MOVE       -- move to x,y pixels */
            case E_MOVE:               /* move to x,y pixels */
                W(flags) &= ~W_SNARFABLE;

                if (cnt > 0) {
                    W(x) = Scalex(*W(esc));
                    W(y) = Scaley(W(esc)[1]);
                } else {
                    W(x) += Scalex(*W(esc));
                }

                if (W(x) + fsizewide > text_rect.w && !(W(flags) & W_NOWRAP)) {
                    W(x) = text_rect.w - fsizewide;
                }

                if (W(y) > text_rect.h) {
                    W(y) = text_rect.h - fsizehigh;
                }

                break;
            /* E_CUP        -- move to col,row (zero based) */
            case E_CUP:

                if (cnt < 1) {
                    break;
                }

                {
                    register int x = W(esc)[cnt - 1] * fsizewide;
                    register int y = W(esc)[cnt] * fsizehigh;

                    if (x == BETWEEN(-1, x, text_rect.w - fsizewide) &&
                        y == BETWEEN(-1, y, text_rect.h)) {
                        W(y) = y + fsizehigh;
                        W(x) = x;
                    }
                }
                break;
            /* E_VI         -- turn on vi hack */
            case E_VI:                 /* turn on vi hack */
                W(flags) |= W_VI;
                break;
            /* E_NOVI       -- turn off vi hack */
            case E_NOVI:               /* turn off vi hack */
                W(flags) &= ~W_VI;
                break;
            /* E_PUSH       -- push environment */
            case E_PUSH:               /* push environment */
                fprintf(stderr, "MGR: E_PUSH disabled during libbitblit removal\n");
                /*
                win_push(win, *W(esc));
                */
                break;
            /* E_POP        -- pop old environment */
            case E_POP:                /* pop old environment */
                fprintf(stderr, "MGR: E_POP disabled during libbitblit removal\n");
                /*
                win_pop(win);
                */

                if (!(W(flags) & W_ACTIVE && mousein(mousex, mousey, win, 0))) {
                    done++;
                }

                break;
            /* E_TEXTREGION -- setup text region */
            case E_TEXTREGION:         /* setup text region */

                switch (cnt) {
                case 1:        /* setup scrolling region (~aka vt100) */

                    if (W(esc)[0] >= 0 && W(esc)[1] >= W(esc)[0] &&
                        W(esc)[1] * fsizehigh < window_rect.h) {
                        W(text.x) = 0;
                        W(text.w) = window_rect.w;
                        W(text.y) = fsizehigh * W(esc[0]);
                        W(text.h) = fsizehigh * (1 + W(esc[1])) - W(text.y);

                        if (W(y) < W(text.y) + fsizehigh) {
                            W(y) = W(text.y) + fsizehigh;
                        }

                        if (W(y) > W(text.h)) {
                            W(y) = W(text.h);
                        }
                    }

                    break;
                case 3:                /* set up entire region */
                    W(text.w) = Scalex(W(esc[2]));
                    W(text.h) = Scaley(W(esc[3]));
                    W(text.x) = Scalex(W(esc[0]));
                    W(text.y) = Scaley(W(esc[1]));

                    if (W(text.h) >= fsizehigh * MIN_Y &&
                        W(text.w) >= fsizewide * MIN_X) {
                        W(x) = 0;
                        W(y) = fsizehigh;
                        W(flags) &= ~W_SNARFABLE;
                        break;
                    }

                    W(text.x) = 0;
                    W(text.y) = 0;
                    W(text.w) = 0;
                    W(text.h) = 0;
                    break;
                case 4:        /* set up entire region (use rows, cols) */
                    W(text.x) = W(esc[0]) * fsizewide;
                    W(text.y) = W(esc[1]) * fsizehigh;
                    W(text.w) = W(esc[2]) * fsizewide;
                    W(text.h) = W(esc[3]) * fsizehigh;

                    if (W(text.h) >= fsizehigh * MIN_Y &&
                        W(text.w) >= fsizewide * MIN_X) {
                        W(x) = 0;
                        W(y) = fsizehigh;
                        break;
                    }

                    break;
                case 0:                /* clear text region */

                    if (W(text.x) % fsizewide != 0 || W(text.y) % fsizehigh != 0) {
                        W(flags) &= ~W_SNARFABLE;
                    }

                    W(text.x) = 0;
                    W(text.y) = 0;
                    W(text.w) = 0;
                    W(text.h) = 0;
                    break;
                }

                done++;
#ifdef REGION_HACK
                set_size(win);  /* emacs trouble when it sets scroll region */
#endif
                break;
            /* E_SETMODE    -- set a window mode */
            case E_SETMODE:

                switch (W(esc)[0]) {
                case M_STANDOUT:
                    standout(win);
                    break;
                case M_BACKGROUND:     /* enable background writes */
                    W(flags) |= W_BACKGROUND;
                    break;
                case M_NOINPUT:        /* disable keyboard input */
                    W(flags) |= W_NOINPUT;
                    break;
                case M_AUTOEXPOSE:     /* auto expose upon write */
                    W(flags) |= W_EXPOSE;
                    break;
                case M_WOB:            /* set white on black */

                    if (W(flags) & W_REVERSE) {
                        break;
                    }

                    W(flags) |= W_REVERSE;
                    {
                        SDL_Color temp = W(fg_color);
                        W(fg_color) = W(bg_color);
                        W(bg_color) = temp;
                    }
                    texture_clear(window, W(bg_color));
                    BORDER(win);
                    break;
                case M_NOWRAP:         /* turn on no-wrap */
                    W(flags) |= W_NOWRAP;
                    break;
                case M_OVERSTRIKE:     /* turn on overstrike */
                    W(style) = PUTOP(W(op), W(style));
                    W(flags) |= W_OVER;
                    break;
                case M_ABS:            /* set absolute coordinate mode */
                    W(flags) |= W_ABSCOORDS;
                    break;
                case M_DUPKEY:         /* duplicate esc char from keyboard */
                    W(flags) |= W_DUPKEY;

                    if (cnt > 0) {
                        W(dup) = W(esc[1]) & 0xff;
                    } else {
                        W(dup) = DUP_CHAR;
                    }

                    break;
                case M_NOBUCKEY:       /* set no buckey interpretation mode */
                    W(flags) |= W_NOBUCKEY;
                    break;
                case M_CONSOLE: /* redirect console to this device */
                    set_console(win, 1);
                    break;
#ifndef NOSTACK
                case M_STACK:          /* enable event stacking */
                    EVENT_SET_MASK(win, EVENT_STACK);
                    break;
#endif
                case M_SNARFLINES:     /* only cut lines */
                    W(flags) |= W_SNARFLINES;
                    break;
                case M_SNARFTABS:      /* change spaces to tabs */
                    W(flags) |= W_SNARFTABS;
                    break;
                case M_SNARFHARD:      /* snarf even if errors */
                    W(flags) |= W_SNARFHARD;
                    break;
                case M_NOREPORT:        /* don't tell kernel our window size */
                    W(flags) |= W_NOREPORT;
                    break;
                case M_ACTIVATE:       /* activate the window */

                    if (win == active) {
                        break;
                    }

                    cursor_off();
                    ACTIVE_OFF();
                    expose(win);
                    ACTIVE_ON();
                    cursor_on();
                    done++;

                    if (!(W(flags) & W_ACTIVE && mousein(mousex, mousey, win, 0))) {
                        break;
                    }
                }

                break;
            /* E_CLEARMODE  -- clear a window mode */
            case E_CLEARMODE:

                switch (W(esc)[0]) {
                case M_STANDOUT:
                    standend(win);
                    break;
                case M_BACKGROUND:     /* don't permit background writes */
                    W(flags) &= ~W_BACKGROUND;
                    break;
                case M_NOINPUT:        /* permit keyboard input */
                    W(flags) &= ~W_NOINPUT;
                    break;
                case M_AUTOEXPOSE:     /* don't auto expose */
                    W(flags) &= ~W_EXPOSE;
                    break;
                case M_WOB:            /* set black-on-white */

                    if (!(W(flags) & W_REVERSE)) {
                        break;
                    }

                    W(flags) &= ~W_REVERSE;
                    {
                        SDL_Color temp = W(fg_color);
                        W(fg_color) = W(bg_color);
                        W(bg_color) = temp;
                    }
                    texture_clear(window, W(bg_color));
                    BORDER(win);
                    break;
                case M_NOWRAP:         /* turn off no-wrap */
                    W(flags) &= ~W_NOWRAP;
                    break;
                case M_OVERSTRIKE:     /* turn off overstrike */

                    if (W(flags) & W_STANDOUT) {
                        W(style) = PUTOP(BIT_NOT(BIT_SRC), W(style));
                    } else {
                        W(style) = PUTOP(BIT_SRC, W(style));
                    }

                    W(flags) &= ~W_OVER;
                    break;
                case M_ABS:            /* set relative coordinate mode */
                    W(flags) &= ~W_ABSCOORDS;
                    break;
                case M_DUPKEY:         /* reset keyboard dup-ky mode */
                    W(flags) &= ~W_DUPKEY;
                    break;
                case M_NOBUCKEY:       /* reset no buckey interpretation mode */
                    W(flags) &= ~W_NOBUCKEY;
                    break;
                case M_CONSOLE: /* quit console redirection to this device */
                    set_console(win, 0);
                    break;
#ifndef NOSTACK
                case M_STACK:          /* enable event stacking */
                    EVENT_CLEAR_MASK(win, EVENT_STACK);
                    break;
#endif
                case M_SNARFLINES:     /* only cut lines */
                    W(flags) &= ~W_SNARFLINES;
                    break;
                case M_SNARFTABS:      /* change spaces to tabs */
                    W(flags) &= ~W_SNARFTABS;
                    break;
                case M_SNARFHARD:      /* snarf even if errors */
                    W(flags) &= ~W_SNARFHARD;
                    break;
                case M_NOREPORT:        /* don't tell kernel our window size */
                    W(flags) &= ~W_NOREPORT;
                    break;
                case M_ACTIVATE:       /* hide the window */

                    if (!(W(flags) & W_ACTIVE) || next_window == 1) {
                        break;
                    }

                    if (win != active) {
                        cursor_off();
                    }

                    ACTIVE_OFF();
                    hide(win);
                    ACTIVE_ON();

                    if (win != active) {
                        cursor_on();
                    }

                    if (!(W(flags) & W_ACTIVE && mousein(mousex, mousey, win, 0))) {

                        done++;
                    }

                    break;
                }

                break;
            /* E_GETINFO    -- send window info back to shell */
            case E_GETINFO:
                get_info(win, window, text);
                break;
            /* E_MAKEWIN    -- make or goto a new window */
            case E_MAKEWIN:            /* make or goto a new window */
                win_make(win, indx);
                done++;
                break;
            /* E_HALFWIN    -- make a 1/2 window */
            case E_HALFWIN:            /* make a 1/2 window */
            {
                register int *p = W(esc);
                char *tty = (char *)0;

                if (cnt < 3 || cnt > 4) {
                    break;
                }

                if (win != active) {
                    cursor_off();
                }

                ACTIVE_OFF();

                switch (cnt) {
                case 4:
                    tty = half_window(p[0], p[1], p[2], p[3], p[4]);
                    break;
                case 3:
                    tty = half_window(p[0], p[1], p[2], p[3], -1);
                    break;
                }

                if (win != active) {
                    cursor_on();
                }

                if (W(flags) & W_DUPKEY) {
                    sprintf(tbuff, "%c %s\n", W(dup), tty?tty:"");
                } else {
                    sprintf(tbuff, "%s\n", tty?tty:"");
                }

                if (tty) {
                    ACTIVE_ON();
                }

                write(W(to_fd), tbuff, strlen(tbuff));

                done++;
            }
            break;
            /* default */
            default: break;
            }

            if (!(W(flags) & W_ESCAPE)) {
                W(flags) &= ~W_MINUS;
            }

            break;
        /* default -- normal character */
        default:
        {
            switch (c) {
            /* ESC -- turn on escape mode */
            case ESC:
                W(flags) |= W_ESCAPE;
                W(flags) &= ~(W_MINUS);
                W(esc_cnt) = 0;
                W(esc[0]) = 0;
                break;
            /* C_NULL -- null character, ignore */
            case C_NULL:
                break;
            /* C_BS -- back space */
            case C_BS:
                W(x) -= fsizewide;

                if (W(x) < 0) {
                    W(x) = 0;
                }

                break;
            /* C_FF -- form feed */
            case C_FF:
                texture_clear(text, W(bg_color));
                W(x) = 0;
                W(y) = fsizehigh;
                W(flags) |= W_SNARFABLE;
                done++;
                break;
            /* C_BELL -- ring the bell */
            case C_BELL:

                /* TODO */
                if (!bell++) {
                    //CLEAR(W(window),BIT_NOT(BIT_DST));
                    //CLEAR(W(window),BIT_NOT(BIT_DST));
                }

                break;
            /* C_TAB -- tab */
            case C_TAB:
                W(x) = ((W(x) / fsizewide + 8) & ~7) * fsizewide;

                if (W(x) + fsizewide >= text_rect.w) {
                    W(x) = 0;

                    if (W(y) + fsizehigh > text_rect.h) {
                        texture_scroll(text, text_rect, 0, fsizehigh, W(bg_color));
                        done++;
                    } else {
                        W(y) += fsizehigh;
                    }
                }

                break;
            /* C_RETURN -- return */
            case C_RETURN:
                W(x) = 0;
                break;
            /* C_NL -- line feed */
            case C_NL:                 /* line feed */

                if (W(y) + fsizehigh > text_rect.h) {
                    texture_scroll(text, text_rect, 0, fsizehigh, W(bg_color));
                    done++;
                } else {
                    W(y) += fsizehigh;
                }

                break;
            /* default -- print a character */
            default:

                if (W(y) > text_rect.h) {
                    W(y) = text_rect.h - fsizehigh;
                }

                /* TODO W(style) implementation... */
                SDL_Point char_loc = {.x = W(x), .y = W(y) - fsizehigh};
                texture_copy_withbg(text, char_loc, font->glyph[c], W(fg_color), W(bg_color));

                W(x) += fsizewide;

                if (W(x) + fsizewide > text_rect.w && !(W(flags) & W_NOWRAP)) {
                    W(x) = 0;
                    W(y) += fsizehigh;

                    if (W(y) > text_rect.h) {
                        W(y) -= fsizehigh;
                        texture_scroll(text, text_rect, 0, fsizehigh, W(bg_color));
                        done++;
                    }
                }

                break;
            }

            break;
        }
        }
    }

    cursor_on();

    /* this is probably wrong */
    if (text != window) {
        texture_destroy(text);
    }

    return(indx);
}
