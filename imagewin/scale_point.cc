/*
 *  Copyright (C) 2009 Exult Team
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "imagewin.h"

#include <cstring>

#include "common_types.h"
#include "manip.h"
#include "scale_point.h"
//
// Point Sampling
//
void Image_window::show_scaled8to8_point(
    int x, int y, int w, int h  // Area to show.
) {
	Manip8to8 manip(paletted_surface->format->palette->colors,
	                paletted_surface->format);
	Scale_point<unsigned char, uint8, Manip8to8>
	(static_cast<uint8 *>(draw_surface->pixels), x, y, w, h,
	 ibuf->line_width, ibuf->height + guard_band,
	 static_cast<uint8 *>(display_surface->pixels),
	 display_surface->pitch,
	 manip, scale);

	UpdateRect(display_surface, scale * x, scale * y, scale * w, scale * h);
}

void Image_window::show_scaled8to16_point(
    int x, int y, int w, int h  // Area to show.
) {
	Manip8to16 manip(paletted_surface->format->palette->colors,
	                 display_surface->format);
	Scale_point<unsigned char, uint16, Manip8to16>
	(static_cast<uint8 *>(draw_surface->pixels), x, y, w, h,
	 ibuf->line_width, ibuf->height + guard_band,
	 static_cast<uint16 *>(display_surface->pixels),
	 display_surface->pitch /
	 display_surface->format->BytesPerPixel,
	 manip, scale);
	UpdateRect(display_surface, scale * x, scale * y, scale * w, scale * h);
}

void Image_window::show_scaled8to555_point(
    int x, int y, int w, int h  // Area to show.
) {
	Manip8to555 manip(paletted_surface->format->palette->colors,
	                  display_surface->format);
	Scale_point<unsigned char, uint16, Manip8to555>
	(static_cast<uint8 *>(draw_surface->pixels), x, y, w, h,
	 ibuf->line_width, ibuf->height + guard_band,
	 static_cast<uint16 *>(display_surface->pixels),
	 display_surface->pitch /
	 display_surface->format->BytesPerPixel,
	 manip, scale);
	UpdateRect(display_surface, scale * x, scale * y, scale * w, scale * h);
}

void Image_window::show_scaled8to565_point(
    int x, int y, int w, int h  // Area to show.
) {
	Manip8to565 manip(paletted_surface->format->palette->colors,
	                  display_surface->format);
	Scale_point<unsigned char, uint16, Manip8to565>
	(static_cast<uint8 *>(draw_surface->pixels), x, y, w, h,
	 ibuf->line_width, ibuf->height + guard_band,
	 static_cast<uint16 *>(display_surface->pixels),
	 display_surface->pitch /
	 display_surface->format->BytesPerPixel,
	 manip, scale);
	UpdateRect(display_surface, scale * x, scale * y, scale * w, scale * h);
}

void Image_window::show_scaled8to32_point(
    int x, int y, int w, int h  // Area to show.
) {
	Manip8to32 manip(paletted_surface->format->palette->colors,
	                 display_surface->format);
	Scale_point<unsigned char, uint32, Manip8to32>
	(static_cast<uint8 *>(draw_surface->pixels), x, y, w, h,
	 ibuf->line_width, ibuf->height + guard_band,
	 static_cast<uint32 *>(display_surface->pixels),
	 display_surface->pitch /
	 display_surface->format->BytesPerPixel,
	 manip, scale);
	UpdateRect(display_surface, scale * x, scale * y, scale * w, scale * h);
}
