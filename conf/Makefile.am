AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/headers -I$(top_srcdir)/files $(SDL_CFLAGS) $(INCDIRS) \
		 $(WINDOWING_SYSTEM) $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

if BUILD_TOOLS
noinst_PROGRAMS = confregress
endif

noinst_LTLIBRARIES = libconf.la

libconf_la_SOURCES =	\
	Configuration.cc	\
	Configuration.h		\
	XMLEntity.cc		\
	XMLEntity.h

confregress_SOURCES =	\
	xmain.cc

confregress_LDADD =	\
	libconf.la	\
	$(top_builddir)/files/libu7file.la	\
	$(SYSLIBS)

EXTRA_DIST = 		\
	config.xml	\
	module.mk	\
	README

CLEANFILES = *~
