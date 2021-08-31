#include <SDL.h>

int mouse_get_sdl(SDL_Event *event, int *x_delta, int *y_delta);
int mouse_get_poll(int *x_delta, int *y_delta);
int mouse_get_wait(int *x_delta, int *y_delta);
int *map_mouse(int button, int map);
/*{{{}}}*/
