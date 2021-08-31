#include <SDL.h>

#include <mgr/bitblit.h>

#define LOGBITS 3
#define BITS (~(~(unsigned)0<<LOGBITS))

#define bit_linesize(wide,depth) ((((depth)*(wide)+BITS)&~BITS)>>3)

#define BIT_SIZE(m) BIT_Size(BIT_WIDE(m), BIT_HIGH(m), BIT_DEPTH(m))
#define BIT_Size(wide,high,depth) (((((depth)*(wide)+BITS)&~BITS)*(high))>>3)
#define BIT_LINE(x) ((((x)->primary->depth*(x)->primary->wide+BITS)&~BITS)>>LOGBITS)

