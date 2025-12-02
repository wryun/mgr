include ../../Configfile

_CPPFLAGS=	$(CPPFLAGS) $(MGRFLAGS) \
		-DFONTDIR=\"$(FONTDIR)\" -DICONDIR=\"$(ICONDIR)\" \
		-DDEFAULT_FONT=\"$(DEFAULT_FONT)\" \
		-DDEFAULT_WIDTH=$(DEFAULT_WIDTH) -DDEFAULT_HEIGHT=$(DEFAULT_HEIGHT) \
                -DSCREEN_DEV=\"$(SCREEN_DEV)\" -DMOUSE_DEV=\"$(MOUSE_DEV)\"

CFLAGS=		$(CCFLAGS) $(_CPPFLAGS)

OBJS=		Write.o border.o copyright.o cut.o \
		data.o debug_flags.o \
		destroy.o do_buckey.o do_button.o do_event.o do_menu.o \
                down_load.o erase_win.o font_subs.o get_font.o get_info.o \
		get_menus.o get_text.o getshell.o graph_subs.o graphics.o \
		bitmap.o intersect.o mgr.o mouse_get.o move.o \
                new_window.o print.o put_window.o \
                set_mode.o shape.o sigdata.o startup.o subs.o utmp.o \
                window_box.o win_make.o win_subs.o write_ok.o popen.o # win_stack.o

all:		mgr # mgrlogin

mgr:		$(OBJS)
		$(CC) $(LDFLAGS) -o mgr $(OBJS) $(EXTRALIBS) $(CRYPTLIB)

setuid:		mgr
		chown root mgr
		chmod 04755 mgr

version.h:
		echo "/* compilation info for mgr, file is created by make(1) */" > version.h
		echo >> version.h
		echo "char version[]=\"MGR version $(VERSION) compiled by `whoami` at `date`.\\nSystem: `uname -a`\\n\";" >> version.h

mgrlogin:	mgrlogin.o mgrlogin_fnt.o set_mode.o
		$(CC) $(LDFLAGS) -o $@ mgrlogin.o mgrlogin_fnt.o set_mode.o $(EXTRALIBS) $(CRYPTLIB)

mgrlogin.o:	mgrlogin.c
		$(CC) $(CFLAGS) -DMGR_BINARY=\"$(BINDIR)/mgr\" -c mgrlogin.c -o $@

mgrlogin_fnt.o:	font_subs.c
		$(CC) $(CFLAGS) -DMGRLOGIN -c font_subs.c -o $@

depend:         version.h
		$(MKDEP) $(MKDEPFLAGS) $(_CPPFLAGS) *.c > $(MKDEPOUT)

clean:
		$(RMF) $(OBJS) mgrlogin_fnt.o mgrlogin.o

clobber:	clean
		$(RMF) core mgr mgrlogin version.h
		$(ZILCH) $(DEPFILE)

install:	all
		install -c -s -m $(SERVER_PERM) mgr $(BINDIR)
		install -c -s -m 755 mgrlogin $(BINDIR)
		install -c -m 644 mgr.1 $(MANDIR)/man1
		install -c -m 644 font.5 $(MANDIR)/man5

-include $(DEPFILE)
