#                        Copyright (c) 1988 Bellcore
#                            All Rights Reserved
#       Permission is granted to copy or use this program, EXCEPT that it
#       may not be sold for profit, the copyright notice must be reproduced
#       on copies, and credit should be given to Bellcore where it is due.
#       BELLCORE MAKES NO WARRANTY AND ACCEPTS NO LIABILITY FOR THIS PROGRAM.

#	makefile for sdl blit library

include ../../../Configfile

CFLAGS=		$(CCFLAGS) $(CPPFLAGS) -I.. -I/usr/include/SDL2

CFILES= mem_rop.c bit_initscreen.c bit_point.c bit_line.c bit_destroy.c bit_size.c bit_blit.c bit_on.c palette.c screen.c sdl_helper.c
OBJS= mem_rop.o bit_initscreen.o bit_point.o bit_line.o bit_destroy.o bit_size.o bit_blit.o bit_on.o palette.o screen.o sdl_helper.o
HFILES= bitmap.h sdl.h
OTHER=Makefile README
LIB=blitlib.a
START=.
MAKE=make

all:		$(LIB)

# bitmap library

$(LIB): $(OBJS)
	ar rv $(LIB) $(OBJS)
	ranlib $(LIB)

clean:
	$(RMF) *.o $(LIB)

list:
	@for i in ${HFILES} ${CFILES} ${OTHER}; do \
		echo "${START}/$$i"; \
	done	

depend:
		$(MKDEP) $(MKDEPFLAGS) $(CPPFLAGS) $(BLITLIBFLAGS) *.c > $(MKDEPOUT)

clobber:
		$(ZILCH) $(DEPFILE)
