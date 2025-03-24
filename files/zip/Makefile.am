AM_CPPFLAGS = $(SDL_CFLAGS) $(ZLIB_CFLAGS) $(INCDIRS) -I$(top_srcdir)/headers $(WINDOWING_SYSTEM) \
		$(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

noinst_LTLIBRARIES = libminizip.la

libminizip_la_SOURCES =	\
	zip.cc \
	zip.h \
	unzip.cc \
	unzip.h

CLEANFILES = *~
