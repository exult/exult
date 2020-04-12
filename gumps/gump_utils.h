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

#ifdef XWIN  /* Only needed in XWIN. */
#include <sys/time.h>
#endif

#ifdef OPENBSD
#include <sys/types.h>
#endif

#include <unistd.h>

#ifndef XWIN
#include "SDL_timer.h"
#endif

/*
 *  Delay between animations.
 */

inline void Delay(
) {
#ifdef XWIN
	/*
	 *  Here's a somewhat better way to delay in X:
	 */
	extern int xfd;
	fd_set rfds;
	struct timeval timer;
	timer.tv_sec = 0;
	timer.tv_usec = 50000;      // Try 1/50 second.
	FD_ZERO(&rfds);
	FD_SET(xfd, &rfds);
	// Wait for timeout or event.
	select(xfd + 1, &rfds, nullptr, nullptr, &timer);
#else                   /* May use this for Linux too. */
	SDL_Delay(10);          // Try 1/100 second.
#endif
}

#endif
