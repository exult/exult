/*
Copyright (C) 2005 The Pentagram Team
Copyright (C) 2010-2022 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "pent_include.h"

#include "BilinearScalerInternal.h"
#include "manip.h"

namespace Pentagram { namespace BilinearScaler {

	template <
			class uintX, class Manip, class uintS,
			typename limit_t = std::nullptr_t>
	// Aspect Correct 2x Scale of incoming 2x6 block of pixels to 2x12
	BSI_FORCE_INLINE void Interpolate6x2BlockByX2Y24(
			const uint8* const a, const uint8* const b, const uint8* const c,
			const uint8* const d, const uint8* const e, const uint8* const f,
			const uint8* const g, const uint8* const h, const uint8* const i,
			const uint8* const j, const uint8* const k, const uint8* const l,
			uint8*& pixel, const uint_fast32_t pitch,
			const limit_t limit = nullptr) {
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				a, b, g, h, 256, 128, 256, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				a, b, g, h, 256, 128, (0x500 * 11 / 12) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				a, b, g, h, 256, 128, (0x500 * 10 / 12) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				b, c, h, i, 256, 128, (0x500 * 9 / 12) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				b, c, h, i, 256, 128, (0x500 * 8 / 12) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				c, d, i, j, 256, 128, (0x500 * 7 / 12) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				c, d, i, j, 256, 128, (0x500 * 6 / 12) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				c, d, i, j, 256, 128, (0x500 * 5 / 12) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				d, e, j, k, 256, 128, (0x500 * 4 / 12) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				d, e, j, k, 256, 128, (0x500 * 3 / 12) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				e, f, k, l, 256, 128, (0x500 * 2 / 12) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				e, f, k, l, 256, 128, (0x500 * 1 / 12) & 0xff, pixel, limit);
		pixel += pitch;
	}

	// 2x Blinear Scaler with Aspect Correction
	// It is a modification of the basic 2x scaler.
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_X2Y24(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src) {
		uint_fast32_t tex_w = tex->w, tex_h = tex->h;

		const uint_fast8_t blockwidth = 2;
		// This is the number of lines advanced per xloop
		const uint_fast8_t linesperxloop = 5;
		// And this is the number of actual lines read in each
		// block. The bottom lines of one block is the same as
		// the top line of the block below it
		const uint_fast8_t blockheight = linesperxloop + 1;

		const uint_fast8_t destblockheight = 12;
		const uint_fast8_t destblockwidth  = 2;

		// Number of loops that can run.
		// this is the number of blocks we can safely scale without
		// per pixel checking for buffer overruns

		int numyloops = (sh - 1) / linesperxloop;
		int numxloops = (sw - 1) / blockwidth;

		// Source buffer pointers
		const int    tpitch = tex->pitch / sizeof(uintS);
		const uintS* texel
				= static_cast<uintS*>(tex->pixels) + (sy * tpitch + sx);
		const uintS* xloop_end = texel + 1 + (numxloops * blockwidth);
		const uintS* yloop_end = texel + (numyloops * linesperxloop) * tpitch;

		// Absolute limit of the source buffer. Must not read at or beyondthis
		const uintS* srclimit
				= static_cast<uintS*>(tex->pixels) + (tex_h * tpitch);
		int tex_diff = (tpitch * linesperxloop) - sw;

		// 2*6 Source RGBA Pixel block being scaled. Alpha values are
		// currently ignored. abcdef are a column and ghijkl are the other
		// column. Columns can be used in either order Initializion values are
		// meaningless
		uint8 a[4] = "A", g[4] = "G";
		uint8 b[4] = "B", h[4] = "H";
		uint8 c[4] = "C", i[4] = "I";
		uint8 d[4] = "D", j[4] = "J";
		uint8 e[4] = "E", k[4] = "K";
		uint8 f[4] = "F", l[4] = "L";

		// Absolute limit of dest buffer. Must not write beyond this
		uint8* const dst_limit = pixel + dh * pitch;

		// if no clipping is requested only disable clipping x if the source
		// actually has the neeeded width to safely read the entire line
		// unclipped widths not multple of blockwidth always need clipping
		bool clip_x = true;
		if ((sx + blockwidth + 1 + numxloops * blockwidth) < tex_w && !clamp_src
			&& !(sw % blockwidth)) {
			clip_x = false;
			numxloops++;
			xloop_end = texel + 1 + (numxloops * blockwidth);
			tex_diff--;
		}

		// clip_y
		bool clip_y = true;
		// if request no clamping, check to see if y remains in the bounds of
		// the texture. If it does we can disable clipping on y
		// heights not multple of linesperxloop always need clipping
		if ((sy + blockheight + numyloops * linesperxloop) < tex_h && !clamp_src
			&& !(sh % linesperxloop)) {
			numyloops++;
			clip_y    = false;
			yloop_end = texel + (numyloops * linesperxloop) * tpitch;
		}

		// Check if enough lines for loop. if not then set clip_y and prevent
		// loop Must have AT LEAST blockheight lines to do a loop
		if (texel + (blockheight)*tpitch > srclimit) {
			yloop_end = texel;
			clip_y    = true;
		}

		// Src Loop Y
		while (texel != yloop_end) {
			if (texel > yloop_end) {
				return true;
			}
			// Read first column of 5 lines into abcde
			ReadTexelsV<Manip>(blockheight, texel, tpitch, a, b, c, d, e, f);
			// advance texel pointer by 1 to the next column
			texel++;

			// Src Loop X, loops while there are 2 or more columns available
			assert(xloop_end == (texel + numxloops * blockwidth));
			while (texel != xloop_end) {
				// Read next column of 5 lines into fghij
				ReadTexelsV<Manip>(
						blockheight, texel, tpitch, g, h, i, j, k, l);
				// advance texel pointer by 1 to the next column
				texel++;

				// Interpolate with existing abcde as left and just read fghij
				// as right
				// Generate all dest pixels for The 4 inputsource pixels

				Interpolate6x2BlockByX2Y24<uintX, Manip, uintS>(
						a, b, c, d, e, f, g, h, i, j, k, l, pixel, pitch,
						dst_limit);
				// Move the pixel pointer to the start of the next block
				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;

				// Read next column of 5 lines into abcde
				// Keeping existing fghij from above
				ReadTexelsV<Manip>(
						blockheight, texel, tpitch, a, b, c, d, e, f);
				// advance texel pointer by 1 to the next column
				texel++;

				// Interpolate with existing fghij as left and just read abcde
				// as right Generate all dest pixels for The 4 input source
				// pixels

				Interpolate6x2BlockByX2Y24<uintX, Manip, uintS>(
						g, h, i, j, k, l, a, b, c, d, e, f, pixel, pitch,
						dst_limit);
				// Move the pixel pointer to the start of the next block
				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;
			}

			// Final X (clipping) if  have a source column available
			if (clip_x) {
				// Read last column of 5 lines into fghij
				// if source width is odd we reread the previous column
				if (sw & 1) {
					texel--;
				}
				ReadTexelsV<Manip>(
						blockheight, texel, tpitch, g, h, i, j, k, l);
				texel++;

				// Interpolate abcde as left and fghij as right
				//
				Interpolate6x2BlockByX2Y24<uintX, Manip, uintS>(
						a, b, c, d, e, f, g, h, i, j, k, l, pixel, pitch,
						dst_limit);
				// Move the pixel pointer to the start of the next block
				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;

				// odd widths do not need the second column
				if (!(sw & 1)) {
					Interpolate6x2BlockByX2Y24<uintX, Manip, uintS>(
							g, h, i, j, k, l, g, h, i, j, k, l, pixel, pitch,
							dst_limit);
					// Move the pixel pointer to the start of the next block
					pixel -= pitch * destblockheight;
					pixel += sizeof(uintX) * destblockwidth;
				}
			}
			pixel += (pitch * destblockheight) - (dw * sizeof(uintX));

			texel += tex_diff;
			xloop_end += tpitch * linesperxloop;
		}

		//
		// Final Rows - Clipped to height
		//
		if (clip_y) {
			// Calculate the number of unscaled source lines
			uint_fast8_t lines_remaining = sh % linesperxloop;
			if (lines_remaining == 0) {
				lines_remaining = linesperxloop;
			}

			// If no clamping was requested and we have pixels available, allow
			// reading beyond the source rect
			if (numyloops * linesperxloop + lines_remaining + sy < tex_h
				&& !clamp_src) {
				// It doesn't matter if this is bigger than linesperxloop
				lines_remaining = std::max<uint_fast16_t>(
						255, tex_h - sy - numyloops * linesperxloop);
			}

			// Read column 0 of block but clipped to clipping lines
			a[0] = 0xff;
			a[1] = 0;
			a[2] = 0;
			ReadTexelsV<Manip>(
					lines_remaining, texel, tpitch, a, b, c, d, e, f);
			texel++;

			// Src Loop X
			while (texel != xloop_end) {
				ReadTexelsV<Manip>(
						lines_remaining, texel, tpitch, g, h, i, j, k, l);
				texel++;

				Interpolate6x2BlockByX2Y24<uintX, Manip, uintS>(
						a, b, c, d, e, f, g, h, i, j, k, l, pixel, pitch,
						dst_limit);
				// Move the pixel pointer to the start of the next block
				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;

				ReadTexelsV<Manip>(
						lines_remaining, texel, tpitch, a, b, c, d, e, f);
				texel++;
				// Move the pixel pointer to the start of the next block
				Interpolate6x2BlockByX2Y24<uintX, Manip, uintS>(
						g, h, i, j, k, l, a, b, c, d, e, f, pixel, pitch,
						dst_limit);

				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;
			};

			// Final X (clipping) if have a source column available
			//
			if (clip_x) {
				// Read last column of 5 lines into fghij
				if (sw & 1) {
					// if source width is odd go back a column so we re-read the
					// column and do not exceed bounds
					texel--;
				}
				ReadTexelsV<Manip>(
						lines_remaining, texel, tpitch, g, h, i, j, k, l);
				texel++;

				Interpolate6x2BlockByX2Y24<uintX, Manip, uintS>(
						a, b, c, d, e, f, g, h, i, j, k, l, pixel, pitch,
						dst_limit);

				// Move the pixel pointer to the start of the next block
				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;

				if (!(sw & 1)) {
					Interpolate6x2BlockByX2Y24<uintX, Manip, uintS>(
							g, h, i, j, k, l, g, h, i, j, k, l, pixel, pitch,
							dst_limit);
				}
			}
		}

		return true;
	}

	InstantiateBilinearScalerFunc(BilinearScalerInternal_X2Y24);

}}    // namespace Pentagram::BilinearScaler
