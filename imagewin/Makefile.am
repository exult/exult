AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir) -I$(top_srcdir)/files \
		-I$(top_srcdir)/shapes -I$(top_srcdir)/objs -I$(top_srcdir)/conf \
		$(SDL_CFLAGS) $(PNG_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) \
		$(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

if HAVE_SDL
noinst_LTLIBRARIES = libimagewin.la

libimagewin_la_SOURCES = \
	ibuf8.cc \
	ibuf8.h \
	imagebuf.cc \
	imagebuf.h \
	iwin8.cc \
	iwin8.h

if BUILD_EXULT
libimagewin_la_SOURCES += \
	imagewin.cc \
	imagewin.h \
	save_screenshot.cc \
	manip.h \
	scale_2x.cc \
	scale_2x.h \
	scale_bilinear.cc \
	scale_bilinear.h \
	scale_hq2x.cc \
	scale_hq2x.h \
	scale_hq3x.cc \
	scale_hq3x.h \
	scale_hq4x.cc \
	scale_hq4x.h \
	scale_hqnx.h \
	scale_interlace.cc \
	scale_interlace.h \
	scale_point.cc \
	scale_point.h \
	scale_2xSaI.cc \
	scale_2xSaI.h \
	ArbScaler.h \
	BilinearScaler.cpp \
	BilinearScaler.h \
	BilinearScalerInternal.h \
	BilinearScalerInternal_2x.cpp \
	BilinearScalerInternal_Arb.cpp \
	BilinearScalerInternal_X1Y12.cpp \
	BilinearScalerInternal_X2Y24.cpp \
	PointScaler.cpp \
	PointScaler.h \
	scale_xbr.cc \
	scale_xbr.h
endif
endif

EXTRA_DIST =
