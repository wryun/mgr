include ../../../Configfile

CFLAGS=		$(CCFLAGS) $(CPPFLAGS) $(BLITLIBFLAGS)
OBJS=		bitmaphead.o bit_create.o bit_alloc.o bit_open.o \
		sdl.o bit_blit.o bit_drawing.o bit_destroy.o bit_present.o bit_other.o

all:		$(OBJS)

install:	all
		install -c -m 644 bitmap.5 $(MANDIR)/man5

depend:
		$(MKDEP) $(MKDEPFLAGS) $(CPPFLAGS) *.c > $(MKDEPOUT)

clean:
		$(RMF) $(OBJS)

clobber:	clean
		$(ZILCH) $(DEPFILE)

include $(DEPFILE)
