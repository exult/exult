/*
 *  Copyright (C) 2001-2022  The Exult Team
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <cctype>
#include <cstdlib>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#endif    // __GNUC__
#include "SDL_events.h"
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "files/U7file.h"
#include "files/utils.h"
#include "flic/playfli.h"
#include "gamewin.h"
#include "Audio.h"
#include "devgame.h"
#include "palette.h"
#include "databuf.h"
#include "font.h"
#include "txtscroll.h"
#include "common_types.h"

