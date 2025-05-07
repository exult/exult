#pragma once
#define NOMINMAX

#define VERSION       "1.11.0git"
#define EXULT_DATADIR "data/"

#ifdef _DEBUG
#	define DEBUG
#endif

// Force Multibyte character set
// Exult does not suppot default unicode on Windows
#ifndef _MBCS
#	define _MBCS 1
#endif
#ifdef UNICODE
#	undef UNICODE
#endif
#ifdef _UNICODE
#	undef _UNICODE
#endif

// Don't need everything in the windows headers
#define WIN32_LEAN_AND_MEAN

// Disable some warnings
#pragma warning(disable : 4786)    // Debug Len > 255
#pragma warning( \
		disable : 4355)    // 'this' : used in base member initializer list

#ifndef ENABLE_EXTRA_WARNINGS
#	define _CRT_NONSTDC_NO_WARNINGS        1
#	define _CRT_SECURE_NO_WARNINGS         1
#	define _CRT_SECURE_NO_WARNINGS_GLOBALS 1
#	define _CRT_OBSOLETE_NO_WARNINGS       1
#	define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#	define FKG_FORCED_USAGE                1 // disables warning about GetVersionEx
// #pragma warning (disable: 4101) // unreferenced local variable
// #pragma warning (disable: 4309) // truncation of constant value
#	pragma warning(disable : 4305)    // truncation from 'const int' to 'char'
#	pragma warning(disable : 4290)    // C++ exception specification ignored
									   // except to indicate a function is not
									   // __declspec(nothrow)

#	pragma warning(disable : 4267 4244)    // conversion from 'type' to 'type'
											// possible loss of data
#	pragma warning(disable : 4805)         // unsafe mix of type 'int' and type
											// 'bool' in operation

#endif
// Enable all the signed/unsigned warnings at level 3
#pragma warning(3 : 4018)
#pragma warning(3 : 4388)
#pragma warning(3 : 4389)
// unused variable warnings
#pragma warning(3 : 4101)
#pragma warning(3 : 4100)
#pragma warning(3 : 4189)

#include <windows.h>
#define USE_XBR_SCALER 1
#define USE_HQ2X_SCALER 1
#define USE_HQ3X_SCALER 1
#define USE_HQ4X_SCALER 1
#define USE_FMOPL_MIDI   1
#define USE_WINDOWS_MIDI 1
#define USE_MT32EMU_MIDI 1
#define USE_FLUIDSYNTH_MIDI 1
#define HAVE_STDIO_H     1
#define HAVE_ZIP_SUPPORT 1
#define HAVE_PNG_H       1
#ifdef USE_CONSOLE
// Disable SDL Main
#	define SDL_MAIN_HANDLED 1
#endif
#define USE_EXULTSTUDIO 1

#if _MSC_VER <= 1938

#	include <algorithm>
#	include <cctype>

#	pragma warning(disable : 4091)    // 'typedef ' : ignored on left of
									   // 'tagGPFIDL_FLAGS' when no variable
#	pragma warning(disable : 4309)    // truncation of constant value

/*
namespace std {
	using ::toupper;
	using ::tolower;
	using ::isdigit;
	using ::toupper;
	using ::isalnum;
	using ::isspace;
	using ::isgraph;
}    // namespace std
*/

#endif    //  _MSC_VER <= 1938
inline int strcasecmp(const char* s1, const char* s2)
{
	return _strcmpi(s1, s2);
}
inline int strncasecmp(const char* s1, const char* s2, size_t n) {
	return _strnicmp(s1, s2, n);
}
