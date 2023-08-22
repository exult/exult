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
#include <cstring>
#include <windows.h>
#endif

#if (defined(__linux__) || defined(__linux) || defined(linux))
#include <fstream>
#include <string>
#endif

#ifdef _WIN32
// Do casts from pointers without too much undefined behavior.
template <typename To, typename From>
To safe_pointer_cast(From pointer) {
	// NOLINTNEXTLINE(bugprone-sizeof-expression)
	static_assert(std::is_pointer<From>::value && std::is_pointer<To>::value && sizeof(From) == sizeof(To), "Pointer sizes do not match");
	To output;
	std::memcpy(static_cast<void*>(&output),
	            static_cast<void*>(&pointer),
	            sizeof(From*));
	return output;
}

#endif

void getVersionInfo(std::ostream &out) {
	/*
	 * 1. Exult version
	 */

	out << "Exult version " << VERSION << std::endl;

	/*
	* 2. Build Architechture
	*/
	out << "Build Architechture: ";

	// AMD64 x86_64
#if defined(__amd64__) || defined(__amd64) || defined(__amd64__) \
|| defined(__amd64) || defined(_M_X64) || defined(_M_AMD64)
	out << "x86_64";
	// ARM THUMB
#elif defined(__thumb__) || defined(__TARGET_ARCH_THUMB) || defined(_M_ARMT)
		out << "ARM Thumb";
#elif defined(__arm__) || defined(__TARGET_ARCH_ARM) || defined(_ARM) \
|| defined(_M_ARM) || defined(__arm)
	out << "ARM";
	// ARM64
#elif defined(__aarch64__) || defined(_M_ARM64)
			out << "ARM64";

			//X86
#elif defined(i386) || defined(__i386) || defined(__i386__) \
|| defined(__i386) || defined(_M_IX86) || defined(__386)
	out << "x86";
#elif defined(__riscv)
	out << "RISC-V"
#else
			out << "unknown architechture update version.cc ";

#endif

	out << std::endl;


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
	 * 4. Various important build options in effect
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
		OSVERSIONINFOEXA info;
		info.dwOSVersionInfoSize = sizeof(info);
		GetVersionEx(safe_pointer_cast<LPOSVERSIONINFOA>(&info));

		// Platform is NT
		if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
			if (info.wProductType == VER_NT_WORKSTATION) {
				if (info.dwMajorVersion < 4) {
					out << "NT";
				} else if (info.dwMajorVersion == 4) {
					out << "NT4";
				} else if (info.dwMajorVersion == 5 && info.dwMinorVersion == 0) {
					out << 2000;
				} else if (info.dwMajorVersion == 5 && info.dwMinorVersion == 1) {
					out << "XP";
				} else if (info.dwMajorVersion == 5 && info.dwMinorVersion == 2) {
					// Only workstation release with version 5.2 was XP x64
					out << "XP";
				} else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 0) {
					out << "Vista";
				} else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 1) {
					out << "7";
				} else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 2) {
					out << "8";
				} else {
					// Note: Without the proper manifest file, GetVersionEx will
					// lie about these.
					if (info.dwMajorVersion == 6 && info.dwMinorVersion == 3) {
						out << "8.1";
					} else if (info.dwMajorVersion == 10) {
						// cut off for Windows 10 and 11 is build 22000 (11 builds
						// with a build number lower than 22000 are not public
						// releases so I don't care)
						if (LOWORD(info.dwBuildNumber & 0xFFFF) < 22000) {
							out << "10";
						} else {
							out << "11";
						}
					} else {
						out << "Unknown NT";
					}
				}
			} else {
				// Server
				// Note: No Server release of 5.1
				if (info.dwMajorVersion < 4) {
					out << "NT Server";
				} else if (info.dwMajorVersion == 4) {
					out << "NT4 Server";
				} else if (info.dwMajorVersion == 5 && info.dwMinorVersion == 0) {
					out << "2000 Server";
				} else if (info.dwMajorVersion == 5 && info.dwMinorVersion == 2) {
					out << " Windows Server 2003";
				} else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 0) {
					out << "Windows Server 2008";
				} else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 1) {
					out << "Windows Server 2008 R2";
				} else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 2) {
					out << "Windows Server 2012";
				} else {
					// Note: Without the proper manifest file, GetVersionEx will
					// lie about these.
					if (info.dwMajorVersion == 6 && info.dwMinorVersion == 3) {
						out << "Windows Server 2012 R2";
					} else if (info.dwMajorVersion == 10) {
						if (LOWORD(info.dwBuildNumber & 0xFFFF) < 17000) {
							out << "Server 2016";
						} else if (LOWORD(info.dwBuildNumber & 0xFFFF) < 19000) {
							out << "Server 2019";
						} else {
							out << "Server 2022";
						}
					} else {
						out << "Unknown NT Server";
					}
				}
			}

		} else {
			out << "Unknown NT";
		}

		if (info.szCSDVersion[0] != 0) {
			out << " " << info.szCSDVersion;
		} else if (info.dwMajorVersion == 4 && info.dwMinorVersion == 0) {
			out << 95;
			if (info.szCSDVersion[1] != ' ') {
				out << info.szCSDVersion;
			}
		} else if (info.dwMajorVersion == 4 && info.dwMinorVersion == 10) {
			out << 98;
			if (info.szCSDVersion[1] == 'A') {
				out << " SE";
			} else if (info.szCSDVersion[1] != ' ') {
				out << info.szCSDVersion;
			}
		} else if (info.dwMajorVersion == 4 && info.dwMinorVersion == 90) {
			out << "Me";
		}

		out << " Version " << info.dwMajorVersion << "." << info.dwMinorVersion
			<< " Build " << LOWORD(info.dwBuildNumber & 0xFFFF) << " ";

		// This function only exists in XP or newer but I see no reason to break
		// compatibility with older windows version here so using it dynamically
		void (WINAPI *fpGetNativeSystemInfo)(LPSYSTEM_INFO lpSystemInfo) = nullptr;
		HMODULE kernel32 = GetModuleHandleA("KERNEL32");
		if (kernel32 != nullptr) {
			using LPNativeSystemInfo = decltype(fpGetNativeSystemInfo);
			fpGetNativeSystemInfo = safe_pointer_cast<LPNativeSystemInfo>(
					GetProcAddress(kernel32, "GetNativeSystemInfo"));
			// We default to GetSystemInfo (win2000 req) if we couldn't get GetNativeSystemInfo
			if (fpGetNativeSystemInfo == nullptr) {
				fpGetNativeSystemInfo = safe_pointer_cast<LPNativeSystemInfo>(
						GetProcAddress(kernel32, "GetSystemInfo"));
			}
		}

		// If we have one of the get systeminfo functions print some details,
		// mostly to know if its ARM64 or AMD64 OS running x86 code
		if (fpGetNativeSystemInfo != nullptr) {
			SYSTEM_INFO sysinfo;
			fpGetNativeSystemInfo(&sysinfo);
			switch (sysinfo.wProcessorArchitecture) {
			case PROCESSOR_ARCHITECTURE_INTEL:
				out << "x86 ";
				break;
			case PROCESSOR_ARCHITECTURE_IA64:
				out << "IA64 ";
				break;
			case PROCESSOR_ARCHITECTURE_ARM64:
				out << "ARM64 ";
				break;
			case PROCESSOR_ARCHITECTURE_ARM:
				out << "ARM ";
				break;
			case PROCESSOR_ARCHITECTURE_AMD64:
				out << "x64 ";
				break;
			default:
				out << "UnknownArch ";
			}

			out << sysinfo.dwNumberOfProcessors << " Processors";
		}
	}
#elif (defined(FREEBSD))
	out << "FreeBSD";
#elif (defined(MACOSX))
	out << "macOS";
#elif (defined(__IOS__))
	out << "iOS";
#elif (defined(NETBSD))
	out << "NetBSD";
#else
	out << "Unknown";
#endif

	out << std::endl;
}
