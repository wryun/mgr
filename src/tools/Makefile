include ../../Configfile

CFLAGS=		$(CCFLAGS) $(CPPFLAGS)

all:		bitmaptoc fonttoc

install:	all
		install -c -m 644 bitmaptoc.1 $(MANDIR)/man1
		install -c -m 644 fonttoc.1 $(MANDIR)/man1

bitmaptoc:	bitmaptoc.c ../libbitblit/libbitblit.a screen.h
		$(CC) $(CFLAGS) bitmaptoc.c ../libbitblit/libbitblit.a $(EXTRALIBS) $(BLITLIBFLAGS) -o $@

../libbitblit/libbitblit.a:
		(cd ../libbitblit; make all)

screen.h:	../libbitblit/bitmap/screen.h
		$(RMF) screen.h
		$(LNS) ../libbitblit/bitmap/screen.h screen.h

fonttoc:	fonttoc.c
		$(CC) $(CFLAGS) fonttoc.c -o $@

depend:

clean:

clobber:
		$(RMF) core bitmaptoc fonttoc screen.h
