// unistd.h so Exult will compile under MSVC

#ifndef UNISTD_H
#define UNISTD_H

// if we have the getopt from vcpkg include it for exult studio
#if __has_include(<getopt.h>)
#	include <getopt.h>
#	define HAVE_GETOPT_LONG 1
#endif

#endif UNISTD_H
