#ifndef GLOBALS_H
#define GLOBALS_H

#ifdef _MSC_VER
#	define strcasecmp  _stricmp
#	define strncasecmp _strnicmp
#endif

#ifdef _WIN32
#	include <windows.h>
typedef HMODULE libhandle_t;
#	define PLUGIN_EXPORT __declspec(dllexport)
#else
typedef void* libhandle_t;
#	define PLUGIN_EXPORT
#endif

#ifdef MAIN
#	define EXTERN
#else
#	define EXTERN extern
#endif

// this is totally arbitrary but seems reasonable. It is the max length of a
// line in the config file
#define MAX_LINE_LENGTH 1024

// since we use indexed images, we have a limitation of 256 colours
#define MAX_COLOURS 256

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

// note: there are some almost static stuff and some very variable stuff
// global's variables
typedef struct g_var_struct {
	SDL_Surface*                  image_out;            // var
	const SDL_PixelFormatDetails* image_out_format;     // var
	SDL_Palette*                  image_out_palette;    // var
	int                           image_out_ncolors;    // var
	int                           global_x;             // var
	int                           global_y;             // var
} glob_variables;

EXTERN glob_variables g_variables;

// global's almost statics
typedef struct g_stat_struct {
	int                           debug;               // stat
	SDL_Surface*                  image_in;            // stat
	const SDL_PixelFormatDetails* image_in_format;     // stat
	SDL_Palette*                  image_in_palette;    // stat
	int                           image_in_ncolors;    // stat
	const char*                   filein;              // stat
	const char*                   fileout;             // stat
	const char*                   config_file;         // stat
} glob_statics;

EXTERN glob_statics g_statics;

typedef Uint32 colour_hex;

typedef colour_hex (*pfnPluginApply)(
		colour_hex ret_col, glob_variables* g_variables);

typedef struct pacman {
	struct pacman* next;
	pfnPluginApply plugin_apply;    // for storing plugins' apply
	libhandle_t    handle;          // for storing dlopens' handle
} node;

// this is what holds the address of the plugin_apply functions to apply to
// colours. probably the most critical variable of the program
EXTERN node* action_table[MAX_COLOURS];

// this is to keep track of all loaded plugins. We need this to cleanly unload
// them later
EXTERN node* hdl_list;

#endif    // GLOBALS_H
