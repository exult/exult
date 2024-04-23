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

#include <algorithm>
#include <string_view>
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "endianio.h"
#include "version.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

// Only include gitinfo.h if it exists and none of the macros have already been
// defined
#if __has_include(                                                  \
		"gitinfo.h") && !defined(GIT_REVISION) && !defined(GIT_TAG) \
		&& !defined(GIT_REMOTE_BRANCH) && !defined(GIT_REMOTE_URL)
#	include "gitinfo.h"
#endif

#ifdef _WIN32
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	include <windows.h>
#endif

#if (defined(__linux__) || defined(__linux) || defined(linux))
#	include <fstream>
#	include <string>
#endif

#ifdef _WIN32
namespace {
	// Do casts from pointers without too much undefined behavior.
	template <typename To, typename From>
	To safe_pointer_cast(From pointer) {
		// NOLINTNEXTLINE(bugprone-sizeof-expression)
		constexpr const size_t SizeFrom = sizeof(From);
		// NOLINTNEXTLINE(bugprone-sizeof-expression)
		constexpr const size_t SizeTo = sizeof(To);
		static_assert(
				std::is_pointer<From>::value && std::is_pointer<To>::value
						&& SizeFrom == SizeTo,
				"Pointer sizes do not match");
		To output;
		std::memcpy(
				static_cast<void*>(&output), static_cast<void*>(&pointer),
				SizeFrom);
		return output;
	}
}    // namespace
#endif
#ifdef GIT_REVISION
static constexpr const std::string_view git_rev{GIT_REVISION};
#else
constexpr static const std::string_view git_rev{};
#endif
#ifdef GIT_TAG
static constexpr const std::string_view git_tag{GIT_TAG};
#else
static constexpr const std::string_view git_tag{};
#endif
#ifdef GIT_REMOTE_BRANCH
static constexpr const std::string_view git_branch{GIT_REMOTE_BRANCH};
#else
static constexpr const std::string_view git_branch{};
#endif
#ifdef GIT_REMOTE_URL
static constexpr const std::string_view git_url{GIT_REMOTE_URL};
#else
static constexpr const std::string_view git_url{};
#endif

std::string_view VersionGetGitRevision(bool shortrev) {
	if constexpr (!git_rev.empty()) {
		if (shortrev) {
			return git_rev.substr(0, 7);
		}
		return git_rev;
	}
	return {};
}

std::string VersionGetGitInfo(bool limitedwidth) {
	// Everything for this function can be known at compile time
	// so could probably be made constexpr with a new enough c++ standard
	// and compiler support
	std::string result;
	result.reserve(256);

	if constexpr (!git_branch.empty()) {
		result += "Git Branch: ";
		result += git_branch;
		result += "\n";
	}
	if constexpr (!git_rev.empty()) {
		result += "Git Revision: ";
		result += VersionGetGitRevision(limitedwidth);
		result += "\n";
	}
	if constexpr (!git_tag.empty()) {
		result += "Git Tag: ";
		result += git_tag;
		result += "\n";
	}

	// Default to the Exult origin repo on github
	std::string src_url = "https://github.com/exult/exult";

	// if the remote url was supplied in the build make sure it is git hub https
	if constexpr (git_url.substr(0, 19) == "https://github.com/") {
		src_url = git_url;
		// if it ends in '.git remove it
#ifdef __cpp_lib_starts_ends_with    // c++20
		if (src_url.ends_with(".git"))
#else    // older
		if (src_url.compare(src_url.size() - 4, 4, ".git") == 0)
#endif
		{
			src_url.resize(src_url.size() - 4);
		}
		// remove ending slash if any
		if (src_url.back() == '/') {
			src_url.pop_back();
		}

		// As we have github remote we can create url to exact revision/tag
		if constexpr (!git_tag.empty()) {
			src_url.reserve(src_url.size() + std::size(git_tag) + 7);
			if (limitedwidth) {
				src_url += "\n";
			}
			src_url += "/tree/";
			src_url += git_tag;
		} else if constexpr (!git_rev.empty()) {
			src_url.reserve(src_url.size() + std::size(git_rev) + 6);
			if (limitedwidth) {
				src_url += "\n";
			}
			src_url += "/tree/";
			src_url += VersionGetGitRevision(limitedwidth);
		}
	}

	result += "Source url: ";
	// if (limitedwidth) {
	//	result += "\n";
	// }
	result += src_url;
	return result;
}

void getVersionInfo(std::ostream& out) {
	/*
	 * 1. Exult version
	 */

	out << "Exult version " << VERSION << '\n';
	/*
	 * 4. Git revision information
	 */
	out << VersionGetGitInfo(false) << '\n';

	/*
	 * 3. Build Architecture
	 */
	out << "Build Architecture: ";

	// AMD64 x86_64
#if defined(__amd64__) || defined(__amd64) || defined(__amd64__) \
		|| defined(__amd64) || defined(_M_X64) || defined(_M_AMD64)
	out << "x86_64";
#	define WANT_CPUID
	// ARM THUMB
#elif defined(__thumb__) || defined(__TARGET_ARCH_THUMB) || defined(_M_ARMT)
	out << "ARM Thumb";
#elif defined(__arm__) || defined(__TARGET_ARCH_ARM) || defined(_ARM) \
		|| defined(_M_ARM) || defined(__arm)
	out << "ARM";
	// ARM64
#elif defined(__aarch64__) || defined(_M_ARM64)
	out << "ARM64";

	// X86
#elif defined(i386) || defined(__i386) || defined(__i386__) || defined(__i386) \
		|| defined(_M_IX86) || defined(__386)
	out << "x86";
#	define WANT_CPUID
#elif defined(__riscv)
	out << "RISC-V";
#else
	out << "unknown architecture update version.cc ";

#endif

	out << '\n';

#ifdef WANT_CPUID
	OutputCPUID(&out);
#endif    //  WANT_CPUID

	/*
	 * 4. Build time
	 */

#if (defined(__TIME__) || defined(__DATE__))
	out << "Built at: ";
#	ifdef __DATE__
	out << __DATE__ << " ";
#	endif
#	ifdef __TIME__
	out << __TIME__;
#	endif
	out << '\n';
#endif

	/*
	 * 5. Various important build options in effect
	 */

	out << "Compile-time options: ";
	bool firstoption = true;

#ifdef DEBUG
	if (!firstoption) {
		out << ", ";
	}
	firstoption = false;
	out << "DEBUG";
#endif

#ifdef USE_TIMIDITY_MIDI
	if (!firstoption) {
		out << ", ";
	}
	firstoption = false;
	out << "USE_TIMIDITY_MIDI";
#endif

#ifdef USE_FMOPL_MIDI
	if (!firstoption) {
		out << ", ";
	}
	firstoption = false;
	out << "USE_FMOPL_MIDI";
#endif

#ifdef USE_MT32EMU_MIDI
	if (!firstoption) {
		out << ", ";
	}
	firstoption = false;
	out << "USE_MT32EMU_MIDI";
#endif

#ifdef USE_ALSA_MIDI
	if (!firstoption) {
		out << ", ";
	}
	firstoption = false;
	out << "USE_ALSA_MIDI";
#endif

#ifdef USE_EXULTSTUDIO
	if (!firstoption) {
		out << ", ";
	}
	firstoption = false;
	out << "USE_EXULTSTUDIO";
#endif

#ifdef USECODE_DEBUGGER
	if (!firstoption) {
		out << ", ";
	}
	firstoption = false;
	out << "USECODE_DEBUGGER";
#endif

#ifdef HAVE_ZIP_SUPPORT
	if (!firstoption) {
		out << ", ";
	}
	firstoption = false;
	out << "HAVE_ZIP_SUPPORT";
#endif

#ifdef ENABLE_MIDISFX
	if (!firstoption) {
		out << ", ";
	}
	firstoption = false;
	out << "ENABLE_MIDISFX";
#endif

	if (firstoption) {
		out << "(none)";
	}
	out << '\n';

	/*
	 * 6. Compiler used to create this binary
	 */

	out << "Compiler: ";
#ifdef __INTEL_COMPILER
#	define COMPILER __VERSION__
#elif defined(__clang__)
#	define COMPILER __VERSION__
#elif defined(__GNUC__)
#	define COMPILER "GCC " __VERSION__
#elif defined(_MSC_FULL_VER)
#	define COMPILER                                                       \
		"Microsoft C/C++ Compiler " << (_MSC_FULL_VER / 10'000'000) << '.' \
									<< ((_MSC_FULL_VER / 100'000) % 100)   \
									<< '.' << ((_MSC_FULL_VER % 100'000))
#else
#	define COMPILER "unknown compiler"
#endif

	out << COMPILER << '\n';
#undef COMPILER

	/*
	 * 7. Platform
	 */

	out << '\n' << "Platform: ";

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
		ZeroMemory(&info, sizeof(info));
		info.dwOSVersionInfoSize = sizeof(info);
		GetVersionExA(safe_pointer_cast<LPOSVERSIONINFOA>(&info));

		// Platform is NT
		if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
			if (info.wProductType == VER_NT_WORKSTATION) {
				if (info.dwMajorVersion < 4) {
					out << "NT";
				} else if (info.dwMajorVersion == 4) {
					out << "NT4";
				} else if (
						info.dwMajorVersion == 5 && info.dwMinorVersion == 0) {
					out << 2000;
				} else if (
						info.dwMajorVersion == 5 && info.dwMinorVersion == 1) {
					out << "XP";
				} else if (
						info.dwMajorVersion == 5 && info.dwMinorVersion == 2) {
					// Only workstation release with version 5.2 was XP x64
					out << "XP";
				} else if (
						info.dwMajorVersion == 6 && info.dwMinorVersion == 0) {
					out << "Vista";
				} else if (
						info.dwMajorVersion == 6 && info.dwMinorVersion == 1) {
					out << "7";
				} else if (
						info.dwMajorVersion == 6 && info.dwMinorVersion == 2) {
					out << "8";
				} else {
					// Note: Without the proper manifest file, GetVersionEx will
					// lie about these.
					if (info.dwMajorVersion == 6 && info.dwMinorVersion == 3) {
						out << "8.1";
					} else if (info.dwMajorVersion == 10) {
						// cut off for Windows 10 and 11 is build 22000 (11
						// builds with a build number lower than 22000 are not
						// public releases so I don't care)
						if (LOWORD(info.dwBuildNumber) < 22000) {
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
				} else if (
						info.dwMajorVersion == 5 && info.dwMinorVersion == 0) {
					out << "2000 Server";
				} else if (
						info.dwMajorVersion == 5 && info.dwMinorVersion == 2) {
					out << " Windows Server 2003";
				} else if (
						info.dwMajorVersion == 6 && info.dwMinorVersion == 0) {
					out << "Windows Server 2008";
				} else if (
						info.dwMajorVersion == 6 && info.dwMinorVersion == 1) {
					out << "Windows Server 2008 R2";
				} else if (
						info.dwMajorVersion == 6 && info.dwMinorVersion == 2) {
					out << "Windows Server 2012";
				} else {
					// Note: Without the proper manifest file, GetVersionEx will
					// lie about these.
					if (info.dwMajorVersion == 6 && info.dwMinorVersion == 3) {
						out << "Windows Server 2012 R2";
					} else if (info.dwMajorVersion == 10) {
						if (LOWORD(info.dwBuildNumber) < 17000) {
							out << "Server 2016";
						} else if (LOWORD(info.dwBuildNumber) < 19000) {
							out << "Server 2019";
						} else {
							out << "Server 2022";
						}
					} else {
						out << "Unknown NT Server";
					}
				}
			}
			if (info.szCSDVersion[0] != 0) {
				out << " " << info.szCSDVersion;
			}

		} else {
			out << "Windows ";

			if (info.dwMajorVersion == 4 && info.dwMinorVersion == 0) {
				out << 95;
				const char value = info.szCSDVersion[1];
				if (value == 'C' || value == 'B') {
					out << "OSR2 ";
				} else if (value != ' ') {
					out << info.szCSDVersion;
				}
			} else if (info.dwMajorVersion == 4 && info.dwMinorVersion == 10) {
				out << 98;
				const char value = info.szCSDVersion[1];
				if (value == 'A') {
					out << " SE";
				} else if (value != ' ') {
					out << info.szCSDVersion;
				}
			} else if (info.dwMajorVersion == 4 && info.dwMinorVersion == 90) {
				out << "Me";
			}
		}
		out << " Version " << info.dwMajorVersion << "." << info.dwMinorVersion
			<< " Build " << LOWORD(info.dwBuildNumber) << " ";

		// This function only exists in XP or newer but I see no reason to break
		// compatibility with older windows version here so using it dynamically
		void(WINAPI * fpGetNativeSystemInfo)(LPSYSTEM_INFO lpSystemInfo)
				= nullptr;
		HMODULE kernel32 = GetModuleHandleA("KERNEL32");
		if (kernel32 != nullptr) {
			using LPNativeSystemInfo = decltype(fpGetNativeSystemInfo);
			fpGetNativeSystemInfo    = safe_pointer_cast<LPNativeSystemInfo>(
                    GetProcAddress(kernel32, "GetNativeSystemInfo"));
			// We default to GetSystemInfo (win2000 req) if we couldn't get
			// GetNativeSystemInfo
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
#	if defined(PROCESSOR_ARCHITECTURE_ARM64)    // This only exists in Windows
			case PROCESSOR_ARCHITECTURE_ARM64:    // 10 sdks or newer, Will fail
#	else                                        // with vista, 7 or 8 sdks
			case 12:
#	endif
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
#elif (defined(SDL_PLATFORM_IOS))
	out << "iOS";
#elif (defined(NETBSD))
	out << "NetBSD";
#else
	out << "Unknown";
#endif

	out << '\n' << std::flush;
}

constexpr const int CPUID_EAX = 0;
constexpr const int CPUID_EBX = 1;
constexpr const int CPUID_ECX = 2;
constexpr const int CPUID_EDX = 3;
using std::uint32_t;

namespace {
#ifdef WANT_CPUID
// CPUID support for x86 cpus, wont work for 386 or 486 cpus but I don't think
// we'd support those anyway
#	if __has_include(<cpuid.h>)
// gcc and clang should have this
#		include <cpuid.h>

	bool CPUID(uint32_t leaf, uint32_t subleaf, std::array<uint32_t, 4>& regs) {
		return __get_cpuid_count(
					   leaf, subleaf, &regs[CPUID_EAX], &regs[CPUID_EBX],
					   &regs[CPUID_ECX], &regs[CPUID_EDX])
			   != 0;
	}
#	elif __has_include(<intrin.h>)
// MSVC should have this
#		include <intrin.h>

	bool CPUID(uint32_t leaf, uint32_t subleaf, std::array<uint32_t, 4>& regs) {
		__cpuidex(safe_pointer_cast<int*>(regs.data()), leaf, subleaf);
		return true;
	}
#	else
#		define NO_CPUID
#	endif

#else
#	define NO_CPUID
#endif

#ifdef NO_CPUID
	bool CPUID(uint32_t leaf, uint32_t subleaf, std::array<uint32_t, 4>& regs) {
		return false;
	}
#endif

	uint32_t CPUID_0(char vendor[13]) {
		std::array<uint32_t, 4> regs = {0};
		if (!CPUID(0, 0, regs)) {
			vendor[0] = 0;
			return 0;
		}

		char* vendor_ptr = vendor;
		little_endian::Write4(vendor_ptr, regs[CPUID_EBX]);
		little_endian::Write4(vendor_ptr, regs[CPUID_EDX]);
		little_endian::Write4(vendor_ptr, regs[CPUID_ECX]);
		*vendor_ptr = 0;
		return regs[CPUID_EAX];
	}

	uint32_t CPUID_ExBrand(char (&brand)[49]) {
		std::fill(std::begin(brand), std::end(brand), 0);
		char* brand_ptr = brand;

		std::array<uint32_t, 4> regs = {0};
		if (!CPUID(0x80000000u, 0, regs)) {
			return 0;
		}

		uint32_t max_ex = regs[CPUID_EAX];
		for (uint32_t i = 0; (i + 0x80000002u) <= max_ex && i < 3; i++) {
			if (!CPUID(0x80000002u + i, 0, regs)) {
				break;
			}

			little_endian::Write4(brand_ptr, regs[CPUID_EAX]);
			little_endian::Write4(brand_ptr, regs[CPUID_EBX]);
			little_endian::Write4(brand_ptr, regs[CPUID_ECX]);
			little_endian::Write4(brand_ptr, regs[CPUID_EDX]);
		}
		*brand_ptr = 0;
		return max_ex;
	}
}    // namespace

bool OutputCPUID(std::ostream* out) {
	bool     cpuidunsupportedfeatures = false;
	char     vendor[13];
	uint32_t maxcpuid = CPUID_0(vendor);
	if (maxcpuid == 0) {
		return cpuidunsupportedfeatures;
	}
	char     brand[49];
	uint32_t max_ex = CPUID_ExBrand(brand);
	if (out != nullptr) {
		*out << "CPUID: " << vendor << ", " << brand << '\n';
	}

	std::array<uint32_t, 4> regs = {0};
	if (!CPUID(1, 0, regs)) {
		std::fill(regs.begin(), regs.end(), 0);
	}

	// Just outputting raw fields here instead of getting real values
	if (out != nullptr) {
		*out << "stepping: " << ((regs[CPUID_EAX] >> 0) & 0xf) << '\n';
	}
	if (out != nullptr) {
		*out << "type: " << ((regs[CPUID_EAX] >> 12) & 0x3) << '\n';
	}
	if (out != nullptr) {
		*out << "family: " << ((regs[CPUID_EAX] >> 8) & 0xf) << '\n';
	}
	if (out != nullptr) {
		*out << "ext family: " << ((regs[CPUID_EAX] >> 20) & 0xff) << '\n';
	}
	if (out != nullptr) {
		*out << "model: " << ((regs[CPUID_EAX] >> 4) & 0xf) << '\n';
	}
	if (out != nullptr) {
		*out << "ext model: " << ((regs[CPUID_EAX] >> 16) & 0xf) << '\n';
	}

	std::array<uint32_t, 4> regs_ex = {0};
	if (max_ex < 0x80000001u || !CPUID(0x80000001u, 0, regs_ex)) {
		std::fill(regs_ex.begin(), regs_ex.end(), 0);
	}

	std::array<uint32_t, 4> regs_efb = {0};
	if (maxcpuid < 7 || !CPUID(7, 0, regs_efb)) {
		std::fill(regs_efb.begin(), regs_efb.end(), 0);
	}

	constexpr const bool need_sse = []() {
	// MSVC x86 floating point mode
#ifdef _M_IX86_FP
#	if _M_IX86_FP == 1 || _M_IX86_FP == 2
		return true;
#	endif
#endif
#if defined(__SSE__) || defined(__SSE_MATH__)
		return true;
#else
		return false;
#endif
	}();
	constexpr const bool need_sse2 = []() {
	// MSVC x86 floating point mode
#ifdef _M_IX86_FP
#	if _M_IX86_FP == 2
		return true;
#	endif
#endif
#if defined(__SSE2__) || defined(__SSE2_MATH__)
		return true;
#else
		return false;
#endif
	}();
	// x86-64-v2
	constexpr const bool need_sse3 = []() {
#ifdef __SSE3__
		return true;
#else
		return false;
#endif
	}();
	// x86-64-v2
	constexpr const bool need_ssse3 = []() {
#ifdef __SSSE3__
		return true;
#else
		return false;
#endif
	}();
	// x86-64-v2
	constexpr const bool need_sse4_1 = []() {
#ifdef __SSE4_1__
		return true;
#else
		return false;
#endif
	}();
	// x86-64-v2
	constexpr const bool need_sse4_2 = []() {
#ifdef __SSE4_2__
		return true;
#else
		return false;
#endif
	}();

	constexpr const bool need_sse4a = []() {
#ifdef __SSE4A__
		return true;
#else
		return false;
#endif
	}();

	// x86-64-v3
	constexpr const bool need_avx = []() {
#ifdef __AVX__
		return true;
#else
		return false;

#endif
	}();
	// x86-64-v3
	constexpr const bool need_avx2 = []() {
#ifdef __AVX2__
		return true;
#else
		return false;
#endif
	}();
	// x86-64-v4
	constexpr const bool need_avx512f = []() {
#ifdef __AVX512F__
		return true;
#else
		return false;
#endif
	}();
	// x86-64-v4
	constexpr const bool need_avx512bw = []() {
#ifdef __AVX512BW__
		return true;
#else
		return false;
#endif
	}();
	// x86-64-v4
	constexpr const bool need_avx512vl = []() {
#ifdef __AVX512VL__
		return true;
#else
		return false;
#endif
	}();
	// x86-64-v4
	constexpr const bool need_avx512cd = []() {
#ifdef __AVX512CD__
		return true;
#else
		return false;
#endif
	}();
	// x86-64-v4
	constexpr const bool need_avx512dq = []() {
#ifdef __AVX512DQ__
		return true;
#else
		return false;
#endif
	}();

	// likely means x86-64-v3 build
	constexpr const bool need_movbe = []() {
#ifdef __MOVBE__
		return true;
#else
		return false;
#endif
	}();
	// likely means x86-64-v2 build
	constexpr const bool need_popcnt = []() {
#ifdef __POPCNT__
		return true;
#else
		return false;
#endif
	}();
	// likely means x86-64-v3 build
	constexpr const bool need_fma = []() {
#ifdef __FMA__
		return true;
#else
		return false;
#endif
	}();
	// likely means x86-64-v2 build
	constexpr const bool need_lahfsahf = []() {
#ifdef __LAHF_SAHF__
		// 32 bit x86 always supports this so only check for it in 64 bit builds
		return sizeof(void*) == 8;
#else
		return false;
#endif
	}();
	// likely means x86-64-v3 build
	constexpr const bool need_bmi1 = []() {
#ifdef __BMI1__
		return true;
#else
		return false;
#endif
	}();
	// likely means x86-64-v3 build
	constexpr const bool need_bmi2 = []() {
#ifdef __BMI2__
		return true;
#else
		return false;
#endif
	}();

	constexpr const bool need_lzcnt = []() {
#ifdef __LZCNT__
		return true;
#else
		return false;
#endif
	}();

	// Check for required features
	if constexpr (need_sse) {
		if ((regs[CPUID_EDX] & (1 << 25)) != 0u) {
			if (out != nullptr) {
				*out << "SSE: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING SSE REQUIRED BUT NOT SUPPORTED BY "
						"THIS CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_sse2) {
		if ((regs[CPUID_EDX] & (1 << 26)) != 0u) {
			if (out != nullptr) {
				*out << "SSE2: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING SSE2 REQUIRED BUT NOT SUPPORTED "
						"BY THIS CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_sse3) {
		if ((regs[CPUID_ECX] & 1) != 0u) {
			if (out != nullptr) {
				*out << "SSE3: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING SSE3 REQUIRED BUT NOT SUPPORTED "
						"BY THIS "
						"CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_ssse3) {
		if ((regs[CPUID_ECX] & (1 << 9)) != 0u) {
			if (out != nullptr) {
				*out << "SSSE3: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING SSSE3 REQUIRED BUT NOT SUPPORTED "
						"BY "
						"THIS CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_sse4_1) {
		if ((regs[CPUID_ECX] & (1 << 19)) != 0u) {
			if (out != nullptr) {
				*out << "SSE4.1: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING SSE4.1 REQUIRED BUT NOT SUPPORTED "
						"BY THIS CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_sse4_2) {
		if ((regs[CPUID_ECX] & (1 << 20)) != 0u) {
			if (out != nullptr) {
				*out << "SSE4.2: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING SSE4.2 REQUIRED BUT NOT "
						"SUPPORTED BY THIS CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_sse4a) {
		if ((regs[CPUID_ECX] & (1 << 6)) != 0u) {
			if (out != nullptr) {
				*out << "SSE4A: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING SSE4A REQUIRED BUT NOT "
						"SUPPORTED BY THIS CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_movbe) {
		if ((regs[CPUID_ECX] & (1 << 22)) != 0u) {
			if (out != nullptr) {
				*out << "MOVBE: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING MOVBE REQUIRED BUT "
						"NOT SUPPORTED BY THIS CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_popcnt) {
		if ((regs[CPUID_ECX] & (1 << 23)) != 0u) {
			if (out != nullptr) {
				*out << "POPCNT: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING POPCNT REQUIRED "
						"BUT NOT SUPPORTED BY THIS "
						"CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_fma) {
		if ((regs[CPUID_ECX] & (1 << 12)) != 0u) {
			if (out != nullptr) {
				*out << "FMA: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING FMA REQUIRED "
						"BUT NOT SUPPORTED BY "
						"THIS CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_lahfsahf) {
		if ((regs_ex[CPUID_ECX] & 1) != 0u) {
			if (out != nullptr) {
				*out << "LAHF_SAHF: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING LAHF_SAHF "
						"REQUIRED BUT NOT "
						"SUPPORTED BY THIS "
						"CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_lzcnt) {
		if ((regs_ex[CPUID_ECX] & (1 << 5)) != 0u) {
			if (out != nullptr) {
				*out << "LZCNT: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING LZCNT "
						"REQUIRED BUT NOT "
						"SUPPORTED BY THIS "
						"CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_bmi1) {
		if ((regs_efb[CPUID_EBX] & (1 << 3)) != 0u) {
			if (out != nullptr) {
				*out << "BMI1: "
						"supported"
					 << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING BMI1 "
						"REQUIRED BUT "
						"NOT SUPPORTED "
						"BY THIS CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_bmi2) {
		if ((regs_efb[CPUID_EBX] & (1 << 8)) != 0u) {
			if (out != nullptr) {
				*out << "BMI2: "
						"supported"
					 << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING "
						"BMI2 "
						"REQUIRED "
						"BUT NOT "
						"SUPPORTED "
						"BY THIS "
						"CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_avx) {
		if ((regs[CPUID_ECX] & (1 << 28)) != 0u) {
			if (out != nullptr) {
				*out << "AVX: "
						"suppor"
						"ted"
					 << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNIN"
						"G AVX "
						"REQUIR"
						"ED "
						"BUT "
						"NOT "
						"SUPPOR"
						"TED "
						"BY "
						"THIS "
						"CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_avx2) {
		if ((regs_efb[CPUID_EBX] & (1 << 5)) != 0u) {
			if (out != nullptr) {
				*out << "AVX2: "
						"suppor"
						"ted"
					 << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNIN"
						"G "
						"AVX2 "
						"REQUIR"
						"ED "
						"BUT "
						"NOT "
						"SUPPOR"
						"TED "
						"BY "
						"THIS "
						"CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_avx512f) {
		if ((regs_efb[CPUID_EBX] & (1 << 16)) != 0u) {
			if (out != nullptr) {
				*out << "AV"
						"X5"
						"12"
						"F:"
						" s"
						"up"
						"po"
						"rt"
						"ed"
					 << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WA"
						"RN"
						"IN"
						"G "
						"AV"
						"X5"
						"12"
						"F "
						"RE"
						"QU"
						"IR"
						"ED"
						" B"
						"UT"
						" N"
						"OT"
						" S"
						"UP"
						"PO"
						"RT"
						"ED"
						" B"
						"Y "
						"TH"
						"IS"
						" C"
						"PU"
					 << '\n';
			}
		}
	}
	if constexpr (need_avx512dq) {
		if ((regs_efb[CPUID_EBX] & (1 << 17)) != 0u) {
			if (out != nullptr) {
				*out << "AVX512DQ: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING AVX512DQ REQUIRED BUT NOT "
						"SUPPORTED BY THIS "
						"CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_avx512bw) {
		if ((regs_efb[CPUID_EBX] & (1 << 30)) != 0u) {
			if (out != nullptr) {
				*out << "AVX512BW: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING AVX512BW REQUIRED BUT NOT "
						"SUPPORTED BY THIS "
						"CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_avx512vl) {
		if ((regs_efb[CPUID_EBX] & (1U << 31)) != 0u) {
			if (out != nullptr) {
				*out << "AVX512VL: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING AVX512VL REQUIRED BUT NOT "
						"SUPPORTED BY THIS "
						"CPU"
					 << '\n';
			}
		}
	}
	if constexpr (need_avx512cd) {
		if ((regs_efb[CPUID_EBX] & (1 << 28)) != 0u) {
			if (out != nullptr) {
				*out << "AVX512CD: supported" << '\n';
			}
		} else {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING AVX512CD REQUIRED BUT NOT "
						"SUPPORTED BY "
						"THIS "
						"CPU"
					 << '\n';
			}
		}
	}

	// x86-64
	// feature
	// levels
	if constexpr (sizeof(void*) == 8) {
		constexpr const bool need_v2 = need_popcnt && need_sse3 && need_ssse3
									   && need_sse4_1 && need_sse4_2
									   && need_lahfsahf;
		const bool v2 = [&]() {
			// CMPXCHG16B
			if ((regs[CPUID_ECX] & (1 << 28)) == 0u) {
				return false;
			}
			// LAHF-SAHF
			if ((regs_ex[CPUID_ECX] & 1) == 0u) {
				return false;
			}    // POPCNT
			if ((regs[CPUID_ECX] & (1 << 23)) == 0u) {
				return false;
			}
			// SSE3
			if ((regs[CPUID_ECX] & 1) == 0u) {
				return false;
			}
			// SSE4_1
			if ((regs[CPUID_ECX] & (1 << 19)) == 0u) {
				return false;
			}
			// SSE4_2
			if ((regs[CPUID_ECX] & (1 << 22)) == 0u) {
				return false;
			}
			// SSSE3
			if ((regs[CPUID_ECX] & (1 << 9)) == 0u) {
				return false;
			}
			return true;
		}();

		constexpr const bool need_v3 = need_avx && need_avx2 && need_movbe
									   && need_fma && need_bmi2 && need_bmi1
									   && need_lzcnt;
		const bool v3 = [&]() {
			// AVX
			if (!(regs[CPUID_ECX] & (1 << 28))) {
				return false;
			}
			// AVX2
			if (!(regs_efb[CPUID_EBX] & (1 << 5))) {
				return false;
			}
			// BMI1
			if (!(regs_efb[CPUID_EBX] & (1 << 3))) {
				return false;
			}
			// BMI2
			if (!(regs_efb[CPUID_EBX] & (1 << 8))) {
				return false;
			}
			// F16C
			if (!(regs[CPUID_ECX] & (1 << 29))) {
				return false;
			}
			// FMA
			if (!(regs[CPUID_ECX] & (1 << 12))) {
				return false;
			}
			// LZCNT
			if (!(regs_ex[CPUID_ECX] & (1 << 5))) {
				return false;
			}
			// MOVBE
			if (!(regs[CPUID_ECX] & (1 << 22))) {
				return false;
			}
			// OSXSAVE
			if (!(regs[CPUID_ECX] & (1 << 27))) {
				return false;
			}
			return v2;
		}();

		constexpr const bool need_v4 = need_avx512cd && need_avx512vl
									   && need_avx512f && need_avx512dq
									   && need_avx512bw;
		const bool v4 = [&]() {
			// avx512-cd
			if (!(regs_efb[CPUID_EBX] & (1 << 28))) {
				return false;
				// avx512-vl
			}
			if (!(regs_efb[CPUID_EBX] & (1U << 31))) {
				return false;
				// avx512-bw
			}
			if (!(regs_efb[CPUID_EBX] & (1 << 30))) {
				return false;
				// avx512-f
			}
			if (!(regs_efb[CPUID_EBX] & (1 << 16))) {
				return false;
				// avx512-dq
			}
			if (!(regs_efb[CPUID_EBX] & (1 << 17))) {
				return false;
			}
			return v3;
		}();

		if (v4) {
			if (out != nullptr) {
				*out << "X86-64-v4 supported" << '\n';
			}
		} else if (v3) {
			if (out != nullptr) {
				*out << "X86-64-v3 supported" << '\n';
			}
		} else if (v2) {
			if (out != nullptr) {
				*out << "X86-64-v2 supported" << '\n';
			}
		} else {
			if (out != nullptr) {
				*out << "Baseline X86-64 supported" << '\n';
			}
		}

		if (need_v4 && !v4) {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING X86-64-v4 FEATURES ARE REQUIRED "
						"BY "
						"THIS BUILD "
						"BUT "
						"ARE NOT SUPPORTED BY THIS CPU"
					 << '\n';
			}
		}
		if (need_v3 && !v3) {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING X86-64-v3 FEATURES ARE REQUIRED "
						"BY THIS BUILD "
						"BUT "
						"ARE NOT SUPPORTED BY THIS CPU"
					 << '\n';
			}
		}
		if (need_v2 && !v2) {
			cpuidunsupportedfeatures = true;
			if (out != nullptr) {
				*out << "WARNING X86-64-v2 FEATURES ARE "
						"REQUIRED BY THIS BUILD "
						"BUT "
						"ARE NOT SUPPORTED BY THIS CPU"
					 << '\n';
			}
		}
	}

	return cpuidunsupportedfeatures;
}

bool CPUIDHasUnsupportedFeatures() {
	return OutputCPUID(nullptr);
}
