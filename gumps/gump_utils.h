/*
 *  Copyright (C) 2000-2022  The Exult Team
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

#ifndef GUMP_UTILS_H
#define GUMP_UTILS_H

#include <unistd.h>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

/*
 *  Delay between animations.
 */

#define DELAY_TOTAL_MS  10
#define DELAY_SINGLE_MS 1

inline void Delay() {
	const Uint32 expiration = DELAY_TOTAL_MS + SDL_GetTicks();
	for (;;) {
		SDL_PumpEvents();
		if ((SDL_PeepEvents(
					 nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST)
			 != 0)
			|| (static_cast<Sint32>(SDL_GetTicks())
				>= static_cast<Sint32>(expiration))) {
			return;
		}

		SDL_Delay(DELAY_SINGLE_MS);
	}
}

#endif
