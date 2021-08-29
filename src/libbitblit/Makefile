include ../../Configfile

all:
		for i in bitmap seqbitmap movie; do (cd $$i; make all); done
		$(RMF) libbitblit.a
		for i in bitmap seqbitmap movie; do (cd $$i; ar qc ../libbitblit.a *.o); done
		ranlib libbitblit.a

install:	all
		install libbitblit.a $(LIBDIR)
		ranlib $(LIBDIR)/libbitblit.a
		install -c -m 644 bitblit.3 $(MANDIR)/man3
		install -c -m 644 bitmap/bitmap.5 $(MANDIR)/man5

depend:
		for i in movie bitmap seqbitmap test; do (cd $$i; make depend); done

clean:
		for i in bitmap seqbitmap movie; do (cd $$i; make clean); done

clobber:
		for i in bitmap seqbitmap movie test; do (cd $$i; make clobber); done
		$(RMF) libbitblit.a
