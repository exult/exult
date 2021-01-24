if BUILD_MT32EMU
MT32EMUFLAGS=-DMT32EMU_EXPORTS_TYPE=0 -DMT32EMU_API_TYPE=0
MT32EMUDRV=	\
	MT32EmuMidiDriver.cpp	\
	MT32EmuMidiDriver.h
endif

AM_CPPFLAGS = -I$(srcdir)/../../headers -I$(srcdir)/../../conf -I$(srcdir)/../.. \
		-I$(srcdir)/.. -I$(srcdir)/../../files -I$(srcdir)/../../imagewin \
		-I../../conf -I../../files -I../../objs -I$(srcdir)/../../shapes \
		$(SDL_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) \
		$(MT32EMUFLAGS) \
		$(DEBUG_FLAGS) $(CPPFLAGS)

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
	XMidiSequence.cpp	\
	XMidiSequence.h		\
	XMidiSequenceHandler.h	\
	FluidSynthMidiDriver.cpp	\
	FluidSynthMidiDriver.h

EXTRA_DIST = 		\
	README

CLEANFILES = *~
