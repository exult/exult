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
#include "ignore_unused_variable_warning.h"
#include "manip.h"

namespace Pentagram { namespace BilinearScaler {

	template <
			class uintX, class Manip, class uintS,
			typename limit_t = std::nullptr_t>
	// Generate 2x2 pixels from source 2x2 block scaling up by 2x
	BSI_FORCE_INLINE void Interpolate2x2BlockBy2x(
			const uint8* const tl, const uint8* const bl, const uint8* const tr,
			const uint8* const br, uint8*& pixel, const uint_fast32_t pitch,
			const limit_t limit = nullptr) {
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				tl, bl, tr, br, 256, 128, 256, pixel, limit);
		pixel += pitch;
		Interpolate2x2BlockTo2x1<uintX, Manip, uintS>(
				tl, bl, tr, br, 256, 128, 128, pixel, limit);
		pixel += pitch;
	}

	// 2x Blinear Scaler
	// 2x scaler is a specialization of Arb. It works almost identically to Arb
	// But instead of using Interpolate2x2BlockByAny it uses
	// Interpolate2x2BlockBy2x without fixed point values used to calculate the
	// filtering coefficents
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_2x(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src) {
		uint_fast32_t      tex_w = tex->w, tex_h = tex->h;
		const uint_fast8_t blockwidth = 2;
		// This is the number of lines advanced per xloop
		const uint_fast8_t linesperxloop = 4;
		// And this is the number of actual lines read in each
		// block. The bottom lines of one block is the same as
		// the top line of the block below it
		const uint_fast8_t blockheight = linesperxloop + 1;

		const uint_fast8_t destblockheight = 8;
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
		int tex_diff = (tpitch * 4) - sw;

		// 2*5 Source RGBA Pixel block being scaled. Alpha values are
		// currently ignored. abcde are a column and fghij are the other column.
		// Columns can be used in either order
		// Initializion values are meaningless
		uint8 a[4] = "A", f[4] = "F";
		uint8 b[4] = "B", g[4] = "G";
		uint8 c[4] = "C", h[4] = "H";
		uint8 d[4] = "D", i[4] = "I";
		uint8 e[4] = "E", j[4] = "J";

		// Absolute limit of dest buffer. Must not write beyond this
		// We only need to use this when performing final y clipping
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
		if (texel + blockheight * tpitch > srclimit) {
			yloop_end = texel;
			clip_y    = true;
		}

		// Src Loop Y
		while (texel != yloop_end) {
			if (texel > yloop_end) {
				return false;
			}
			// Read first column of 5 lines into abcde
			ReadTexelsV<Manip>(blockheight, texel, tpitch, a, b, c, d, e);
			// advance texel pointer by 1 to the next column
			texel++;

			// Src Loop X, loops while there are 2 or more columns available
			assert(xloop_end == (texel + numxloops * blockwidth));
			while (texel != xloop_end) {
				// Read next column of 5 lines into fghij
				ReadTexelsV<Manip>(blockheight, texel, tpitch, f, g, h, i, j);
				// advance texel pointer by 1 to the next column
				texel++;

				// Interpolate with existing abcde as left and just read fghij
				// as right
				// Generate all dest pixels for The 4 inputsource pixels

				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						a, b, f, g, pixel, pitch);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						b, c, g, h, pixel, pitch);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						c, d, h, i, pixel, pitch);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						d, e, i, j, pixel, pitch);

				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;

				// Read next column of 5 lines into abcde
				// Keeping existing fghij from above
				ReadTexelsV<Manip>(blockheight, texel, tpitch, a, b, c, d, e);
				// advance texel pointer by 1 to the next column
				texel++;

				// Interpolate with existing fghij as left and just read abcde
				// as right Generate all dest pixels for The 4 input source
				// pixels

				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						f, g, a, b, pixel, pitch);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						g, h, b, c, pixel, pitch);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						h, i, c, d, pixel, pitch);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						i, j, d, e, pixel, pitch);

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
				ReadTexelsV<Manip>(blockheight, texel, tpitch, f, g, h, i, j);
				texel++;

				// Interpolate abcde as left and fghij as right
				//
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						a, b, f, g, pixel, pitch);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						b, c, g, h, pixel, pitch);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						c, d, h, i, pixel, pitch);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						d, e, i, j, pixel, pitch);

				// Move the pixel pointer to the start of the next block
				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;

				// odd widths do not need the second column
				if (!(sw & 1)) {
					Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
							f, g, f, g, pixel, pitch);
					Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
							g, h, g, h, pixel, pitch);
					Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
							h, i, h, i, pixel, pitch);
					Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
							i, j, i, j, pixel, pitch);
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
			ReadTexelsV<Manip>(lines_remaining, texel, tpitch, a, b, c, d, e);
			texel++;

			// Src Loop X
			while (texel != xloop_end) {
				ReadTexelsV<Manip>(
						lines_remaining, texel, tpitch, f, g, h, i, j);
				texel++;

				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						a, b, f, g, pixel, pitch, dst_limit);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						b, c, g, h, pixel, pitch, dst_limit);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						c, d, h, i, pixel, pitch, dst_limit);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						d, e, i, j, pixel, pitch, dst_limit);
				// Move the pixel pointer to the start of the next block
				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * 2;

				ReadTexelsV<Manip>(
						lines_remaining, texel, tpitch, a, b, c, d, e);
				texel++;

				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						f, g, a, b, pixel, pitch, dst_limit);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						g, h, b, c, pixel, pitch, dst_limit);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						h, i, c, d, pixel, pitch, dst_limit);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						i, j, d, e, pixel, pitch, dst_limit);
				// Move the pixel pointer to the start of the next block
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
						lines_remaining, texel, tpitch, f, g, h, i, j);
				texel++;

				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						a, b, f, g, pixel, pitch, dst_limit);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						b, c, g, h, pixel, pitch, dst_limit);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						c, d, h, i, pixel, pitch, dst_limit);
				Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
						d, e, i, j, pixel, pitch, dst_limit);
				// Move the pixel pointer to the start of the next block
				pixel -= pitch * destblockheight;
				pixel += sizeof(uintX) * destblockwidth;

				if (!(sw & 1)) {
					Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
							f, g, f, g, pixel, pitch, dst_limit);
					Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
							g, h, g, h, pixel, pitch, dst_limit);
					Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
							h, i, h, i, pixel, pitch, dst_limit);
					Interpolate2x2BlockBy2x<uintX, Manip, uintS>(
							i, j, i, j, pixel, pitch, dst_limit);
				}
			}
		}

		return true;
	}

	InstantiateBilinearScalerFunc(BilinearScalerInternal_2x);

}}    // namespace Pentagram::BilinearScaler
