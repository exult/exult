AM_CPPFLAGS = -I$(srcdir)/.. -I$(srcdir)/../headers -I$(srcdir)/../files $(SDL_CFLAGS) $(INCDIRS) \
		 $(WINDOWING_SYSTEM) $(DEBUG_FLAGS) $(CPPFLAGS) $(WARNINGS)

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
	../files/libu7file.la	\
	$(SYSLIBS)

EXTRA_DIST = 		\
	config.xml	\
	module.mk	\
	README

CLEANFILES = *~
