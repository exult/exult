ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/headers -I$(srcdir)/imagewin -I$(srcdir)/shapes \
	-I$(srcdir)/server -I$(srcdir)/data -I$(srcdir)/gamemgr \
	-I$(srcdir)/objs -I$(srcdir)/conf -I$(srcdir)/files -I$(srcdir)/gumps \
	-I$(srcdir)/audio -I$(srcdir)/audio/midi_drivers -I$(srcdir)/pathfinder \
	-I$(srcdir)/usecode -I$(srcdir)/shapes/shapeinf \
	$(SDL_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) \
	$(DEBUG_FLAGS) $(CPPFLAGS) -DEXULT_DATADIR=\"$(EXULT_DATADIR)\"

CXXLINK = $(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link $(CXXLD) $(AM_CXXFLAGS) $(DEBUG_FLAGS) $(CXXFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $(EXE_TARGET)

if BUILD_MODS
MODSDIR=content
endif

SUBDIRS =  files conf data shapes imagewin pathfinder \
	gamemgr flic usecode tools audio gumps objs server \
	mapedit desktop docs $(MODSDIR)

bin_PROGRAMS = exult

exult_SOURCES =	\
	actions.cc	\
	actions.h	\
	actorio.cc	\
	actors.cc	\
	actors.h	\
	args.cc		\
	args.h		\
	headers/array_size.h	\
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
	readnpcs.cc	\
	rect.h		\
	schedule.cc	\
	schedule.h	\
	shapeid.cc	\
	shapeid.h	\
	singles.h	\
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
	files/zip/libminizip.la

exult_LDADD = \
	$(EXULTLIBS) \
	$(SDL_LIBS) $(SYSLIBS) $(x_libraries) $(ICON_FILE) \
	$(ZLIB_LIBS) $(VORBIS_LIBS) $(VORBISFILE_LIBS) $(OGG_LIBS) $(MT32EMU_LIBS)

exult_DEPENDENCIES = $(ICON_FILE) $(EXULTLIBS)

EXTRA_DIST = 	\
	autogen.sh \
	README \
	FAQ \
	README.md \
	README.win32 \
	README.MacOSX \
	Makefile.common \
	Makefile.mingw \
	Info.plist.in \
	stamp-h.in \
	windrag.cc \
	windrag.h \
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
	mapedit/tools/mockup/LICENCE \
	mapedit/tools/mockup/Makefile \
	mapedit/tools/mockup/README \
	mapedit/tools/mockup/defs.h \
	mapedit/tools/mockup/main.h \
	mapedit/tools/mockup/map.png \
	mapedit/tools/mockup/mappings.txt \
	mapedit/tools/mockup/mappings_alternative.txt \
	mapedit/tools/mockup/main.c \
	mapedit/tools/smooth/AUTHORS \
	mapedit/tools/smooth/COPYING \
	mapedit/tools/smooth/ChangeLog \
	mapedit/tools/smooth/INSTALL \
	mapedit/tools/smooth/README \
	mapedit/tools/smooth/config.h \
	mapedit/tools/smooth/globals.h \
	mapedit/tools/smooth/image.h \
	mapedit/tools/smooth/linked.h \
	mapedit/tools/smooth/param.h \
	mapedit/tools/smooth/plugin.h \
	mapedit/tools/smooth/plugins/README \
	mapedit/tools/smooth/plugins/README.Stream \
	mapedit/tools/smooth/plugins/plugin_randomize.c \
	mapedit/tools/smooth/plugins/plugin_smooth.c \
	mapedit/tools/smooth/plugins/plugin_stream.c \
	mapedit/tools/smooth/rough.bmp \
	mapedit/tools/smooth/smooth.c \
	mapedit/tools/smooth/smooth.h \
	mapedit/tools/smooth/smooth.sln \
	mapedit/tools/smooth/smooth.vcproj \
	mapedit/tools/smooth/smoothed.bmp \
	mapedit/tools/smooth/config.c \
	mapedit/tools/smooth/image.c \
	mapedit/tools/smooth/linked.c \
	mapedit/tools/smooth/param.c \
	mapedit/tools/smooth/plugin.c \
	server/servewin32.cc \
	server/servewin32.h

win32/exultico.o: $(srcdir)/win32/exultico.rc $(srcdir)/win32/exult.ico
	$(WINDRES) $(srcdir)/win32/exultico.rc win32/exultico.o

CLEANFILES = *~

if MACOSX

bundle_name = Exult.app
studio_name = Exult_Studio.app

bundle: ${EXE_TARGET}
	mkdir -p $(bundle_name)/Contents/MacOS
	mkdir -p $(bundle_name)/Contents/Resources/data/
	mkdir -p $(bundle_name)/Contents/Documents
	echo "APPL????" > $(bundle_name)/Contents/PkgInfo
	$(INSTALL_DATA) Info.plist $(bundle_name)/Contents/
	$(INSTALL_PROGRAM) $< $(bundle_name)/Contents/MacOS/
	$(INSTALL_DATA) $(srcdir)/macosx/exult.icns $(bundle_name)/Contents/Resources/
	$(INSTALL_DATA) $(srcdir)/data/exult.flx $(bundle_name)/Contents/Resources/data
	$(INSTALL_DATA) $(srcdir)/data/exult_bg.flx $(bundle_name)/Contents/Resources/data
	$(INSTALL_DATA) $(srcdir)/data/exult_si.flx $(bundle_name)/Contents/Resources/data
	$(INSTALL_DATA) $(srcdir)/AUTHORS $(bundle_name)/Contents/Documents
	$(INSTALL_DATA) $(srcdir)/COPYING $(bundle_name)/Contents/Documents
	$(INSTALL_DATA) $(srcdir)/FAQ $(bundle_name)/Contents/Documents
	$(INSTALL_DATA) $(srcdir)/NEWS $(bundle_name)/Contents/Documents
	$(INSTALL_DATA) $(srcdir)/README $(bundle_name)/Contents/Documents
	$(INSTALL_DATA) $(srcdir)/README.MacOSX $(bundle_name)/Contents/Documents
if WITH_OSX_CODE_SIGNATURE
	codesign --options runtime --deep -f -s "$(OSX_CODE_SIGNATURE)" $(bundle_name)
endif

studiobundle: ${EXE_TARGET}
	$(eval export SOURCE=$(shell pwd))
	gtk-mac-bundler ./macosx/exult_studio.bundle
if WITH_OSX_CODE_SIGNATURE
	codesign --options runtime --deep -f -s "$(OSX_CODE_SIGNATURE)" --entitlements ./macosx/exult_studio_entitlements.plist $(studio_name)
	codesign --options runtime -f -s "$(OSX_CODE_SIGNATURE)" $(studio_name)/contents/resources/lib/*.dylib
	codesign --options runtime -f -s "$(OSX_CODE_SIGNATURE)" $(studio_name)/Contents/Resources/lib/gdk-pixbuf-2.0/2.10.0/loaders/*.so
endif

install-exec-local: bundle
	mkdir -p $(DESTDIR)/Applications/
	cp -r $(bundle_name) $(DESTDIR)/Applications/

osxdmg: bundle
	mkdir -p Exult-snapshot
	mkdir -p ./Exult-snapshot/images
	cp $(srcdir)/AUTHORS ./Exult-snapshot/Authors
	cp $(srcdir)/COPYING ./Exult-snapshot/License
	cp $(srcdir)/NEWS ./Exult-snapshot/News
	cp $(srcdir)/README.MacOSX ./Exult-snapshot/ReadMe.MacOSX
	cp $(srcdir)/docs/ReadMe.html ./Exult-snapshot/ReadMe.html
	cp $(srcdir)/docs/faq.html ./Exult-snapshot/FAQ.html
	cp $(srcdir)/docs/images/docs*.* ./Exult-snapshot/images
	cp $(srcdir)/docs/images/*.gif ./Exult-snapshot/images
	cp $(srcdir)/macosx/diskback.png ./Exult-snapshot/images/diskback.png
	cp $(srcdir)/macosx/DS_Store ./Exult-snapshot/.DS_Store
	SetFile -a V ./Exult-snapshot/.DS_Store
	SetFile -a V ./Exult-snapshot/images
	SetFile -t ttro -c ttxt ./Exult-snapshot/Authors
	SetFile -t ttro -c ttxt ./Exult-snapshot/License
	SetFile -t ttro -c ttxt ./Exult-snapshot/News
	SetFile -t ttro -c ttxt ./Exult-snapshot/ReadMe.MacOSX
	cp -R $(bundle_name) ./Exult-snapshot/
	hdiutil create -ov -format UDZO -imagekey zlib-level=9 -fs HFS+ \
					-srcfolder Exult-snapshot \
					-volname "Exult Git snapshot$(REVISION)" \
					Exult-snapshot.dmg
	rm -rf Exult-snapshot

studiodmg:
	mkdir -p Studio-snapshot
	mkdir -p ./Studio-snapshot/images
	cp $(srcdir)/AUTHORS ./Studio-snapshot/Authors
	cp $(srcdir)/COPYING ./Studio-snapshot/License
	cp $(srcdir)/README.MacOSX ./Studio-snapshot/ReadMe.MacOSX
	cp $(srcdir)/docs/exult_studio.html ./Studio-snapshot/Exult_Studio.html
	cp $(srcdir)/docs/images/studio*.* ./Studio-snapshot/images
	cp $(srcdir)/docs/images/*.gif ./Studio-snapshot/images
	cp $(srcdir)/macosx/diskback.png ./Studio-snapshot/images/diskback.png
	cp $(srcdir)/macosx/DS_Store ./Studio-snapshot/.DS_Store
	SetFile -a V ./Studio-snapshot/.DS_Store
	SetFile -a V ./Studio-snapshot/images
	SetFile -t ttro -c ttxt ./Studio-snapshot/Authors
	SetFile -t ttro -c ttxt ./Studio-snapshot/License
	cp -R $(studio_name) ./Studio-snapshot/
	hdiutil create -ov -format UDZO -imagekey zlib-level=9 -fs HFS+ \
					-srcfolder Studio-snapshot \
					-volname "Exult Studio Git snapshot$(REVISION)" \
					ExultStudio-snapshot.dmg
	rm -rf Studio-snapshot

clean-local:
	-rm -f Exult-snapshot.dmg
	-rm -f ExultStudio-snapshot.dmg
	-rm -rf $(bundle_name)
	-rm -rf $(studio_name)

else
install-exec-local:

clean-local:

endif
