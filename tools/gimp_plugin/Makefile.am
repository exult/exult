AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/files -I$(top_srcdir)/headers \
	-I$(top_srcdir)/shapes -I$(top_srcdir)/imagewin -I$(top_srcdir)/mapedit \
	$(GIMP_CFLAGS) $(GTK_CFLAGS) $(PNG_CFLAGS) $(ICU_CFLAGS) $(INCDIRS) \
	$(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS) -DHAVE_CONFIG_H

if GIMP3_PLUGIN
GIMP_PLUGIN_SRC = u7shp
GIMP_PLUGINS = u7shp
else
if GIMP2_PLUGIN
GIMP_PLUGIN_SRC = u7shp-old
GIMP_PLUGINS = u7shp
else
GIMP_PLUGINS =
endif
endif

u7shp_SOURCES = $(GIMP_PLUGIN_SRC).cc

u7shp_LDADD = \
	$(top_builddir)/imagewin/libimagewin.la	\
	$(top_builddir)/shapes/libshapes.la \
	$(top_builddir)/files/libu7file.la \
	$(GIMP_LIBS)

noinst_PROGRAMS = $(GIMP_PLUGINS)

if GIMP3_PLUGIN
install-exec-local:
	install -d $(DESTDIR)$(GIMP_PLUGIN_PREFIX)/u7shp
	install -c $(GIMP_PLUGINS) $(DESTDIR)$(GIMP_PLUGIN_PREFIX)/u7shp
else
if GIMP2_PLUGIN
install-exec-local:
	install -d $(DESTDIR)$(GIMP_PLUGIN_PREFIX)
	install -c $(GIMP_PLUGINS) $(DESTDIR)$(GIMP_PLUGIN_PREFIX)
else
install-exec-local:
endif
endif

CLEANFILES = *~
