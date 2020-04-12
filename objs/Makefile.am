AM_CPPFLAGS = -I$(srcdir)/../headers -I$(srcdir)/.. -I$(srcdir)/../imagewin -I$(srcdir)/../shapes \
		-I$(srcdir)/../pathfinder -I$(srcdir)/../files \
		-I$(srcdir)/../server -I$(srcdir)/../gumps -I$(srcdir)/../shapes/shapeinf \
		-I$(srcdir)/../audio -I$(srcdir)/../audio/midi_drivers -I$(srcdir)/../usecode \
		$(SDL_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS) \
		$(INCDIRS) $(WINDOWING_SYSTEM) $(DEBUG_FLAGS) $(CPPFLAGS)

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

CLEANFILES = *~
