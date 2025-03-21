AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir) -I$(top_srcdir)/shapes \
	-I$(top_srcdir)/imagewin -I$(top_srcdir)/conf -I$(top_srcdir)/gamemgr \
	-I$(top_srcdir)/files -I$(top_srcdir)/server  -I$(top_srcdir)/usecode \
	-I$(top_srcdir)/shapes/shapeinf \
	$(GTK_CFLAGS) $(PNG_CFLAGS) $(ICU_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) \
	$(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS) -DEXULT_DATADIR=\"$(EXULT_DATADIR)\"

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

#if ASEPRITE_PLUGIN
SUBDIRS = tools/aseprite_plugin
#end

if BUILD_STUDIO
bin_PROGRAMS = exult_studio $(GIMP_PLUGINS)
else
bin_PROGRAMS = $(GIMP_PLUGINS)
endif

u7shp_SOURCES = $(GIMP_PLUGIN_SRC).cc

u7shp_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/files -I$(top_srcdir)/headers \
	-I$(top_srcdir)/shapes -I$(top_srcdir)/imagewin -I$(top_srcdir)/files \
	$(GIMP_INCLUDES) $(GTK_CFLAGS) $(PNG_CFLAGS) $(ICU_CFLAGS) $(INCDIRS) \
	$(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS) -DHAVE_CONFIG_H

u7shp_LDADD = \
	$(top_builddir)/imagewin/libimagewin.la	\
	$(top_builddir)/shapes/libshapes.la \
	$(top_builddir)/files/libu7file.la \
	$(GIMP_LIBS)

exult_studio_SOURCES = \
	bargeedit.cc \
	chunklst.cc \
	chunklst.h \
	combo.cc \
	combo.h \
	compile.cc \
	eggedit.cc \
	execbox.h \
	execbox.cc \
	locator.cc \
	locator.h \
	maps.cc \
	npcedit.cc \
	npclst.cc \
	npclst.h \
	objedit.cc \
	contedit.cc \
	shapeedit.cc \
	studio.cc \
	studio.h \
	exult_studio.cc \
	objbrowse.h \
	objbrowse.cc \
	paledit.cc \
	paledit.h \
	shapedraw.cc \
	shapedraw.h \
	shapefile.cc \
	shapefile.h \
	shapegroup.cc \
	shapegroup.h \
	shapelst.cc \
	shapelst.h \
	ucbrowse.cc \
	ucbrowse.h \
	gtk_redefines.h


noinst_PROGRAMS = $(GIMP_PLUGINS)

exult_studio_LDADD = \
	$(top_builddir)/conf/libconf.la		\
	$(top_builddir)/imagewin/libimagewin.la	\
	$(top_builddir)/gamemgr/libgamemgr.la	\
	$(top_builddir)/shapes/libshapes.la 		\
	$(top_builddir)/files/libu7file.la		\
	$(top_builddir)/files/zip/libminizip.la		\
	$(top_builddir)/server/libserver.la		\
	$(top_builddir)/usecode/libusecode.la		\
	$(top_builddir)/shapes/shapeinf/libshapeinf.la	\
	$(PNG_LIBS) $(FREETYPE2_LIBS) $(SYSLIBS) $(x_libraries) $(GTK_LIBS) $(ICU_LIBS) $(ZLIB_LIBS)

exult_studio_CFLAGS = $(GTK_CFLAGS) $(PNG_CFLAGS) $(ICU_CFLAGS)
exult_studio_LDFLAGS = -export-dynamic		# For Gtk+ 2.x

mapeditdir = $(datadir)/exult

if BUILD_STUDIO
mapedit_DATA = exult_studio.glade
else
mapedit_DATA =
endif

EXTRA_DIST=	\
	gimpwin32.txt \
	PhotoshopShapePlugin.zip \
	exult_studio.glade \
	uniquepal.c \
	logo.xpm

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

CLEANFILES = *~ u7shp$(EXEEXT)

