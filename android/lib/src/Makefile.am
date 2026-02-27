AM_CPPFLAGS = -I$(top_srcdir)/android/lib/include -I$(top_srcdir) \
		-I$(top_srcdir)/headers -I$(top_srcdir)/conf $(SDL_CFLAGS) $(INCDIRS) \
		$(WINDOWING_SYSTEM) $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

noinst_LTLIBRARIES = libandroid.la

libandroid_la_SOURCES = \
	TouchUI_Android.cc
