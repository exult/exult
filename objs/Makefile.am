AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir) -I$(top_srcdir)/imagewin -I$(top_srcdir)/shapes \
		-I$(top_srcdir)/pathfinder -I$(top_srcdir)/files \
		-I$(top_srcdir)/server -I$(top_srcdir)/gumps -I$(top_srcdir)/shapes/shapeinf \
		-I$(top_srcdir)/audio -I$(top_srcdir)/audio/midi_drivers -I$(top_srcdir)/usecode \
		$(SDL_CFLAGS) $(OGG_CFLAGS) \
		$(INCDIRS) $(WINDOWING_SYSTEM) $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

if BUILD_EXULT
noinst_LTLIBRARIES = libobjs.la

libobjs_la_SOURCES = \
	animate.cc \
	animate.h \
	barge.cc \
	barge.h \
	chunks.cc \
	chunks.h \
	chunkter.cc \
	chunkter.h \
	citerate.h \
	contain.cc \
	contain.h \
	egg.cc \
	egg.h \
	find_nearby.h \
	flags.h \
	iregobjs.cc \
	iregobjs.h \
	mappatch.cc \
	mappatch.h \
	objiter.h \
	objiter.cc \
	objlist.h \
	objs.cc \
	objs.h \
	ordinfo.h \
	spellbook.cc \
	spellbook.h \
	virstone.cc \
	virstone.h \
	jawbone.cc \
	jawbone.h \
	objnames.cc
endif

CLEANFILES = *~
