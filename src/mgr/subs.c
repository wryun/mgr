/*{{{}}}*/
/*{{{  Notes*/
/*                        Copyright (c) 1987 Bellcore
 *                            All Rights Reserved
 *       Permission is granted to copy or use this program, EXCEPT that it
 *       may not be sold for profit, the copyright notice must be reproduced
 *       on copies, and credit should be given to Bellcore where it is due.
 *       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.
 */

/* misc window and screen mangement routines */
/*}}}  */
/*{{{  #includes*/
#include <mgr/bitblit.h>
#include <mgr/font.h>
#undef P_ALL
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef sun
#include <sundev/kbio.h>
#endif

#include "defs.h"
#include "event.h"

#include "proto.h"
#include "subs.h"
#include "border.h"
#include "do_button.h"
#include "do_event.h"
#include "erase_win.h"
#include "font_subs.h"
#include "icon_server.h"
#include "intersect.h"
#include "mgr.h"
#include "mouse_get.h"
#include "set_mode.h"
#include "sigdata.h"
#include "startup.h"
/*}}}  */

/*{{{  set_covered -- deactivate all windows covered by win*/
void set_covered(check)
register WINDOW *check;			/* window to check covering against */
   {
   register WINDOW *win;

   for(win=active;win != (WINDOW *) 0;win=win->next)
       if (win!=check && intersect(win,check) && W(flags)&W_ACTIVE) {
          do_event(EVENT_COVERED,win,E_MAIN);
          W(flags) &= ~W_ACTIVE;
          if (!(W(flags)&W_LOOK)) {
             FD_CLR( W(to_fd), &mask);
          }
          dbgprintf('o',(stderr,"\t%s covers %s\r\n",check->tty,W(tty)));
          }
   }
/*}}}  */
/*{{{  un_covered -- find and activate all windows previously covered by win*/
void un_covered()
   {
   register WINDOW *win,*check;
   register int cover;

   for(win=active;win != (WINDOW *) 0;win=W(next)) {
      dbgprintf('o',(stderr,"	un_cover: %s)\n",W(tty)));
      for(cover=0,check=active;check!=win && cover==0;check=check->next)
         if (intersect(win,check)) cover=1;

      if (cover && W(flags)&W_ACTIVE) {
          do_event(EVENT_COVERED,win,E_MAIN);
          W(flags) &= ~W_ACTIVE;
          if (!(W(flags)&W_LOOK)) {
             FD_CLR( W(to_fd), &mask);
          }
          dbgprintf('o',(stderr,"becoming inactive (covered by %s)\r\n",
			check->tty));
          }
      else if (!cover && !(W(flags)&W_ACTIVE)) {
          do_event(EVENT_UNCOVERED,win,E_MAIN);
          W(flags) |= W_ACTIVE;
          if (!(W(flags)&W_DIED)) {
             FD_SET( W(to_fd), &mask);
             redo_select();
          }
          dbgprintf('o',(stderr,"becoming active\r\n"));
          }
      else if (cover && !(W(flags)&W_ACTIVE))  {
         dbgprintf('o',(stderr,"remains inactive (covered by %s)\r\n",
		       check->tty));
         }
      else if (!cover && W(flags)&W_ACTIVE) {
         dbgprintf('o',(stderr,"remains active\r\n"));
         }
      else {
	 dbgprintf('o',(stderr,"%s: unknown covering state\r\n",W(tty)));
	 }
      }
   }
/*}}}  */
/*{{{  expose -- bring a window to the top*/
void expose(win)
register WINDOW *win;			/* window to expose */
   {
   dbgprintf('o',(stderr,"exposing %s\r\n",W(tty)));
   
   /* reorder windows */

   if (win == active) return;

   W(prev)->next = W(next);
   if (W(next))
      W(next)->prev = W(prev);
   else
      ACTIVE(prev) = W(prev);

   W(prev) = ACTIVE(prev);
   W(next) = active;

   ACTIVE(prev) = win;
   active = win;

   if (!(W(flags)&W_ACTIVE)) {
      un_covered();
      }
   else {
      dbgprintf('o',(stderr,"expose: %s already active (0%o)\r\n",
		    ACTIVE(tty),(unsigned)ACTIVE(flags)));
      }
   {
      /* I think the mouse is always off here, but I cannot prove it. vpb */
      SETMOUSEICON( DEFAULT_MOUSE_CURSOR);	/* because active win chg */
   }
   }
/*}}}  */
/*{{{  bury -- move a window at the bottom of window list*/
int
bury(win)
register WINDOW *win;			/* window to bury */
   {
   dbgprintf('o',(stderr,"burying %s\r\n",W(tty)));
   if (!win || !W(next))
      return(0);

   if (win == active)
      active = W(next);

   W(prev)->next = W(next);
   W(next)->prev = W(prev);

   W(prev) = ACTIVE(prev);
   ACTIVE(prev)->next = win;

   ACTIVE(prev) = win;
   W(next) = (WINDOW *) 0;
   return(1);
   }
/*}}}  */
/*{{{  hide -- bury a window at the bottom of the screen*/
void hide(win)
register WINDOW *win;			/* window to hide */
   {
   dbgprintf('o',(stderr,"hiding %s\r\n",W(tty)));
   if (bury(win)==0) return;
   SETMOUSEICON( DEFAULT_MOUSE_CURSOR);	/* because active win chg */
   }
/*}}}  */
/*{{{  move_mouse*/
/*****************************************************************************
 *	move the mouse, keep exclusive control 
 *	"how" specifies how we recognize completion:
 *		how == 0:	all buttons were up at start of action,
 *				any button pushed down completes the action.
 *		how != 0:	some button was down at start of action,
 *				all buttons released completes the action.
 */

int
move_mouse(screen,mouse,x,y,how)
BITMAP *screen;
int mouse, *x, *y;
int how;
   {
   register int button = 0;
   do {
      button=mouse_get_poll(x,y);
      if (button == -1) {
         bit_present(screen);
         button=mouse_get_wait(x,y);
      }
      }
   while (how ? button!= 0 : button==0);
   if( how )
	do_button( 0 );
   return(button);
   }
/*}}}  */
/*{{{  parse -- parse a line into fields*/
#define iswhite(x)	(strchr(" \t",x))

int
parse(line,fields)
register char *line;
register char **fields;
   {
   int inword = 0;
   int count = 0;
   char *start;
   register char c;

   for(start = line;(c = *line) && c != '\n';line++)
      if (inword && iswhite(c)) {
         inword = 0;
         *line = '\0';
         *fields++ = start;
         count++;
         }
      else if (!inword && !iswhite(c)) {
         start = line;
         inword = 1;
         }

   if (inword) {
      *fields++ = start;
      count++;
      if (c == '\n')
         *line = '\0';
      }
   *fields = (char *) 0;
   return(count);
   }
/*}}}  */
/*{{{  trans -- parse a string to interpret \'s*/
char *
trans(s)
char *s;
   {
   char *result = s;
   register int i=0;
   register char c;
   register int got_slash=0;

   while((c = (*s++)&0x7f)) {
      if (got_slash){
         switch(c) {
            case 'e':
            case 'E': result[i++] = '\033'; break;
            case 'n': result[i++] = '\n';   break;
            case 'r': result[i++] = '\r';   break;
            case 'b': result[i++] = '\b';   break;
            case 'f': result[i++] = '\f';   break;
            case 'g': result[i++] = '\007'; break;
            case 's': result[i++] = ' ';    break;
            case '\\': result[i++] = '\\';  break;
            case 'M': result[i++] = *s++|0x80; break;
            default:  result[i++] = c;      break;
            }
         got_slash = 0;
         }
      else if (c=='\\')
         got_slash++;
      else 
         result[i++] = c;
      } 
   result[i] = '\0';
   return(result);
   }
/*}}}  */
/*{{{  set_console*/
void set_console(WINDOW *win, int on)
{
  /*{{{  variables*/
#  ifdef TIOCCONS
  int one = 1;
  int cfd;
#  endif
  WINDOW *run;
  /*}}}  */

  /*{{{  TIOCCONS*/
#  ifdef TIOCCONS
  cfd=open("/dev/console",O_RDWR);
#  ifndef linux
  if (!on)
#  endif
  ioctl(cfd,TIOCCONS,&one);
  close(cfd);
#  endif
  /*}}}  */
  if (on)
  {
    for (run=active; run!=(WINDOW*)0; run=run->next) run->flags&=~W_CONSOLE;
    win->flags|=W_CONSOLE;
    /*{{{  TIOCCONS*/
#    ifdef TIOCCONS
    if (ioctl(win->to_fd,TIOCCONS,&one)<0) fprintf(stderr,"can't set new console device\n");
#    endif
    /*}}}  */
  }
  else
  {
    win->flags&=~W_CONSOLE;
  }
}
/*}}}  */
/*{{{  suspend -- suspend MGR*/
void suspend(void)
   {
#ifdef SIGSTOP
   register WINDOW *win;


   for(win=active;win!=(WINDOW *) 0;win=win->next) {
      killpg(W(pid),SIGSTOP);
      }

   do_cmd( 's' );	/* do the suspention command */

   /* courtesy DVW */
   signal(SIGTSTP, SIG_DFL);
   kill(0,SIGTSTP);			/* send stop signal to self */
   sleep(1);				/* wait for CONT signal */
   signal(SIGTSTP, catch);

   do_cmd( 'r' );	/* do the resumption command */

   erase_win(screen);
   if (active) {
      for(win=ACTIVE(prev);win!=active;win=W(prev)) {
         killpg(W(pid),SIGCONT);
         }
      killpg(ACTIVE(pid),SIGCONT);
      }
#endif
   }
/*}}}  */
#ifdef MGR_ALIGN
/*{{{  alignwin -- align a window so a byte boundary occurs somewhere insode the border*/
void
alignwin(screen,x,dx,slop)
register BITMAP *screen;
register int *x, *dx;
int slop;
   {
   register int adjust = (BIT_X(screen)+ *x) & 7;

   if (adjust>0 && adjust<(8-slop)) {
      *x -= adjust;
      dbgprintf('A',(stderr,"Adjusting x by %d",adjust));
      }
      dbgprintf('A',(stderr," at [%d/%d]\r\n",*x,(*x)&7));

   adjust = (adjust + *dx) &7;

   if (adjust>slop) { 
      *dx += 8-adjust;
      dbgprintf('A',(stderr,"Adjusting dx by %d\r\n",8-adjust));
      }
      dbgprintf('A',(stderr," at [%d/%d]\r\n",*x + *dx,(*x + *dx)&7));
   }
/*}}}  */
#endif /* MGR_ALIGN */

/*{{{  cursor_ok -- make sure icon is valid*/
int
cursor_ok(map)
BITMAP *map;			/* cursor icon */
   {
   return(map != NULL && BIT_WIDE(map) >= 16  && BIT_HIGH(map) >= 32);
   /* might like to check contents of bitmap to be reasonable */
   }
/*}}}  */
/*{{{  do_cursor*/
static void
do_cursor(win)
WINDOW *win;
	{
	switch(W(curs_type)) {
		case CS_BLOCK:
			bit_blit(W(window), W(x)+W(text.x),
				W(y)+W(text.y)-W(font->head.high),
				W(font->head.wide), W(font->head.high),
				PUTOP(BIT_NOT(BIT_DST),W(style)), 0, 0, 0);
			break;
		case CS_BOX:
			bit_blit(W(window), W(x)+W(text.x),
				W(y)+W(text.y)-W(font->head.high)+1,
				W(font->head.wide), W(font->head.high)-2,
				PUTOP(BIT_NOT(BIT_DST),W(style)), 0, 0, 0);
			bit_blit(W(window), W(x)+W(text.x)-2,
				W(y)+W(text.y)-W(font->head.high)-1,
				W(font->head.wide)+4, W(font->head.high)+2,
				PUTOP(BIT_NOT(BIT_DST),W(style)), 0, 0, 0);
			break;
		case CS_LEFT:
			bit_blit(W(window), W(x)+W(text.x) - 1,
				W(y)+W(text.y)-W(font->head.high),
				2, W(font->head.high),
				PUTOP(BIT_NOT(BIT_DST),W(style)), 0, 0, 0);
			break;
		case CS_RIGHT:
			bit_blit(W(window), W(x)+W(text.x)+W(font->head.wide)-1,
				W(y)+W(text.y)-W(font->head.high),
				2, W(font->head.high),
				PUTOP(BIT_NOT(BIT_DST),W(style)), 0, 0, 0);
			break;
		case CS_UNDER:
			bit_blit(W(window), W(x)+W(text.x),
				W(y)+W(text.y)-1,
				W(font->head.wide), 2,
				PUTOP(BIT_NOT(BIT_DST),W(style)), 0, 0, 0);
			break;
		}
	}
/*}}}  */
/*{{{  cursor_on, cursor_off*/
static int cursoron = 0;

void cursor_on()
{
	if( !active ) {
		cursoron = 0;
		return;
	}
	if( cursoron )
		return;
	do_cursor(active);
	cursoron = 1;
}

void cursor_off()
{
	if( !active ) {
		cursoron = 0;
		return;
	}
	if( !cursoron )
		return;
	cursoron = 0;
	do_cursor(active);
}
/*}}}  */
/*{{{  system command - turn off root privaleges*/
/* system command - turn off root privaleges */

int systemcmd(command) char *command;
{
        int status, pid, w;
        register void (*istat)(), (*qstat)();

	if (!command  ||  *command == '\0')
		return(0);
        if ((pid = vfork()) == 0) { /* does vfork work? */

                /* make sure commands doesn't run as root */
      
                int uid = getuid();
                int gid = getgid();
                setreuid(uid,uid);
                setregid(gid,gid);

		close(0);
		open("/dev/null",O_RDONLY);

                execl("/bin/sh", "sh", "-c", command, 0);
                _exit(127);
        }
        istat = signal(SIGINT, SIG_IGN);
        qstat = signal(SIGQUIT, SIG_IGN);
        while ((w = wait(&status)) != pid && w != -1)
                ;
        if (w == -1)
                status = -1;
        signal(SIGINT, istat);
        signal(SIGQUIT, qstat);
        return(status);
}
/*}}}  */
