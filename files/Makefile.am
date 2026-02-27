if BUILD_EXULT
SDLRWOPS= \
	sdlrwopsstreambuf.cc \
	sdlrwopsstreambuf.h \
	sdlrwopsistream.cc \
	sdlrwopsistream.h \
	sdlrwopsostream.cc \
	sdlrwopsostream.h
else
SDLRWOPS=
endif

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/headers $(SDL_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) \
		$(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS) -DEXULT_DATADIR=\"$(EXULT_DATADIR)\"

SUBDIRS = sha1 zip

noinst_LTLIBRARIES = libu7file.la

noinst_PROGRAMS = rwregress

rwregress_SOURCES = \
	rwregress.cc \
	utils.h

rwregress_LDADD = \
	libu7file.la \
	$(SYSLIBS)

libu7file_la_SOURCES =	\
	Flat.cc		\
	Flat.h		\
	Flex.cc		\
	Flex.h		\
	Table.cc	\
	Table.h		\
	IFF.cc		\
	IFF.h		\
	U7file.cc	\
	U7file.h	\
	U7fileman.cc	\
	U7fileman.h	\
	U7obj.cc	\
	U7obj.h	\
	utils.cc	\
	utils.h		\
	endianio.h	\
	databuf.h	\
	listfiles.cc	\
	listfiles.h	\
	crc.cc		\
	crc.h		\
	msgfile.cc	\
	msgfile.h	\
	$(SDLRWOPS)

EXTRA_DIST = 		\
	module.mk	\
	README

CLEANFILES = *~
