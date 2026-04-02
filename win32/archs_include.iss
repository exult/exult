; This file sets up various arcitchture related macros used by the installer scripts so the scripts can conditionally choose what files to include and not need all arch files to exist

// If sys is not specified use a blank string meaning all
#ifndef MINGW_SYS
  #define MINGW_SYS ""
#endif

; if arch disable  macros arent defined, set them to a default based on the sys

#ifndef NO_i686
  #if (MINGW_SYS == "mingw32") || (MINGW_SYS == "")
    #define NO_i686 0
  #else
    #define NO_i686 1
   #endif
 #pragma message "NO_i686 being defined as "+Str(NO_i686)
#else
  #pragma message "NO_i686 already defined as "+Str(NO_i686)
#endif
   #define WANT_i686 !Int(NO_i686)

#ifndef NO_x64
  #if (MINGW_SYS == "UCRT64") || (MINGW_SYS == "mingw64") || (MINGW_SYS == "clang64") || (MINGW_SYS == "")
    #define NO_x64 0
  #else
    #define NO_x64 1
  #endif
 #pragma message "NO_x64 being defined as "+Str(NO_x64)
#else
  #pragma message "NO_x64 already defined as "+Str(NO_x64)
#endif
#define WANT_x64 !Int(NO_x64)

#ifndef NO_ARM64
  #if (MINGW_SYS == "clangarm64") || (MINGW_SYS == "")
    #define NO_ARM64 0    
  #else
    #define NO_ARM64 1
  #endif
 #pragma message "NO_ARM64 being defined as "+Str(NO_ARM64)
#else
  #pragma message "NO_ARM64 already defined as "+Str(NO_ARM64)
#endif

#define WANT_ARM64 !Int(NO_ARM64)

; Define arch string macros 

#if WANT_i686
  #define archi686 " x86compatible"
#else
  #define archi686 ""
#endif

#if WANT_x64
  #define archx64 " X64Compatible"
#else
  #define archx64 ""
#endif

; This defines x64Check based on WANT_ARM64 because x64 files should not be installed on ARM64 systems when ARM64 files are included in the installer but if there are no ARM64 files the x64 files should be installed
#if WANT_ARM64
  #define archarm64 " arm64"
  #define x64Check "IsX64OS"
#else
  #define archarm64 ""
  #define x64Check "IsX64Compatible"
#endif

#define archs archi686 + archx64 + archarm64
#define archs64 archx64 + archarm64

 #pragma message "archs= "+archs
 #pragma message "archs64= "+archs64
 

#if SameStr(archs,"")
#error No architectures selected for inclusion in installer
#endif