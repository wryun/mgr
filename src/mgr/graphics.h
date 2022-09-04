#include <SDL.h>

/* Wrapper around an SDL texture that allows us to:
 *  - have easy access to its dimensions
 *  - potentially have a 'child' texture (see texture_create_child)
 *    which points to a sub-rectangle (set orig = 0)
 *  - note that we assume if you texture_destroy when orig = 1,
 *    you've cleaned up the children (we don't ref count).
 */
typedef struct texture {
    SDL_Texture *sdl_texture;
    SDL_Rect rect;
    int orig;
} TEXTURE;

extern TEXTURE *screen_init(int width, int height);
extern void screen_present();
extern void screen_flush();

extern void cursor_warp(SDL_Point point);
extern SDL_Cursor *cursor_create(void *pixels, int wide, int high, int depth);

extern TEXTURE *texture_create_child(TEXTURE *src_texture, SDL_Rect rect);
extern TEXTURE *texture_create_empty(int width, int height);
extern TEXTURE *texture_create_from_pixels(void *pixels, int wide, int high, int depth);

extern void texture_destroy(TEXTURE *texture);

extern void texture_fill_rect(TEXTURE *texture, SDL_Rect rect, SDL_Color color);
extern void texture_point(TEXTURE *texture, SDL_Point point, SDL_Color color);
extern void texture_line(TEXTURE *texture, SDL_Point start, SDL_Point end, SDL_Color color);
extern void texture_copy(TEXTURE *dst_texture, SDL_Point point, TEXTURE *src_texture, SDL_Color fg_color);
extern void texture_copy_withbg(TEXTURE *dst_texture, SDL_Point point, TEXTURE *src_texture, SDL_Color fg_color, SDL_Color bg_color);
