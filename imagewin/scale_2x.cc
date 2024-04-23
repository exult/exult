/*
 * Scale2X algorithm by Andrea Mazzoleni.
 *
 * Copyright (C) 2001-2011 Andrea Mazzoleni
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
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "scale_2x.h"

#include "common_types.h"
#include "imagewin.h"
#include "manip.h"

#include <cstring>

//
// Scale2x (no blurring) by Andrea Mazzoleni.
//
void Image_window::show_scaled8to8_2x_noblur(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to8 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale2x_noblur<uint8, Manip8to8>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint8*>(inter_surface->pixels), inter_surface->pitch,
			manip);
}

void Image_window::show_scaled8to16_2x_noblur(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to16 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale2x_noblur<unsigned char, uint16, Manip8to16>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

void Image_window::show_scaled8to555_2x_noblur(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to555 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale2x_noblur<unsigned char, uint16, Manip8to555>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

void Image_window::show_scaled8to565_2x_noblur(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to565 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale2x_noblur<unsigned char, uint16, Manip8to565>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

void Image_window::show_scaled8to32_2x_noblur(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to32 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale2x_noblur<unsigned char, uint32, Manip8to32>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint32*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}
