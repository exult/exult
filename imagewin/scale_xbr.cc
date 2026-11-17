/*
 * xBR algorithm by Hyllian
 * Based on HqMAME version by Zenju
 * Initial Exult version by Marzo Sette Torres Junior
 *
 * Copyright (C) 2011, 2012 - Hyllian/Jararaca
 * Copyright (C) 2012 - Zenju
 * Copyright (C) 2012 - Marzo Sette Torres Junior
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifdef USE_XBR_SCALER

#	include "common_types.h"
#	include "imagewin.h"
#	include "manip.h"
#	include "scale_xbr.h"

#	include <cstdlib>
#	include <cstring>

//
// 2xBR Filtering
//
void Image_window::show_scaled8to16_2xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format     = SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette*                  paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface);
	const Manip8to16              manip(paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to16, Scaler2xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band, y + guard_band, w, h, ibuf->line_width,
			ibuf->height + guard_band, static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel, manip);
}

void Image_window::show_scaled8to555_2xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format     = SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette*                  paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface);
	const Manip8to555             manip(paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to555, Scaler2xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band, y + guard_band, w, h, ibuf->line_width,
			ibuf->height + guard_band, static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel, manip);
}

void Image_window::show_scaled8to565_2xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format     = SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette*                  paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface);
	const Manip8to565             manip(paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to565, Scaler2xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band, y + guard_band, w, h, ibuf->line_width,
			ibuf->height + guard_band, static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel, manip);
}

void Image_window::show_scaled8to32_2xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format     = SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette*                  paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface);
	const Manip8to32              manip(paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint32, Manip8to32, Scaler2xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band, y + guard_band, w, h, ibuf->line_width,
			ibuf->height + guard_band, static_cast<uint32*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel, manip);
}

//
// 3xBR Filtering
//
void Image_window::show_scaled8to16_3xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format     = SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette*                  paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface);
	const Manip8to16              manip(paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to16, Scaler3xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band, y + guard_band, w, h, ibuf->line_width,
			ibuf->height + guard_band, static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel, manip);
}

void Image_window::show_scaled8to555_3xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format     = SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette*                  paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface);
	const Manip8to555             manip(paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to555, Scaler3xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band, y + guard_band, w, h, ibuf->line_width,
			ibuf->height + guard_band, static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel, manip);
}

void Image_window::show_scaled8to565_3xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format     = SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette*                  paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface);
	const Manip8to565             manip(paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to565, Scaler3xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band, y + guard_band, w, h, ibuf->line_width,
			ibuf->height + guard_band, static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel, manip);
}

void Image_window::show_scaled8to32_3xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format     = SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette*                  paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface);
	const Manip8to32              manip(paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint32, Manip8to32, Scaler3xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band, y + guard_band, w, h, ibuf->line_width,
			ibuf->height + guard_band, static_cast<uint32*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel, manip);
}

//
// 4xBR Filtering
//
void Image_window::show_scaled8to16_4xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format     = SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette*                  paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface);
	const Manip8to16              manip(paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to16, Scaler4xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band, y + guard_band, w, h, ibuf->line_width,
			ibuf->height + guard_band, static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel, manip);
}

void Image_window::show_scaled8to555_4xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format     = SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette*                  paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface);
	const Manip8to555             manip(paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to555, Scaler4xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band, y + guard_band, w, h, ibuf->line_width,
			ibuf->height + guard_band, static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel, manip);
}

void Image_window::show_scaled8to565_4xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format     = SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette*                  paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface);
	const Manip8to565             manip(paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to565, Scaler4xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band, y + guard_band, w, h, ibuf->line_width,
			ibuf->height + guard_band, static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel, manip);
}

void Image_window::show_scaled8to32_4xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format     = SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette*                  paletted_surface_palette = SDL_GetSurfacePalette(paletted_surface);
	const Manip8to32              manip(paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint32, Manip8to32, Scaler4xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band, y + guard_band, w, h, ibuf->line_width,
			ibuf->height + guard_band, static_cast<uint32*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel, manip);
}

#endif    // USE_XBR_SCALER
