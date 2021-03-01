AM_CPPFLAGS = -I$(srcdir)/../headers -I$(srcdir)/.. -I$(srcdir)/../files \
	-I$(srcdir)/../imagewin -I$(srcdir)/../audio -I$(srcdir)/../shapes/shapeinf \
	-I$(srcdir)/../data -I$(srcdir)/../usecode -I$(srcdir)/../objs \
	$(SDL_CFLAGS) $(FREETYPE2_INCLUDES) \
	$(INCDIRS) $(WINDOWING_SYSTEM) $(DEBUG_FLAGS) $(CPPFLAGS)

SUBDIRS = shapeinf

noinst_LTLIBRARIES = libshapes.la

libshapes_la_SOURCES = \
	baseinf.h \
	data_utils.h \
	font.cc \
	font.h \
	fontgen.cc \
	fontgen.h \
	fontvga.cc \
	fontvga.h \
	items.cc \
	items.h \
	miscinf.cc \
	miscinf.h \
	pngio.cc \
	pngio.h \
	ready.h \
	shapeinf.cc \
	shapeinf.h \
	shapevga.cc \
	shapevga.h \
	shapewrite.cc \
	u7drag.cc \
	u7drag.h \
	vgafile.cc \
	vgafile.h

CLEANFILES = *~

