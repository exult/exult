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
#include "PointScaler.h"
#include "manip.h"
#include "ignore_unused_variable_warning.h"

namespace Pentagram {

// Very very simple point scaler
template<class uintX, class Manip, class uintS = uintX> class PointScalerInternal {
public:
	static bool Scale(SDL_Surface *tex  , sint32 sx, sint32 sy, sint32 sw, sint32 sh,
	                  uint8 *pixel, sint32 dw, sint32 dh, sint32 pitch, bool clamp_src) {
		ignore_unused_variable_warning(clamp_src);
		// Source buffer pointers
		int tpitch = tex->pitch / sizeof(uintS);
		uintS *texel = static_cast<uintS *>(tex->pixels) + (sy * tpitch + sx);
		uintS *tline_end = texel + sw;
		uintS *tex_end = texel + sh * tpitch;
		int tex_diff = tpitch - sw;


		// First detect integer up scalings, since they are 'easy'
		bool x_intscale = ((dw / sw) * sw) == dw;
		bool y_intscale = ((dh / sh) * sh) == dh;

		//
		// 2x
		//
		if ((sw * 2 == dw) && (sh * 2 == dh)) {
			uint8 *pixel2 = pixel + pitch;
			int p_diff    = (pitch * 2) - (dw * sizeof(uintX));

			// Src Loop Y
			do {
				// Src Loop X
				do {
					uintX p = Manip::copy(*texel);

					*(reinterpret_cast<uintX *>(pixel + 0)) = p;
					*(reinterpret_cast<uintX *>(pixel + sizeof(uintX))) = p;
					*(reinterpret_cast<uintX *>(pixel2 + 0)) = p;
					*(reinterpret_cast<uintX *>(pixel2 + sizeof(uintX))) = p;
					pixel  += sizeof(uintX) * 2;
					pixel2 += sizeof(uintX) * 2;
					texel++;
				} while (texel != tline_end);

				pixel  += p_diff;
				pixel2 += p_diff;

				texel += tex_diff;
				tline_end += tpitch;
			} while (texel != tex_end);

		}
		//
		// Integer scaling, x and y
		//
		else if (x_intscale && y_intscale) {
			int xf = dw / sw;
			int yf = dh / sh;

			uint8 *px_end = pixel + xf * sizeof(uintX);
			uint8 *py_end = pixel + yf * pitch;

			int block_w = xf * sizeof(uintX);
			int block_h = pitch * yf;
			int block_xdiff = pitch - block_w;
			int p_diff = block_h - dw * sizeof(uintX);

			// Src Loop Y
			do {
				// Src Loop X
				do {
					uintX p = Manip::copy(*texel);

					//
					// Inner loops
					//

					// Dest Loop Y
					do {
						// Dest Loop X
						do {
							*(reinterpret_cast<uintX *>(pixel)) = p;
							pixel += sizeof(uintX);
						} while (pixel != px_end);

						pixel += block_xdiff;
						px_end += pitch;
					} while (pixel != py_end);

					pixel  += block_w - block_h;
					px_end += block_w - block_h;
					py_end += block_w;
					texel++;
				} while (texel != tline_end);

				pixel += p_diff;
				py_end += p_diff;
				px_end += p_diff;

				texel += tex_diff;
				tline_end += tpitch;
			} while (texel != tex_end);

		}
		//
		// 2x X and Arbitrary Upscaled Y
		// Specifically to handle 320x200 -> 640x480
		//
		else if ((sw * 2 == dw) && (dh >= sh)) {
			uint32 pos_y;
			uint32 end_y = dh;
			uint32 dst_y = 0;
			uint8 *next_block = nullptr;

			// Src Loop Y
			do {
				next_block = pixel;

				// Src Loop X
				do {
					pos_y = dst_y;

					uintX p = Manip::copy(*texel);

					//
					// Inner loops
					//
					pixel = next_block;
					next_block = next_block + sizeof(uintX) * 2;

					// Dest Loop Y
					do {
						*(reinterpret_cast<uintX *>(pixel + 0)) = p;
						*(reinterpret_cast<uintX *>(pixel + sizeof(uintX))) = p;
						pixel += pitch;
						pos_y += sh;
					} while (pos_y < end_y);

					texel++;
				} while (texel != tline_end);

				pixel -= sizeof(uintX) * (dw - 2);
				dst_y = pos_y;
				end_y += dh;

				texel += tex_diff;
				tline_end += tpitch;
			} while (texel != tex_end);
		}
		//
		// 1x X and Arbitrary Upscaled Y
		// Specifically to handle 640x400 -> 640x480
		//
		else if ((sw == dw) && (dh >= sh)) {
			uint32 pos_y;
			uint32 end_y = dh;
			uint32 dst_y = 0;
			uint8 *next_block = nullptr;

			// Src Loop Y
			do {
				next_block = pixel;

				// Src Loop X
				do {
					pos_y = dst_y;

					uintX p = Manip::copy(*texel);

					//
					// Inner loops
					//
					pixel = next_block;
					next_block = next_block + sizeof(uintX);

					// Dest Loop Y
					do {
						*(reinterpret_cast<uintX *>(pixel)) = p;
						pixel += pitch;
						pos_y += sh;
					} while (pos_y < end_y);

					texel++;
				} while (texel != tline_end);

				pixel -= sizeof(uintX) * (dw - 1);
				dst_y = pos_y;
				end_y += dh;

				texel += tex_diff;
				tline_end += tpitch;
			} while (texel != tex_end);
		}
		//
		// Arbitrary scaling X and Y (optimized for upscaling)
		//
		else {
			uint32 pos_y = 0;
			uint32 pos_x = 0;
			uint32 end_y = dh;
			uint32 dst_y = 0;
			uint8 *blockline_start = nullptr;
			uint8 *next_block = nullptr;

			// Src Loop Y
			do {
				uint32 end_x = dw;
				uint32 dst_x = 0;

				next_block = pixel;

				// Src Loop X
				do {
					pos_y = dst_y;

					uintX p = Manip::copy(*texel);

					//
					// Inner loops
					//
					blockline_start = next_block;
					next_block = nullptr;

					// Dest Loop Y
					while (pos_y < end_y) {
						pos_x = dst_x;
						pixel = blockline_start;

						// Dest Loop X
						while (pos_x < end_x) {
							*(reinterpret_cast<uintX *>(pixel)) = p;

							pixel += sizeof(uintX);
							pos_x += sw;
						}
						if (!next_block) next_block = pixel;

						blockline_start += pitch;

						pos_y += sh;
					}

					dst_x = pos_x;
					end_x += dw;
					texel++;
				} while (texel != tline_end);

				pixel += pitch - sizeof(uintX) * (dw);

				dst_y = pos_y;
				end_y += dh;

				texel += tex_diff;
				tline_end += tpitch;
			} while (texel != tex_end);
		}

		return true;
	}

};


PointScaler::PointScaler() {
	Scale8To8 = PointScalerInternal<uint8, Manip8to8, uint8>::Scale;
	Scale8To32 = PointScalerInternal<uint32, Manip8to32, uint8>::Scale;
	Scale32To32 = PointScalerInternal<uint32, Manip32to32, uint32>::Scale;

	Scale8To16 = PointScalerInternal<uint16, Manip8to16, uint8>::Scale;
	Scale8To555 = PointScalerInternal<uint16, Manip8to555, uint8>::Scale;
	Scale8To565 = PointScalerInternal<uint16, Manip8to565, uint8>::Scale;

	Scale16To16 = PointScalerInternal<uint16, Manip16to16, uint16>::Scale;
	Scale555To555 = PointScalerInternal<uint16, Manip555to555, uint16>::Scale;
	Scale565To565 = PointScalerInternal<uint16, Manip565to565, uint16>::Scale;
}

uint32 PointScaler::ScaleBits() const {
	return 0xFFFFFFFF;
}
bool PointScaler::ScaleArbitrary() const {
	return true;
}

const char *PointScaler::ScalerName() const {
	return "point";
}
const char *PointScaler::ScalerDesc() const {
	return "Fast Simple Point Scaler";
}
const char *PointScaler::ScalerCopyright() const {
	return "Copyright (C) 2005 The Pentagram Team, 2010 The Exult Team";
}


}
