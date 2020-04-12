/*
Copyright (C) 2005 The Pentagram Team
Copyright (C) 2010 The Exult Team

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

namespace Pentagram {

template<class uintX, class Manip, class uintS>
bool BilinearScalerInternal_Arb(SDL_Surface *tex, sint32 sx, sint32 sy, sint32 sw, sint32 sh,
                                uint8 *pixel, sint32 dw, sint32 dh, sint32 pitch, bool clamp_src) {
	// Source buffer pointers
	int tpitch = tex->pitch / sizeof(uintS);
	uintS *texel = static_cast<uintS *>(tex->pixels) + (sy * tpitch + sx);
	uintS *tline_end = texel + (sw - 1);
	uintS *tex_end = texel + (sh - 4) * tpitch;
	int tex_diff = (tpitch * 4) - sw;

	uint8 a[4];
	uint8 b[4];
	uint8 c[4];
	uint8 d[4];
	uint8 e[4];
	uint8 f[4];
	uint8 g[4];
	uint8 h[4];
	uint8 i[4];
	uint8 j[4];

	uint32 pos_y = 0;
	uint32 pos_x = 0;

	uint32 add_y = (sh << 16) / dh;
	uint32 add_x = (sw << 16) / dw;

	uint32 start_x = (sw << 16) - (add_x * dw);
	uint32 dst_y = (sh << 16) - (add_y * dh);
	uint32 end_y = 1 << 16;

	if (sw == dw * 2) start_x += 0x8000;
	if (sh == dh * 2) dst_y += 0x8000;

	uint8 *blockline_start = nullptr;
	uint8 *next_block = nullptr;

//	uint8* pixel_start = pixel;

	bool clip_x = true;
	if (sw + sx < tpitch && !clamp_src) {
		clip_x = false;
		tline_end = texel + (sw + 1);
		tex_diff--;
	}

	bool clip_y = true;
	if (sh + sy < tex->h && !clamp_src) {
		clip_y = false;
		tex_end = texel + (sh) * tpitch;
	}

	// Src Loop Y
	do {
		Read5(a, b, c, d, e);
		texel++;

		uint32 end_x = 1 << 16;
		uint32 dst_x = start_x;

		next_block = pixel;

		// Src Loop X
		do {
			pos_y = dst_y;

			Read5(f, g, h, i, j);
			texel++;

			blockline_start = next_block;
			next_block = nullptr;

			ArbInnerLoop(a, b, f, g);
			ArbInnerLoop(b, c, g, h);
			ArbInnerLoop(c, d, h, i);
			ArbInnerLoop(d, e, i, j);

			end_y -= 4 << 16;
			dst_x = pos_x;
			end_x += 1 << 16;
			pos_y = dst_y;

			Read5(a, b, c, d, e);
			texel++;

			blockline_start = next_block;
			next_block = nullptr;

			ArbInnerLoop(f, g, a, b);
			ArbInnerLoop(g, h, b, c);
			ArbInnerLoop(h, i, c, d);
			ArbInnerLoop(i, j, d, e);

			end_y -= 4 << 16;
			dst_x = pos_x;
			end_x += 1 << 16;
		} while (texel != tline_end);

		// Final X (clipping)
		if (clip_x) {
			pos_y = dst_y;

			Read5(f, g, h, i, j);
			texel++;

			blockline_start = next_block;
			next_block = nullptr;

			ArbInnerLoop(a, b, f, g);
			ArbInnerLoop(b, c, g, h);
			ArbInnerLoop(c, d, h, i);
			ArbInnerLoop(d, e, i, j);

			end_y -= 4 << 16;
			dst_x = pos_x;
			end_x += 1 << 16;
			pos_y = dst_y;

			blockline_start = next_block;
			next_block = nullptr;

			ArbInnerLoop(f, g, f, g);
			ArbInnerLoop(g, h, g, h);
			ArbInnerLoop(h, i, h, i);
			ArbInnerLoop(i, j, i, j);

			end_y -= 4 << 16;
			dst_x = pos_x;
			end_x += 1 << 16;
		};

		pixel += pitch - sizeof(uintX) * (dw);

		dst_y = pos_y;
		end_y += 4 << 16;

		texel += tex_diff;
		tline_end += tpitch * 4;
	} while (texel != tex_end);


	//
	// Final Rows - Clipping
	//

	// Src Loop Y
	if (clip_y) {
		Read5_Clipped(a, b, c, d, e);
		texel++;

		uint32 end_x = 1 << 16;
		uint32 dst_x = start_x;

		next_block = pixel;

		// Src Loop X
		do {
			pos_y = dst_y;

			Read5_Clipped(f, g, h, i, j);
			texel++;

			blockline_start = next_block;
			next_block = nullptr;

			ArbInnerLoop(a, b, f, g);
			ArbInnerLoop(b, c, g, h);
			ArbInnerLoop(c, d, h, i);
			ArbInnerLoop(d, e, i, j);

			end_y -= 4 << 16;
			dst_x = pos_x;
			end_x += 1 << 16;
			pos_y = dst_y;

			Read5_Clipped(a, b, c, d, e);
			texel++;

			blockline_start = next_block;
			next_block = nullptr;

			ArbInnerLoop(f, g, a, b);
			ArbInnerLoop(g, h, b, c);
			ArbInnerLoop(h, i, c, d);
			ArbInnerLoop(i, j, d, e);

			end_y -= 4 << 16;
			dst_x = pos_x;
			end_x += 1 << 16;
		} while (texel != tline_end);

		// Final X (clipping)
		if (clip_x) {
			pos_y = dst_y;

			Read5_Clipped(f, g, h, i, j);
			texel++;

			blockline_start = next_block;
			next_block = nullptr;

			ArbInnerLoop(a, b, f, g);
			ArbInnerLoop(b, c, g, h);
			ArbInnerLoop(c, d, h, i);
			ArbInnerLoop(d, e, i, j);

			end_y -= 4 << 16;
			dst_x = pos_x;
			end_x += 1 << 16;
			pos_y = dst_y;

			blockline_start = next_block;
			next_block = nullptr;

			ArbInnerLoop(f, g, f, g);
			ArbInnerLoop(g, h, g, h);
			ArbInnerLoop(h, i, h, i);
			ArbInnerLoop(i, j, i, j);

			end_y -= 4 << 16;
			dst_x = pos_x;
			end_x += 1 << 16;
		};
	}


	return true;
}

InstantiateBilinearScalerFunc(BilinearScalerInternal_Arb);

}

