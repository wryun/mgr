#define DATA unsigned char

#include <SDL.h>
#include <mgr/bitblit.h>

#define LOGBITS 3
#define BITS (~(~(unsigned)0 << LOGBITS))

#define bit_linesize(wide, depth) ((((depth) * (wide) + BITS) & ~BITS) >> 3)

#define BIT_LINE(x) ((((x)->primary->depth * (x)->primary->wide + BITS) & ~BITS) >> LOGBITS)

#define BIT_Size(wide,high,depth) (((((depth)*(wide)+BITS)&~BITS)*(high))>>3)

struct device_info {
  SDL_Window *window;
  SDL_Texture *texture;
};

extern SDL_Renderer *sdl_renderer;

int sdl_helper_setup(SDL_Renderer *renderer);
SDL_Texture *sdl_create_texture_target(SDL_Renderer *renderer, int x, int y);
SDL_Texture *sdl_create_texture_from_static_bitmap(SDL_Renderer *renderer, BITMAP *bitmap);
void sdl_use_func(int func);
