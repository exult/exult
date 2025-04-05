AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/headers \
	$(SDL_CFLAGS) $(SDL_IMAGE_CFLAGS) $(INCDIRS) \
	$(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS) -DHAVE_CONFIG_H

bin_PROGRAMS = mockup

mockup_SOURCES = main.c main.h defs.h

mockup_LDADD = \
	$(SDL_LIBS) \
	$(SDL_IMAGE_LIBS)

# Custom test target
check-local: mockup
	./mockup map.png mappings.txt

# Make sure these files are included in distribution
EXTRA_DIST = map.png mappings.txt

# noinst_PROGRAMS = mockup

# Clean up additional files
CLEANFILES = *~ u7map
