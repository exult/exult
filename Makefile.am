ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/headers -I$(srcdir)/imagewin -I$(srcdir)/shapes \
	-I$(srcdir)/server -I$(srcdir)/data -I$(srcdir)/gamemgr \
	-I$(srcdir)/objs -I$(srcdir)/conf -I$(srcdir)/files -I$(srcdir)/gumps \
	-I$(srcdir)/audio -I$(srcdir)/audio/midi_drivers -I$(srcdir)/pathfinder \
	-I$(srcdir)/usecode -I$(srcdir)/shapes/shapeinf \
	$(SDL_CFLAGS) $(OGG_CFLAGS) $(PNG_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) \
	$(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS) -DEXULT_DATADIR=\"$(EXULT_DATADIR)\"

SUBDIRS = files conf data shapes imagewin pathfinder \
	gamemgr flic usecode tools audio gumps objs server \
	mapedit desktop docs

if BUILD_MODS
SUBDIRS += content
endif

# Build top-level android dir when building the apk with a native build.
if ANDROID_APK
SUBDIRS += android
endif

# Build android lib dir when cross-compiling for android.
if ANDROID
SUBDIRS += android/lib
AM_CPPFLAGS += -I$(srcdir)/android/lib/include
ANDROID_LIB = android/lib/src/libandroid.la
endif

EXULTSOURCES =	\
	actions.cc	\
	actions.h	\
	actorio.cc	\
	actors.cc	\
	actors.h	\
	args.cc		\
	args.h		\
	browser.cc	\
	browser.h	\
	cheat.cc	\
	cheat.h		\
	cheat_screen.cc	\
	cheat_screen.h	\
	combat.cc	\
	combat.h	\
	combat_opts.h	\
	headers/common_types.h	\
	dir.cc		\
	dir.h		\
	drag.cc		\
	drag.h		\
	effects.cc	\
	effects.h	\
	headers/exceptions.h	\
	exult.cc	\
	exult.h		\
	exulticon.h	\
	exultmenu.cc	\
	exultmenu.h	\
	exult_constants.h	\
	fnames.h	\
	frameseq.h	\
	game.cc		\
	game.h		\
	gameclk.cc	\
	gameclk.h	\
	gamedat.cc	\
	gamemap.cc	\
	gamemap.h	\
	gamerend.cc	\
	gamerend.h	\
	gamewin.cc	\
	gamewin.h	\
	headers/gamma.h	\
	hash_utils.h	\
	headers/ignore_unused_variable_warning.h	\
	headers/ios_state.hpp	\
	istring.cc	\
	istring.h	\
	keys.cc		\
	keys.h		\
	keyactions.cc	\
	keyactions.h	\
	menulist.cc	\
	menulist.h	\
	monsters.cc	\
	monsters.h	\
	mouse.cc	\
	mouse.h		\
	npcnear.cc	\
	npcnear.h	\
	npctime.cc	\
	npctime.h	\
	palette.cc	\
	palette.h	\
	party.cc	\
	party.h		\
	paths.cc	\
	paths.h		\
	headers/pent_include.h	\
	playscene.cc	\
	playscene.h	\
	readnpcs.cc	\
	rect.h		\
	schedule.cc	\
	schedule.h	\
	shapeid.cc	\
	shapeid.h	\
	singles.h	\
	headers/span.h	\
	touchui.cc	\
	touchui.h	\
	tiles.h		\
	tqueue.cc	\
	tqueue.h	\
	txtscroll.cc	\
	txtscroll.h	\
	verify.cc	\
	verify.h	\
	version.cc	\
	version.h

EXULTLIBS = \
	pathfinder/libpathfinder.la		\
	audio/libaudio.la	\
	audio/midi_drivers/libmididrv.la	\
	audio/midi_drivers/timidity/libtimidity.la	\
	conf/libconf.la		\
	imagewin/libimagewin.la	\
	server/libserver.la \
	gamemgr/libgamemgr.la	\
	flic/libflic.la \
	usecode/libusecode.la	\
	shapes/libshapes.la \
	shapes/shapeinf/libshapeinf.la \
	objs/libobjs.la \
	gumps/libgumps.la \
	files/libu7file.la \
	files/sha1/libsha1.la \
	files/zip/libminizip.la \
	$(ANDROID_LIB)

EXULTLIBADD = \
	$(EXULTLIBS) $(PNG_LIBS) \
	$(SDL_LIBS) $(SYSLIBS) $(x_libraries) $(ICON_FILE) \
	$(ZLIB_LIBS) $(OGG_LIBS) $(MT32EMU_LIBS) $(FLUID_LIBS) $(ALSA_LIBS)

EXULTDEPENDENCIES = $(ICON_FILE) $(EXULTLIBS)

# If we've been asked to build exult as a library...
if BUILD_LIBEXULT
lib_LTLIBRARIES = libexult.la
libexult_la_SOURCES = $(EXULTSOURCES)
libexult_la_LIBADD = $(EXULTLIBADD)
libexult_la_DEPENDENCIES = $(EXULTDEPENDENCIES)
libexult_la_LDFLAGS=-release $(PACKAGE_VERSION)

# Otherwise we must be building an executable
else # !BUILD_LIBEXULT
if BUILD_EXULT
bin_PROGRAMS = exult
exult_SOURCES = $(EXULTSOURCES)
exult_LDADD = $(EXULTLIBADD)
exult_DEPENDENCIES = $(EXULTDEPENDENCIES)

# Workaround for automake issue with using the same sources for both a library and an executable.
# See https://www.gnu.org/software/automake/manual/html_node/Objects-created-both-with-libtool-and-without.html
exult_CPPFLAGS = $(AM_CPPFLAGS)
else # !BUILD_EXULT
bin_PROGRAMS =
endif
endif

EXTRA_DIST = 	\
	README \
	FAQ \
	README.md \
	README.windows \
	README.macOS \
	Makefile.common \
	Makefile.mingw \
	Info.plist.in \
	stamp-h.in \
	win32/exult.ico \
	win32/exultico.rc \
	win32/exultstudio.ico \
	win32/exultstudioico.rc \
	win32/exconfig.cc \
	win32/exconfig.def \
	win32/exconfig.h \
	win32/exconfig.rc \
	win32/exult.exe.manifest \
	win32/exult_installer.iss \
	win32/exult_shpplugin_installer.iss \
	win32/exult_studio.exe.manifest \
	win32/exult_studio_installer.iss \
	win32/exult_tools_installer.iss \
	win32/glib-xp.patch \
	win32/README-SDL.txt \
	macosx/exult.icns \
	macosx/exult_studio_info.plist.in \
	macosx/diskback.png \
	macosx/DS_Store \
	macosx/exult_studio.bundle \
	macosx/exult_studio_entitlements.plist \
	macosx/exult_studio.icns \
	macosx/exult_studio_launcher.sh \
	msvcstuff/vs2019/.gitignore \
	msvcstuff/vs2019/Exult.sln \
	msvcstuff/vs2019/Exult.vcxproj \
	msvcstuff/vs2019/Exult.vcxproj.filters \
	msvcstuff/vs2019/dirent.h \
	msvcstuff/vs2019/expack/expack.vcxproj \
	msvcstuff/vs2019/expack/expack.vcxproj.filters \
	msvcstuff/vs2019/msvc_include.h \
	msvcstuff/vs2019/packages.config \
	msvcstuff/vs2019/unistd.h \
	ios/Exult.xcodeproj \
	ios/Exult.xcodeproj/project.pbxproj \
	ios/GamePadView.h \
	ios/GamePadView.m \
	ios/Images/Icon.png \
	ios/Images/btn.png \
	ios/Images/btnpressed.png \
	ios/Images/dpadglass-east.png \
	ios/Images/dpadglass-north.png \
	ios/Images/dpadglass-northeast.png \
	ios/Images/dpadglass-northwest.png \
	ios/Images/dpadglass-south.png \
	ios/Images/dpadglass-southeast.png \
	ios/Images/dpadglass-southwest.png \
	ios/Images/dpadglass-west.png \
	ios/Images/dpadglass.png \
	ios/Images/joypad-glass-east.png \
	ios/Images/joypad-glass-north.png \
	ios/Images/joypad-glass-northeast.png \
	ios/Images/joypad-glass-northwest.png \
	ios/Images/joypad-glass-south.png \
	ios/Images/joypad-glass-southeast.png \
	ios/Images/joypad-glass-southwest.png \
	ios/Images/joypad-glass-west.png \
	ios/Images/joypad-glass.png \
	ios/Images/joythumb-glass.png \
	ios/Info.plist \
	ios/Media.xcassets/Contents.json \
	ios/Media.xcassets/AppIcon.appiconset/Contents.json \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-20x20@1x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-20x20@2x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-20x20@3x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-29x29@1x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-29x29@2x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-29x29@3x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-40x40@1x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-40x40@2x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-40x40@3x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-57x57@1x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-57x57@2x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-60x60@2x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-60x60@3x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-72x72@1x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-72x72@2x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-76x76@1x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-76x76@2x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-App-83.5x83.5@2x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-AppStore.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-Small-50x50@1x.png \
	ios/Media.xcassets/AppIcon.appiconset/Icon-Small-50x50@2x.png \
	ios/SDL2/dummy \
	ios/data/dummy \
	ios/include/config.h \
	ios/include/ios_utils.h \
	ios/ios_utils.mm \
	ios/libmt32emu/dummy \
	ios/libmt32emu.xcodeproj/project.pbxproj \
	ios/libogg/dummy \
	ios/libogg.xcodeproj/project.pbxproj \
	ios/libvorbis/dummy \
	ios/libvorbis.xcodeproj/project.pbxproj \
	tools/mockup/LICENCE \
	tools/mockup/Makefile \
	tools/mockup/README \
	tools/mockup/defs.h \
	tools/mockup/main.h \
	tools/mockup/map.png \
	tools/mockup/mappings.txt \
	tools/mockup/mappings_alternative.txt \
	tools/mockup/main.c \
	tools/smooth/AUTHORS \
	tools/smooth/COPYING \
	tools/smooth/ChangeLog \
	tools/smooth/INSTALL \
	tools/smooth/README \
	tools/smooth/config.h \
	tools/smooth/globals.h \
	tools/smooth/image.h \
	tools/smooth/linked.h \
	tools/smooth/param.h \
	tools/smooth/plugin.h \
	tools/smooth/plugins/README \
	tools/smooth/plugins/README.Stream \
	tools/smooth/plugins/plugin_randomize.c \
	tools/smooth/plugins/plugin_smooth.c \
	tools/smooth/plugins/plugin_stream.c \
	tools/smooth/rough.bmp \
	tools/smooth/smooth.c \
	tools/smooth/smooth.h \
	tools/smooth/smooth.sln \
	tools/smooth/smooth.vcproj \
	tools/smooth/smoothed.bmp \
	tools/smooth/config.c \
	tools/smooth/image.c \
	tools/smooth/linked.c \
	tools/smooth/param.c \
	tools/smooth/plugin.c \
	server/servewin32.cc \
	server/servewin32.h

win32/exultico.o: $(srcdir)/win32/exultico.rc $(srcdir)/win32/exult.ico
	$(WINDRES) $(srcdir)/win32/exultico.rc win32/exultico.o

#explicitly add gitinfo.h as dependency of version.o
exult-version.$(OBJEXT) version.$(OBJEXT): gitinfo.h

#gitinfo.h will be generated if certain git files change
gitinfo.h: $(srcdir)/.git/index $(srcdir)/.git/HEAD $(srcdir)/.git/config Makefile
	@cd $(srcdir); \
	if git rev-parse HEAD >/dev/null 2>&1;then  \
		echo //this file is generated by the exult build process > gitinfo-new.h; \
		echo "#define GIT_REVISION \"`git rev-parse HEAD`\"" >> gitinfo-new.h; \
		echo "static constexpr const char * GIT_TAG[] = {" >> gitinfo-new.h; \
		git tag -l --points-at HEAD --format="\"%(refname:short)\"," >> gitinfo-new.h; \
		echo "\"\"};">> gitinfo-new.h; \
		git branch --list "`git branch --show-current`" --format="%(if)%(upstream)%(then)#define GIT_REMOTE_BRANCH \"%(upstream:strip=-1)\"%(end)" >> gitinfo-new.h; \
		echo >> gitinfo-new.h; \
		echo "#define GIT_REMOTE_URL \"`git ls-remote --get-url`\"" >> gitinfo-new.h; \
		echo >> gitinfo-new.h; \
		echo Built gitinfo.h; \
		if test -f gitinfo.h && diff -q gitinfo-new.h gitinfo.h >/dev/null; then \
			rm gitinfo-new.h; \
		else \
			mv gitinfo-new.h gitinfo.h; \
		fi; \
	fi


#these do nothing but are here just incase the files dosn't exist so prerequities are met
$(srcdir)/.git/index:
$(srcdir)/.git/HEAD:
$(srcdir)/.git/config:

BUILT_SOURCES=gitinfo.h

CLEANFILES = gitinfo.h *~


if MACOSX
include $(srcdir)/macosx/macosx.am
else
install-exec-local:

clean-local:

endif
