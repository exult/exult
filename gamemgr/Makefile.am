AM_CPPFLAGS = -I$(srcdir)/.. -I$(srcdir)/../headers -I$(srcdir)/../files	\
		-I$(srcdir)/../shapes -I$(srcdir)/../audio -I$(srcdir)/../audio/midi_drivers	\
		-I$(srcdir)/../conf -I$(srcdir)/../objs -I$(srcdir)/../imagewin	\
		-I$(srcdir)/../gumps $(SDL_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS) $(INCDIRS) \
		$(WINDOWING_SYSTEM) $(DEBUG_FLAGS) $(CPPFLAGS) \
		-DEXULT_DATADIR=\"$(EXULT_DATADIR)\"

noinst_LTLIBRARIES = libgamemgr.la

libgamemgr_la_SOURCES =	\
	bggame.cc	\
	bggame.h	\
	sigame.cc	\
	sigame.h	\
	devgame.cc	\
	devgame.h	\
	modmgr.cc	\
	modmgr.h

CLEANFILES = *~
