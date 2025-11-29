extern TEXTURE *screen_init(int width, int height);
extern void screen_render(int with_base_texture);
extern void screen_present();
extern void screen_size(int *, int *);

extern void cursor_warp(SDL_Point point);
SDL_Cursor *cursor_create_from_icon(char *iconpath);

extern TEXTURE *texture_create_child(TEXTURE *src_texture, SDL_Rect rect);
extern TEXTURE *texture_create_empty(int width, int height);
extern TEXTURE *texture_create_from_pixels(void *pixels, int wide, int high, int depth);
extern TEXTURE *texture_create_from_icon(const char *iconpath);
extern TEXTURE *texture_clone(TEXTURE *src_texture);
extern TEXTURE *texture_create_from_file(const char *path);
extern int texture_save_to_file(const char *path, TEXTURE *t);

extern void texture_destroy(TEXTURE *texture);

extern SDL_Rect texture_get_rect(TEXTURE *texture);

extern void texture_clear(TEXTURE *texture, SDL_Color color);
extern void texture_fill_rect(TEXTURE *texture, SDL_Rect rect, SDL_Color color);
extern void texture_rect(TEXTURE *texture, SDL_Rect rect, SDL_Color color, int line_width);
extern void texture_point(TEXTURE *texture, SDL_Point point, SDL_Color color);
extern void texture_line(TEXTURE *texture, SDL_Point start, SDL_Point end, SDL_Color color);
extern void texture_scroll(TEXTURE *texture, SDL_Rect region, int delta_x, int delta_y, SDL_Color bg_color);
extern void texture_copy(TEXTURE *dst_texture, SDL_Point point, TEXTURE *src_texture, SDL_Color fg_color);
extern void texture_copy_withbg(TEXTURE *dst_texture, SDL_Point point, TEXTURE *src_texture, SDL_Color fg_color, SDL_Color bg_color);

extern TEXTURE *def_pattern;
extern SDL_Cursor *mouse_arrow;
extern SDL_Cursor *mouse_box;
extern SDL_Cursor *mouse_bull;
extern SDL_Cursor *mouse_bull2;
extern SDL_Cursor *mouse_cross;
extern SDL_Cursor *mouse_cup;
extern SDL_Cursor *mouse_cut;

extern int load_server_icons();
