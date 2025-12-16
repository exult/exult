AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir) -I$(top_srcdir)/files \
	-I$(top_srcdir)/objs -I$(top_srcdir)/imagewin -I$(top_srcdir)/shapes \
	-I$(top_srcdir)/usecode -I$(top_srcdir)/gumps -I$(top_srcdir)/audio -I$(top_srcdir)/audio/midi_drivers \
	$(SDL_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

noinst_LTLIBRARIES = libserver.la

libserver_la_SOURCES = 	\
	objserial.cc	\
	objserial.h	\
	servemsg.cc	\
	servemsg.h	\
	server.cc	\
	server.h	\
	servewin32.cc	\
	servewin32.h

CLEANFILES = *~
