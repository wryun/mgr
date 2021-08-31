/*{{{}}}*/
/*{{{  #includes*/
#include <sys/time.h>
#include <mgr/share.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

/* Note that mgr expects to be able to wait on _just_ mouse events,
 * whereas SDL's model is that all events come through the same queue.
 * This means we throw other events away at the moment! Not too bad
 * when we're just throwing away keyboard/quit events waiting on a mouse action,
 * but will be more serious if we properly centralise on SDL's event
 * queue and get rid of the double poll (on the SDL queue and using
 * select...).
 *
 * Really, that MGR 'blocks' on mouse events is probably what should be
 * fixed here.
 */
#include <SDL.h>

#include "proto.h"
#include "mouse.h"
/*}}}  */

/*{{{  variables*/
static int button_map[8] = {0,1,2,3,4,5,6,7};
/*}}}  */

/*{{{  mouse_get*/
/* primary mouse interface
*/
int mouse_get_wait(int *x_delta, int *y_delta) {
   SDL_Event sdl_ev;

   while (SDL_WaitEvent(&event)) {
      switch (event.type) {
      case SDL_MOUSEMOTIONEVENT:
      case SDL_MOUSEBUTTONDOWN:
#ifdef MOVIE
         log_time();
#endif
         return mouse_get_sdl(&event, x_delta, y_delta);
      default: /* TODO - losing events (see comment above) */
         break;
      }
   }

   return -1; /* TODO - handle SDL error... */
}
/*}}}  */

int mouse_get_poll(int *x_delta, int *y_delta)
{
   SDL_Event event;

   while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_MOUSEMOTIONEVENT:
      case SDL_MOUSEBUTTONDOWN:
#ifdef MOVIE
         log_time();
#endif
         return mouse_get_sdl(&event, x_delta, y_delta);
      default: /* TODO - losing events (see comment above) */
         break;
      }
   }

   return -1; /* -1 means there was nothing there. */
}

int mouse_get_sdl(SDL_Event *event, int *x_delta, int *y_delta)
   int x, y;
   switch (event.type) {
   case SDL_MOUSEMOTIONEVENT:
      *x_delta = event.motion.xrel;
      *y_delta = -event.motion.yrel;
      return button_map[event.motion.state];
   case SDL_MOUSEBUTTONDOWN:
   case SDL_MOUSEBUTTONUP:
      *x_delta = 0;
      *y_delta = 0;
      return button_map[SDL_GetMouseState(&x, &y)];
   default:
      assert(0);
      return;
   }
}

/*{{{  map_mouse buttons (for non-left handers)*/
int *map_mouse(button,map) int button, map;
{
	if (button>0 && button<8) button_map[button]=map;
	return(button_map);
}
/*}}}  */
