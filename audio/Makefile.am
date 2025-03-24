AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir) -I$(top_srcdir)/imagewin -I$(top_srcdir)/shapes \
		-I$(top_srcdir)/objs -I$(top_srcdir)/files  -I$(top_srcdir)/ -I$(top_srcdir)/gumps \
		-I$(top_srcdir)/conf -I$(top_srcdir)/audio/midi_drivers \
		$(SDL_CFLAGS) $(OGG_CFLAGS) \
		$(INCDIRS) $(WINDOWING_SYSTEM) $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

if BUILD_EXULT
SUBDIRS = midi_drivers

noinst_LTLIBRARIES = libaudio.la

libaudio_la_SOURCES =	\
	Audio.cc	\
	Audio.h		\
	Midi.cc		\
	Midi.h		\
	conv.cc		\
	conv.h		\
	soundtest.cc	\
	soundtest.h	\
	convmusic.h     \
	AudioChannel.cc \
	AudioChannel.h  \
	AudioMixer.cc   \
	AudioMixer.h    \
	AudioSample.cc  \
	AudioSample.h   \
	OggAudioSample.cc \
	OggAudioSample.h  \
	RawAudioSample.cc \
	RawAudioSample.h  \
	VocAudioSample.cc \
	VocAudioSample.h  \
	WavAudioSample.cc \
	WavAudioSample.h

EXTRA_DIST = 		\
	module.mk	\
	mtest		\
	README
endif

CLEANFILES = *~
