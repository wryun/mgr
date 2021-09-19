#include <SDL.h>

extern SDL_Renderer *sdl_renderer;
extern SDL_Window *sdl_window;

int sdl_helper_setup(SDL_Renderer *renderer);
SDL_Texture *sdl_create_texture_target(SDL_Renderer *renderer, int x, int y);
SDL_Cursor *sdl_create_cursor_from_static_bitmap(void *pixels, int wide, int high, int depth);
SDL_Texture *sdl_create_texture_from_static_bitmap(SDL_Renderer *renderer, void *pixels, int wide, int high, int depth);
void sdl_use_func(int func);
