AM_CPPFLAGS = -I$(srcdir)/../headers -I$(srcdir)/.. -I$(srcdir)/../shapes \
	-I$(srcdir)/../imagewin -I$(srcdir)/../conf -I$(srcdir)/../gamemgr \
	-I$(srcdir)/../files -I$(srcdir)/../server  -I$(srcdir)/../usecode \
	-I$(srcdir)/../shapes/shapeinf \
	$(GTK_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) $(DEBUG_FLAGS) $(CPPFLAGS) -DEXULT_DATADIR=\"$(EXULT_DATADIR)\"

if GIMP_PLUGIN
GIMP_PLUGINS=u7shp
else
GIMP_PLUGINS=
endif

if BUILD_STUDIO
bin_PROGRAMS = exult_studio
else
bin_PROGRAMS =
endif

u7shp_SOURCES = u7shp.cc

u7shp_CFLAGS = -I$(srcdir)/../shapes -I$(srcdir)/../imagewin -I$(srcdir)/../files $(DEBUG_FLAGS) $(CPPFLAGS)

u7shp_LDADD = \
	../imagewin/libimagewin.la	\
	../shapes/libshapes.la \
	../files/libu7file.la

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
	../conf/libconf.la		\
	../imagewin/libimagewin.la	\
	../gamemgr/libgamemgr.la	\
	../shapes/libshapes.la 		\
	../files/libu7file.la		\
	../files/zip/libminizip.la		\
	../server/libserver.la		\
	../usecode/libusecode.la		\
	../shapes/shapeinf/libshapeinf.la	\
	-lpng $(FREETYPE2_LIBS) $(SYSLIBS) $(x_libraries) $(GTK_LIBS) $(ZLIB_LIBS)

exult_studio_CFLAGS = $(GTK_CFLAGS)
exult_studio_LDFLAGS = -export-dynamic		# For Gtk+ 2.x

mapeditdir = $(datadir)/exult

if BUILD_STUDIO
mapedit_DATA = exult_studio.glade
else
mapedit_DATA =
endif

u7shp$(EXEEXT): u7shp.cc $(top_builddir)/config.h
	CPPFLAGS="$(u7shp_CFLAGS)" CXXFLAGS="-O2 -I$(top_srcdir) -DHAVE_CONFIG_H" $(GIMPTOOL) --build $<

EXTRA_DIST=	\
	gimpwin32.txt \
	PhotoshopShapePlugin.zip \
	exult_studio.glade \
	uniquepal.c \
	logo.xpm

if GIMP_PLUGIN
install-exec-local:
	install -d $(DESTDIR)$(GIMP_PLUGIN_PREFIX)
	install -c $(GIMP_PLUGINS) $(DESTDIR)$(GIMP_PLUGIN_PREFIX)
else
install-exec-local:
endif

CLEANFILES = *~ u7shp$(EXEEXT)

