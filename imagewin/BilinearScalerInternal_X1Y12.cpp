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
	// Scale incoming 1x6 block of pixels to aspect corrected 1x6
	BSI_FORCE_INLINE void Interpolate1x6BlockByX1Y12(
			const uint8* const a, const uint8* const b, const uint8* const c,
			const uint8* const d, const uint8* const e, const uint8* const f,
			uint8*& pixel, const uint_fast32_t pitch,
			const limit_t limit = nullptr) {
		// A nothing source pixel. This can be anything
		// Using all zeros so the compiler has an easier time optimizing away
		// the interpolation calculations after inlining
		const uint8 n[] = {0, 0, 0};

		Interpolate2x2BlockTo1<uintX, Manip, uintS>(
				a, b, n, n, 256, 256, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo1<uintX, Manip, uintS>(
				a, b, n, n, 256, (0x500 * 5 / 6) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo1<uintX, Manip, uintS>(
				b, c, n, n, 256, (0x500 * 4 / 6) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo1<uintX, Manip, uintS>(
				c, d, n, n, 256, (0x500 * 3 / 6) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo1<uintX, Manip, uintS>(
				d, e, n, n, 256, (0x500 * 2 / 6) & 0xff, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo1<uintX, Manip, uintS>(
				e, f, n, n, 256, (0x500 * 1 / 6) & 0xff, pixel, limit);
		pixel += pitch;
	}

	// 2x Blinear Scaler with Aspect Correction
	// It is a modification of the 2xY2.4 scaler.
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_X1Y12(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src) {
		uint_fast32_t tex_h = tex->h;

		const uint_fast8_t blockwidth       = 1;
		const uint_fast8_t blocks_per_xloop = 2;
		const uint_fast8_t texels_per_xloop = blockwidth * blocks_per_xloop;
		// This is the number of lines advanced per xloop
		const uint_fast8_t blockheight = 5;

		// And this is the number of actual lines read in each
		// block. The bottom lines of one block is the same as
		// the top line of the block below it
		const uint_fast8_t actualblockheight = blockheight + 1;

		const uint_fast8_t destblockwidth  = 1;
		const uint_fast8_t destblockheight = 6;

		// Number of loops that can run.
		// this is the number of blocks we can safely scale without
		// per pixel checking for buffer overruns

		int numyloops = (sh - 1) / blockheight;
		int numxloops = (sw - 1) / texels_per_xloop;

		// Source buffer pointers
		const int    tpitch = tex->pitch / sizeof(uintS);
		const uintS* texel
				= static_cast<uintS*>(tex->pixels) + (sy * tpitch + sx);
		const uintS* xloop_end = texel + 1 + (numxloops * texels_per_xloop);
		const uintS* yloop_end = texel + (numyloops * blockheight) * tpitch;

		// Absolute limit of the source buffer. Must not read at or beyondthis
		const uintS* srclimit
				= static_cast<uintS*>(tex->pixels) + (tex_h * tpitch);
		int tex_diff = (tpitch * blockheight) - sw;

		// 1*6 Source RGBA Pixel block being scaled. Alpha values are
		// currently ignored.
		// Initializion values are meaningless
		uint8 a[4] = "A";
		uint8 b[4] = "B";
		uint8 c[4] = "C";
		uint8 d[4] = "D";
		uint8 e[4] = "E";
		uint8 f[4] = "F";

		// Absolute limit of dest buffer. Must not write beyond this
		// We only need to use this when performing final y clipping
		uint8* const dst_limit = pixel + dh * pitch;

		// No x clipping is needed if width is even
		bool clip_x = true;
		if (!(sw % texels_per_xloop)) {
			clip_x = false;
			numxloops++;
			xloop_end = texel + (numxloops * texels_per_xloop);
		}

		// clip_y
		bool clip_y = true;
		// if request no clamping, check to see if y remains in the bounds of
		// the texture. If it does we can disable clipping on y
		// heights not multple of blockheight always need clipping
		if ((sy + actualblockheight + numyloops * blockheight) < tex_h
			&& !clamp_src && !(sh % blockheight)) {
			numyloops++;
			clip_y    = false;
			yloop_end = texel + (numyloops * blockheight) * tpitch;
		}

		// Check if enough lines for loop. if not then set clip_y and prevent
		// loop Must have AT LEAST actualblockheight lines to do a loop
		if (texel + actualblockheight * tpitch > srclimit) {
			yloop_end = texel;
			clip_y    = true;
		}

		// Src Loop Y
		while (texel != yloop_end) {
			// Src Loop X, loops while there are 2 or more columns available
			assert(xloop_end == (texel + numxloops * texels_per_xloop));
			while (texel != xloop_end) {
				// Read next column of 5
				ReadTexelsV<Manip>(
						actualblockheight, texel, tpitch, a, b, c, d, e, f);
				// advance texel pointer by 1 to the next column
				texel++;

				Interpolate1x6BlockByX1Y12<uintX, Manip, uintS>(
						a, b, c, d, e, f, pixel, pitch, dst_limit);

				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;

				// Read next column of 5
				ReadTexelsV<Manip>(
						actualblockheight, texel, tpitch, a, b, c, d, e, f);
				// advance texel pointer by 1 to the next column
				texel++;

				Interpolate1x6BlockByX1Y12<uintX, Manip, uintS>(
						a, b, c, d, e, f, pixel, pitch, dst_limit);
				// Move the pixel pointer to the start of the next block
				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;
			}

			// Final X (clipping) Doesn't actually do any
			// clipping just reads and scales the final column if the width is
			// odd
			if (clip_x) {
				ReadTexelsV<Manip>(
						actualblockheight, texel, tpitch, a, b, c, d, e, f);
				texel++;

				// Interpolate abcde as left and fghij as right
				//
				Interpolate1x6BlockByX1Y12<uintX, Manip, uintS>(
						a, b, c, d, e, f, pixel, pitch, dst_limit);
				// Move the pixel pointer to the start of the next block
				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;
			}
			pixel += (pitch * destblockheight) - (dw * sizeof(uintX));

			texel += tex_diff;
			xloop_end += tpitch * blockheight;
		}

		//
		// Final Rows - Clipped to height
		//
		if (clip_y) {
			// Calculate the number of unscaled source lines
			uint_fast8_t lines_remaining = sh % blockheight;
			if (lines_remaining == 0) {
				lines_remaining = blockheight;
			}

			// If no clamping was requested and we have pixels available, allow
			// reading beyond the source rect
			if (numyloops * blockheight + lines_remaining + sy < tex_h
				&& !clamp_src) {
				// It doesn't matter if this is bigger than blockheight
				lines_remaining = std::max<uint_fast16_t>(
						255, tex_h - sy - numyloops * blockheight);
			}

			// Src Loop X
			while (texel != xloop_end) {
				ReadTexelsV<Manip>(
						lines_remaining, texel, tpitch, a, b, c, d, e, f);
				texel++;

				Interpolate1x6BlockByX1Y12<uintX, Manip, uintS>(
						a, b, c, d, e, f, pixel, pitch, dst_limit);
				// Move the pixel pointer to the start of the next block
				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;

				ReadTexelsV<Manip>(
						lines_remaining, texel, tpitch, a, b, c, d, e, f);
				texel++;

				Interpolate1x6BlockByX1Y12<uintX, Manip, uintS>(
						a, b, c, d, e, f, pixel, pitch, dst_limit);

				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;
			};

			// Final X (clipping) Doesn't actually do any
			// clipping just reads and scales the final column if the width is
			// odd
			if (clip_x) {
				ReadTexelsV<Manip>(
						lines_remaining, texel, tpitch, a, b, c, d, e, f);
				texel++;

				Interpolate1x6BlockByX1Y12<uintX, Manip, uintS>(
						a, b, c, d, e, f, pixel, pitch, dst_limit);
			}
		}

		return true;
	}

	InstantiateBilinearScalerFunc(BilinearScalerInternal_X1Y12);

}}    // namespace Pentagram::BilinearScaler
