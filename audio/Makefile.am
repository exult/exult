AM_CPPFLAGS = -I$(srcdir)/../headers -I$(srcdir)/.. -I$(srcdir)/../imagewin -I$(srcdir)/../shapes \
		-I$(srcdir)/../objs -I$(srcdir)/../files -I$(srcdir)/../gumps \
		-I$(srcdir)/../conf -I$(srcdir)/midi_drivers -I../files -I../conf \
		$(SDL_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS) \
		$(INCDIRS) $(WINDOWING_SYSTEM) $(DEBUG_FLAGS) $(CPPFLAGS)

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
	soundtest.h	    \
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
	README

CLEANFILES = *~
