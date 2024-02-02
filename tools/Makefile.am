AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir)/files -I$(top_srcdir)/usecode \
	-I$(top_srcdir) -I$(top_srcdir)/shapes -I$(top_srcdir)/imagewin \
	-I$(top_srcdir)/conf $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS) $(SDL_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS)

if CROSS_COMPILING
if BUILD_TOOLS
EXPACK_BIN = expack
else
EXPACK_BIN =
endif
else
EXPACK_BIN = expack
endif

if HAVE_PNG
if HAVE_SDL
IPACK = ipack
else
IPACK =
endif
GTHUMB=
endif

if BUILD_GTHUMB
GTHUMB=gnome_shp_thumbnailer
else
GTHUMB=
endif

if BUILD_TOOLS
NOINSTTOOLS =
EXTRATOOLS = wuc mklink rip cmanip splitshp shp2pcx textpack u7voice2syx
else
EXTRATOOLS =
NOINSTTOOLS =
endif

bin_PROGRAMS = $(EXPACK_BIN) $(EXTRATOOLS) $(IPACK) $(GTHUMB)
noinst_PROGRAMS = $(NOINSTTOOLS)

expack_SOURCES = \
	expack.cc

expack_LDADD = \
	../files/libu7file.la \
	$(SYSLIBS)

ipack_SOURCES = \
	ipack.cc

ipack_LDADD = \
	../files/libu7file.la \
	../shapes/libshapes.la \
	../imagewin/libimagewin.la \
	-lpng -lz $(SYSLIBS)

gnome_shp_thumbnailer_SOURCES = \
	gnome_shp_thumbnailer.cc

gnome_shp_thumbnailer_LDADD = \
	../shapes/libshapes.la \
	../imagewin/libimagewin.la \
	../files/libu7file.la \
	-lpng -lz $(SYSLIBS) $(GDK_PIXBUF_LIBS)

if BUILD_GTHUMB
## Does not work:
#thumbnailerdir=$(datadir)/thumbnailers
## Needs to be in /usr/share/thumbnailers
thumbnailerdir=/usr/share/thumbnailers
thumbnailer_in_files=gnome_u7shapes.thumbnailer.in
thumbnailer_DATA=$(thumbnailer_in_files:.thumbnailer.in=.thumbnailer)
gnome_shp_thumbnailer_DATA=x-shapefile.xml
gnome_shp_thumbnailerdir=$(DESTDIR)$(datadir)/mime/packages

# Rule to make the service file with bindir expanded
$(thumbnailer_DATA): $(thumbnailer_in_files)
	@$(SED) -e "s|\@bindir\@|$(bindir)|" $(thumbnailer_in_files) > $@
endif

wuc_SOURCES = \
	wuc.cc \
	uctools.h

mklink_SOURCES = mklink.cc

rip_SOURCES = rip.cc

shp2pcx_SOURCES = shp2pcx.cc

shp2pcx_LDADD = $(SYSLIBS)

splitshp_SOURCES = splitshp.cc

splitshp_LDADD = $(SYSLIBS)

textpack_SOURCES = textpack.cc

textpack_LDADD = \
	../files/libu7file.la \
	$(SYSLIBS)

u7voice2syx_SOURCES = u7voice2syx.cc

u7voice2syx_LDADD = \
	../files/libu7file.la \
	$(SYSLIBS)

cmanip_SOURCES = \
	cmanip.cc

cmanip_LDADD = \
	../conf/libconf.la \
	../files/libu7file.la \
	$(SYSLIBS)

if BUILD_GTHUMB
install-data-hook:
	update-mime-database $(DESTDIR)$(datadir)/mime || :

uninstall-hook:
	update-mime-database $(DESTDIR)$(datadir)/mime || :
else
install-data-hook:
uninstall-hook:
endif

EXTRA_DIST = \
	intrins1.txt \
	intrins2.txt \
	README \
	u7bgflag.txt \
	u7siflag.txt \
	ucformat.txt \
	expack.1 \
	gnome_u7shapes.thumbnailer.in \
	ipack.1 \
	ipack.txt \
	shp2pcx.1 \
	shp2pcx.txt \
	splitshp.1 \
	splitshp.txt \
	textpack.1 \
	textpack.txt \
	x-shapefile.xml \
	expack.txt

CLEANFILES = *~ gnome_u7shapes.thumbnailer
