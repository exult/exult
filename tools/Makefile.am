AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir)/files -I$(top_srcdir)/usecode \
	-I$(top_srcdir) -I$(top_srcdir)/shapes -I$(top_srcdir)/imagewin \
	-I$(top_srcdir)/conf $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS) $(SDL_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) $(PNG_CFLAGS)

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
EXULT_THUMB=
endif

if BUILD_EXULT_THUMB
EXULT_THUMB=exult_shp_thumbnailer
else
EXULT_THUMB=
endif

if BUILD_COMPILER
COMPILER_DIR = compiler
else
COMPILER_DIR =
endif

if BUILD_ASEPRITE_PLUGIN
ASEPRITE_DIR = aseprite_plugin
else
ASEPRITE_DIR =
endif

if BUILD_GIMP_PLUGIN
GIMP_DIR = gimp_plugin
else
GIMP_DIR =
endif

if BUILD_TOOLS
NOINSTTOOLS =
EXTRATOOLS = wuc mklink rip cmanip splitshp shp2pcx textpack u7voice2syx
else
EXTRATOOLS =
NOINSTTOOLS =
endif

if HAVE_SDL_IMAGE
MOCKUP_DIR = mockup
SMOOTH_DIR = smooth
else
MOCKUP_DIR =
SMOOTH_DIR =
endif

SUBDIRS = . ucxt $(COMPILER_DIR) $(ASEPRITE_DIR) $(GIMP_DIR) $(MOCKUP_DIR) $(SMOOTH_DIR)

bin_PROGRAMS = $(EXPACK_BIN) $(EXTRATOOLS) $(IPACK) $(EXULT_THUMB)
noinst_PROGRAMS = $(NOINSTTOOLS)

expack_SOURCES = \
	expack.cc

expack_LDADD = \
	$(top_builddir)/files/libu7file.la \
	$(SYSLIBS)

ipack_SOURCES = \
	ipack.cc

ipack_LDADD = \
	$(top_builddir)/files/libu7file.la \
	$(top_builddir)/shapes/libshapes.la \
	$(top_builddir)/imagewin/libimagewin.la \
	$(PNG_LIBS) $(ZLIB_LIBS) $(SYSLIBS)

exult_shp_thumbnailer_SOURCES = \
	exult_shp_thumbnailer.cc

exult_shp_thumbnailer_LDADD = \
	$(top_builddir)/shapes/libshapes.la \
	$(top_builddir)/imagewin/libimagewin.la \
	$(top_builddir)/files/libu7file.la \
	$(PNG_LIBS) $(ZLIB_LIBS) $(SYSLIBS) $(GDK_PIXBUF_LIBS)

if BUILD_EXULT_THUMB
# The actual executable "exult_shp_thumbnailer" is in <Exult>/bin
#  It needs two files visible from XDG_DATA_HOME or one of XDG_DATA_DIRS :
#   <XDG_DATA_HOME or one of XDG_DATA_DIRS>/mime/packages/x-shapefile.xml
#     Defines the MIME type "image/x-shapefile"
#   <XDG_DATA_HOME or one of XDG_DATA_DIRS>/thumbnailers/exult_u7shapes.thumbnailer
#     Thumbnails the U7 Shapes with "exult_shp_thumbnailer"
# The installation XDG_DATA_HOME or XDG_DATA_DIRS are chosen using
#  --enable-shp-thumbnailer=Target
#   with Target "yes" or "system" ( default ) => /usr/share ( requires root )
#   with Target "local"                       => /usr/local/share ( requires root )
#   with Target "user"                        => ~/.local/share
#   with Target "ingame"                      => <Exult>/share ( requires setting this folder in XDG_DATA_DIRS )
#
thumbnailerdir=$(DESTDIR)$(EXULT_THUMB_PREFIX)/thumbnailers
thumbnailer_in_files=$(top_srcdir)/tools/exult_u7shapes.thumbnailer.in
thumbnailer_DATA=$(thumbnailer_in_files:.thumbnailer.in=.thumbnailer)
exult_shp_thumbnailer_DATA=x-shapefile.xml
exult_shp_thumbnailerdir=$(DESTDIR)$(EXULT_THUMB_PREFIX)/mime/packages

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
	$(top_builddir)/files/libu7file.la \
	$(SYSLIBS)

u7voice2syx_SOURCES = u7voice2syx.cc

u7voice2syx_LDADD = \
	$(top_builddir)/files/libu7file.la \
	$(SYSLIBS)

cmanip_SOURCES = \
	cmanip.cc

cmanip_LDADD = \
	$(top_builddir)/conf/libconf.la \
	$(top_builddir)/files/libu7file.la \
	$(SYSLIBS)

if BUILD_EXULT_THUMB
install-data-hook:
	update-mime-database $(DESTDIR)$(EXULT_THUMB_PREFIX)/mime || :

uninstall-hook:
	update-mime-database $(DESTDIR)$(EXULT_THUMB_PREFIX)/mime || :
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
	exult_u7shapes.thumbnailer.in \
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

CLEANFILES = *~ exult_u7shapes.thumbnailer
