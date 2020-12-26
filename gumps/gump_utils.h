/*
 *  Copyright (C) 2000-2013  The Exult Team
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

#include "SDL_timer.h"
#include "SDL_events.h"

/*
 *  Delay between animations.
 */

#define DELAY_TOTAL_MS 10
#define DELAY_SINGLE_MS 1

inline void Delay(
) {
	Uint32 expiration = DELAY_TOTAL_MS + SDL_GetTicks();
	for (;;) {
		SDL_PumpEvents();
		if ((SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) != 0) ||
		    (static_cast<Sint32>(SDL_GetTicks()) >= static_cast<Sint32>(expiration))) return;

		SDL_Delay(DELAY_SINGLE_MS);
	}
}

#endif
