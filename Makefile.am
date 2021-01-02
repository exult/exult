ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/headers -I$(srcdir)/imagewin -I$(srcdir)/shapes \
	-I$(srcdir)/server -I$(srcdir)/data -I$(srcdir)/gamemgr \
	-I$(srcdir)/objs -I$(srcdir)/conf -I$(srcdir)/files -I$(srcdir)/gumps \
	-I$(srcdir)/audio -I$(srcdir)/audio/midi_drivers -I$(srcdir)/pathfinder \
	-I$(srcdir)/usecode -I$(srcdir)/shapes/shapeinf \
	$(SDL_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS) $(INCDIRS) $(WINDOWING_SYSTEM) \
	$(DEBUG_FLAGS) $(CPPFLAGS) -DEXULT_DATADIR=\"$(EXULT_DATADIR)\"

CXXLINK = $(LIBTOOL) --mode=link $(CXXLD) $(AM_CXXFLAGS) $(DEBUG_FLAGS) $(CXXFLAGS) $(LDFLAGS) -o $(EXE_TARGET)

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
	browser.cc	\
	browser.h	\
	cheat.cc	\
	cheat.h		\
	cheat_screen.cc	\
	cheat_screen.h	\
	combat.cc	\
	combat.h	\
	combat_opts.h	\
	common_types.h	\
	dir.cc		\
	dir.h		\
	drag.cc		\
	drag.h		\
	effects.cc	\
	effects.h	\
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
	hash_utils.h	\
	ignore_unused_variable_warning.h	\
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
	readnpcs.cc	\
	ready.h		\
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
	README.win32 \
	README.MacOSX \
	Makefile.common \
	Makefile.mingw \
	Makefile.studio.mingw \
	Info.plist.in \
	exult.spec \
	windrag.cc	\
	windrag.h \
	win32/exult.ico \
	win32/exultico.rc \
	win32/exultstudio.ico \
	win32/exultstudioico.rc \
	macosx/exult.icns \
	msvcstuff/expack.vcproj \
	msvcstuff/Exult.vcproj \
	msvcstuff/Exult.sln \
	msvcstuff/Exult_bg_flx.vcproj \
	msvcstuff/Exult_flx.vcproj \
	msvcstuff/Exult_si_flx.vcproj \
	msvcstuff/mklink.vcproj \
	msvcstuff/msvc_kludges.cc \
	msvcstuff/msvc_kludges.h \
	msvcstuff/rip.vcproj \
	msvcstuff/shp2pcx.vcproj \
	msvcstuff/splitshp.vcproj \
	msvcstuff/unistd.h \
	msvcstuff/wuc.vcproj \
	msvcstuff/Zip.vcproj \
	msvcstuff/exconfig/exconfig.cpp \
	msvcstuff/exconfig/exconfig.def \
	msvcstuff/exconfig/exconfig.vcproj \
	msvcstuff/exconfig/exconfig.h \
	msvcstuff/exconfig/exconfig.rc \
	msvcstuff/exconfig/resource.h \
	msvcstuff/exconfig/StdAfx.cpp \
	msvcstuff/exconfig/StdAfx.h \
	msvc9/expack.vcproj \
	msvc9/Exult.vcproj \
	msvc9/Exult.sln \
	msvc9/Exult_bg_flx.vcproj \
	msvc9/Exult_flx.vcproj \
	msvc9/Exult_si_flx.vcproj \
	msvc9/mklink.vcproj \
	msvc9/msvc_kludges.cc \
	msvc9/msvc_kludges.h \
	msvc9/rip.vcproj \
	msvc9/shp2pcx.vcproj \
	msvc9/splitshp.vcproj \
	msvc9/unistd.h \
	msvc9/wuc.vcproj \
	msvc9/Zip.vcproj \
	msvc9/exconfig/exconfig.cpp \
	msvc9/exconfig/exconfig.def \
	msvc9/exconfig/exconfig.vcproj \
	msvc9/exconfig/exconfig.h \
	msvc9/exconfig/exconfig.rc \
	msvc9/exconfig/resource.h \
	msvc9/exconfig/StdAfx.cpp \
	msvc9/exconfig/StdAfx.h \
	headers/common_types.h \
	headers/exceptions.h \
	headers/gamma.h	\
	headers/pent_include.h \
	server/servewin32.cc \
	server/servewin32.h

win32/exultico.o: $(srcdir)/win32/exultico.rc $(srcdir)/win32/exult.ico
	$(WINDRES) $(srcdir)/win32/exultico.rc win32/exultico.o

CLEANFILES = *~

if MACOSX

bundle_name = Exult.app

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
	cp $(srcdir)/docs/images/* ./Exult-snapshot/images
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

clean-local:
	-rm -f Exult-snapshot.dmg
	-rm -rf $(bundle_name)

else
install-exec-local:

clean-local:

endif
