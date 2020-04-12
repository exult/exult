#!/bin/sh

MACPORTS_URL=https://distfiles.macports.org/MacPorts
MACPORTS_PKG=MacPorts-2.5.3-10.13-HighSierra.pkg
MACPORTS_PREFIX=/opt/local
MACPORTS_PATH=/tmp/$MACPORTS_PKG

# Download and install MacPorts
curl $MACPORTS_URL/$MACPORTS_PKG > $MACPORTS_PATH || exit 1
sudo installer -pkg $MACPORTS_PATH -target / || exit 2
export PATH=$MACPORTS_PREFIX/bin:$PATH

# Just to be sure
sudo port -q selfupdate | cat
sudo port -q upgrade outdated | cat

# Install actual dependencies
sudo port -q install automake libtool pkgconfig libvorbis libpng zlib libglade2 | cat
sudo port -q install libsdl2 +x11 | cat
