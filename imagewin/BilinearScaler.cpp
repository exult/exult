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

#include "BilinearScaler.h"

#include "BilinearScalerInternal.h"
#include "manip.h"

namespace Pentagram { namespace BilinearScaler {

	template <class uintX, class Manip, class uintS>
	class BilinearScalerInternal {
	public:
		static bool ScaleBilinear(
				SDL_Surface* tex, sint32 sx, sint32 sy, sint32 sw, sint32 sh,
				uint8* pixel, sint32 dw, sint32 dh, sint32 pitch,
				bool clamp_src) {
			//
			// Clip the source rect to the size of tex and adjust dest rect
			// as appropriate
			//
			int_fast32_t tex_w = tex->w, tex_h = tex->h;

			// clip y
			if ((sh + sy) > tex_h) {
				auto nsh = tex_h - sy;
				dh       = (dh * nsh) / sh;
				sh       = nsh;
			}
			// clip x
			if ((sw + sx) > tex_w) {
				auto nsw = tex_w - sx;
				dw       = (dh * nsw) / sw;
				sw       = nsw;
			}

			//
			// Call the correct specialized function as appropriate
			//
			bool result = false;
			// 2x Scaling
			if (!result && (sw * 2 == dw) && (sh * 2 == dh)) {
				result = BilinearScalerInternal_2x<uintX, Manip, uintS>(
						tex, sx, sy, sw, sh, pixel, dw, dh, pitch, clamp_src);
			}
			// 2 X 2.4 Y aka Aspect Correcting 2x
			// This has some implicit requirements.
			// sh must be a multiple of 5 and dh must be a multiple of 12
			// This is pretty much has to be true if source and dest heights are
			// integers Source height scaled by 2.4 only results in an integer
			// if source height is a multiple of 5 and 2.4 multipled by a
			// multiple of 5 is always a multiple of 12
			if (!result && (sw * 2 == dw) && (dh * 5 == sh * 12)) {
				result = BilinearScalerInternal_X2Y24<uintX, Manip, uintS>(
						tex, sx, sy, sw, sh, pixel, dw, dh, pitch, clamp_src);
			}
			// 1 X 1.2 Y aka Aspect Correction with no scaling
			// Same as above there is implicit requiements except dh must be a
			// multiple of 6
			if (!result && (sw == dw) && (dh * 5 == sh * 6) && !(sh % 5)
				&& !(sw % 4)) {
				result = BilinearScalerInternal_X1Y12<uintX, Manip, uintS>(
						tex, sx, sy, sw, sh, pixel, dw, dh, pitch, clamp_src);
			}
			// Arbitrary has no restrictions
			if (!result) {
				return BilinearScalerInternal_Arb<uintX, Manip, uintS>(
						tex, sx, sy, sw, sh, pixel, dw, dh, pitch, clamp_src);
			}

			return result;
		}
	};

	Scaler::Scaler() {
		Scale8To8  = nullptr;
		Scale8To32 = BilinearScalerInternal<
				uint32_t, Manip8to32, uint8>::ScaleBilinear;
		Scale32To32 = BilinearScalerInternal<
				uint32_t, Manip32to32, uint32_t>::ScaleBilinear;

#ifdef COMPILE_ALL_BILINEAR_SCALERS
		Scale8To565 = BilinearScalerInternal<
				uint16, Manip8to565, uint8>::ScaleBilinear;
		Scale8To16 = BilinearScalerInternal<
				uint16, Manip8to16, uint8>::ScaleBilinear;
		Scale8To555 = BilinearScalerInternal<
				uint16, Manip8to555, uint8>::ScaleBilinear;

		Scale16To16 = BilinearScalerInternal<
				uint16, Manip16to16, uint16>::ScaleBilinear;
		Scale555To555 = BilinearScalerInternal<
				uint16, Manip555to555, uint16>::ScaleBilinear;
		Scale565To565 = BilinearScalerInternal<
				uint16, Manip565to565, uint16>::ScaleBilinear;
#endif
	}

	bool Scaler::ScaleArbitrary() const {
		return true;
	}

	const char* Scaler::ScalerName() const {
		return "bilinear";
	}

	const char* Scaler::ScalerDesc() const {
		return "Bilinear Filtering Scaler";
	}

	const char* Scaler::ScalerCopyright() const {
		return "Copyright (C) 2005 The Pentagram Team, 2025 The Exult Team";
	}

}}    // namespace Pentagram::BilinearScaler
