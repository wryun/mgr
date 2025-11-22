/* Teminal emulator functions called from put_window() and down_load() */
/* #includes */
#include <mgr/bitblit.h>
#include <mgr/font.h>
#include <stdlib.h>
#include <stdio.h>

#include "defs.h"
#include "graph_subs.h"
#include "win_subs.h"
#include "graphics.h"

static void texture_bit_blit(
    TEXTURE *dst_map,     /* destination bitmap */
    int x_dst, int y_dst, /* destination coords */
    int wide, int high,   /* bitmap size */
    int op,               /* bitmap function */
    TEXTURE *src_map,     /* source bitmap */
    int x_src, int y_src  /* source coords */
    )
{
    op = op & 0xf; /* ignore colour info */

    if (src_map == NULL) {
        SDL_Rect dst_rect = {.x = x_dst, .y = y_dst, .w = wide, .h = high};
        switch (op) {
        case BIT_CLR:
            texture_fill_rect(dst_map, dst_rect, C_BLACK);
            break;
        case BIT_SET:
            texture_fill_rect(dst_map, dst_rect, C_WHITE);
            break;
        default:
            break;
        }
    } else {
        SDL_Point dst_point = {.x = x_dst, .y = y_dst};
        SDL_Rect src_rect = {.x = x_src, .y = y_src, .w = wide, .h = high};
        TEXTURE *src_texture = texture_create_child(src_map, src_rect);
        switch (op) {
        case BIT_OR:
            texture_copy(dst_map, dst_point, src_texture, C_WHITE);
            break;
        case BIT_SRC:
            texture_copy_withbg(dst_map, dst_point, src_texture, C_WHITE, C_BLACK);
            break;
        default:
            break;
        }
        texture_destroy(src_texture);
    }
}

static SDL_Color convert_op_to_color(WINDOW *win, int op) {
    op = 0xf & op;
    if (op == BIT_CLR) {
        return  W(bg_color);
    } else if (op == BIT_XOR) {
        /* We can't rely on SDL having the appropriate blend mode (and this would be hard
         * to pull off anyway with lines), and reading the pixels is hard and slow. Let's
         * just average the current colours and add some transparency.
         */
        SDL_Color c = {
            c.r = (W(fg_color).r & W(bg_color).r) + ((W(fg_color).r ^ W(bg_color).r) >> 1),
            c.g = (W(fg_color).g & W(bg_color).g) + ((W(fg_color).g ^ W(bg_color).g) >> 1),
            c.b = (W(fg_color).b & W(bg_color).b) + ((W(fg_color).b ^ W(bg_color).b) >> 1),
            c.a = 0x7f
        };
        return c;
    } else {
        /* Hopefully this makes sense! */
        return  W(fg_color);
    }

}

static void texture_bit_line(WINDOW *win, TEXTURE *dst, int x1, int y1, int x2, int y2, int op)
{
    SDL_Point start = {.x = x1, .y = y1};
    SDL_Point end = {.x = x2, .y = y2};

    texture_line(dst, start, end, convert_op_to_color(win, op));
}

/* win_rop -- Do raster ops */
void win_rop(WINDOW *win, TEXTURE *window)
{
    register int *p = W(esc);
    register int op;
    TEXTURE *target = window, *source = window;
    SDL_Rect window_rect = texture_get_rect(window);

    op = W(op);
    dbgprintf('B', (stderr, "%s: blit\t", W(tty)));

    switch (W(esc_cnt)) {
    /* 0 -- set raster op function */
    case 0:
        W(op) = PUTOP(*p, op);  /* change op, leave colors alone */

        if (W(flags) & W_OVER) {
            W(style) = PUTOP(*p, W(op));
        }

        dbgprintf('B', (stderr, "setting function %d\r\n", p[0]));
        dbgprintf('B', (stderr, "  new W(op): op=%d, fg=%d, bg=%d\n",
                        OPCODE(W(op)), GETFCOLOR(W(op)), GETBCOLOR(W(op))));
        break;
    /* 1 -- set raster op fg and bg colors */
    case 1:
        W(op) = BUILDOP(op,
                        p[0] >= 0?p[0]:GETFCOLOR(op),
                        p[1] >= 0?p[1]:GETBCOLOR(op));
        dbgprintf('B', (stderr, "setting colors %d, %d\r\n", p[0], p[1]));
        dbgprintf('B', (stderr, "  new W(op): op=%d, fg=%d, bg=%d\n",
                        OPCODE(W(op)), GETFCOLOR(W(op)), GETBCOLOR(W(op))));
        break;
    /* 3 -- ras_write */
    case 3:
        texture_bit_blit(window, Scalex(p[0]), Scaley(p[1]), Scalex(p[2]), Scaley(p[3]), op, (DATA *)0, 0, 0);
        break;
    /* 4 -- ras_write  specify dest */
    case 4:
        if (p[4] > 0) {
            if (p[4] > MAXBITMAPS) {
                break;
            }

            if (W(bitmaps)[p[4] - 1] == NULL) {
                W(bitmaps)[p[4] - 1] = texture_create_empty(Scalex(p[0]) + Scalex(p[2]), Scaley(p[1]) + Scaley(p[3]));
            }

            target = W(bitmaps)[p[4] - 1];
        }

        texture_bit_blit(target, Scalex(p[0]), Scaley(p[1]), Scalex(p[2]), Scaley(p[3]), op, 0, 0, 0);
        break;
    /* 5 -- ras_copy */
    case 5:
        texture_bit_blit(window, Scalex(p[0]), Scaley(p[1]), Scalex(p[2]), Scaley(p[3]), op, window, Scalex(p[4]), Scaley(p[5]));
        break;
    /* 7 -- ras_copy specify dst,src */
    case 7:

        if (p[6] > MAXBITMAPS || p[7] > MAXBITMAPS) {
            break;
        }

        if (p[6] > 0) {
            if (W(bitmaps)[p[6] - 1] == NULL) {
                W(bitmaps)[p[6] - 1] = texture_create_empty(Scalex(p[0]) + Scalex(p[2]), Scaley(p[1]) + Scaley(p[3]));
            }

            target = W(bitmaps)[p[6] - 1];
        }

        if (p[7] > 0) {
            source = window;
        }

        dbgprintf('B', (stderr, "blitting %d to %d (%d x %d)\r\n", p[7], p[6], p[2], p[3]));
        texture_bit_blit
        (
            target,
            Scalex(p[0]), Scaley(p[1]),
            Scalex(p[2]), Scaley(p[3]),
            op,
            source,
            Scalex(p[4]), Scaley(p[5])
        );
    }
}
/* win_map -- down load a bit map  - parse escape sequence */
void win_map(WINDOW *win, TEXTURE *window)
{
    /* variables */
    int cnt = W(esc_cnt);
    int *p = W(esc);
    int op = W(op);
    SDL_Rect window_rect = texture_get_rect(window);

#if 0
    if (W(code) == T_BITMAP) {
        /* convert external bitmap data from snarf buffer to tmp bitmap */
        W(bitmap) = bit_load(p[0], p[1], p[4], p[cnt], W(snarf));
        p[4] = p[5];
        p[5] = p[6];
        cnt--;
    }
#endif

#ifdef MOVIE
    SET_DIRTY(W(bitmap));
#endif

    switch (cnt) {
    /* 2 -- bitmap to graphics point */
    case 2:
        texture_bit_blit(window, Scalex(W(gx)), Scaley(W(gy)), p[0], p[1], op, W(bitmap), 0, 0);
        break;
    /* 3 -- bitmap to graphics point specify dest */
    case 3:

        if (p[2] > MAXBITMAPS) {
            break;
        }

        if (p[2] > 0 && W(bitmaps)[p[2] - 1] == NULL) {
            W(bitmaps)[p[2] - 1] = W(bitmap);
            W(bitmap) = NULL;
        } else {
            texture_bit_blit(p[2]?W(bitmaps)[p[2] - 1]:window, Scalex(W(gx)), Scaley(W(gy)), p[0], p[1], op, W(bitmap), 0, 0);
        }

        break;
    /* 4 -- bitmap to specified point */
    case 4:
        texture_bit_blit(window, p[2], p[3], p[0], p[1], op, W(bitmap), 0, 0);
        break;
    /* 5 -- bitmap to specified point specify dest */
    case 5:

        if (p[4] > MAXBITMAPS) {
            break;
        }

        if (p[4] > 0 && W(bitmaps)[p[4] - 1] == NULL) {
            W(bitmaps)[p[4] - 1] = W(bitmap);
            W(bitmap) = NULL;
        } else {
            texture_bit_blit(p[4]?W(bitmaps)[p[4] - 1]:window, p[2], p[3], p[0], p[1], op, W(bitmap), 0, 0);
        }

        break;
    }

    if (W(bitmap)) {
        texture_destroy(W(bitmap)); W(bitmap) = NULL;
    }

    if (W(snarf)) {
        free(W(snarf)); W(snarf) = NULL;
    }
}
/* win_plot -- plot a line */
void win_plot(WINDOW *win, TEXTURE *window)
{
    register int *p = W(esc);
    int op;
    SDL_Rect window_rect = texture_get_rect(window);

    op = W(op);

    switch (W(esc_cnt)) {
    /* 0 -- set cursor to graphics point */
    case 0:
        W(x) = Scalex(W(gx));
        W(y) = Scaley(W(gy));
        break;
    /* 1 -- draw to graphics point */
    case 1:
        texture_bit_line(win, window, Scalex(W(gx)), Scaley(W(gy)), Scalex(p[0]), Scaley(p[1]), op);
        W(gx) = p[0];
        W(gy) = p[1];
        break;
    /* 3 */
    case 3:
        texture_bit_line(win, window, Scalex(p[0]), Scaley(p[1]), Scalex(p[2]), Scaley(p[3]), op);
        W(gx) = p[2];
        W(gy) = p[3];
        break;
    /* 4 */
    case 4:

        if (p[4] == 0 || (p[4] > 0 && p[4] <= MAXBITMAPS && W(bitmaps)[p[4] - 1])) {
            texture_bit_line(win, p[4]?W(bitmaps)[p[4] - 1]:window, Scalex(p[0]), Scaley(p[1]), Scalex(p[2]), Scaley(p[3]), op);
        }

        break;
    }
}

/* grunch -- experimantal graphics crunch mode */
void grunch(WINDOW *win, TEXTURE *dst)
{
    register char *buf = W(snarf);
    register int cnt = W(esc)[W(esc_cnt)];
    int op;
    int penup = 0;
    int *p = W(esc);
    register int x, y, x1, y1;
    SDL_Rect window_rect = texture_get_rect(dst);

    op = W(op);

    /* set starting point */

    if (W(esc_cnt) > 1) {
        x = p[0];
        y = p[1];
    } else {
        x = W(gx);
        y = W(gy);
    }

    while (cnt-- > 0) {
        x1 = (*buf >> 4 & 0xf) - 8;
        y1 = (*buf & 0xf) - 8;

        if (x1 == 0 && y1 == 0) {
            penup = 1;
        } else if (penup == 0) {
            texture_bit_line(win, dst, Scalex(x), Scaley(y), Scalex(x + x1), Scaley(y + y1), op);
            dbgprintf('y', (stderr, "%s: line [%d] %d,%d + %d,%d\n", W(tty), op, x, y, x1, y1));
            x += x1;
            y += y1;
        } else {
            x += x1;
            y += y1;
            penup = 0;
        }

        buf++;
    }

    W(gx) = x;
    W(gy) = y;
}
/* circle_plot -- plot a circle */
void circle_plot(WINDOW *win, TEXTURE *window)
{
    register int *p = W(esc);
    int op;
    SDL_Rect window_rect = texture_get_rect(window);

    op = W(op);

    switch (W(esc_cnt)) {
    /* 0 -- draw a 'circle'  at graphics point */
    case 0:
        circle(window, Scalex(W(gx)), Scaley(W(gy)), Scalexy(p[0]), convert_op_to_color(win, op));
        break;
    /* 1 -- draw an 'ellipse' at graphics point */
    case 1:             /* draw an 'ellipse' at graphics point */
        ellipse(window, Scalex(W(gx)), Scaley(W(gy)),
                Scalex(p[0]), Scaley(p[1]), convert_op_to_color(win, op));
        break;
    /* 2 -- draw a 'circle' */
    case 2:
        circle(window, Scalex(p[0]), Scaley(p[1]), Scalexy(p[2]), convert_op_to_color(win, op));
        break;
    /* 3 -- draw an 'ellipse' */
    case 3:
        ellipse(window, Scalex(p[0]), Scaley(p[1]),
                Scalex(p[2]), Scaley(p[3]), convert_op_to_color(win, op));
        break;
    /* 4 -- draw an 'ellipse' to offscreen bitmap */
    case 4:

        if (p[4] > 0 && p[4] <= MAXBITMAPS && W(bitmaps)[p[4] - 1]) {
            ellipse(W(bitmaps)[p[4] - 1], Scalex(p[0]), Scaley(p[1]),
                    Scalex(p[2]), Scaley(p[3]), convert_op_to_color(win, op));
        }

        break;
    /* 5 -- draw an arc  ccw centered at p0,p1 */
    case 5:
        arc(window, Scalex(p[0]), Scaley(p[1]), Scalex(p[2]), Scaley(p[3]),
            Scalex(p[4]), Scaley(p[5]), convert_op_to_color(win, op));
        break;
    /* 6 -- draw an arc  ccw centered at p0,p1  to offscreen bitmap */
    case 6:

        if (p[6] > 0 && p[6] <= MAXBITMAPS && W(bitmaps)[p[6] - 1]) {
            arc(W(bitmaps)[p[6] - 1], Scalex(p[0]), Scaley(p[1]), Scalex(p[2]),
                Scaley(p[3]), Scalex(p[4]), Scaley(p[5]), convert_op_to_color(win, op));
        }

        break;
    }
}
/* win_go -- move the graphics pointer */
void win_go(WINDOW *win)
{
    register int *p = W(esc);

    switch (W(esc_cnt)) {
    /* 0 -- set the graphics point to cursor pos */
    case 0:
        W(gx) = W(x);
        W(gy) = W(y);
        break;
    /* 1,2 -- set the graphics point */
    case 1:
    case 2:
        W(gx) = p[0];
        W(gy) = p[1];
        break;
    }
}
#if 0
/* bmap_size -- determine bitmap style from size */
int bmap_size(int w, int h, int count)
{
    int format;  /* bitmap format */
    int bytes = count / h;      /* bytes/line */

    if (bytes == ((w + 31) & ~31)) { /* 8 bit, 32 bit aligned */
        format = 31 | 0x80;
    } else if (bytes == ((w + 15) & ~15)) { /* 8 bit 16 bit aligned */
        format = 15 | 0x80;
    } else if (bytes == ((w + 7) & ~7)) { /* 8 bit 8 bit aligned */
        format = 7 | 0x80;
    } else if (bytes * 8 == ((w + 31) & ~31)) { /* 1 bit, 32 bit aligned */
        format = 31;
    } else if (bytes * 8 == ((w + 15) & ~15)) { /* 1 bit 16 bit aligned */
        format = 15;
    } else if (bytes * 8 == ((w + 7) & ~7)) { /* 1 bit 8 bit aligned */
        format = 7;
    } else {                       /* unknown format */
        format = 0;
    }

    return(format);
}
#endif
