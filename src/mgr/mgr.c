/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/* main routine for MGR */

#include <mgr/font.h>
#include <mgr/share.h>
#include <sys/time.h>
#undef P_ALL
#include <sys/wait.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#if defined(sun)
#define NSIG 32
#endif
#include <stdlib.h>
#include <stdio.h>

#include <SDL2/SDL.h>

#include "defs.h"
#include "event.h"
#include "menu.h"
#include "version.h"

#include "proto.h"
#include "border.h"
#include "copyright.h"
#include "destroy.h"
#include "do_buckey.h"
#include "do_button.h"
#include "do_event.h"
#include "erase_win.h"
#include "font_subs.h"
#include "graphics.h"
#include "mouse_get.h"
#include "put_window.h"
#include "sigdata.h"
#include "set_mode.h"
#include "startup.h"
#include "subs.h"

/* variables */
static struct timespec set_poll = {
    (long)0, (long)(POLL_INT * 1000)
};
static char *mouse_dev = MOUSE_DEV;
static char *mouse_type = NULL;
TEXTURE *pattern;
#ifdef MOVIE
char *log_command = NULL;         /* process to pipe logging info to */
FILE *log_file = NULL;            /* file pointer for logging */
static int log_now = 0;
#endif
void redo_select(void);

#define HEIGHT 600
#define WIDTH 800

/* sig_child -- catch dead children */
static void sig_child(sig) int sig;
{
    WINDOW *win;
    pid_t pid;
    int status, someonedied;

    /* see if a shell has died, mark deleted */

    dbgprintf('d', (stderr, "Looking for dead windows\r\n"));

#ifdef WSTOPSIG /* POSIX */
    pid = waitpid(-1, &status, WUNTRACED);
    someonedied = pid > 0 && !WIFSTOPPED(status);
#else
#ifdef WUNTRACED        /* BSD */
    pid = wait3(&status, WUNTRACED, (void *)NULL);
    someonedied = pid > 0 && !WIFSTOPPED(status);
#else                   /* other Unix */
    pid = wait(&status);
    someonedied = pid > 0;
#endif
#endif

    if (someonedied) {
        win = active;

        for (win = active; win != (WINDOW *)0; win = W(next)) {
            if (W(pid) == pid && !(W(flags) & W_NOKILL) && kill(W(pid), 0) != 0) {
                W(flags) |= W_DIED;
                dbgprintf('d', (stderr, "window %d, tty %s, pid %d\r\n",
                                W(num), W(tty), W(pid)));
            }
        }
    }

    signal(SIGCHLD, sig_child);
}
/* sig_share -- switch logging on/off */
static void sig_share(int n)
{
# ifdef MOVIE

    if (n == SIGUSR1) {
        log_start(log_file);
    } else {
        log_end();
    }

# endif
}

/* could use faster longs instead of chars if sizeof(fd_set)%sizeof(long)==0 */
typedef char FDword;

int FD_COMMON( fd_set *left, fd_set *right)
{
    /* predicate returns true iff any fds are set in both left and right */
    FDword *lp = (FDword *)left;
    FDword *ep = (FDword *)(left + 1);
    FDword *rp = (FDword *)right;

    while (lp < ep) {
        if (*lp++ & *rp++) {
            return 1;
        }
    }

    return 0;
}

void FD_SET_DIFF( fd_set *target, fd_set *left, fd_set *right)
{
    /* assigns the set difference of left-right to target */
    FDword *tp = (FDword *)target;
    FDword *lp = (FDword *)left;
    FDword *ep = (FDword *)(left + 1);
    FDword *rp = (FDword *)right;

    while (lp < ep) {
        *tp++ = *lp++ & ~(*rp++);
    }
}
#ifdef DEBUG
#define HD(_fds) (*(unsigned long int *)&(_fds))
#endif


/* protected by mutex */
static fd_set reads;                        /* masks, result of select */


void read_fds_into_windows()
{
    register WINDOW *win;               /* current window to update */
    int shellbuf = MAXSHELL;            /* # chars processed per shell */

    /* process shell output */

    for (win = active; win != (WINDOW *) 0; win = W(next)) {
        /* read data into buffer */

        if (W(from_fd) && FD_ISSET( W(from_fd), &reads)
            && !FD_ISSET( W(from_fd), &to_poll)) {
            W(current) = 0;

            if ((W(max) = read(W(from_fd), W(buff), shellbuf - 1)) > 0) {
                FD_SET( W(from_fd), &to_poll);
                dbgprintf('p', (stderr, "%s: reading %d [%.*s]\r\n", W(tty),
                                W(max), W(max), W(buff)));
            } else {
                FD_CLR( W(from_fd), &to_poll);
#ifdef KILL

                if (W(flags) & W_NOKILL) {
                    W(flags) |= W_DIED;
                }

#endif
#ifdef DEBUG

                if (debug) {
                    fprintf(stderr, "%s: read failed after select on fd(%d) returning %d\r\n",
                            W(tty), W(from_fd), W(max));
                    perror(W(tty));
                }

#endif
            }
        }
    }
}

int update_windows()
{
    register WINDOW *win;               /* current window to update */
    register int count;                 /* # chars read from shell */
    register int i;                     /* counter */
    int maxbuf = MAXBUF;                /* # chars processed per window */
    int dirty = 0;

    /* see if any window died */
    for (win = active; win != (WINDOW *) 0;) {
        if (W(flags) & W_DIED) {
            dbgprintf('d', (stderr, "Destroying %s-%d\r\n", W(tty), W(num)));
            destroy(win);
            win = active;
            dirty = 1;
        } else {
            win = W(next);
        }
    }

    int should_redo_select = 0;

    for (win = active; win != (WINDOW *) 0; win = W(next)) {
        /* check for window to auto-expose */

        if (W(from_fd) && FD_ISSET( W(from_fd), &to_poll)
            && W(flags) & W_EXPOSE && !(W(flags) & W_ACTIVE)) {
            dbgprintf('o', (stderr, "%s: activating self\r\n", W(tty)));
            cursor_off();
            ACTIVE_OFF();
            expose(win);
            ACTIVE_ON();
            cursor_on();

            dirty = 1;
        }

        /* write data into the window */

        if (W(from_fd) && FD_ISSET( W(from_fd), &to_poll)
            && W(flags) & (W_ACTIVE | W_BACKGROUND)) {

#ifdef PRIORITY                 /* use priority scheduling */

            if (win == active) {
                count = Min(maxbuf, W(max) - W(current));
            } else if (W(flags) & W_ACTIVE) {
                count = Min(maxbuf >> 1, W(max) - W(current));
            } else {
                count = Min(maxbuf >> 2, W(max) - W(current));
            }

#else                           /* use round robin scheduling */
            count = Min(maxbuf, W(max) - W(current));
#endif

            i = put_window(win, W(buff) + W(current), count);
            dirty = 1;
            dbgprintf('w', (stderr, "%s: writing %d/%d %.*s [%.*s]\r\n",
                            W(tty), i, count, i, W(buff) + W(current), count - i,
                            W(buff) + W(current) + i));

            W(current) += i;

            if (W(current) >= W(max)) {
                FD_CLR( W(from_fd), &to_poll);
                should_redo_select = 1;
            }
        }
    }

    if (should_redo_select) {
        redo_select();
    }

#ifdef MOVIE
    log_time();
#endif

    return dirty;
}


void display_windows()
{
    register WINDOW *win;               /* current window to update */

    if (active == NULL) {
        screen_render();

        return;
    }

    win = active->prev;

    do {
        SDL_Point window_point = {.x = W(x0), .y = W(y0)};
        texture_copy(screen, window_point, W(border), C_WHITE);
    } while ((win = W(prev)) != active->prev);

    screen_render();
}


static void *sdl_die_if_null(void *result)
{
    if (result == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
        SDL_Quit();
    }

    return result;
}


static Uint32 sdl_die_if_negative(Uint32 result)
{
    if (result < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
        SDL_Quit();
    }

    return result;
}


static SDL_Event select_event;
static SDL_mutex *select_mutex;
static SDL_cond *select_cond;
static int selfpipe_fds[2];


void redo_select()
{
    write(selfpipe_fds[1], "!", 1);
}


static void setup_select_args()
{
    FD_SET_DIFF( &reads, &mask, &to_poll); /* reads = mask & ~to_poll */
    dbgprintf('l', (stderr, "select: mask=0x%lx to_poll=0x%lx 0x%lx got\r\n",
                    HD(mask), HD(to_poll), HD(reads)));
}


/* Use a separate thread to select on our fds so we don't have to poll
 * in the SDL event loop.
 *
 * Err, figure out how signals actually work...
 */
static int watch_for_select(void *ignored)
{
    dbgprintf('l', (stderr, "Locking mutex\n"));
    SDL_LockMutex(select_mutex);

    while (1) {
        dbgprintf('l', (stderr, "Doing pselect\n"));

        if (pselect(FD_SETSIZE, &reads, 0, 0, NULL, NULL) < 0) {
            /* In case of error, reads is left unmodified */
#ifdef DEBUG
            dbgprintf('l', (stderr, "select failed %ld->%ld\n",
                            HD(reads), (HD(mask) & ~HD(to_poll))));

            if (debug) {
                perror("Select:");
            }

#endif
            continue;
        }

        dbgprintf('l', (stderr, "reads=0x%lx\n", HD(reads)));

        if (FD_ISSET(selfpipe_fds[0], &reads)) {
            char c;
            read(selfpipe_fds[0], &c, 1);
        }

        do {
            select_event.user.code += 1;
            SDL_PushEvent(&select_event);

            /* Wait until the main thread has picked this up and reset 'reads' appropriately.
             * Timeout on this in case we've missed the select event in the main loop
             * (i.e. we haven't been processing events).
             */
        } while (SDL_CondWaitTimeout(select_cond, select_mutex, 1000) == SDL_MUTEX_TIMEDOUT);
    }

    /* We should never arrive here, but for clarity... */

    SDL_UnlockMutex(select_mutex);

    return 1; /* not used - required by sdl thread definition */
}


void handle_select_event(SDL_Event *current_event)
{
    /* At the moment, our only user event is the select one. */
    if (select_event.user.code != current_event->user.code) {
        /* Some stinky old event. Let's drop it on the floor,
         * since the select thread gave up waiting for us
         * and must have sent another event, and it's important
         * we correctly CondSignal that one so that selecting
         * can continue.
         */
        return;
    }

    SDL_LockMutex(select_mutex);

    read_fds_into_windows();
    setup_select_args();

    SDL_CondSignal(select_cond);
    SDL_UnlockMutex(select_mutex);
}


static void setup_select()
{
    select_cond = sdl_die_if_null(SDL_CreateCond());
    select_mutex = sdl_die_if_null(SDL_CreateMutex());
    select_event.type = sdl_die_if_negative(SDL_RegisterEvents(1));

    if (pipe(selfpipe_fds) == -1) {
        puts("Failed to make a pipe.");
        SDL_Quit();
    }

    FD_SET(selfpipe_fds[0], &mask);

    for (int i = 0; i < sizeof(selfpipe_fds) / sizeof(int); ++i) {
        int flags = fcntl(selfpipe_fds[i], F_GETFL);

        if (flags == -1) {
            puts("Failed to set non-blocking on self-pipe.");
            SDL_Quit();
        }

        fcntl(selfpipe_fds[i], F_SETFL, flags | O_NONBLOCK);
    }

    setup_select_args();

    SDL_DetachThread(sdl_die_if_null(SDL_CreateThread(watch_for_select, "watch_for_select", NULL)));
}


/* main */
int main(int argc, char **argv)
{
    register int i;                     /* counter */
    register int count;                 /* # chars read from shell */
    int maxbuf = MAXBUF;                /* # chars processed per window */
    int shellbuf = MAXSHELL;            /* # chars processed per shell */
    int flag;
    char c;
    char start_file[MAX_PATH];          /* name of startup file */
    char *screen_dev = SCREEN_DEV;      /* name of frame buffer */
    char *default_font = (char * )0;    /* default font */

    timestamp();                                /* initialize the timestamp */

    sprintf(start_file, "%s/%s", getenv("HOME"), STARTFILE);

    /* parse arguments */
    while ((flag = getopt(argc, argv, "d:p:vxm:s:F:M:P:b:B:f:i:S:z:Z:")) != EOF) {
        switch (flag) {
            /* d */
#     ifdef DEBUG
        case 'd': debug = 1;
            strcpy(debug_level, optarg);
            fprintf(stderr, "Debug level: [%s]\n", debug_level);
            break;
#     endif
        /* p -- set background pattern */
        case 'p':
        {
            FILE *fp;

#if 0
            // libbitblit refactor
            if ((fp = fopen(optarg, "rb")) != (FILE *)0) {
                if ((pattern = bitmapread(fp)) == (BITMAP *)0) {
                    fprintf(stderr, "mgr: Invalid background pattern bitmap %s.\n", optarg);
                    exit(1);
                }

                fclose(fp);
            } else {
                fprintf(stderr, "mgr: Can't open background pattern bitmap %s.\n", optarg);
                exit(1);
            }
#endif

            break;
        }
        /* v -- print version number and quit */
        case 'v':
            fputs(version, stdout);
            exit(1);
        /* x -- use /dev/null as startfile */
        case 'x': strcpy(start_file, "/dev/null");
            break;
        /* m -- set mouse device */
        case 'm':
            mouse_dev = optarg;
            break;
        /* M -- set mouse protocol type  */
        case 'M':
            mouse_type = optarg;
            break;
        /* s -- set start file */
        case 's':
            strcpy(start_file, optarg);
            break;
        /* F -- set default font */
        case 'F':
            default_font = optarg;
            break;
        /* P -- set polling timeout */
        case 'P':
            set_poll.tv_nsec = (long) (atoi(optarg) * 1000);
            break;
        /* b -- set shell buffering */
        case 'b':
            shellbuf = atoi(optarg);
            shellbuf = BETWEEN(5, shellbuf, 1024);
            break;
        /* B -- set window buffering */
        case 'B':
            maxbuf = atoi(optarg);
            maxbuf = BETWEEN(1, maxbuf, shellbuf);
            break;
        /* f -- set font directory */
        case 'f':
            font_dir = optarg;
            break;
        /* i -- set icon directory */
        case 'i':
            icon_dir = optarg;
            break;
        /* S -- set alternate frame buffer */
        case 'S':
            screen_dev = optarg;
            break;
#   ifdef MOVIE
        /* Z -- set save file and start logging now */
        case 'Z':
            log_command = optarg;
            log_now++;
            dbgprintf('L', (stderr, "Starting logging NOW at [%s]\n", log_command));
            break;
        /* z -- set save file */
        case 'z':
            log_command = optarg;
            dbgprintf('L', (stderr, "Starting logging LATER at [%s]\n", log_command));
            break;
#   endif
        /* default -- invalid flag */
        default:
            fprintf(stderr, "Usage: mgr ");
#ifdef DEBUG
            fprintf(stderr, "[-d <level>]");
#endif
            fprintf(stderr, "[-vx][-m <mouse>][-s <.rc file>][-F <default font>]\n");
            fprintf(stderr, "           [-P <polling timeout>][-b <shell buffer>][-B <window buffer>]\n");
            fprintf(stderr, "           [-f <font directory>][-i <icon directory>][-S <frame buffer>]\n");
#     ifdef MOVIE
            fprintf(stderr, "           [-Z <logfile>][-z <logfile>]\n");
#     endif
            fprintf(stderr, "           [-p <pattern>]\n");
            exit(1);
        }
    }

    /* keep mgr from being run within itself */
    {
        char *t = getenv("TERM");

        if (t && strncmp(t, TERMNAME, strlen(TERMNAME)) == 0) {
            fprintf(stderr, "mgr: I can't invoke me from within myself.\n");
            exit(1);
        }
    }
    /* save tty modes for ptty's */
    save_modes(0);
    /* free all unused fd's */
    /* free all unused fd's */
    count = getdtablesize();

    for (i = 3; i < count; i++) {
        close(i);
    }

    mousex = mousey = 32;

    if (getenv("MGRSIZE")) {
        int x, y, w, h;

        sscanf(getenv("MGRSIZE"), "%d %d %d %d", &x, &y, &w, &h);
        /* TODO - remove MGRSIZE references, and replace with 'normal' resolution selector...
         * e.g. 1920x1080x32 + windowed vs full-screen
         * Settable by command line.
         * Default is full screen in 'optimal' res.
         */
    }

    /* sdl init */
    if ((screen = screen_init(WIDTH, HEIGHT)) == NULL) {
        perror("mgr: Can't open the screen.");
        exit(2);
    }

    /* SDL init */
    if (!(SDL_GetModState() & KMOD_CTRL)) {
        SDL_StartTextInput();
    }

    SDL_ShowCursor(SDL_ENABLE);

    /* get the default font file */
    if (default_font || (default_font = getenv(DEFAULT_FONT_VAR))) {
        font = open_font(default_font);
    }

    if (font == (struct font *) 0) {
        font = open_font("");
    }

    font->ident = 0;

    // TODO - this needs debugging. May need pselect?
    // Also watch out for SDL turning things off.
    /* catch the right interrupts */
    for (i = 0; i < NSIG; i++) {
        switch (i) {
        case SIGUSR1:   /* experimental logging stuff */
        case SIGUSR2:     signal(i, sig_share);
            break;
        case SIGCHLD:     signal(SIGCHLD, sig_child);
            break;
        case SIGILL:    /* <= 3.0 abort gererates this one */
        case SIGCONT:
        case SIGIOT:    /* 3.2 abort generates this (gee thanks, SUN!) */
        case SIGQUIT:
            break;
        case SIGTTIN:
        case SIGTTOU:     signal(i, SIG_IGN);
            break;
        default:          signal(i, catch);
            break;
        }
    }

    if (!load_server_icons()) {
        perror("mgr: Unable to load some of the default server icons");
    }
    pattern = def_pattern;

    copyright(screen, "");
    SETMOUSEICON(mouse_cup);
    /* always look for keyboard and mouse input */
    FD_ZERO( &mask);
    FD_ZERO( &to_poll);
    FD_ZERO( &reads);
    /* get default font definitions */
    {
        char buff[MAX_PATH];
        sprintf(buff, "%s/%s", font_dir, STARTFILE);
        startup(buff);
    }
#   ifdef MOVIE

    /* start logging */
    if (log_now) {
        log_noinitial = 1;              /* no need to save initial display image */
        do_buckey('S' | 0x80);          /* simulate the key-press */
    }

#   endif
    /* process startup file */
    startup(start_file);

    if (active != (WINDOW *) 0) {
        ACTIVE_ON();
    } else {
        erase_win(screen);
    }

    /* turn on mouse cursor */
    SETMOUSEICON(DEFAULT_MOUSE_CURSOR);
    /* main polling loop */

    int dirty = 1;
    int last_render_ticks = 0;
    int ctrl_enabled = 0;
    int UPDATE_INTERVAL_MS = 30; /* tweak this lower to increase frame rate */

    /* We use the SDL event loop, so we move the traditional
     * 'select' handling to a thread (and have it push a userevent).
     */
    dbgprintf('m', (stderr, "------ setup select\r\n"));
    setup_select();

    dbgprintf('m', (stderr, "------ start update\r\n"));

    while (1) {
        dirty |= update_windows();

        int ticks = SDL_GetTicks();
        int time_since_render_ms = ticks - last_render_ticks;

        /* Without this, SDL sometimes seems to decide to not
         * flush on RenderPresent, and since we don't have a normal 60fps
         * game style loop we _really_ need the flush.
         * TODO: confirm this behaviour.
         */
        screen_flush();

        if (dirty && time_since_render_ms > UPDATE_INTERVAL_MS) {
            erase_win(screen);
            display_windows();
            screen_present();

            last_render_ticks = ticks;
            dirty = 0;
            time_since_render_ms = 0;
        }

        SDL_Event event;
        int timeout = -1; /* i.e. never */

        if (FD_COMMON(&to_poll, &mask)) {
            timeout = 0;
        } else if (dirty) {
            timeout = UPDATE_INTERVAL_MS - time_since_render_ms;
        }

        if (!SDL_WaitEventTimeout(&event, timeout)) {
            continue;
        }

        int dx, dy;

        switch (event.type) {
        case SDL_USEREVENT:
            handle_select_event(&event);
            break;
        case SDL_QUIT:
            // TODO
            exit(1);
        case SDL_TEXTINPUT:

            if (!active) {
#ifdef BUCKEY
                do_buckey(event.text.text[0]);
#endif
                break;
            }

            int len = strlen(event.text.text);

            if (!(ACTIVE(flags) & W_NOINPUT)) {
#ifdef BUCKEY

                if ((ACTIVE(flags) & W_NOBUCKEY) || !do_buckey(event.text.text[0])) {
                    write(ACTIVE(to_fd), event.text.text, len);
                }

#else
                write(ACTIVE(to_fd), event.text.text, len);
#endif

                if (ACTIVE(flags) & W_DUPKEY && event.text.text[0] == ACTIVE(dup)) {
                    write(ACTIVE(to_fd), event.text.text, len);
                }
            }

            break;
        case SDL_KEYDOWN:

            switch (event.key.keysym.sym) {
            case SDLK_LCTRL:
            case SDLK_RCTRL:
                ctrl_enabled = 1;
                SDL_StopTextInput();
                break;
            case SDLK_BACKSPACE:
            case SDLK_TAB:
            case SDLK_RETURN:
            case SDLK_ESCAPE:

                if (active && !(ACTIVE(flags) & W_NOINPUT)) {
                    write(ACTIVE(to_fd), &event.key.keysym.sym, 1);
                }

                break;
            case SDLK_UP:

                if (active && !(ACTIVE(flags) & W_NOINPUT)) {
                    write(ACTIVE(to_fd), &"\033[A", 3);
                }

                break;
            case SDLK_DOWN:

                if (active && !(ACTIVE(flags) & W_NOINPUT)) {
                    write(ACTIVE(to_fd), &"\033[B", 3);
                }

                break;
            case SDLK_RIGHT:

                if (active && !(ACTIVE(flags) & W_NOINPUT)) {
                    write(ACTIVE(to_fd), &"\033[C", 3);
                }

                break;
            case SDLK_LEFT:

                if (active && !(ACTIVE(flags) & W_NOINPUT)) {
                    write(ACTIVE(to_fd), &"\033[D", 3);
                }

                break;
            default:

                if (active && ctrl_enabled && !(ACTIVE(flags) & W_NOINPUT)) {
                    c = event.key.keysym.sym - 'a' + 1;

                    if (iscntrl(c)) {
                        write(ACTIVE(to_fd), &c, 1);
                    }
                }

                break;
            }

            break;
        case SDL_KEYUP:

            if (event.key.keysym.sym == SDLK_LCTRL || event.key.keysym.sym == SDLK_RCTRL) {
                ctrl_enabled = 0;
                SDL_StartTextInput();
            }

            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            do_button(mouse_get_sdl(&event, &dx, &dy));
            dirty = 1;
            break;
        case SDL_MOUSEMOTION:
            mousex = BETWEEN(0, event.motion.x, WIDTH - 1);
            mousey = BETWEEN(0, event.motion.y, HEIGHT - 1);
            break;
        default:
            break;
        }
    }
}
