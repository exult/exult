AM_CPPFLAGS = -I$(srcdir)/../headers -I$(srcdir)/.. -I$(srcdir)/../files -I$(srcdir)/../imagewin \
		-I$(srcdir)/../shapes -I$(srcdir)/../objs -I$(srcdir)/../audio -I$(srcdir)/../audio/midi_drivers \
		-I$(srcdir)/../gumps -I$(srcdir)/../tools -I$(srcdir)/../shapes/shapeinf \
		-I$(srcdir)/../server $(SDL_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS) $(INCDIRS) \
		$(WINDOWING_SYSTEM) $(DEBUG_FLAGS) $(CPPFLAGS)

SUBDIRS = . compiler ucxt

noinst_LTLIBRARIES = libusecode.la

libusecode_la_SOURCES = \
	bgintrinsics.h	\
	siintrinsics.h	\
	sibetaintrinsics.h	\
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
	ucfunction.cc	\
	ucsymtbl.cc	\
	ucsymtbl.h	\
	usefuns.h

CLEANFILES = *~
