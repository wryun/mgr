void put_str(TEXTURE *map, int x, int y, struct font *font, SDL_Color fgcolor, SDL_Color bgcolor, char *str);
struct menu_state *menu_define(struct font *font, char *list[], char *values[], int max);
struct menu_state *menu_setup(struct menu_state *state, TEXTURE *screen, int x, int y, int start);
int menu_get(struct menu_state *state, int mouse, int button, int exit);
struct menu_state *menu_remove(struct menu_state *state);
int menu_destroy(struct menu_state *state);
struct menu_state *menu_copy(register struct menu_state *menu);
