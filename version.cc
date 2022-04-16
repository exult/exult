/*
 *  Copyright (C) 2001-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if (defined(__linux__) || defined(__linux) || defined(linux))
#include <fstream>
#include <string>
#endif

void getVersionInfo(std::ostream &out) {
	/*
	 * 1. Exult version
	 */

	out << "Exult version " << VERSION << std::endl;

	/*
	 * 2. Build time
	 */

#if (defined(__TIME__) || defined(__DATE__))
	out << "Built at: ";
#ifdef __DATE__
	out << __DATE__ << " ";
#endif
#ifdef __TIME__
	out << __TIME__;
#endif
	out << std::endl;
#endif

	/*
	 * 3. Various important build options in effect
	 */

	out << "Compile-time options: ";
	bool firstoption = true;

#ifdef DEBUG
	if (!firstoption) out << ", ";
	firstoption = false;
	out << "DEBUG";
#endif

#ifdef USE_TIMIDITY_MIDI
	if (!firstoption) out << ", ";
	firstoption = false;
	out << "USE_TIMIDITY_MIDI";
#endif

#ifdef USE_FMOPL_MIDI
	if (!firstoption) out << ", ";
	firstoption = false;
	out << "USE_FMOPL_MIDI";
#endif

#ifdef USE_MT32EMU_MIDI
	if (!firstoption) out << ", ";
	firstoption = false;
	out << "USE_MT32EMU_MIDI";
#endif

#ifdef USE_ALSA_MIDI
	if (!firstoption) out << ", ";
	firstoption = false;
	out << "USE_ALSA_MIDI";
#endif

#ifdef USE_EXULTSTUDIO
	if (!firstoption) out << ", ";
	firstoption = false;
	out << "USE_EXULTSTUDIO";
#endif

#ifdef USECODE_DEBUGGER
	if (!firstoption) out << ", ";
	firstoption = false;
	out << "USECODE_DEBUGGER";
#endif

#ifdef NO_SDL_PARACHUTE
	if (!firstoption) out << ", ";
	firstoption = false;
	out << "NO_SDL_PARACHUTE";
#endif

#ifdef HAVE_ZIP_SUPPORT
	if (!firstoption) out << ", ";
	firstoption = false;
	out << "HAVE_ZIP_SUPPORT";
#endif

#ifdef ENABLE_MIDISFX
	if (!firstoption) out << ", ";
	firstoption = false;
	out << "ENABLE_MIDISFX";
#endif

	if (firstoption) out << "(none)";
	out << std::endl;

	/*
	 * 4. Compiler used to create this binary
	 */

	out << "Compiler: ";
#ifdef __INTEL_COMPILER
#	define COMPILER __VERSION__
#elif defined(__clang__)
#	define COMPILER "GCC " __VERSION__
#elif defined(__GNUC__)
#	define COMPILER "GCC " __VERSION__
#elif defined(_MSC_FULL_VER)
#	define COMPILER "Microsoft C/C++ Compiler " \
                    << (_MSC_FULL_VER / 10'000'000) << '.' \
                    << ((_MSC_FULL_VER / 100'000) % 100) << '.'   \
                    << ((_MSC_FULL_VER % 100'000))
#else
#	define COMPILER "unknown compiler"
#endif

	out << COMPILER << std::endl;
#undef COMPILER

	/*
	 * 5. Platform
	 */

	out << std::endl << "Platform: ";

#if (defined(ANDROID))
	out << "Android";
#elif (defined(__linux__) || defined(__linux) || defined(linux))
	std::string ver;

	try {
		std::ifstream procversion("/proc/version");
		if (!procversion) {
			ver = "Linux";
		} else {
			std::getline(procversion, ver);
			procversion.close();
			ver = ver.substr(0, ver.find('('));
		}
	} catch (...) {
		ver = "Linux";
	}
	out << ver;
#elif (defined(__sun__) || defined(__sun))
	out << "Solaris";
#elif (defined(_WIN32))
	out << "Windows ";
	{
		// Get the version
		OSVERSIONINFO info;
		info.dwOSVersionInfoSize = sizeof(info);
		GetVersionEx(&info);

		// Platform is NT
		if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
			if (info.dwMajorVersion < 4) out << "NT";
			else if (info.dwMajorVersion == 4) out << "NT4";
			else if (info.dwMajorVersion == 5 && info.dwMinorVersion == 0) out << 2000;
			else if (info.dwMajorVersion == 5 && info.dwMinorVersion == 1) out << "XP";
			else if (info.dwMajorVersion == 5 && info.dwMinorVersion == 2) out << 2003;
			else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 0) out << "Vista";
			else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 1) out << "7";
			else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 2) out << "8";
			// Note: Without the proper manifest file, GetVersionEx will lie about these.
			else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 3) out << "8.1";
			else if (info.dwMajorVersion == 10 && info.dwMinorVersion == 0) out << "10";
			else out << "Unknown NT";

			if (info.szCSDVersion[0]) out << " " << info.szCSDVersion;
		}
		else if (info.dwMajorVersion == 4 && info.dwMinorVersion == 0) {
			out << 95;
			if (info.szCSDVersion[1] != ' ') out << info.szCSDVersion;
		} else if (info.dwMajorVersion == 4 && info.dwMinorVersion == 10) {
			out << 98;
			if (info.szCSDVersion[1] == 'A') out << " SE";
			else if (info.szCSDVersion[1] != ' ') out << info.szCSDVersion;
		} else if (info.dwMajorVersion == 4 && info.dwMinorVersion == 90)
			out << "Me";

		out << " Version " << info.dwMajorVersion << "." << info.dwMinorVersion << " Build " << LOWORD(info.dwBuildNumber & 0xFFFF);
	}
#elif (defined(MACOSX))
	out << "Mac OS X";
#elif (defined(__IPHONEOS__))
	out << "iOS";
#elif (defined(NETBSD))
	out << "NetBSD";
#else
	out << "Unknown";
#endif

	out << std::endl;
}
