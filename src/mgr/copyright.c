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

#include <mgr/font.h>
#include <pwd.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <security/pam_appl.h>

#include "defs.h"

#include "proto.h"
#include "font_subs.h"
#include "get_menus.h"
#include "graphics.h"

extern TEXTURE *default_font;

#define SSIZE   3               /* star size */

#define MAXZ 500 /* maximum z depth */
#define MAXZ 500 /* maximum z depth */
#define NSTARS 256 /* maximum number of stars */
#define SPEED   4               /* star speed */
#define SCALE   (short)6        /* for rotator */
#define COUNT   (short)2        /* for rotator */
#define Random() ((unsigned int)rand())

/* variables */
static char *logo_icon_files[] =
{
    "server/ball_1", "server/ball_2", "server/ball_3", "server/ball_4", "server/ball_5", "server/ball_6", "server/ball_7", "server/ball_8"
};

static int w, h;
static int clockwise = 0;

static struct st {
    short x, y, z;
}
stars[NSTARS];   /* our galaxy */

static int pam_conv_func(int num_msg, const struct pam_message **msg,
                         struct pam_response **responses, void *appdata_ptr)
{
    *responses = calloc(num_msg, sizeof(struct pam_response));
    if (!*responses) return PAM_CONV_ERR;

    for (int i = 0; i < num_msg; i++) {
        if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
            (*responses)[i].resp = strdup(appdata_ptr);
        } else {
            (*responses)[i].resp = NULL;
        }
        (*responses)[i].resp_retcode = 0;
    }

    return PAM_SUCCESS;
}

int check_password(char *password)
{
    struct passwd *pw = getpwuid(getuid());
    if (!pw)
        return 0;

    pam_handle_t *pamh = NULL;
    struct pam_conv conv = { pam_conv_func, password };

    if (pam_start("login", pw->pw_name, &conv, &pamh) != PAM_SUCCESS)
        return 0;

    int ret = pam_authenticate(pamh, 0);
    pam_end(pamh, ret);

    return ret == PAM_SUCCESS;
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

static int project(x, y, z)
register short x, y, z;
{
    /* one-point perspective projection */
    /* the offsets (maxh/2) and maxv/2) ensure that the
     * projection is screen centered
     */
    x = (x / z) + (w >> 1);
    y = (y / z) + (h >> 1);

    /* are we on the screen? If not, let the caller know*/
    if (x < 0 || x >= w || y < 0 || y >= h) {
        return 1;
    }

    SDL_Rect rect = {.x = x, .y = y, .w = SSIZE, .h = SSIZE};
    texture_fill_rect(NULL, rect, C_WHITE);
    return 0;
}

static void dofly()
{
    register short i;
    register struct st *stp;

    i = NSTARS;
    stp = stars;

    do {
        if ((stp->z -= SPEED) <= 0) { /* star went past us */
            stp->z = stp->x ? MAXZ : (Random() % MAXZ) + 1;
            stp->x = Random();
            stp->y = Random();
        } else {        /* rotate universe */
            cordic(&stp->x, &stp->y, SCALE, COUNT);
        }

        if (project(stp->x, stp->y, stp->z)) {
            /* if projection is off screen, get a new position */
            stp->x = Random();
            stp->y = Random();
            stp->z = MAXZ;
        }

        ++stp;
    } while (--i);
}

int calc_delay(int previous, int ms) {
    int remaining = ms + previous - SDL_GetTicks();
    return remaining > 0 ? remaining : 0;
}

/* copyright */
void copyright(int at_startup)
{
    int i = 0;
    char rbuf[64], *readp = rbuf;

    screen_size(&w, &h);

    TEXTURE *notice = texture_create_from_icon("server/cr");
    TEXTURE *logo[sizeof(logo_icon_files) / sizeof(logo_icon_files[0])];
    for (i = 0; i < sizeof(logo_icon_files) / sizeof(logo_icon_files[0]); ++i) {
        logo[i] = texture_create_from_icon(logo_icon_files[i]);
    }

    SDL_Rect notice_rect = texture_get_rect(notice);
    SDL_Rect logo_rect = texture_get_rect(logo[0]);

    SDL_Point notice_point = {
        .x = (w - notice_rect.w) / 2,
        .y = (3 * h - 2 * notice_rect.h) / 4
    };

    SDL_Point logo_point = {
        .x = (w - logo_rect.w) / 2,
        .y = (h - 2 * logo_rect.h) / 4
    };

#ifdef MESSAGE
    SDL_Point message_point = {
        .x = 10 - SSIZE,
        .y = mode.h - font->head.high - 10 - SSIZE
    };

    TEXTURE *message = texture_create_empty(SSIZE + strlen(MESSAGE) * font->head.wide, SSIZE + font->head.high);
    SDL_Rect message_rect = texture_get_rect(message);
    put_str(message, SSIZE, 10 + SSIZE, font, C_WHITE, C_BLACK, MESSAGE);
#endif

    /* keep drawing stars until enough read from kb to stop */
    for (;;) {
        int previous = SDL_GetTicks();
        screen_render(0);
        if (at_startup) {
            texture_copy(NULL, notice_point, notice, C_WHITE);
        }
        dofly();
        if (at_startup) {
            texture_copy_withbg(NULL, logo_point, logo[(++i / 2) % 8], C_WHITE, C_BLACK);
#ifdef MESSAGE
            texture_copy_withbg(NULL, message_point, message, C_WHITE, C_BLACK);
#endif
        }
        screen_flush();
        screen_present();

        SDL_Event event;

        while (SDL_WaitEventTimeout(&event, calc_delay(previous, 65))) {
            if (event.type == SDL_QUIT) {
                // TODO
                exit(1);
            }

            if (at_startup) {
                if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_KEYDOWN) {
                    goto exit;
                }
            } else {
                switch (event.type) {
                case SDL_TEXTINPUT:
                    int len = strlen(event.text.text);
                    memcpy(readp, event.text.text, len);
                    readp += len;
                    break;
                case SDL_KEYDOWN:
                    switch(event.key.keysym.sym) {
                    case SDLK_BACKSPACE:
                        if (readp > rbuf) {
                            readp -= 1;
                        }
                        break;
                    case SDLK_RETURN:
                        *readp = '\0';

                        if (check_password(rbuf)) {
                            memset(rbuf, 0, sizeof(rbuf));
                            goto exit;
                        }

                        readp = rbuf;
                        clockwise = !clockwise;
                        break;
                    default:
                        break;
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

exit:
    texture_destroy(notice);
#ifdef MESSAGE
    texture_destroy(message);
#endif
    for (i = 0; i < sizeof(logo_icon_files) / sizeof(logo_icon_files[0]); ++i) {
        texture_destroy(logo[i]);
    }
}
