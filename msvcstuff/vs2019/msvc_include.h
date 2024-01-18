#pragma once
#pragma warning(disable : 4996)
#define NOMINMAX

#define VERSION       "1.9.0git"
#define EXULT_DATADIR "data/"

// Don't need everything in the windows headers
#define WIN32_LEAN_AND_MEAN

// Disable some warnings
#pragma warning(disable : 4786)    // Debug Len > 255
#pragma warning( \
		disable : 4355)    // 'this' : used in base member initializer list

#ifndef ENABLE_EXTRA_WARNINGS
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

#include <windows.h>

#define USE_FMOPL_MIDI   1
#define USE_WINDOWS_MIDI 1
// #define USE_MT32EMU_MIDI 1
#define HAVE_STDIO_H 1

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
