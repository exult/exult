AM_CPPFLAGS = -I$(srcdir)/../headers -I$(srcdir)/../files -I$(srcdir)/../usecode \
	-I$(srcdir)/.. -I$(srcdir)/../shapes -I$(srcdir)/../imagewin \
	-I$(srcdir)/../conf $(DEBUG_FLAGS) $(CPPFLAGS) $(SDL_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS)

if HAVE_PNG
IPACK = ipack
else
IPACK =
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

bin_PROGRAMS = expack $(EXTRATOOLS) $(IPACK) $(GTHUMB)
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
	@$(SED) -e "s|\@bindir\@|$(bindir)|" $< > $@
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
	expack.txt

CLEANFILES = *~ gnome_u7shapes.thumbnailer
