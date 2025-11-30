/* erase a pixrect to background pattern */
/* #includes */
#include <stdio.h>

#include "defs.h"
#include "graphics.h"

#include "mgr.h"

void erase_win(TEXTURE *map)
{
    texture_copy_repeat(map, pattern, C_WHITE, C_BLACK);
}
