if BUILD_MT32EMU
MT32EMUFLAGS=-DMT32EMU_EXPORTS_TYPE=0 -DMT32EMU_API_TYPE=0
MT32EMUDRV=	\
	MT32EmuMidiDriver.cpp	\
	MT32EmuMidiDriver.h
endif

AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir)/conf -I$(top_srcdir) \
		-I$(top_srcdir)/audio -I$(top_srcdir)/files  -I$(top_srcdir)/gumps -I$(top_srcdir)/imagewin \
		-I$(top_srcdir)/objs -I$(top_srcdir)/shapes \
		$(SDL_CFLAGS) $(MT32EMU_CFLAGS) $(ALSA_CFLAGS) $(FLUID_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) \
		$(MT32EMUFLAGS) \
		$(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

SUBDIRS = timidity

noinst_LTLIBRARIES = libmididrv.la

libmididrv_la_SOURCES =	\
	ALSAMidiDriver.cpp	\
	ALSAMidiDriver.h	\
	CoreAudioMidiDriver.cpp	\
	CoreAudioMidiDriver.h	\
	CoreMidiDriver.cpp \
	CoreMidiDriver.h \
	fmopl.cpp		\
	fmopl.h			\
	FMOplMidiDriver.cpp	\
	FMOplMidiDriver.h	\
	LowLevelMidiDriver.cpp	\
	LowLevelMidiDriver.h	\
	MidiDriver.cpp		\
	MidiDriver.h		\
	$(MT32EMUDRV)	\
	TimidityMidiDriver.cpp	\
	TimidityMidiDriver.h	\
	UnixSeqMidiDriver.cpp	\
	UnixSeqMidiDriver.h	\
	WindowsMidiDriver.cpp	\
	WindowsMidiDriver.h	\
	XMidiEvent.h		\
	XMidiEventList.cpp	\
	XMidiEventList.h	\
	XMidiFile.cpp		\
	XMidiFile.h		\
	XMidiNoteStack.h	\
	XMidiRecyclable.h \
	XMidiSequence.cpp	\
	XMidiSequence.h		\
	XMidiSequenceHandler.h	\
	FluidSynthMidiDriver.cpp	\
	FluidSynthMidiDriver.h

EXTRA_DIST = 		\
	README

CLEANFILES = *~
