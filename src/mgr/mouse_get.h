union SDL_Event;
typedef union SDL_Event SDL_Event;

int mouse_get_sdl(SDL_Event *event, int *x, int *y);
int mouse_get_poll(int *x, int *y);
int mouse_get_wait(int *x, int *y);
int *map_mouse(int button, int map);
