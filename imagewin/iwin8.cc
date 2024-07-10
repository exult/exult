/**
 ** Iwin8.h - 8-bit image window.
 **
 ** Written: 8/13/98 - JSF
 **/

/*
Copyright (C) 1998 Jeffrey S. Freedman
Copyright (C) 2000-2022 The Exult Team

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "common_types.h"
#include "gamma.h"
#include "iwin8.h"

#include <algorithm>
#include <climits>
#include <cstring>

using std::make_unique;
using std::rotate;
using std::unique_ptr;

GammaTable<uint8> Image_window8::GammaRed(256);
GammaTable<uint8> Image_window8::GammaBlue(256);
GammaTable<uint8> Image_window8::GammaGreen(256);

Image_window8::Image_window8(
		unsigned int w, unsigned int h, unsigned int gwidth,
		unsigned int gheight, int scl, bool fs, int sclr,
		Image_window::FillMode fillmode, unsigned int fillsclr)
		: Image_window(
				  new Image_buffer8(0, 0, nullptr), w, h, gwidth, gheight, scl,
				  fs, sclr, fillmode, fillsclr) {
	ib8 = static_cast<Image_buffer8*>(ibuf);
}

void Image_window8::get_gamma(double& r, double& g, double& b) {
	r = GammaRed.get_gamma();
	g = GammaGreen.get_gamma();
	b = GammaBlue.get_gamma();
}

void Image_window8::set_gamma(double r, double g, double b) {
	GammaRed.set_gamma(r);
	GammaGreen.set_gamma(g);
	GammaBlue.set_gamma(b);
}

/*
 *  Convert rgb value.
 */

inline unsigned char Get_color8(
		unsigned char val, int maxval,
		int brightness    // 100=normal.
) {
	const uint32 c
			= (static_cast<uint32>(val) * brightness * 255L) / (100 * maxval);
	return c <= 255L ? static_cast<unsigned char>(c) : 255;
}

/*
 *  Set palette.
 */

void Image_window8::set_palette(
		const unsigned char* rgbs,         // 256 3-byte entries.
		int                  maxval,       // Highest val. for each color.
		int                  brightness    // Brightness control (100 = normal).
) {
	// Get the colors.
	SDL_Color colors2[256];
	for (int i = 0; i < 256; i++) {
		colors2[i].r = colors[i * 3]
				= GammaRed[Get_color8(rgbs[3 * i], maxval, brightness)];
		colors2[i].g = colors[i * 3 + 1]
				= GammaGreen[Get_color8(rgbs[3 * i + 1], maxval, brightness)];
		colors2[i].b = colors[i * 3 + 2]
				= GammaBlue[Get_color8(rgbs[3 * i + 2], maxval, brightness)];
	}
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	SDL_SetPaletteColors(paletted_surface_palette, colors2, 0, 256);

	if (paletted_surface != draw_surface) {
		SDL_Palette* draw_surface_palette
				= SDL_GetSurfacePalette(paletted_surface);
		SDL_SetPaletteColors(draw_surface_palette, colors2, 0, 256);
	}
}

/*
 *  Rotate a range of colors.
 */

void Image_window8::rotate_colors(
		int first,    // Palette index of 1st.
		int num,      // # in range.
		int upd       // 1 to update hardware palette.
) {
	first *= 3;
	num *= 3;
	const int      cnt    = abs(num);
	unsigned char* start  = colors + first;
	unsigned char* finish = start + cnt;
	if (num > 0) {
		// Shift upward.
		rotate(start, finish - 3, finish);
	} else {
		// Shift downward.
		rotate(start, start + 3, finish);
	}

	if (upd) {    // Take effect now?
		SDL_Color colors2[256];
		for (int i = 0; i < 256; i++) {
			colors2[i].r = colors[i * 3];
			colors2[i].g = colors[i * 3 + 1];
			colors2[i].b = colors[i * 3 + 2];
		}
		SDL_Palette* paletted_surface_palette
				= SDL_GetSurfacePalette(paletted_surface);
		SDL_SetPaletteColors(paletted_surface_palette, colors2, 0, 256);

		if (paletted_surface != draw_surface) {
			SDL_Palette* draw_surface_palette
					= SDL_GetSurfacePalette(paletted_surface);
			SDL_SetPaletteColors(draw_surface_palette, colors2, 0, 256);
		}
	}
}

static inline int pow2(int x) {
	return x * x;
}

// a nearest-average-colour 1/3 scaler
unique_ptr<unsigned char[]> Image_window8::mini_screenshot() {
	int i;
	if (!paletted_surface) {
		return nullptr;
	}

	auto                 buf    = make_unique<Uint8[]>(96 * 60);
	const int            w      = 3 * 96;
	const int            h      = 3 * 60;
	const unsigned char* pixels = ibuf->get_bits();
	const int            pitch  = ibuf->get_line_width();

	for (int y = 0; y < h; y += 3) {
		for (int x = 0; x < w; x += 3) {
			// calculate average colour
			int r = 0;
			int g = 0;
			int b = 0;
			for (i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					const int pix = pixels
							[pitch * (j + y + (get_game_height() - h) / 2) + i
							 + x + (get_game_width() - w) / 2];
					r += colors[3 * pix + 0];
					g += colors[3 * pix + 1];
					b += colors[3 * pix + 2];
				}
			}
			r = r / 9;
			g = g / 9;
			b = b / 9;

			// find nearest-colour in non-rotating palette
			int bestdist  = INT_MAX;
			int bestindex = -1;
			for (i = 0; i < 224; i++) {
				const int dist = pow2(colors[3 * i + 0] - r)
								 + pow2(colors[3 * i + 1] - g)
								 + pow2(colors[3 * i + 2] - b);
				if (dist < bestdist) {
					bestdist  = dist;
					bestindex = i;
				}
			}
			buf[y * w / 9 + x / 3] = bestindex;
		}
	}

	return buf;
}
