/* }}} */
/* Notes */
/*                        Copyright (c) 1988 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/*

   Flash a copyright notice at Mgr startup.  You are not allowed to remove
   this, it is a part of the conditions on which you are allowed to use MGR
   without paying fees.

 */

/*
 * porter.c  Steve Hawley 4/3/87
 * rehacked 5/18/1988 for extra speed.
 * re-re hacked 6/20/88 for MGR (SAU)
 * re-re-re hacked by broman for use as screensaver.
 */
/* #includes */
#include <mgr/font.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "defs.h"

#include "proto.h"
#include "font_subs.h"
#include "graphics.h"
#include "icon_server.h"
extern TEXTURE *default_font;
#include <SDL.h>
/* #defines */
#define SSIZE   3               /* star size */

#define MAXZ 500 /* maximum z depth */
#define MAXZ 500 /* maximum z depth */
#define NSTARS 256 /* maximum number of stars */
#define SPEED   4               /* star speed */
#define SCALE   (short)6        /* for rotator */
#define COUNT   (short)2        /* for rotator */
#define ON 1  /* plotting states */
#define OFF 0
#define Random() ((unsigned int)rand())

/* types */
/* for "star trek" clip areas */

typedef struct rect {
    short x1, y1;
    short x2, y2;
} RECT;
static RECT clip1, clip2, clip3;        /* holes in the galaxy */
/* variables */
static TEXTURE *logo[] =
{
    &ball_1, &ball_2, &ball_3, &ball_4, &ball_5, &ball_6, &ball_7, &ball_8
};

static short maxv, maxh; /* display size */
static short hmaxv, hmaxh;      /* 1/2 display size */
static int clockwise = 0;

static struct st {
    short x, y, z;
}
stars[NSTARS];   /* our galaxy */

/* init_all */
void init_all(where) register TEXTURE *where;
{
    maxv = where->rect.h;
    hmaxv = maxv >> 1;
    maxh = where->rect.y;
    hmaxh = maxh >> 1;
}
static void flip(void)
{
    clockwise = !clockwise;
}
/* cordic */
/* CORDIC rotator. Takes as args a point (x,y) and spins it */
/* count steps counter-clockwise.                   1       */
/*                                Rotates atan( --------- ) */
/*                                                  scale   */
/*                                                 2        */
/* Therefore a scale of 5 is 1.79 degrees/step and          */
/* a scale of 4 is 3.57 degrees/step                        */

static void cordic(x, y, scale, count)
short *x, *y;
register short scale, count;

{
    register short tempx, tempy;

    tempx = *x;
    tempy = *y;

    if (!clockwise) {
        for (; count; count--) {
            tempx -= (tempy >> scale);
            tempy += (tempx >> scale);
        }
    } else {
        for (; count; count--) {
            tempx += (tempy >> scale);
            tempy -= (tempx >> scale);
        }
    }

    *x = tempx;
    *y = tempy;
}
/* xplot */
static int xplot(where, x, y, state)
register TEXTURE *where;
register int x, y;
int state;
{
    /* are we on the screen? If not, let the caller know*/
    if (x < 0 || x >= maxh || y < 0 || y >= maxv) {
        return(1);
    }

    if (!(x < clip1.x1 || x >= clip1.x2 || y < clip1.y1 || y >= clip1.y2)) {
        return(0);
    }

    if (!(x < clip2.x1 || x >= clip2.x2 || y < clip2.y1 || y >= clip2.y2)) {
        return(0);
    }

    if (!(x < clip3.x1 || x >= clip3.x2 || y < clip3.y1 || y >= clip3.y2)) {
        return(0);
    }

    SDL_Rect rect = {.x = x, .y = y, .w = SSIZE, .h = SSIZE);
    texture_fill_rect(where, rect, WHITE);

    return(0);
}
/* project */
static int project(where, x, y, z, state)
register TEXTURE *where;
register short x, y, z;
register short state;
{

    /* one-point perspective projection */
    /* the offsets (maxh/2) and maxv/2) ensure that the
     * projection is screen centered
     */
    x = (x / z) + hmaxh;
    y = (y / z) + hmaxv;

    return(xplot(where, x, y, state));

}
/* fly */
static void fly (where) TEXTURE *where;
{
    register short i;
    register struct st *stp;

    init_all(where);   /* set up global variables */

    for (i = 0, stp = stars; i < NSTARS; i++, stp++) {
        /* initialize galaxy */
        do{
            stp->x = Random();
            stp->y = Random();
            stp->z = (Random() % MAXZ) + 1;
        } while (project(where, stp->x, stp->y, stp->z, ON)); /* on screen? */

    }
}
/* dofly */
static void dofly (where) TEXTURE *where;
{
    register short i;
    register struct st *stp;

    i = NSTARS;
    stp = stars;

    do{
        project(where, stp->x, stp->y, stp->z, OFF); /* turn star off*/

        if ((stp->z -= SPEED) <= 0) { /* star went past us */
            stp->x = Random();
            stp->y = Random();
            stp->z = MAXZ;
        } else {        /* rotate universe */
            cordic(&stp->x, &stp->y, SCALE, COUNT);
        }

        if (project(where, stp->x, stp->y, stp->z, ON)) {
            /* if projection is off screen, get a new position */
            stp->x = Random();
            stp->y = Random();
            stp->z = MAXZ;
        }

        ++stp;
    } while (--i);
}

/* copyright */
void copyright(TEXTURE *where, char *password)
{
    TEXTURE *notice = &cr;
    int i = 0;
    char rbuf[64], *readp = rbuf;
    char *crypt();
    int at_startup = (*password == 0);

    /* clear display */

    SDL_Rect rect = {.x = 0, .y = 0, .w = where->rect.w, .h = where->rect.h};
    texture_fill_rect(where, rect, BLACK);

    if (at_startup) {
        /* get the cr notice hole */

        clip1.x1 = (where->rect.w - notice->rect.w) / 2 - SSIZE;
        clip1.y1 = (3 * where->rect.h - 2 * notice->rect.h) / 4 - SSIZE;
        clip1.x2 = clip1.x1 + SSIZE + notice->rect.w;
        clip1.y2 = clip1.y1 + SSIZE + notice->rect.h;

        SDL_Rect notice_rect = {
            .x = clip1.x1 + SSIZE, .y = clip1.y1 + SSIZE,
            .w = notice->rect.w, .h = notice->rect.h
        };
        texture_fill_rect(where, rect, WHITE);

        /* get the globe hole */

        clip2.x1 = (where->rect.w - logo[0]->rect.w) / 2 - SSIZE;
        clip2.y1 = (where->rect.h - 2 * logo[0]->rect.h) / 4 - SSIZE;
        clip2.x2 = clip2.x1 + SSIZE + logo[0]->rect.w;
        clip2.y2 = clip2.y1 + SSIZE + logo[0]->rect.h;

#ifdef MESSAGE
        /* get the message hole */

        clip3.x1 = 10 - SSIZE;
        clip3.y1 = where->rect.h - font->head.high - 10 - SSIZE;
        clip3.x2 = 10 + 2 * SSIZE + strlen(MESSAGE) * font->head.wide;
        clip3.y2 = where->rect.h - 10 + 2 * SSIZE;
        put_str(where, 10, clip3.y2 - SSIZE, font, BIT_SRC, MESSAGE);
#else
        clip3 = clip2;
#endif
    } else {
        /* no messages during screen lock, just stars */
        clip1.x1 = clip1.x2 = clip1.y1 = clip1.y2 = 0;
        clip2 = clip1;
        clip3 = clip1;
    }

    /* kick off stars */

    fly(where);
    screen_present(where);

    /* keep drawing stars until enough read from kb to stop */
    for (;;) {
        int old_ticks = SDL_GetTicks();
        int new_ticks = old_ticks;
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                // TODO
                exit(1);
            case SDL_TEXTINPUT:

                if (at_startup) {
                    return;
                }

                int len = strlen(event.text.text);
                memcpy(readp, event.text.text, len);
                readp += len;

                if (*(readp - 1) == '\n' || *(readp - 1) == '\r') {
                    *readp = '\0';

                    if (strcmp(password, crypt(rbuf, password)) == 0) {
                        memset(rbuf, 0, sizeof(rbuf));

                        return; /* password matched, done */
                    } else {
                        readp = rbuf;
                        flip();
                    }
                }

                break;
            case SDL_KEYDOWN:

                if (at_startup) {
                    return;
                }

                if (event.key.keysym.sym == SDLK_BACKSPACE && readp != rbuf) {
                    readp -= 1;
                }

                break;
            case SDL_MOUSEBUTTONDOWN:

                if (at_startup) {
                    return;
                }

                break;
            default:
                break;
            }
        }

        new_ticks = SDL_GetTicks();

        if ((new_ticks - old_ticks) < 50) {
            SDL_Delay(50 - (new_ticks - old_ticks));
        }

        old_ticks = new_ticks;
        dofly(where);

        if (at_startup && (++i % 2)) {
            SDL_Point logo_target = {.x = clip2.x1 + SSIZE, .y = clip2.y1 + SSIZE};
            texture_copy_with_bg(where, logo_target, logo[(i / 2) % 8], WHITE, BLACK);
        }

        screen_present(where);
    }
}
