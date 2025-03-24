AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/headers -I$(top_srcdir)/files	\
		-I$(top_srcdir)/shapes -I$(top_srcdir)/audio -I$(top_srcdir)/audio/midi_drivers	\
		-I$(top_srcdir)/conf -I$(top_srcdir)/objs -I$(top_srcdir)/imagewin	\
		-I$(top_srcdir)/gumps $(SDL_CFLAGS) $(OGG_CFLAGS) $(INCDIRS) \
		$(WINDOWING_SYSTEM) $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS) \
		-DEXULT_DATADIR=\"$(EXULT_DATADIR)\"

noinst_LTLIBRARIES = libgamemgr.la

libgamemgr_la_SOURCES =	\
	modmgr.cc	\
	modmgr.h

if BUILD_EXULT
libgamemgr_la_SOURCES +=	\
	bggame.cc	\
	bggame.h	\
	sigame.cc	\
	sigame.h	\
	devgame.cc	\
	devgame.h
endif

CLEANFILES = *~
