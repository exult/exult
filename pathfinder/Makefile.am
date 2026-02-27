AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/headers -I$(top_srcdir)/objs $(SDL_CFLAGS) $(INCDIRS) \
		$(WINDOWING_SYSTEM) $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

if BUILD_EXULT
noinst_LTLIBRARIES = libpathfinder.la

libpathfinder_la_SOURCES =	\
	Astar.cc		\
	Astar.h			\
	PathFinder.cc		\
	PathFinder.h		\
	Zombie.cc		\
	Zombie.h		\
	path.cc		\
	path.h
endif

CLEANFILES = *~
