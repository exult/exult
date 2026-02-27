
// The following ifdef block is the standard way of creating macros which make
// exporting from a DLL simpler. All files within this DLL are compiled with the
// EXCONFIG_EXPORTS symbol defined on the command line. this symbol should not
// be defined on any project that uses this DLL. This way any other project
// whose source files include this file see EXCONFIG_API functions as being
// imported from a DLL, wheras this DLL sees symbols defined with this macro as
// being exported.

// Insert your headers here
#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN    // Exclude rarely-used stuff from Windows
								   // headers
#endif

#include <windows.h>

#include <string>
