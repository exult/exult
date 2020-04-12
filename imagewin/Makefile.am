AM_CPPFLAGS = -I$(srcdir)/../headers -I$(srcdir)/.. -I$(srcdir)/../files $(SDL_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) \
		-I$(srcdir)/../conf -I../conf \
		$(DEBUG_FLAGS) $(CPPFLAGS)

noinst_LTLIBRARIES = libimagewin.la

libimagewin_la_SOURCES = \
	ibuf16.cc \
	ibuf16.h \
	ibuf8.cc \
	ibuf8.h \
	imagebuf.cc \
	imagebuf.h \
	imagewin.cc \
	imagewin.h \
	iwin8.cc \
	iwin8.h \
	savepcx.cc \
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

EXTRA_DIST =
