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
	// This takes a 2x2 block and generates the scaled pixels between the texels
	// The fixed point inputs control how many pixels to generate
	// and the needed filtering coefficents
	BSI_FORCE_INLINE uint8* Interpolate2x2BlockByAny(
			const uint8* const tl, const uint8* const bl, const uint8* const tr,
			const uint8* const br, uint8*& blockline_start, uint8*& next_block,
			fixedu1616& pos_y, fixedu1616& pos_x, fixedu1616& end_y,
			const fixedu1616 end_x, const fixedu1616 add_y,
			const fixedu1616 add_x, const fixedu1616 block_start_x,
			const uint_fast32_t pitch, const limit_t limit = nullptr) {
		uint8*     pixel = blockline_start;
		fixedu1616 posy = pos_y, posx = pos_x;
		while (posy < end_y && IsUnclipped(blockline_start, limit)) {
			// reset posx to block_start_x for each row
			posx = block_start_x;
			// Set pixel to blockline_start and increment blockline_start to the
			// next line
			pixel = blockline_start;
			blockline_start += pitch;
			while (posx < end_x && IsUnclipped(pixel, limit)) {
				Interpolate2x2BlockTo1<uintX, Manip, uintS>(
						tl, bl, tr, br, (end_x - posx) >> 8,
						(end_y - posy) >> 8, pixel, limit);
				pixel += sizeof(uintX);
				posx += add_x;
			};
			if (!next_block) {
				next_block = pixel;
			}
			posy += add_y;
		}

		pos_y = posy;
		pos_x = posx;
		end_y += 1 << 16;
		return pixel;
	}

	// Arbitrary Bilinear Scaler
	// It works on blocks of 2x5 source pixels at a time. Uses 16.16 Fixed point
	// to calculate filtering coefficients for each destination pixel
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_Arb(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src) {
		uint_fast32_t tex_w = tex->w, tex_h = tex->h;

		const uint_fast8_t blockwidth = 2;
		// This is the number of lines advanced per xloop
		const uint_fast8_t linesperxloop = 4;
		// And this is the number of actual lines read in each
		// block. The bottom lines of one block is the same as
		// the top line of the block below it
		const uint_fast8_t blockheight = linesperxloop + 1;

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
		uint8* const dst_limit = pixel + dh * pitch;

		// Fixedpoint position increments when advancing dest pixels (this is 1/
		// scalefactor) effectively means how many source pixels are advanced
		// per dest pixel written
		const fixedu1616 add_y = (sh << 16) / dh;
		const fixedu1616 add_x = (sw << 16) / dw;

		// Limits for block being worked on
		// start_x and start_y are calculated backward from the last pixel
		// so the last pixel has no fractional component when
		// filtering and to limit rounding related problems from the calculation
		// of the add values above
		// end_y is the y limit for current block
		// end_x is the x limit for the current block
		// After a block is finished the end values are advanced by 1<<16 that represents 1 source pixel
		fixedu1616 start_x = (sw << 16) - (add_x * dw);
		fixedu1616 start_y = (sh << 16) - (add_y * dh);
		fixedu1616 end_y   = 1 << 16;
		fixedu1616 end_x = 1 << 16;

		// Offset by half a source pixel when doing 0.5x scaling
		// looks a bit better
		if (sw == dw * 2) {
			start_x += 0x8000;
		}
		if (sh == dh * 2) {
			start_y += 0x8000;
		}

		// Current Fixed point position in source buffer 
		// This is avanced by add values from above when a dest pixel is 
		// written
		// Dest pixels will be written while pos_? is less than end_?
		// The filtering coefficents for dest pixels are calculated using the
		// fractional part of end_? - pos_?
		fixedu1616 pos_y = start_y;
		fixedu1616 pos_x = start_x;

		//This is the dest pointer to the start of the current line of the current block
		//This gets set to the value of next_block when advancing to the block to right
		uint8* blockline_start = nullptr;
		// This is the dest pointer of the top left pixel of the next blocck to the right
		uint8* next_block      = nullptr;

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
		fixedu1616    block_start_x = start_x;
		fixedu1616 block_start_y = pos_y;

		while (texel != yloop_end) {
			// Read first column of 5 lines into abcde
			ReadTexelsV<Manip>(blockheight, texel, tpitch, a, b, c, d, e);
			// advance texel pointer by 1 to the next column
			texel++;
			end_x = 1 << 16;
			block_start_x       = start_x;
			block_start_y       = pos_y;

			next_block = pixel;
			// Src Loop X, loops while there are 2 or more columns available
			assert(xloop_end == (texel + numxloops * blockwidth));
			while (texel != xloop_end) {
				pos_y = block_start_y;

				// Read next column of 5 lines into fghij
				ReadTexelsV<Manip>(blockheight, texel, tpitch, f, g, h, i, j);
				// advance texel pointer by 1 to the next column
				texel++;

				blockline_start = next_block;
				next_block      = nullptr;

				// Interpolate with existing abcde as left and just read fghij
				// as right
				// Generate all dest pixels for The 4 inputsource pixels

				// a f
				// b g
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						a, b, f, g, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch);
				// b g
				// c h
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						b, c, g, h, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch);
				// c h
				// d i
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						c, d, h, i, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch);
				// d i
				// e j
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						d, e, i, j, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch);

				end_y -= 4 << 16;
				block_start_x = pos_x;
				end_x += 1 << 16;
				pos_y = block_start_y;

				// Read next column of 5 lines into abcde
				// Keeping existing fghij from above
				ReadTexelsV<Manip>(blockheight, texel, tpitch, a, b, c, d, e);
				// advance texel pointer by 1 to the next column
				texel++;

				blockline_start = next_block;
				next_block      = nullptr;

				// Interpolate with existing fghij as left and just read abcde
				// as right Generate all dest pixels for The 4 input source
				// pixels

				// f a
				// g b
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						f, g, a, b, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch);
				//  g b
				//  h c
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						g, h, b, c, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch);
				// h c
				// i d
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						h, i, c, d, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch);
				// i d
				// j e
				pixel = Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						i, j, d, e, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch);
				end_y -= 4 << 16;
				block_start_x = pos_x;
				end_x += 1 << 16;
			}

			// Final X (clipping) if  have a source column available
			if (clip_x) {
				pos_y = block_start_y;

				// Read last column of 5 lines into fghij
				// if source width is odd we reread the previous column
				if (sw & 1) {
					texel--;
				}
				ReadTexelsV<Manip>(blockheight, texel, tpitch, f, g, h, i, j);
				texel++;

				blockline_start = next_block;
				next_block      = nullptr;

				// Interpolate abcde as left and fghij as right
				//
				// a f
				// b g
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						a, b, f, g, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch);
				// b g
				// c h
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						b, c, g, h, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch);
				// c h
				// d i
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						c, d, h, i, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch);
				// d i
				// e j
				pixel = Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						d, e, i, j, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch);

				block_start_x = pos_x;
				end_x += 1 << 16;

				assert(next_block);
				// odd widths do not need the second column
				blockline_start = next_block;
				end_y -= 4 << 16;
				if (!(sw & 1)) {
					pos_y      = block_start_y;
					next_block = nullptr;
					// Interpolate with fghij as both columns, duplicates right
					// most source column into right edge of destination but
					// still interpolates vertically

					// f f
					// g g
					Interpolate2x2BlockByAny<uintX, Manip, uintS>(
							f, g, f, g, blockline_start, next_block, pos_y,
							pos_x, end_y, end_x, add_y, add_x, block_start_x,
							pitch);
					// g g
					// h h
					Interpolate2x2BlockByAny<uintX, Manip, uintS>(
							g, h, g, h, blockline_start, next_block, pos_y,
							pos_x, end_y, end_x, add_y, add_x, block_start_x,
							pitch);
					// h h
					// i i
					Interpolate2x2BlockByAny<uintX, Manip, uintS>(
							h, i, h, i, blockline_start, next_block, pos_y,
							pos_x, end_y, end_x, add_y, add_x, block_start_x,
							pitch);
					// i i
					// j j
					pixel = Interpolate2x2BlockByAny<uintX, Manip, uintS>(
							i, j, i, j, blockline_start, next_block, pos_y,
							pos_x, end_y, end_x, add_y, add_x, block_start_x,
							pitch);
					end_y -= 4 << 16;
					block_start_x = pos_x;
					end_x += 1 << 16;
				}
			}
			assert(next_block);

			pixel += pitch - sizeof(uintX) * (dw);

			block_start_y = pos_y;
			end_y += 4 << 16;
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
			ReadTexelsV<Manip>(lines_remaining, texel, tpitch, a, b, c, d, e);
			texel++;

			end_x         = 1 << 16;
			block_start_x = start_x;

			next_block = pixel;

			// Src Loop X
			while (texel != xloop_end) {
				pos_y = block_start_y;

				ReadTexelsV<Manip>(
						lines_remaining, texel, tpitch, f, g, h, i, j);
				texel++;

				blockline_start = next_block ? next_block : pixel;
				next_block      = nullptr;

				// a f
				// b g
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						a, b, f, g, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch,
						dst_limit);
				// b g
				// c h
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						b, c, g, h, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch,
						dst_limit);
				// c h
				// d i
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						c, d, h, i, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch,
						dst_limit);

				// d i
				// e j
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						d, e, i, j, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch,
						dst_limit);

				end_y -= 4 << 16;
				block_start_x = pos_x;
				end_x += 1 << 16;
				pos_y = block_start_y;

				ReadTexelsV<Manip>(
						lines_remaining, texel, tpitch, a, b, c, d, e);
				texel++;

				blockline_start = next_block ? next_block : pixel;
				next_block      = nullptr;

				// f a
				// g b
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						f, g, a, b, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch,
						dst_limit);
				//  g b
				//  h c
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						g, h, b, c, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch,
						dst_limit);
				// h c
				// i d
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						h, i, c, d, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch,
						dst_limit);
				// i d
				// j e
				pixel = Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						i, j, d, e, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch,
						dst_limit);

				end_y -= 4 << 16;
				block_start_x = pos_x;
				end_x += 1 << 16;
			};

			// Final X (clipping) if have a source column available
			//
			if (clip_x) {
				pos_y = block_start_y;

				// Read last column of 5 lines into fghij
				if (sw & 1) {
					// if source width is odd go back a column so we re-read the
					// column and do not exceed bounds
					texel--;
				}
				ReadTexelsV<Manip>(
						lines_remaining, texel, tpitch, f, g, h, i, j);
				texel++;

				blockline_start = next_block ? next_block : pixel;
				next_block      = nullptr;

				// a f
				// b g
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						a, b, f, g, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch,
						dst_limit);
				// b g
				// c h
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						b, c, g, h, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch,
						dst_limit);
				// c h
				// d i
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						c, d, h, i, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch,
						dst_limit);
				// d i
				// e j
				Interpolate2x2BlockByAny<uintX, Manip, uintS>(
						d, e, i, j, blockline_start, next_block, pos_y, pos_x,
						end_y, end_x, add_y, add_x, block_start_x, pitch,
						dst_limit);

				end_y -= 4 << 16;
				block_start_x = pos_x;
				end_x += 1 << 16;
				pos_y = block_start_y;

				blockline_start = next_block;
				next_block      = nullptr;

				if (!(sw & 1)) {
					// f f
					// g g
					Interpolate2x2BlockByAny<uintX, Manip, uintS>(
							f, g, f, g, blockline_start, next_block, pos_y,
							pos_x, end_y, end_x, add_y, add_x, block_start_x,
							pitch, dst_limit);
					// g g
					// h h
					Interpolate2x2BlockByAny<uintX, Manip, uintS>(
							g, h, g, h, blockline_start, next_block, pos_y,
							pos_x, end_y, end_x, add_y, add_x, block_start_x,
							pitch, dst_limit);
					// h h
					// i i
					Interpolate2x2BlockByAny<uintX, Manip, uintS>(
							h, i, h, i, blockline_start, next_block, pos_y,
							pos_x, end_y, end_x, add_y, add_x, block_start_x,
							pitch, dst_limit);
					// i i
					// j j
					Interpolate2x2BlockByAny<uintX, Manip, uintS>(
							i, j, i, j, blockline_start, next_block, pos_y,
							pos_x, end_y, end_x, add_y, add_x, block_start_x,
							pitch, dst_limit);
				}
			}
		}

		return true;
	}

	InstantiateBilinearScalerFunc(BilinearScalerInternal_Arb);

}}    // namespace Pentagram::BilinearScaler
