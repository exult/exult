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
	scale_scalefx.h \
	scalefx_gpu.h \
	scalefx_gpu_ui.h \
	scaler_shaders.h \
	shaders/scalers_spirv.h \
	shaders/scalers_metal.h \
	shaders/scalers_dxil.h \
	shaders/scalers_hq_lut.h \
	scale_xbr.cc \
	scale_xbr.h
endif
endif

EXTRA_DIST = \
	tests/validate_spirv.py \
	tests/validate_metal.py \
	tests/run_gpu_tests.ps1 \
	tests/test_gpu_fallback.cc \
	tests/test_shader_selection.cc \
	tests/run_gpu_tests.sh \
	shaders/LICENSE.scalers \
	tests/test_gpu_lifecycle.cc \
	tests/test_gpu_scalers.cc \
	tests/scaler_reference.h \
	tests/scaler_reference.cc \
	tests/hq_reference.cc \
	shaders/generate_scalers.py \
	shaders/scalers.frag \
	shaders/scalers-output.frag \
	tests/test_scalefx_gpu_ui.cc \
	shaders/scalefx-reduce.frag \
	shaders/scalefx-matte.frag \
	shaders/LICENSE.scalefx \
	shaders/scalefx.vert \
	shaders/scalefx-0.frag \
	shaders/scalefx-1.frag \
	shaders/scalefx-2.frag \
	shaders/scalefx-3.frag \
	shaders/scalefx-4.frag \
	shaders/compile_scalefx.py \
	tests/test_scalefx_gpu.cc \
	tests/test_scalefx.cc \
	tests/benchmark_scalefx.cc \
	tests/compare_scalefx.py \
	tests/slopes-32-24.txt \
	tests/noise-17-19.txt
