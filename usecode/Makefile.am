AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir) -I$(top_srcdir)/files -I$(top_srcdir)/imagewin \
		-I$(top_srcdir)/shapes -I$(top_srcdir)/objs -I$(top_srcdir)/audio -I$(top_srcdir)/audio/midi_drivers \
		-I$(top_srcdir)/gumps -I$(top_srcdir)/tools -I$(top_srcdir)/shapes/shapeinf \
		-I$(top_srcdir)/server -I$(top_srcdir)/conf \
		$(SDL_CFLAGS) $(OGG_CFLAGS) $(INCDIRS) \
		$(WINDOWING_SYSTEM) $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

noinst_LTLIBRARIES = libusecode.la

libusecode_la_SOURCES = \
	opcodes.h	\
	bgintrinsics.h	\
	siintrinsics.h	\
	sibetaintrinsics.h	\
	ucsymtbl.cc	\
	ucsymtbl.h	\
	usefuns.h

if BUILD_EXULT
libusecode_la_SOURCES += \
	intrinsics.cc	\
	ucinternal.cc	\
	ucinternal.h	\
	ucmachine.cc	\
	ucmachine.h	\
	ucsched.h	\
	ucsched.cc	\
	ucscriptop.h	\
	useval.cc	\
	useval.h	\
	conversation.cc \
	conversation.h	\
	keyring.cc	\
	keyring.h	\
	ucdisasm.cc	\
	ucdebugging.cc	\
	ucdebugging.h	\
	debugserver.cc	\
	debugserver.h	\
	debugmsg.h	\
	stackframe.cc	\
	stackframe.h	\
	ucserial.h	\
	ucserial.cc	\
	ucfunction.h	\
	ucfunction.cc
endif

CLEANFILES = *~
