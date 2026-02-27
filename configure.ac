dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([Exult],[1.13.1git],[],[exult],[https://exult.info/])
AC_CONFIG_SRCDIR([exult.cc])

# ---------------------------------------------------------------------
# System/version info
# ---------------------------------------------------------------------

# check host/target systems
# (build = system we're building on, host = system we're building for,
# target = system the program we're building will build for)
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([-Wno-portability])
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Package Name])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Package Version])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AH_BOTTOM([
#define USE_FMOPL_MIDI
])

# ---------------------------------------------------------------------
# Host system settings
# ---------------------------------------------------------------------

AC_EXEEXT

SYSLIBS=""
ICON_FILE=""
EXE_TARGET="exult$EXEEXT"
EXULT_DATADIR="$datadir/exult"
ARCH=""

# Default C anc C++ compiler set
cxx_compilers="g++ clang++ c++ gpp aCC CC cxx cc++ cl.exe FCC KCC RCC xlC_r xlC"

# determine windowing system from 'host'
AC_MSG_CHECKING([windowing system])
HOME_CFGDIR="${XDG_CONFIG_HOME:-${HOME}/.config}"
HOME_SHRDIR="${XDG_DATA_HOME:-${HOME}/.local/share}"
case "$host_os" in
	linux-android*)
		# Android cross compiled from Linux, supported.
		AC_DEFINE(ANDROID, 1, [Using Android])
		ARCH=android
		AC_MSG_RESULT([Android])
		CXXFLAGS="$CXXFLAGS -pthread"
		;;
	linux*)
		# Linux, supported.
		WINDOWING_SYSTEM="-DXWIN"
		AC_MSG_RESULT([X11 (GNU/Linux)])
		CXXFLAGS="$CXXFLAGS -pthread"
		;;
	mingw32*)
		# Windows as MinGW, placeholder, true build by Makefile.mingw.
		WINDOWING_SYSTEM="-D_WIN32"
		AC_DEFINE(MINGW, 1, [Using MinGW])
		AC_MSG_RESULT([Win32 (mingw32)])
		CXXFLAGS="$CXXFLAGS -D_USE_MATH_DEFINES -pthread"
		SYSLIBS="-luuid -lole32 -lwinmm -lstdc++ -lws2_32"
		ICON_FILE="win32/exultico.o"
		;;
	cygwin*)
		# Windows as Cygwin, placeholder, true build by Makefile.mingw.
		WINDOWING_SYSTEM="-D_WIN32"
		AC_DEFINE(CYGWIN, 1, [Using Cygwin])
		AC_MSG_RESULT([Win32 (cygwin)])
		CXXFLAGS="$CXXFLAGS -mno-cygwin -pthread"
		SYSLIBS="-lwinmm"
		ICON_FILE="win32/exultico.o"
		;;
	openbsd*)
		# OpenBSD, placeholder.
		WINDOWING_SYSTEM="-DXWIN"
		AC_DEFINE(OPENBSD, 1, [Using OpenBSD])
		AC_MSG_RESULT([X11 (OpenBSD)])
		CXXFLAGS="$CXXFLAGS -pthread"
		SYSLIBS="-L/usr/X11R6/lib -lX11 -lXext -lXxf86vm -lXxf86dga"
		;;
	freebsd*)
		# FreeBSD, supported.
		WINDOWING_SYSTEM="-DXWIN"
		AC_DEFINE(FREEBSD, 1, [Using FreeBSD])
		AC_MSG_RESULT([X11 (FreeBSD)])
		CXXFLAGS="$CXXFLAGS -pthread"
		;;
	netbsd*)
		# NetBSD, placeholder.
		WINDOWING_SYSTEM="-DXWIN"
		AC_MSG_RESULT([X11 (NetBSD)])
		CXXFLAGS="$CXXFLAGS -pthread"
		;;
	solaris*)
		# Solaris, placeholder.
		WINDOWING_SYSTEM="-DXWIN"
		AC_MSG_RESULT([X11 (Solaris)])
		CXXFLAGS="$CXXFLAGS -pthread"
		SYSLIBS="-lsocket"
		;;
	darwin*)
		# MacOS as Darwin, supported.
		dnl
		dnl We have a problem here: both Mac OS X and Darwin report
		dnl the same signature "powerpc-apple-darwin*" - so we have
		dnl to do more to distinguish them. Plain Darwin will propably
		dnl use X-Windows; and it is of course lacking Cocoa. For
		dnl now I am lazy and do not add proper detection code.
		dnl
		WINDOWING_SYSTEM="-DMACOSX"
		AC_DEFINE(MACOSX, 1, [Using MacOSX])
		AC_MSG_RESULT([Mac OS X])
		SYSLIBS="-framework CoreFoundation -framework AudioUnit -framework AudioToolbox -framework CoreMIDI"
		CXXFLAGS="$CXXFLAGS -stdlib=libc++ -pthread"
		EXULT_DATADIR="/Library/Application\ Support/Exult/data"
		HOME_CFGDIR="${HOME}/Library/Application\ Support"
		ARCH=macosx
		dnl swap around clang for OSX
		cxx_compilers="clang++ g++ c++ gpp aCC CC cxx cc++ cl.exe FCC KCC RCC xlC_r xlC"
		;;
	*)
		WINDOWING_SYSTEM="-DXWIN"
		AC_MSG_RESULT([not sure... guessing X11])
		;;
esac

if echo $host_os | grep "cygwin\|mingw" > /dev/null 2>&1; then
	AC_CHECK_TOOL([WINDRES], [windres])
	if test "x$WINDRES" = "x"; then
		AC_MSG_ERROR([windres could not be found, please make sure this program is within your path.])
	fi
fi
AC_SUBST(WINDRES)

AM_CONDITIONAL(MACOSX, test x$ARCH = xmacosx)
AM_CONDITIONAL(ANDROID, test x$ARCH = xandroid)

# ---------------------------------------------------------------------
# Compilers and other tools
# ---------------------------------------------------------------------

AC_PROG_AWK

AC_PROG_CXX([$cxx_compilers])

AC_LANG([C++])

AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory])
AC_PROG_CXXCPP

AC_PROG_INSTALL
dnl autoconf 2.69 AC_PROG_LEX has no parameters
dnl autoconf 2.70 AC_PROG_LEX prints a deprecation warning without params
m4_if(m4_version_compare(m4_defn([AC_AUTOCONF_VERSION]), [2.70]), [1], [dnl
  dnl autoconf >= 2.70
  AC_PROG_LEX([noyywrap])
], [
  dnl autoconf < 2.70
  AM_PROG_LEX
])

AC_PROG_YACC
AM_CONDITIONAL(LEXYACC, test -n "$YACC")

LT_INIT
AC_SUBST(LIBTOOL_DEPS)

# ---------------------------------------------------------------------
# Checks for integer types with specific sizes.
# ---------------------------------------------------------------------

AC_CHECK_TYPES([int8_t, int16_t, int32_t, int64_t, intptr_t],
               [], [AC_MSG_ERROR([*** Cannot find suitably-sized signed integers!])],
               [#include <cstdint>])
AC_CHECK_TYPES([uint8_t, uint16_t, uint32_t, uint64_t, uintptr_t],
               [], [AC_MSG_ERROR([*** Cannot find suitably-sized unsigned integers!])],
               [#include <cstdint>])

# ---------------------------------------------------------------------
# Checks for header files.
# ---------------------------------------------------------------------

AC_HEADER_DIRENT
AC_CHECK_HEADERS(limits.h sys/time.h unistd.h)
AC_CHECK_HEADERS(sys/types.h sys/socket.h netdb.h)
AC_CHECK_HEADERS(sys/wait.h signal.h getopt.h)

# ---------------------------------------------------------------------
# Checks for typedefs, structures, and compiler characteristics.
# ---------------------------------------------------------------------

AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_CHECK_HEADERS([sys/time.h])
AC_STRUCT_TM

# ---------------------------------------------------------------------
# Checks for library functions
# ---------------------------------------------------------------------

dnl Disabled this for now (undefined in autoconf < 2.5)
dnl AC_FUNC_MALLOC

AC_FUNC_MEMCMP

AC_CHECK_FUNCS([atexit dup2 getcwd isascii memchr memmove memset mkdir pow select socket strcasecmp strchr strstr strtol strtoul getopt_long])

AC_MSG_CHECKING([for getaddrinfo()])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
]], [[
	struct addrinfo a;
	//getaddrinfo(0, 0, 0, 0);
]])],[ac_cv_func_getaddrinfo=yes],[ac_cv_func_getaddrinfo=no])
AC_MSG_RESULT($ac_cv_func_getaddrinfo)
if test x$ac_cv_func_getaddrinfo = xyes ; then
	AC_DEFINE(HAVE_GETADDRINFO, 1, [Have addrinfo/getaddrinfo])
fi

AC_MSG_CHECKING([for mkstemp()])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
]], [[
	mkstemp(0);
]])],[ac_cv_func_mkstemp=yes],[ac_cv_func_mkstemp=no])
AC_MSG_RESULT($ac_cv_func_mkstemp)
if test x$ac_cv_func_mkstemp = xyes ; then
	AC_DEFINE(HAVE_MKSTEMP, 1, [Have mkstemp])
fi

# do we need special X11 libraries?
AC_MSG_CHECKING([for special X11 libraries])
if test x$x_libraries = xNONE; then
	AC_MSG_RESULT(no)
	unset x_libraries
else
	x_libraries="-L$x_libraries -lX11 -lXext"
	AC_MSG_RESULT($x_libraries)
	AC_SUBST(x_libraries)
fi

# ---------------------------------------------------------------------
# Build Android APK?
# Note: version numbers checked in this section are known-good baselines,
# the working minimums have not been tested
# ---------------------------------------------------------------------

AM_CONDITIONAL(ANDROID_APK, false)
AC_ARG_ENABLE(android-apk, AS_HELP_STRING([--enable-android-apk=debug|release], [Build an Android APK @<:@default no@:>@]),enable_android_apk="$enableval",enable_android_apk=no)
AC_MSG_CHECKING([whether to build an Android APK])
if test x$enable_android_apk != xno; then
	AC_MSG_RESULT($enable_android_apk)

	# Needed to stage the sources in an out-of-tree build directory
	AC_PROG_MKDIR_P
	AC_PROG_LN_S

	# Needed to download and patch SDL sources
	AC_CHECK_PROGS(GIT, git)
	if test -z "$GIT"; then
		AC_MSG_ERROR([not found])
	fi
	AC_CHECK_PROGS(PATCH, patch)
	if test -z "$PATCH"; then
		AC_MSG_ERROR([not found])
	fi

	# Check for gradle
	# TODO: any reasonable way to use the gradle bundled with android studio?
	AC_CHECK_PROGS(GRADLE, gradle)
	if test -z "$GRADLE"; then
		AC_MSG_ERROR([not found])
	fi

	# Checking against a known-good baseline gradle version
	AC_MSG_CHECKING([gradle version])
	gradle_version=`$GRADLE --version | grep Gradle | awk '{print $2;}' | tr -d '\r\n'`
	AX_COMPARE_VERSION([$gradle_version], [ge], [8.9], [dnl
		dnl $gradle_version >= 8.9
		AC_MSG_RESULT([found $gradle_version >= 8.9])
	], [
		dnl $gradle_version < 8.9
		AC_MSG_ERROR([found $gradle_version < 8.9])
	])

	# Check for javac and java
	AX_PROG_JAVAC
	AX_PROG_JAVA

	# Check for required Java version
	AC_MSG_CHECKING([java version])
	java_version=`$JAVAC -version | grep javac | awk '{print $2;}' | tr -d '\r\n'`
	AX_COMPARE_VERSION([$java_version], [ge], [17], [dnl
		dnl $gradle_version >= 17
		AC_MSG_RESULT([found $java_version >= 17])
	], [
		dnl $java_version < 17
		AC_MSG_ERROR([found $java_version < 17])
	])

	# Use sdkmanager to check Android package versions.
	AC_PATH_PROGS(SDKMANAGER, sdkmanager)

	# Check for required Android SDK version.
	AC_ARG_WITH(android_sdk,
		AS_HELP_STRING([--with-android-sdk=sdk_version], [Android SDK version @<:@default 34@:>@]),
		[with_android_sdk="$withval"],[with_android_sdk=34])
	ANDROID_SDK_VERSION="$with_android_sdk"
	AC_MSG_CHECKING([for Android SDK $ANDROID_SDK_VERSION])
	if $SDKMANAGER --list|grep -q "platforms;android-$ANDROID_SDK_VERSION"; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_ERROR([$ANDROID_SDK_VERSION is not a valid Android SDK version.])
	fi
	AC_SUBST(ANDROID_SDK_VERSION, $ANDROID_SDK_VERSION)

	# Check for required Android SDK Build-Tools version.
	AC_ARG_WITH(android_build_tools,
		AS_HELP_STRING([--with-android-build-tools=build_tools_version], [Android build tools version @<:@default 34.0.0@:>@]),
		[with_android_build_tools="$withval"],[with_android_build_tools=34.0.0])
	ANDROID_BUILD_TOOLS_VERSION="$with_android_build_tools"
	AC_MSG_CHECKING([for Android SDK Build-Tools $ANDROID_BUILD_TOOLS_VERSION])
	if $SDKMANAGER --list|grep -q "build-tools;$ANDROID_BUILD_TOOLS_VERSION"; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_ERROR([$ANDROID_BUILD_TOOLS_VERSION is not a valid Android build tools version.])
	fi
	AC_SUBST(ANDROID_BUILD_TOOLS_VERSION, $ANDROID_BUILD_TOOLS_VERSION)

	# Check for required Android NDK version.
	AC_ARG_WITH(android_ndk,
		AS_HELP_STRING([--with-android-ndk=ndk_version], [Android NDK version @<:@default 27.2.12479018@:>@]),
		[with_android_sdk="$withval"],[with_android_ndk=27.2.12479018])
	ANDROID_NDK_VERSION="$with_android_ndk"
	AC_MSG_CHECKING([for Android NDK $ANDROID_NDK_VERSION])
	if $SDKMANAGER --list|grep -q "ndk;$ANDROID_NDK_VERSION"; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_ERROR([$ANDROID_NDK_VERSION is not a valid Android NDK version.])
	fi
	AC_SUBST(ANDROID_NDK_VERSION, $ANDROID_NDK_VERSION)

	# Check for required Android CMake version.
	AC_ARG_WITH(android_cmake,
		AS_HELP_STRING([--with-android-cmake=cmake_version], [Android CMake version @<:@default 3.22.1@:>@]),
		[with_android_cmake="$withval"],[with_android_cmake=3.22.1])
	ANDROID_CMAKE_VERSION="$with_android_cmake"
	AC_MSG_CHECKING([for Android CMake $ANDROID_CMAKE_VERSION])
	if $SDKMANAGER --list|grep -q "cmake;$ANDROID_CMAKE_VERSION"; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_ERROR([$ANDROID_CMAKE_VERSION is not a valid Android CMake version.])
	fi
	AC_SUBST(ANDROID_CMAKE_VERSION, $ANDROID_CMAKE_VERSION)

	# Pass down SDK directory, keep _HOME and _SDK_ROOT in sync
	AC_SUBST(ANDROID_HOME, `AS_DIRNAME(["$SDKMANAGER"])`/../../..)
	AC_SUBST(ANDROID_SDK_ROOT, $ANDROID_HOME)

	# Enable Android APK build
	AM_CONDITIONAL(ANDROID_APK, true)
	AC_SUBST(APK_BUILD_TYPE, $enable_android_apk)
	if test x$enable_android_apk = xrelease; then
		# The apk filename has an additional suffix for release builds
		AC_SUBST(APK_SUFFIX, -unsigned)
	fi

	# The APK building uses its own build system
	# so disable everything except generating the data files
	if test x$enable_android_apk != xno; then
		enable_exult=no
		enable_tools=no
	fi

	# When building a debug apk, enable Exult's debug_level "messages"
	# which can be overriden with "--with-debug="
	if test x$enable_android_apk = xdebug; then
		AC_SUBST(EXULT_ANDROID_DEBUG, messages)
	fi

	# Build a single Android architecture
	AC_ARG_WITH(android_arch,
		AS_HELP_STRING([--with-android-arch=ARCH], [Android build single arch @<:@default x86, x86_64, armeabi-v7a, arm64-v8a@:>@]),
		[with_android_arch="$withval"],)
	if test "x$with_android_arch" != x; then
		ANDROID_ARCHITECTURE="abiFilters "\"$with_android_arch"\""
		AC_SUBST(ANDROID_ARCHITECTURE, $ANDROID_ARCHITECTURE)
	fi
else
	AC_MSG_RESULT(no)
fi

# ---------------------------------------------------------------------
# Build Exult at all
# This is meant to facilitate building the tools on the host
# system for posterior cross-compilation
# ---------------------------------------------------------------------

AC_ARG_ENABLE(exult, AS_HELP_STRING([--enable-exult], [Build Exult @<:@default yes@:>@]),,enable_exult=yes)

# ---------------------------------------------------------------------
# Build Exult as a library?
# ---------------------------------------------------------------------

AM_CONDITIONAL(BUILD_SHAPES, false)
AC_ARG_ENABLE(libexult, AS_HELP_STRING([--enable-libexult], [Build Exult as a library @<:@default no@:>@]),,enable_libexult=no)
if test x$enable_libexult = xyes -a x$enable_exult = xno; then
	AC_MSG_ERROR([*** It is not possible build Exult as a library if building Exult is disabled ***])
else
	AC_MSG_CHECKING([whether to build Exult])
	if test x$enable_libexult = xyes; then
		AC_MSG_RESULT(as a library)
		AM_CONDITIONAL(BUILD_EXULT, true)
		AM_CONDITIONAL(BUILD_LIBEXULT, true)
		AM_CONDITIONAL(BUILD_SHAPES, true)
	elif test x$enable_exult = xyes; then
		AC_MSG_RESULT(yes)
		AM_CONDITIONAL(BUILD_EXULT, true)
		AM_CONDITIONAL(BUILD_LIBEXULT, false)
		AM_CONDITIONAL(BUILD_SHAPES, true)
	else
		AC_MSG_RESULT(no)
		AM_CONDITIONAL(BUILD_EXULT, false)
		AM_CONDITIONAL(BUILD_LIBEXULT, false)
	fi
fi

# ---------------------------------------------------------------------
# Statically link libraries?
# ---------------------------------------------------------------------

AC_ARG_ENABLE(static-libraries, AS_HELP_STRING([--enable-static-libraries], [Enable static linking of libraries]),,enable_static_libs=no)
AC_MSG_CHECKING([if we want to statically compile libraries])
if test x$enable_static_libs != xno; then
	if test x$ARCH != xmacosx; then
		# Assuming GCC. This is probably not portable.
		STATIC_LD="-static"
		LDFLAGS="$LDFLAGS -static"
	else
		AC_MSG_ERROR([*** static linking is not supported on macOS])
	fi
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

# ---------------------------------------------------------------------
# Check for pkg-config
# ---------------------------------------------------------------------

if test x$enable_android_apk = xno; then
	PKG_PROG_PKG_CONFIG
	if test x$PKG_CONFIG = x; then
		AC_MSG_ERROR([*** To Build Exult 1.9+, pkg-config is required. ***])
	fi
fi

# ---------------------------------------------------------------------
# SDL
# ---------------------------------------------------------------------

if test x$enable_android_apk = xno; then
	PKG_CHECK_MODULES(SDL, sdl3, , AC_MSG_ERROR([[*** SDL not found!]]))
	AC_DEFINE(HAVE_SDL_H, 1, [Define to 1 if you have the "SDL.h" header file])
	AM_CONDITIONAL(HAVE_SDL, true)
else
	AM_CONDITIONAL(HAVE_SDL, false)
fi

# ---------------------------------------------------------------------
# libpng
# ---------------------------------------------------------------------

if test x$ARCH != xandroid; then
	PKG_CHECK_MODULES(PNG, libpng, have_png=yes, have_png=no)
else
	have_png=no
fi
if test x$have_png = xyes; then
	AM_CONDITIONAL(HAVE_PNG, true)
	AC_DEFINE(HAVE_PNG_H, 1, [Define to 1 if you have the <png.h> header file.])
else
	AM_CONDITIONAL(HAVE_PNG, false)
fi

# ---------------------------------------------------------------------
# libogg, libvorbis, libvorbisfile
# ---------------------------------------------------------------------

if test x$enable_android_apk = xno -a x$enable_exult = xyes; then
	PKG_CHECK_MODULES(OGG, ogg >= 1.0 vorbis >= 1.0.1 vorbisfile, , AC_MSG_ERROR([*** must have Ogg/Vorbis installed!]))
fi

# ---------------------------------------------------------------------
# Mac OS X code signing
# ---------------------------------------------------------------------

AM_CONDITIONAL(WITH_OSX_CODE_SIGNATURE, false)
AC_ARG_WITH(macosx-code-signature,
	AS_HELP_STRING([--with-macosx-code-signature=identity], [identity for code signing (macOS bundles only) @<:@default "Developer ID Application"@:>@]),
	[with_macosx_code_signature="$withval"],
	[with_macosx_code_signature=""])
if test x$ARCH = xmacosx; then
	if test x"$with_macosx_code_signature" != x; then
		if test x"$with_macosx_code_signature" = xyes; then
			with_macosx_code_signature="Developer ID Application"
		fi
		AM_CONDITIONAL(WITH_OSX_CODE_SIGNATURE, true)
		OSX_CODE_SIGNATURE="$with_macosx_code_signature"
		AC_SUBST(OSX_CODE_SIGNATURE)
	fi
fi

AM_CONDITIONAL(WITH_OSX_INSTALLER_SIGNATURE, false)
AC_ARG_WITH(macosx-installer-signature,
	AS_HELP_STRING([--with-macosx-installer-signature=identity], [identity for code signing macOS installers @<:@default "Developer ID Installer"@:>@]),
	[with_macosx_installer_signature="$withval"],
	[with_macosx_installer_signature=""])
if test x$ARCH = xmacosx; then
	if test x"$with_macosx_installer_signature" != x; then
		if test x"$with_macosx_installer_signature" = xyes; then
			with_macosx_installer_signature="Developer ID Installer"
		fi
		AM_CONDITIONAL(WITH_OSX_INSTALLER_SIGNATURE, true)
		OSX_INSTALLER_SIGNATURE="$with_macosx_installer_signature"
		AC_SUBST(OSX_INSTALLER_SIGNATURE)
	fi
fi
# ---------------------------------------------------------------------
# Optional components
# ---------------------------------------------------------------------

# Timidity MIDI driver
AC_ARG_ENABLE(timidity_midi, AS_HELP_STRING([--disable-timidity-midi], [Disable timidity midi support]),,enable_timidity_midi=yes)
AC_ARG_WITH(timidity, AS_HELP_STRING([--with-timidity=path], [path to timidity.cfg (optional)]),,)
AC_MSG_CHECKING([if we can use timidity midi])
if test x$enable_timidity_midi = xyes; then
	AC_MSG_RESULT(yes)
	AC_DEFINE(USE_TIMIDITY_MIDI, 1, [Enable timidity midi])
	if test x$with_timidity != x; then
		if test ! -d $with_timidity; then
			with_timidity=`echo "$with_timidity" | sed 's/timidity.cfg//'`
		fi
		AC_DEFINE_UNQUOTED(DEFAULT_TIMIDITY_PATH, "$with_timidity", [Default timidity path])
	fi
else
	AC_MSG_RESULT(no)
fi

# ALSA MIDI driver
AC_ARG_ENABLE(alsa, AS_HELP_STRING([--disable-alsa], [Disable ALSA midi support]),,enable_alsa=yes)
AC_MSG_CHECKING([if we want to use alsa midi])
if test x$enable_alsa = xyes; then
	AC_MSG_RESULT(yes)
	PKG_CHECK_MODULES(ALSA, alsa, have_alsa=yes, have_alsa=no)
	AC_MSG_CHECKING([if we can use alsa midi])
	if test x$have_alsa = xyes; then
		AC_MSG_RESULT(yes)
		AC_DEFINE(USE_ALSA_MIDI, 1, [Enable ALSA midi])
	else
		AC_MSG_RESULT([no; alsa not installed])
	fi
else
	AC_MSG_RESULT(no)
fi

# FluidSynth and FluidLite MIDI driver
AC_ARG_ENABLE(fluidsynth,
	AS_HELP_STRING([--enable-fluidsynth@<:@=yes|lite|no@:>@],
		[Enable fluidsynth midi support, lite for fluidlite, @<:@default yes@:>@]),,enable_fluidsynth=yes)
AC_MSG_CHECKING([if we want to use fluidsynth midi])
if test x$enable_fluidsynth = xyes; then
	AC_MSG_RESULT(yes)
	PKG_CHECK_MODULES(FLUIDSYNTH, fluidsynth, have_fluidsynth=yes, have_fluidsynth=no)
	PKG_CHECK_MODULES(FLUIDLITE, fluidlite, have_fluidlite=yes, have_fluidlite=no)
	AC_MSG_CHECKING([if we can use fluidsynth midi])
	if test x$have_fluidsynth = xyes; then
		AC_MSG_RESULT(yes)
		AC_DEFINE(USE_FLUIDSYNTH_MIDI, 1, [Enable fluidsynth midi])
		AC_DEFINE(USING_FLUIDSYNTH, 1,[Using FluidSynth])
		FLUID_CFLAGS="$FLUIDSYNTH_CFLAGS"
		FLUID_LIBS="$FLUIDSYNTH_LIBS"
	elif test x$have_fluidlite = xyes; then
		AC_MSG_RESULT([no, using fluidlite])
		AC_DEFINE(USE_FLUIDSYNTH_MIDI, 1, [Enable fluidsynth midi])
		AC_DEFINE(USING_FLUIDLITE, 1,[Using FluidLite])
		FLUID_CFLAGS="$FLUIDLITE_CFLAGS"
		FLUID_LIBS="$FLUIDLITE_LIBS"
		enable_fluidsynth=lite
	else
		AC_MSG_RESULT([no; fluidsynth or fluidlite not installed])
	fi
elif test x$enable_fluidsynth = xlite; then
	AC_MSG_RESULT([yes, using fluidlite])
	PKG_CHECK_MODULES(FLUIDLITE, fluidlite, have_fluidlite=yes, have_fluidlite=no)
	PKG_CHECK_MODULES(FLUIDSYNTH, fluidsynth, have_fluidsynth=yes, have_fluidsynth=no)
	AC_MSG_CHECKING([if we can use fluidlite midi])
	if test x$have_fluidlite = xyes; then
		AC_MSG_RESULT([yes, using fluidlite])
		AC_DEFINE(USE_FLUIDSYNTH_MIDI, 1, [Enable fluidsynth midi])
		AC_DEFINE(USING_FLUIDLITE, 1,[Using FluidLite])
		FLUID_CFLAGS="$FLUIDLITE_CFLAGS"
		FLUID_LIBS="$FLUIDLITE_LIBS"
	elif test x$have_fluidsynth = xyes; then
		AC_MSG_RESULT([no, using fluidsynth instead])
		AC_DEFINE(USE_FLUIDSYNTH_MIDI, 1, [Enable fluidsynth midi])
		AC_DEFINE(USING_FLUIDSYNTH, 1,[Using FluidSynth])
		FLUID_CFLAGS="$FLUIDSYNTH_CFLAGS"
		FLUID_LIBS="$FLUIDSYNTH_LIBS"
		enable_fluidsynth=yes
	else
		AC_MSG_RESULT([no; fluidlite or fluidsynth not installed])
	fi
else
	AC_MSG_RESULT(no)
	enable_fluidsynth=no
fi
AC_SUBST([FLUID_CFLAGS])
AC_SUBST([FLUID_LIBS])

# mt32emu midi driver
AC_ARG_ENABLE(mt32emu, AS_HELP_STRING([--disable-mt32emu], [Disable mt32emu midi support]),,enable_mt32emu=yes)
AC_MSG_CHECKING([if we want to use mt32emu midi])
if test x$enable_mt32emu = xyes; then
	AC_MSG_RESULT(yes)
	PKG_CHECK_MODULES(MT32EMU, mt32emu, have_mt32emu=yes, have_mt32emu=no)
	AC_MSG_CHECKING([if we can use mt32emu midi])
	if test x$have_mt32emu = xyes; then
		AC_MSG_RESULT(yes)
		AC_DEFINE(USE_MT32EMU_MIDI, 1, [Enable mt32emu])
		AM_CONDITIONAL(BUILD_MT32EMU, true)
	else
		AC_MSG_RESULT([no; mt32emu not installed])
		AM_CONDITIONAL(BUILD_MT32EMU, false)
	fi
else
	AC_MSG_RESULT(no)
	AM_CONDITIONAL(BUILD_MT32EMU, false)
fi

# dylibbundler and create-dmg on macOS
if test x$ARCH = xmacosx; then
	AC_CHECK_PROG(has_dylibbundler, dylibbundler, yes)
	if test x$has_dylibbundler != xyes; then
		echo "Warning: Bundling a shared Exult app on macOS requires dylibbundler:"
		echo "         https://github.com/auriamg/macdylibbundler"
	fi
	AC_CHECK_PROG(has_create_dmg, create-dmg, yes)
	if test x$has_create_dmg != xyes; then
		echo "Warning: Creating disk images on macOS requires create-dmg:"
		echo "         https://github.com/create-dmg/create-dmg"
	fi
fi

# zipped savegame support
AC_ARG_ENABLE(zip-support, AS_HELP_STRING([--enable-zip-support], [Enable zipped savegame support @<:@default yes@:>@]),,enable_zip_support=yes)
AC_MSG_CHECKING([if we want to use zipped savegame support])
if test x$enable_zip_support = xyes ; then
	AC_MSG_RESULT(yes)
	# disabled for now (non-portable):

	# link statically against zlib if using gcc
	# if test x$GCC = xyes ; then
	#	ZLIB_LIBS="-Wl,-Bstatic -lz -Wl,-Bdynamic"
	# else
	if test x$enable_static_libs = xno -o x$ARCH != xmacosx; then
		PKG_CHECK_MODULES(ZLIB, zlib, have_zlib=yes, have_zlib=no)
	else
		have_zlib=yes
	fi
	# fi
	AC_MSG_CHECKING([if we can use zipped savegame support])
	if test x$have_zlib = xyes; then
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_ZIP_SUPPORT, 1, [Have zip support])
	else
		AC_MSG_RESULT([no; zlib not installed])
	fi
else
	AC_MSG_RESULT(no)
fi

# HQnX scaler
AC_ARG_ENABLE(all_hq_scalers, AS_HELP_STRING([--enable-all-hq-scalers], [Enable hq2x, hq3x, hq4x scaler support @<:@default yes@:>@]),,enable_all_hq_scalers=yes)
AC_MSG_CHECKING([if we should build all hq scalers])
if test x$enable_all_hq_scalers = xyes; then
	enable_hq2x=yes
	enable_hq3x=yes
	enable_hq4x=yes
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

# HQ2X scaler
AC_ARG_ENABLE(hq2x, AS_HELP_STRING([--enable-hq2x],[Enable hq2x scaler support @<:@default no@:>@]),,enable_hq2x=no)
AC_MSG_CHECKING(if we should build the hq2x scaler)
if test x$enable_hq2x = xyes; then
	AC_MSG_RESULT(yes)
	AC_DEFINE(USE_HQ2X_SCALER, 1, [Build hq2x scaler])
else
	AC_MSG_RESULT(no)
fi

# HQ3X scaler
AC_ARG_ENABLE(hq3x, AS_HELP_STRING([--enable-hq3x],[Enable hq3x scaler support @<:@default no@:>@]),,enable_hq3x=no)
AC_MSG_CHECKING(if we should build the hq3x scaler)
if test x$enable_hq3x = xyes; then
	AC_MSG_RESULT(yes)
	AC_DEFINE(USE_HQ3X_SCALER, 1, [Build hq3x scaler])
else
	AC_MSG_RESULT(no)
fi

# HQ4X scaler
AC_ARG_ENABLE(hq4x, AS_HELP_STRING([--enable-hq4x],[Enable hq4x scaler support @<:@default no@:>@]),,enable_hq4x=no)
AC_MSG_CHECKING(if we should build the hq4x scaler)
if test x$enable_hq4x = xyes; then
	AC_MSG_RESULT(yes)
	AC_DEFINE(USE_HQ4X_SCALER, 1, [Build hq4x scaler])
else
	AC_MSG_RESULT(no)
fi

# NxBR scalers
AC_ARG_ENABLE(nxbr, AS_HELP_STRING([--enable-nxbr],[Enable 2xBR, 3xBR and 4xBR scaler support @<:@default yes@:>@]),,enable_nxbr=yes)
AC_MSG_CHECKING([if we should build the 2xBR, 3xBR and 4xBR scalers])
if test x$enable_nxbr = xyes; then
	AC_MSG_RESULT(yes)
	AC_DEFINE(USE_XBR_SCALER, 1, [Build NxBR scalers])
else
	AC_MSG_RESULT(no)
fi

# Midi Sfx
AC_ARG_ENABLE(midi-sfx, AS_HELP_STRING([--enable-midi-sfx], [Support for Midi Sfx (sounds horrible) @<:@default no@:>@]),,enable_midi_sfx=no)
AC_MSG_CHECKING([whether to enable midi sfx])
if test x$enable_midi_sfx = xyes; then
	AC_DEFINE(ENABLE_MIDISFX, 1, [Enable Midi Sfx])
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

# ---------------------------------------------------------------------
# Exult Studio
# ---------------------------------------------------------------------

AC_ARG_ENABLE(exult-studio, AS_HELP_STRING([--enable-exult-studio], [Build Exult Studio @<:@default no@:>@]),,enable_exult_studio=no)
if test x$enable_android_apk = xyes; then
	enable_exult_studio=no
fi

# library dependencies
AC_MSG_CHECKING([if we want to build Exult Studio])
if test x$enable_exult_studio = xyes; then
	AC_MSG_RESULT(yes)
# Icu
	PKG_CHECK_MODULES(ICU, icu-uc, have_icu=yes, have_icu=no)
# Gtk
	PKG_CHECK_MODULES(GTK, gtk+-3.0 >= 3.16, have_gtk=yes, have_gtk=no)
# Freetype2 (optional, used in ExultStudio, shapes/fontgen.cc)
	PKG_CHECK_MODULES(FREETYPE2, freetype2, have_freetype=yes, have_freetype=no)
	if test x$have_freetype = xyes; then
		AC_DEFINE(HAVE_FREETYPE2, 1, [Have freetype2])
	fi
else
	AC_MSG_RESULT(no)
fi

# Exult Studio
if test x$enable_exult_studio = xyes; then
	AC_MSG_CHECKING([if we can build Exult Studio])
	if test x$have_gtk = xno; then
		AC_MSG_RESULT([no; gtk+ is not installed])
		echo "Exult Studio requires the GTK+ (aka the GIMP Tool Kit), but it is not installed."
		echo "Please try again, either with the GTK+ installed as 3.16 or newer, or with '--disable-exult-studio'."
		exit 1
	fi
	if test x$have_icu = xno; then
		AC_MSG_RESULT([no; icu is not installed])
		echo "Exult Studio requires the ICU (aka the International Components for Unicode), but it is not installed."
		echo "Please try again, either with the ICU installed, or with '--disable-exult-studio'."
		exit 1
	fi
	if test x$have_png = xno; then
		AC_MSG_RESULT([no; libpng is not installed])
		echo "Exult Studio requires the PNG (aka the Portable Network Graphics), but it is not installed."
		echo "Please try again, either with the PNG installed, or with '--disable-exult-studio'."
		exit 1
	fi
	if test x$ARCH = xmacosx; then
		AC_MSG_RESULT(yes)
		AC_CHECK_PROG(has_gtk_mac_bundler, gtk-mac-bundler, yes)
		if test x$has_gtk_mac_bundler != xyes; then
			echo "Warning: Bundling Exult Studio on macOS requires GTK Mac Bundler"
			echo "         https://gitlab.gnome.org/GNOME/gtk-mac-bundler"
		fi
	else
		AC_MSG_RESULT(yes)
	fi
	AM_CONDITIONAL(BUILD_STUDIO, true)
	AM_CONDITIONAL(BUILD_SHAPES, true)
else
	AM_CONDITIONAL(BUILD_STUDIO, false)
fi

# support for Exult Studio
AC_ARG_ENABLE(exult-studio-support, AS_HELP_STRING([--enable-exult-studio-support], [Build Exult Studio support into Exult @<:@default no@:>@]),,enable_exult_studio_support=no)
AC_MSG_CHECKING([if we want to build support for Exult Studio])
if test x$enable_exult_studio_support = xyes -o x$enable_exult_studio = xyes; then
	# Implicitly, --enable-exult-studio drags --enable-exult-studio-support
	enable_exult_studio_support=yes
	AC_MSG_RESULT(yes)
	AC_MSG_CHECKING([if we can build support for Exult Studio])
	if test "$WINDOWING_SYSTEM" != -DXWIN -a "$WINDOWING_SYSTEM" != -D_WIN32 -a "$WINDOWING_SYSTEM" != -DMACOSX; then
		AC_MSG_RESULT([no; windowing system "$WINDOWING_SYSTEM" not supported])
		enable_exult_studio_support=no
	else
		AC_MSG_RESULT(yes)
		AC_DEFINE(USE_EXULTSTUDIO, 1, [Use Exult Studio])
	fi
else
	AC_MSG_RESULT(no)
fi

# ---------------------------------------------------------------------
# External features
# ---------------------------------------------------------------------

# Build any external programs?
AM_CONDITIONAL(HAVE_SDL_IMAGE, false)
AC_ARG_ENABLE(tools, AS_HELP_STRING([--disable-tools], [Only build the main program]),,enable_tools=yes)
AC_MSG_CHECKING([whether to build only the main program])
if test x$enable_tools = xno; then
	AC_MSG_RESULT(yes)
	AM_CONDITIONAL(BUILD_TOOLS, false)
	enable_gimp_plugin=no
	enable_compiler=no
else
	AM_CONDITIONAL(BUILD_TOOLS, true)
	AC_MSG_RESULT(no)
	PKG_CHECK_MODULES(SDL_IMAGE, sdl3-image, have_sdl_image=yes, have_sdl_image=no)
	if test x$have_sdl_image = xyes; then
		AM_CONDITIONAL(HAVE_SDL_IMAGE, true)
	else
		AC_MSG_WARN([Unable to find sdl3-image library (https://github.com/libsdl-org/SDL_image)])
		AC_MSG_WARN([Tools mockup and smooth disabled])
	fi
fi

# Build compiler?
AC_ARG_ENABLE(compiler, AS_HELP_STRING([--enable-compiler], [Build the usecode compiler @<:@default no@:>@]),,enable_compiler=no)
AC_MSG_CHECKING([whether to build the usecode compiler])
if test x$enable_compiler = xno; then
	AC_MSG_RESULT(no)
	AM_CONDITIONAL(BUILD_COMPILER, false)
else
	AC_MSG_RESULT(yes)
	AM_CONDITIONAL(BUILD_COMPILER, true)
fi

# Build data files?
AC_ARG_ENABLE(data, AS_HELP_STRING([--enable-data], [Create the data files @<:@default yes@:>@]),,enable_data=yes)
AC_MSG_CHECKING([whether to build the data files])
if test x$enable_data = xno; then
	AC_MSG_RESULT(no)
	AM_CONDITIONAL(DATA_FILES, false)
else
	AM_CONDITIONAL(DATA_FILES, true)
	AC_MSG_RESULT(yes)
fi

# Build mods?
AC_ARG_ENABLE(mods, AS_HELP_STRING([--enable-mods], [Build the Exult mods (requires usecode compiler) @<:@default no@:>@]),,enable_mods=no)
AC_MSG_CHECKING([whether to build the Exult mods])
if test x$enable_mods = xno -o x$enable_compiler = xno; then
	AC_MSG_RESULT(no)
	AM_CONDITIONAL(BUILD_MODS, false)
else
	AM_CONDITIONAL(BUILD_MODS, true)
	AC_MSG_RESULT(yes)
fi

# Usecode debugger
AC_ARG_WITH([usecode-debugger],
            AS_HELP_STRING([--with-usecode-debugger=no|console|yes],
                           [Experimental and buggy support for usecode debugging @<:@default no@:>@]),
            [enable_usecode_debugger="$withval"], [enable_usecode_debugger="no"])
AC_MSG_CHECKING([whether to enable the usecode debugger])
if test x$enable_usecode_debugger = xconsole -o x$enable_usecode_debugger = xyes; then
	AC_MSG_RESULT(yes)
	if test x$enable_usecode_debugger = xconsole; then
		AC_DEFINE(USECODE_CONSOLE_DEBUGGER, 1, [Enable Usecode debugging on console])
	elif test x$enable_exult_studio != xyes; then
		echo "But we are not building Exult Studio."
		echo "Please try again, either with '--enable-exult-studio', or without the usecode debugger"
		exit 1
	elif test x$enable_exult_studio_support != xyes; then
		echo "But we are not building Exult with Exult Studio support."
		echo "Please try again, either with '--enable-exult-studio-support', or without the usecode debugger"
		exit 1
	fi
	AC_DEFINE(USECODE_DEBUGGER, 1, [Enable Usecode debugging])
else
	AC_MSG_RESULT(no)
fi

# shp-thumbnailer
AC_ARG_ENABLE(shp-thumbnailer, AS_HELP_STRING([--enable-shp-thumbnailer@<:@=yes|system|local|user|ingame|no@:>@],
		[Build Exult SHP Thumbnailer, yes or system and local for system (local) install (requires root), user for user install, ingame for Exult ingame install (requires XDG_DATA_DIRS) @<:@default no@:>@]),,enable_shp_thumbnailer=no)
AC_MSG_CHECKING([if we want to build the Exult SHP Thumbnailer])
if test x$enable_shp_thumbnailer != xno; then
	AC_MSG_RESULT(yes)
	# needs Gdk-Pixbuf
	PKG_CHECK_MODULES(GDK_PIXBUF, gdk-pixbuf-2.0, have_gdk_pixbuf=yes, have_gdk_pixbuf=no)
	AC_MSG_CHECKING([if we can build the Exult SHP Thumbnailer])
	if test x$have_gdk_pixbuf = xno; then
		AC_MSG_RESULT([no; gdk-pixbuf is not installed])
		echo "The Exult shp thumbnailer requires Gdk-Pixbuf."
		echo "Please try again, either with Gdk-Pixbuf-2.0, or with '--disable-shp-thumbnailer'."
		exit 1
	fi
	if test x$have_png = xno; then
		AC_MSG_RESULT([no; libpng is not installed])
		echo "The Exult shp thumbnailer requires the PNG (aka the Portable Network Graphics)."
		echo "Please try again, either with the PNG installed, or with '--disable-shp-thumbnailer'."
		exit 1
	fi
	AC_MSG_RESULT(yes)
	AM_CONDITIONAL(BUILD_EXULT_THUMB, true)
	if   test x$enable_shp_thumbnailer == xlocal; then
		EXULT_THUMB_PREFIX="/usr/local/share"
	elif test x$enable_shp_thumbnailer == xuser; then
		EXULT_THUMB_PREFIX="${HOME_SHRDIR}"
	elif test x$enable_shp_thumbnailer == xingame; then
		EXULT_THUMB_PREFIX="${datadir}"
	else
		EXULT_THUMB_PREFIX="/usr/share"
	fi
	AC_SUBST(EXULT_THUMB_PREFIX)
else
	AM_CONDITIONAL(BUILD_EXULT_THUMB, false)
	AC_MSG_RESULT(no)
fi

# Aseprite plugin
AM_CONDITIONAL(BUILD_ASEPRITE_PLUGIN, false)
AC_ARG_ENABLE(aseprite-plugin,
	AS_HELP_STRING([--enable-aseprite-plugin], [Build the Aseprite plugin @<:@default no@:>@]),, enable_aseprite_plugin=no)
AC_MSG_CHECKING([whether to build the Aseprite plugin])
if test x$enable_aseprite_plugin = xyes; then
	AC_MSG_RESULT(yes)
	if test x$have_png = xno; then
		echo "The Aseprite Plugin requires the PNG (aka the Portable Network Graphics)."
		echo "Please try again, either with the PNG installed, or with '--disable_aseprite_plugin'."
		exit 1
	fi
	AM_CONDITIONAL(BUILD_ASEPRITE_PLUGIN, true)
else
	AC_MSG_RESULT(no)
fi

# GIMP plugin
AM_CONDITIONAL(GIMP3_PLUGIN, false)
AM_CONDITIONAL(GIMP2_PLUGIN, false)
AM_CONDITIONAL(BUILD_GIMP_PLUGIN, false)
AC_ARG_ENABLE(gimp-plugin,
	AS_HELP_STRING([--enable-gimp-plugin@<:@=yes|system|user|no@:>@],
		[Build the GIMP plugin, yes or system for system install (requires root), user for user install, @<:@default no@:>@]),,enable_gimp_plugin=no)
AC_MSG_CHECKING([if we want to build the GIMP plugin])
if test x$enable_gimp_plugin != xno; then
	AC_MSG_RESULT(yes)
	PKG_CHECK_MODULES(GIMP, [gimpui-3.0 gimp-3.0 >= 3.0.0], have_gimp3=yes, have_gimp3=no)
	if test x$have_gimp3 = xyes; then
		AM_CONDITIONAL(GIMP3_PLUGIN, true)
		AM_CONDITIONAL(BUILD_GIMP_PLUGIN, true)
		if test x$enable_gimp_plugin != xuser; then
			PKG_CHECK_VAR(GIMP_PLUGIN_PREFIX, gimp-3.0, gimplibdir)
		else
			GIMP_PLUGIN_PREFIX="${HOME_CFGDIR}/GIMP/3.0"
		fi
		GIMP_PLUGIN_PREFIX="$GIMP_PLUGIN_PREFIX/plug-ins"
		AC_SUBST(GIMP_PLUGIN_PREFIX)
		AC_DEFINE(HAVE_GIMP, 1, [Have GIMP])
		AC_MSG_CHECKING([if we can build the GIMP 3 plugin])
		AC_MSG_RESULT([yes; for GIMP $gimp_version >= 3.0.0])
	else
		PKG_CHECK_MODULES(GIMP, [gimpui-2.0 gimp-2.0 >= 2.8.0], have_gimp2=yes, have_gimp2=no)
		if test x$have_gimp2 = xyes; then
			AM_CONDITIONAL(GIMP2_PLUGIN, true)
			AM_CONDITIONAL(BUILD_GIMP_PLUGIN, true)
			if test x$enable_gimp_plugin != xuser; then
				PKG_CHECK_VAR(GIMP_PLUGIN_PREFIX, gimp-2.0, gimplibdir)
			else
				GIMP_PLUGIN_PREFIX="${HOME_CFGDIR}/GIMP/2.0"
			fi
			GIMP_PLUGIN_PREFIX="$GIMP_PLUGIN_PREFIX/plug-ins"
			AC_SUBST(GIMP_PLUGIN_PREFIX)
			AC_DEFINE(HAVE_GIMP, 1, [Have GIMP])
			AC_MSG_CHECKING([if we can build the GIMP 2 plugin])
			AC_MSG_RESULT([yes; for GIMP $gimp_version >= 2.8.0])
		else
			AC_MSG_CHECKING([if we can build the GIMP plugin])
			AC_MSG_RESULT([no; GIMP 2 and GIMP 3 not installed])
			echo "The Gimp plugin requires the Gimp either in version 3 or in version 2, but neither is installed."
			echo "Please try again, either with the Gimp installed, or with '--disable-gimp-plugin'."
			exit 1
		fi
	fi
else
	AC_MSG_RESULT(no)
fi

# ---------------------------------------------------------------------
# Alternative directories
# ---------------------------------------------------------------------

DESKTOPDIR="${datadir}/applications"
AC_ARG_WITH([desktopdir],
	AS_HELP_STRING([--with-desktopdir=DIR],[change desktop directory]),
	[case "${withval}" in
		yes)
		;;
		no)
		;;
		*)
			DESKTOPDIR="${withval}"
		;;
	esac])
AC_SUBST([DESKTOPDIR])

ICONDIR="${datadir}/icons/hicolor/512x512/apps"
AC_ARG_WITH([icondir],
	AS_HELP_STRING([--with-icondir=DIR],[change icon directory]),
	[case "${withval}" in
		yes)
		;;
		no)
		;;
		*)
			ICONDIR="${withval}"
		;;
	esac])
AC_SUBST([ICONDIR])

SVGDIR="${datadir}/icons/hicolor/scalable/apps"
AC_ARG_WITH([svgdir],
	AS_HELP_STRING([--with-svgdir=DIR],[change svg directory]),
	[case "${withval}" in
		yes)
		;;
		no)
		;;
		*)
			SVGDIR="${withval}"
		;;
	esac])
AC_SUBST([SVGDIR])

METADIR="${datadir}/metainfo"
AC_SUBST([METADIR])

# ---------------------------------------------------------------------
# Workaround for 'ar' warning in libtool
# ---------------------------------------------------------------------

if test "x$AR_FLAGS" = "xcru" ; then
	AR_FLAGS="cr"
fi

# ---------------------------------------------------------------------
# Optimization options
# ---------------------------------------------------------------------

AC_ARG_WITH([optimization],
            AS_HELP_STRING([--with-optimization=none|size|debug|light|normal|heavy],
                           [Select optimization level @<:@default=from CXXFLAGS@:>@]),
            [opt_level="$withval"], [opt_level=""])
AC_MSG_CHECKING([desired optimization level])
if test x$opt_level = x; then
	AC_MSG_RESULT(from CXXFLAGS)
else
	# For Android builds we pass the level through
	if test x$enable_android_apk != xno; then
		AC_SUBST(EXULT_ANDROID_OPT, $opt_level)
	fi
	# TODO: Maybe making a function to do these.
	# Lets clean up CFLAGS of -O[0-3gs]?
	for ccflag in $CFLAGS; do
		AS_CASE([$ccflag],
			[-O|-O0|-O1|-O2|-O3|-Os|-Og], [],
			[NEWCFLAGS="$NEWCFLAGS $ccflag"])
	done
	CFLAGS="$NEWCFLAGS"

	# Lets clean up CXXFLAGS of -O[0-3gs]?
	for cxxflag in $CXXFLAGS; do
		AS_CASE([$cxxflag],
			[-O|-O0|-O1|-O2|-O3|-Os|-Og], [],
			[NEWCXXFLAGS="$NEWCXXFLAGS $cxxflag"])
	done
	CXXFLAGS="$NEWCXXFLAGS"

	if test x$opt_level = xnone; then
		AC_MSG_RESULT(none)
		OPT_LEVEL="$OPT_LEVEL -O0"
	elif test x$opt_level = xsize; then
		AC_MSG_RESULT(size)
		AX_CHECK_COMPILE_FLAG([-Os], [have_os_opt=yes], [have_os_opt=no], [$OPT_LEVEL -Werror])
		if test x$have_os_opt = xyes; then
			OPT_LEVEL="$OPT_LEVEL -Os"
		else
			# Sane default if compiler does not support -Os
			OPT_LEVEL="$OPT_LEVEL -O2"
			for opt_flag in -falign-functions -falign-jumps -falign-loops -falign-labels -freorder-blocks -freorder-blocks-and-partition -fprefetch-loop-arrays -ftree-vect-loop-version
			do
				AX_CHECK_COMPILE_FLAG([$opt_flag], [OPT_LEVEL="$OPT_LEVEL $opt_flag"], [], [$OPT_LEVEL -Werror])
			done
		fi
	elif test x$opt_level = xdebug; then
		AC_MSG_RESULT(debug)
		AX_CHECK_COMPILE_FLAG([-Og], [have_og_opt=yes], [have_og_opt=no], [$OPT_LEVEL -Werror])
		if test x$have_og_opt = xyes; then
			OPT_LEVEL="$OPT_LEVEL -Og"
		else
			# Sane default if compiler does not support -Og
			OPT_LEVEL="$OPT_LEVEL -O1"
			for opt_flag in -fno-branch-count-reg -fno-if-conversion -fno-if-conversion2 -fno-inline-functions-called-once -fno-move-loop-invariants -fno-ssa-phiopt -fno-tree-bit-ccp -fno-tree-pta -fno-tree-sra
			do
				AX_CHECK_COMPILE_FLAG([$opt_flag], [OPT_LEVEL="$OPT_LEVEL $opt_flag"], [], [$OPT_LEVEL -Werror])
			done
		fi
	elif test x$opt_level = xlight; then
		AC_MSG_RESULT(light)
		OPT_LEVEL="$OPT_LEVEL -O1"
	elif test x$opt_level = xnormal; then
		AC_MSG_RESULT(normal)
		OPT_LEVEL="$OPT_LEVEL -O2"
	elif test x$opt_level = xheavy; then
		AC_MSG_RESULT(heavy)
		OPT_LEVEL="$OPT_LEVEL -O3"
	else
		AC_MSG_RESULT(invalid)
		echo "Invalid option given for --with-optimization: $opt_level."
		echo "Expected one of 'none', 'size', 'debug', 'light', 'normal', 'heavy'."
		exit 1
	fi
fi

# link-time optimization
GB_ENABLE_LTO
DEBUG_LEVEL="$DEBUG_LEVEL $LTO_CFLAGS"

# ---------------------------------------------------------------------
# Debugging options
# ---------------------------------------------------------------------

AC_ARG_WITH([debug],
            AS_HELP_STRING([--with-debug=no|messages|symbols|full|extreme],
                           [Enable debug mode @<:@default=from CXXFLAGS@:>@]),
            [dbg_level="$withval"], [dbg_level=""])
AC_MSG_CHECKING([whether to enable debugging mode])
if test x$dbg_level = x; then
	AC_MSG_RESULT(from CXXFLAGS)
else
	# For Android builds we pass the level through
	if test x$enable_android_apk != xno; then
		AC_SUBST(EXULT_ANDROID_DEBUG, $dbg_level)
	fi
	# TODO: Maybe making a function to do these.
	# Lets clean up CFLAGS of -g[0-3]?
	for ccflag in $CFLAGS; do
		AS_CASE([$ccflag],
			[-g|-g1|-g2|-g3], [],
			[NEWCFLAGS="$NEWCFLAGS $ccflag"])
	done
	CFLAGS="$NEWCFLAGS"

	# Lets clean up CXXFLAGS of -g[0-3]?
	for cxxflag in $CXXFLAGS; do
		AS_CASE([$cxxflag],
			[-g|-g1|-g2|-g3], [],
			[NEWCXXFLAGS="$NEWCXXFLAGS $cxxflag"])
	done
	CXXFLAGS="$NEWCXXFLAGS"

	if test x$dbg_level = xno; then
		AC_MSG_RESULT(no)
	elif test x$dbg_level = xmessages; then
		AC_MSG_RESULT(messages only)
		AC_DEFINE(DEBUG, 1, [Enable debug mode])
	elif test x$dbg_level = xsymbols -o x$dbg_level = xfull -o x$dbg_level = xextreme; then
		AC_MSG_RESULT($dbg_level)
		if test x$dbg_level != xsymbols; then
			AC_DEFINE(DEBUG, 1, [Enable debug mode])
		fi
		if test x$dbg_level = xextreme; then
			DEBUG_LEVEL="$DEBUG_LEVEL -g3"
		else
			DEBUG_LEVEL="$DEBUG_LEVEL -g"
		fi
		# TODO: Maybe add warning that -O0 and -Og work better for debug symbols?
	else
		AC_MSG_RESULT(invalid)
		echo "Invalid option given for --with-debug: $dbg_level."
		echo "Expected one of 'no', 'messages', 'symbols', 'full', 'extreme'."
		exit 1
	fi
fi

AC_ARG_WITH([sanitizers],
            AS_HELP_STRING([--with-sanitizers=no|address|thread|memory|undefined|dataflow|safe-stack],
                           [Enable sanitizers @<:@default no@:>@]),
            [enable_sanitizers="$withval"], [enable_sanitizers="no"])
AC_MSG_CHECKING([whether to enable sanitizers])
case "${enable_sanitizers}" in
	no)
		AC_MSG_RESULT(no)
		CHK_SAN=""
	;;
	address|thread|memory|undefined|dataflow|safe-stack)
		AC_MSG_RESULT($enable_sanitizers)
		CHK_SAN="-fsanitize=$enable_sanitizers"
	;;
	*)
		AC_MSG_RESULT(invalid)
		echo "Invalid option given for --with-sanitizers: $enable_sanitizers."
		echo "Expected one of 'no', 'address', 'thread', 'memory', 'yes'."
		exit 1
	;;
esac

SANITIZERS=""
for cxx_flag in $CHK_SAN
do
	AX_CHECK_COMPILE_FLAG([$cxx_flag], [SANITIZERS="$SANITIZERS $cxx_flag"], [], [$CXXFLAGS -Werror])
done

CXXFLAGS="$CXXFLAGS $SANITIZERS"

# ---------------------------------------------------------------------
# Warning level
# ---------------------------------------------------------------------

CHK_WARN="-Wall -Wextra -pedantic"
CHK_WARN="$CHK_WARN -Walloc-zero -Walloca"
CHK_WARN="$CHK_WARN -Wc++17-compat -Wc++20-compat -Wbool-compare -Wbool-operation"
CHK_WARN="$CHK_WARN -Wcatch-value=3 -Wcast-align -Wcast-align=strict -Wcast-qual -Wcast-function-type"
CHK_WARN="$CHK_WARN -Wconditionally-supported -Wctor-dtor-privacy -Wdisabled-optimization"
CHK_WARN="$CHK_WARN -Wduplicated-branches -Wduplicated-cond -Wextra-semi"
CHK_WARN="$CHK_WARN -Wformat-nonliteral -Wformat-security"
CHK_WARN="$CHK_WARN -Wlogical-not-parentheses -Wlogical-op"
CHK_WARN="$CHK_WARN -Wmissing-include-dirs -Wnon-virtual-dtor -Wnull-dereference"
CHK_WARN="$CHK_WARN -Wold-style-cast -Woverloaded-virtual -Wplacement-new"
CHK_WARN="$CHK_WARN -Wredundant-decls -Wshift-negative-value -Wshift-overflow"
CHK_WARN="$CHK_WARN -Wtrigraphs -Wundef -Wuninitialized -Wuseless-cast -Wwrite-strings"
# Warnings that give lots of warnings
#CHK_WARN="$CHK_WARN -Wformat-signedness -Wcast-align=strict -Weffc++ -Wpadded -Wshadow -Wsign-conversion"
# GCC attributes
#CHK_WARN="$CHK_WARN -Wsuggest-attribute=cold -Wsuggest-attribute=const -Wsuggest-attribute=format -Wsuggest-attribute=malloc -Wsuggest-attribute=noreturn -Wsuggest-attribute=pure"
# C++11 and newer
#CHK_WARN="$CHK_WARN -Wsuggest-final-methods -Wsuggest-final-types -Wsuggest-override -Wzero-as-null-pointer-constant"
CHK_WARN="$CHK_WARN -Wzero-as-null-pointer-constant"
# Clang warnings
CHK_WARN="$CHK_WARN -Wunused-const-variables -Wabsolute-value -Wdeprecated -Wdeprecated-register -Wmismatched-tags -Wunused-private-field"

# Eliminate some spurious warnings due to low optimization level.
if test x$opt_level = xnone -o x$opt_level = xdebug; then
	CHK_WARN="$CHK_WARN -Wno-maybe-uninitialized"
fi

# Warnings give errors
AC_ARG_ENABLE(warning-errors, AS_HELP_STRING([--enable-warning-errors], [Turn all warnings into errors @<:@default no@:>@]),,enable_warning_errors=no)
if test x$enable_warning_errors = xyes; then
	CHK_WARN="-Werror $CHK_WARN"
fi

# Pedantic warnings give errors
AC_ARG_ENABLE(pedantic-errors, AS_HELP_STRING([--enable-pedantic-errors], [Turn all pedantic warnings into errors @<:@default no@:>@]),,enable_pedantic_errors=no)
if test x$enable_pedantic_errors = xyes; then
	CHK_WARN="-pedantic-errors $CHK_WARN"
fi

WARNINGS=""
for cxx_flag in $CHK_WARN
do
	AX_CHECK_COMPILE_FLAG([$cxx_flag], [WARNINGS="$WARNINGS $cxx_flag"], [], [$DEBUG_LEVEL -Werror])
done

# ---------------------------------------------------------------------
# Cross-Compiling
# ---------------------------------------------------------------------

AM_CONDITIONAL(CROSS_COMPILING, test $cross_compiling = yes)

AS_IF([ test $cross_compiling = yes ], [
	if test x$enable_data = xyes; then
		AC_PATH_PROG(EXPACK_PROG, expack, no)
		echo "While cross compiling you need to have a build system native"
		echo "expack in your path to generate Exult's data files."
		echo "After you natively compiled Exult you will find expack in the tools subfolder."
		if test x$EXPACK_PROG = xno; then
			AC_MSG_ERROR([Could not find expack in your path, cannot generate data files.])
		fi
	fi

	if test x$enable_tools = xyes; then
		AC_PATH_PROG(HEAD2DATA_PROG, head2data, no)
		echo "While cross compiling you need to have a build system native"
		echo "head2data in your path to build part of Exult's tools."
		echo "After you natively compiled Exult you will find head2data"
		echo "in the tools/ucxt subfolder."
		if test x$HEAD2DATA_PROG = xno; then
			AC_MSG_ERROR([Could not find head2data in your path, cannot generate data files for tools.])
		fi
	fi
])

# ---------------------------------------------------------------------
# Generate output
# ---------------------------------------------------------------------

AC_SUBST(WINDOWING_SYSTEM)
AC_SUBST(EXE_TARGET)
AC_SUBST(EXULT_DATADIR)
AC_SUBST(SYSLIBS)
AC_SUBST(ICON_FILE)
AC_SUBST(DEBUG_LEVEL)
AC_SUBST(OPT_LEVEL)
AC_SUBST(WARNINGS)
AC_SUBST(AR_FLAGS)

AC_CONFIG_FILES([
exult.spec
Info.plist
macosx/exult_studio_info.plist
Makefile
android/Makefile
android/app/build.gradle
android/lib/Makefile
android/lib/src/Makefile
audio/Makefile
audio/midi_drivers/Makefile
audio/midi_drivers/timidity/Makefile
conf/Makefile
content/Makefile
content/sifixes/Makefile
content/si/Makefile
content/bgkeyring/Makefile
content/bg/Makefile
content/islefaq/Makefile
files/Makefile
files/sha1/Makefile
files/zip/Makefile
gamemgr/Makefile
gumps/Makefile
pathfinder/Makefile
flic/Makefile
tools/Makefile
tools/compiler/Makefile
tools/ucxt/Makefile
tools/ucxt/Docs/Makefile
tools/ucxt/data/Makefile
tools/ucxt/include/Makefile
tools/ucxt/src/Makefile
tools/aseprite_plugin/Makefile
tools/gimp_plugin/Makefile
tools/mockup/Makefile
tools/smooth/Makefile
data/Makefile
docs/Makefile
desktop/Makefile
objs/Makefile
imagewin/Makefile
shapes/Makefile
shapes/shapeinf/Makefile
usecode/Makefile
mapedit/Makefile
server/Makefile
])
AC_OUTPUT

dnl ****************
dnl Configure Status
dnl ****************

echo
echo Exult v$VERSION
echo
printf "Build Exult................ : "
if test x$enable_libexult = xyes; then
	echo as library
elif test x$enable_exult = xyes; then
	echo yes
else
	echo no
fi
echo Build Exult Studio......... : $enable_exult_studio
echo Exult Studio support....... : $enable_exult_studio_support
echo Create data files.......... : $enable_data
if test x$enable_android_apk != xno; then
	echo Build Android APK.......... : $enable_android_apk
	if test "x$with_android_arch" != x; then
		echo Android Architecture\(s\).....: $with_android_arch
	else
		echo Android Architecture\(s\).....: x86, x86_64, armeabi-v7a, arm64-v8a
	fi
fi
echo
echo C++ Compiler............... : $CXX
echo C++ Compiler Version....... : $($CXX --version | head -1)
if test "x$opt_level" != "x"; then
	echo C++ optimization........... : $opt_level
fi
if test "x$dbg_level" != "x"; then
	echo C++ debug.................. : $dbg_level
fi
echo
	echo Dependencies :
if test x$enable_android_apk != xno; then
	echo Android SDK................ : $ANDROID_SDK_VERSION
	echo Android NDK................ : $ANDROID_NDK_VERSION
	echo Android SDK CMake.......... : $ANDROID_CMAKE_VERSION
	echo Gradle..................... : $gradle_version
	echo Java....................... : $java_version
	echo
else
	echo SDL........................ : `$PKG_CONFIG --modversion sdl3`
	if test x$have_sdl_image = xyes; then
		PKG_CHECK_EXISTS(sdl3-image,
			echo SDL_image.................. : `$PKG_CONFIG --modversion sdl3-image`)
	fi
	PKG_CHECK_EXISTS(ogg,
		echo Ogg.........................: `$PKG_CONFIG --modversion ogg`)
	PKG_CHECK_EXISTS(vorbis,
		echo Vorbis......................: `$PKG_CONFIG --modversion vorbis`)
	if test x$have_png = xyes; then
		PKG_CHECK_EXISTS(libpng,
			echo libpng..................... : `$PKG_CONFIG --modversion libpng`)
	fi
	if test x$enable_mt32emu = xyes; then
		PKG_CHECK_EXISTS(mt32emu,
			echo MT32emu.................... : `$PKG_CONFIG --modversion mt32emu`)
	fi
	if test x$enable_fluidsynth != xno; then
		if test x$enable_fluidsynth = xyes; then
			PKG_CHECK_EXISTS(fluidsynth,
				echo Fluidsynth................. : `$PKG_CONFIG --modversion fluidsynth`)
		fi
		if test x$enable_fluidsynth = xlite; then
			PKG_CHECK_EXISTS(fluidlite,
				echo FluidLite.................. : `$PKG_CONFIG --modversion fluidlite`)
		fi
	fi
	if test x$enable_zip_support = xyes ; then
		PKG_CHECK_EXISTS(zlib,
			echo zlib....................... : `$PKG_CONFIG --modversion zlib`)
	fi
	if test x$enable_exult_studio = xyes; then
		echo ICU........................ : `$PKG_CONFIG --modversion icu-uc`
	fi
	if test x$enable_exult_studio = xyes; then
		echo GLIB....................... : `$PKG_CONFIG --modversion glib-2.0`
		echo GTK+....................... : `$PKG_CONFIG --modversion gtk+-3.0`
	fi
	if test x$have_freetype = xyes; then
		echo FreeType2.................. : `$PKG_CONFIG --modversion freetype2`
	fi
	if test x$enable_shp_thumbnailer = xyes; then
		echo GDK-Pixbuf................. : `$PKG_CONFIG --modversion gdk-pixbuf-2.0`
	fi
	if test x$have_gimp3 = xyes;then
		echo GIMP....................... : `$PKG_CONFIG --modversion gimpui-3.0`
	fi
	if test x$have_gimp2 = xyes;then
		echo GIMP....................... : `$PKG_CONFIG --modversion gimpui-2.0`
	fi
	echo
fi

if test x$enable_exult = xyes; then
	echo Scalers :
	if test x$enable_all_hq_scalers = xyes; then
		echo Build all HQ scalers....... : $enable_all_hq_scalers
		else
		echo Build HQ2X scaler.......... : $enable_hq2x
		echo Build HQ3X scaler.......... : $enable_hq3x
		echo Build HQ4X scaler.......... : $enable_hq4x
	fi
	echo Build NxBR scalers......... : $enable_nxbr
	echo
	echo Audio :
	echo Enable ALSA midi........... : $enable_alsa
	echo Enable Fluidsynth midi..... : $enable_fluidsynth
	echo Enable mt32emu midi........ : $enable_mt32emu
	echo Enable Timidity midi....... : $enable_timidity_midi
	echo Enable SFX as midi......... : $enable_midi_sfx
	echo
fi
echo Addons :
echo Build tools................ : $enable_tools
echo Build usecode compiler..... : $enable_compiler
echo Build Exult mods........... : $enable_mods
echo Create zipped Savegames.... : $enable_zip_support
if test x$enable_gimp_plugin != xno; then
	echo Build GIMP SHP plugin...... : $enable_gimp_plugin into $GIMP_PLUGIN_PREFIX
else
	echo Build GIMP SHP plugin...... : $enable_gimp_plugin
fi
if test x$enable_shp_thumbnailer != xno; then
	echo Build Exult SHP Thumbnailer : $enable_shp_thumbnailer into $EXULT_THUMB_PREFIX
else
	echo Build Exult SHP Thumbnailer : $enable_shp_thumbnailer
fi
echo Build Aseprite SHP plugin...: $enable_aseprite_plugin
echo
echo Debug :
echo Build Usecode debugger..... : $enable_usecode_debugger
echo
echo Compilation :
echo Enable compiler sanitizers. : $enable_sanitizers
echo Build with static libraries : $enable_static_libs
echo Turn all warnings to errors : $enable_warning_errors
echo Turn pedantics to errors... : $enable_pedantic_errors
if test x$ARCH = xmacosx; then
	if test x$enable_static_libs != xno; then
		echo macOS static libs....... : $with_macosx_static_lib_path
	fi
fi
if test x"$with_macosx_code_signature" != x; then
	echo macOS code sign ID...... : $with_macosx_code_signature
fi
if test x"$with_macosx_installer_signature" != x; then
	echo macOS Installer sign ID. : $with_macosx_installer_signature
fi
if test ${cross_compiling} = yes; then
	echo
	echo Cross Compiling for $host
	if test x$enable_data = xyes; then
		echo expack path................ : $EXPACK_PROG
	fi
	if test x$enable_tools = xyes; then
		echo head2data path............. : $HEAD2DATA_PROG
	fi
fi
echo
echo "Now enter 'make'"
