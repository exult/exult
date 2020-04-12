#!/bin/sh
chmod a+x ./autogen.sh

if [ "$TRAVIS_OS_NAME" = "linux" ]; then
	export LIBOPTS=""
	export EXTRA_TOOLS="--enable-gimp-plugin --enable-gnome-shp-thumbnailer --enable-compiler --enable-mods --with-usecode-debugger=yes"
	export NPROC=$(nproc)
else
	export LIBOPTS="--enable-static-libraries --disable-alsa --disable-fluidsynth --disable-timidity-midi --with-macosx-static-lib-path=/opt/local/lib"
	export EXTRA_TOOLS=""
	export NPROC=$(sysctl -n hw.ncpu)
	export SDK=' -w -mmacosx-version-min=10.7 '
	export MACOSX_DEPLOYMENT_TARGET=10.7
	export CPPFLAGS='-I/opt/local/include '$SDK
	export CFLAGS='-I/opt/local/include '$SDK
	export CXXFLAGS='-I/opt/local/include '$SDK
	export LDFLAGS='-L/opt/local/lib '$SDK
	export PKG_CONFIG_PATH="/opt/local/lib/pkgconfig"
	export PKG_CONFIG=/opt/local/bin/pkg-config
	export LIBTOOLFLAGS="--silent"
fi

./autogen.sh && ./configure --with-cxx=$(which $CXX) $LIBOPTS $EXTRA_OPT	\
                            --disable-oggtest --disable-vorbistest	\
                            --enable-exult-studio --enable-exult-studio-support	\
                            --enable-mt32emu --enable-zip-support	\
                            --enable-shared --enable-midi-sfx	\
                            $EXTRA_TOOLS && make -j $NPROC

