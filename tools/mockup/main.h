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

int    main(int argc, char** argv);
Uint32 getpixel(SDL_Surface* surface, int x, int y);
