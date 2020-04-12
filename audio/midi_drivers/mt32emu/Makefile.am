MT32EMU_VERSION_MAJOR=2
MT32EMU_VERSION_MINOR=0
MT32EMU_VERSION_PATCH=0
MT32EMU_VERSION=\"$(MT32EMU_VERSION_MAJOR).$(MT32EMU_VERSION_MINOR).$(MT32EMU_VERSION_PATCH)\"

AM_CPPFLAGS = -I$(srcdir)/../../../headers -I$(srcdir)/../../../conf \
			-I$(srcdir)/../../.. -I$(srcdir)/../.. -I$(srcdir)/.. \
			-I$(srcdir)/../../../files -I$(srcdir)/../../../imagewin \
			-I$(srcdir)/../../../shapes \
			$(SDL_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) \
			-I../../../conf -I../../../files \
			-DMT32EMU_EXPORTS_TYPE=0 -DMT32EMU_API_TYPE=0 \
			-DMT32EMU_VERSION_MAJOR=$(MT32EMU_VERSION_MAJOR) \
			-DMT32EMU_VERSION_MINOR=$(MT32EMU_VERSION_MINOR) \
			-DMT32EMU_VERSION_PATCH=$(MT32EMU_VERSION_PATCH) \
			-DMT32EMU_VERSION=$(MT32EMU_VERSION) \
			$(DEBUG_FLAGS) $(CPPFLAGS)

noinst_LTLIBRARIES = libmt32emu.la

libmt32emu_la_SOURCES =	\
	Analog.cpp \
	Analog.h \
	BReverbModel.cpp \
	BReverbModel.h \
	Enumerations.h \
	File.cpp \
	File.h \
	FileStream.cpp \
	FileStream.h \
	globals.h \
	internals.h \
	LA32FloatWaveGenerator.h \
	LA32Ramp.cpp \
	LA32Ramp.h \
	LA32WaveGenerator.cpp \
	LA32WaveGenerator.h \
	MemoryRegion.h \
	MidiEventQueue.h \
	MidiStreamParser.cpp \
	MidiStreamParser.h \
	mmath.h \
	mt32emu.h \
	Part.cpp \
	Part.h \
	Partial.cpp \
	Partial.h \
	PartialManager.cpp \
	PartialManager.h \
	Poly.cpp \
	Poly.h \
	ROMInfo.cpp \
	ROMInfo.h \
	Structures.h \
	Synth.cpp \
	Synth.h \
	Tables.cpp \
	Tables.h \
	TVA.cpp \
	TVA.h \
	TVF.cpp \
	TVF.h \
	TVP.cpp \
	TVP.h \
	Types.h

CLEANFILES = *~
