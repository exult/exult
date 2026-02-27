# Enable subdirectory objects
AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/headers \
	$(SDL_CFLAGS) $(SDL_IMAGE_CFLAGS) $(INCDIRS) \
	$(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS) -DHAVE_CONFIG_H

bin_PROGRAMS = smooth

smooth_SOURCES = smooth.c smooth.h config.c config.h globals.h \
	image.c image.h linked.c linked.h param.c param.h plugin.c plugin.h

smooth_LDADD = $(SDL_LIBS) $(SDL_IMAGE_LIBS) -lm

# Plugin libraries
pkglib_LTLIBRARIES = libsmooth_randomize.la libsmooth_stream.la libsmooth_smooth.la

AM_LDFLAGS = -module -avoid-version -shared 

libsmooth_randomize_la_SOURCES = plugins/plugin_randomize.c
libsmooth_randomize_la_LDFLAGS = $(AM_LDFLAGS)
libsmooth_randomize_la_LIBADD = $(SDL_LIBS) $(SDL_IMAGE_LIBS) -lm

libsmooth_stream_la_SOURCES = plugins/plugin_stream.c
libsmooth_stream_la_LDFLAGS = $(AM_LDFLAGS)
libsmooth_stream_la_LIBADD = $(SDL_LIBS) $(SDL_IMAGE_LIBS) -lm

libsmooth_smooth_la_SOURCES = plugins/plugin_smooth.c
libsmooth_smooth_la_LDFLAGS = $(AM_LDFLAGS)
libsmooth_smooth_la_LIBADD = $(SDL_LIBS) $(SDL_IMAGE_LIBS) -lm

all-local: $(pkglib_LTLIBRARIES)
	cp .libs/libsmooth_randomize.so $(builddir)/
	cp .libs/libsmooth_stream.so $(builddir)/
	cp .libs/libsmooth_smooth.so $(builddir)/

# Test target
tests: smooth
	./smooth -d 4

# Make sure test files are included in distribution
EXTRA_DIST = rough.bmp

# Clean up additional files
CLEANFILES = *~ *.so
