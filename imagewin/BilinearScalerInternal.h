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

#include "BilinearScaler.h"
#include "manip.h"

#include <cstddef>
#include <cstring>
#include <type_traits>
#define COMPILE_ALL_BILINEAR_SCALERS

// Note BSI in these Macrsos is juat a standin for Bilinear Scaler Internal to
// make sure they do not step on the toes of Macros that may have been defined
// elsewhere
#ifdef _MSC_VER
// MSVC macro for Force inlining a function
#	define BSI_FORCE_INLINE __forceinline
#elif defined(__clang__)
// Clang macro for Force inlining a function
#	define BSI_FORCE_INLINE [[clang::always_inline]] inline
#elif defined __GNUC__
// GCC macro for Force inlining a function
#	define BSI_FORCE_INLINE [[gnu::always_inline]] inline
#else
// Fallback Macro without forced inlining for other compilers
#	define BSI_FORCE_INLINE inline
#endif

namespace Pentagram { namespace BilinearScaler {

	// A simple typedef representing an unsigned 16.16 fixed point value
	//  Is actually a uint_fast32_t because need 32 bits but don't really care
	//  how the compiler actually represents it and we prefer the compiler gives
	//  us the fastest compatible integer type that is at least 32 bit
	typedef uint_fast32_t fixedu1616;

	template <typename limit_t = std::nullptr_t>
	// Checks to see if the destination pixel pointer has exceeded the limit
	// If it has the pointer has exceeded the limit it return false = clipped
	// If limit is unset or set to nullptr then no check is done and
	// it returns true = unclipped
	BSI_FORCE_INLINE bool IsUnclipped(uint8* dest, limit_t limit = nullptr) {
		return (std::is_null_pointer<limit_t>::value
				|| dest < static_cast<uint8*>(limit));
	}

	template <typename uintX, typename limit_t = std::nullptr_t>
	// Write a piexl if desthas not exceeded limit
	BSI_FORCE_INLINE void WritePix(
			uint8* dest, uintX val, limit_t limit = nullptr) {
		if (IsUnclipped(dest, limit)) {
			std::memcpy(dest, &val, sizeof(uintX));
		}
	}

	// Do a linear interpolation between 2 values using an 8 bit
	// interpolation factor
	// result is shifted left by 8
	// if fac is 256 the result is a
	// if fac is 0 the result is b
	BSI_FORCE_INLINE uint_fast32_t
			SimpleLerp8(uint_fast32_t a, uint_fast32_t b, uint_fast32_t fac) {
		return (b << 8) + (a - b) * fac;
	}

	// Do a linear interpolation between 2 values using a 16 bit
	// interpolation factor
	// result is shifted left by 16
	// if fac is 65536 the result is a
	// if fac is 0 the result is b
	BSI_FORCE_INLINE uint_fast32_t
			SimpleLerp16(uint_fast32_t a, uint_fast32_t b, uint_fast32_t fac) {
		return (b << 16) + (a - (b)) * fac;
	}

	template <
			class uintX, class Manip, class uintS,
			typename limit_t = std::nullptr_t>
	// interpolate a pixel from a 2x2 texel block
	BSI_FORCE_INLINE void Interpolate2x2BlockTo1(
			const uint8* const tl, const uint8* const bl, const uint8* const tr,
			const uint8* const br, const uint_fast32_t fx,
			const uint_fast32_t fy, uint8* const pixel,
			limit_t limit = nullptr) {
		if (IsUnclipped(pixel, limit)) {
			WritePix<uintX>(
					pixel,
					Manip::rgb(
							SimpleLerp8(
									SimpleLerp8(tl[0], tr[0], fx),
									SimpleLerp8(bl[0], br[0], fx), fy)
									>> 16,
							SimpleLerp8(
									SimpleLerp8(tl[1], tr[1], fx),
									SimpleLerp8(bl[1], br[1], fx), fy)
									>> 16,
							SimpleLerp8(
									SimpleLerp8(tl[2], tr[2], fx),
									SimpleLerp8(bl[2], br[2], fx), fy)
									>> 16),
					limit);
		}
	}

	template <
			class uintX, class Manip, class uintS,
			typename limit_t = std::nullptr_t>
	/// This is mostly a specialization of Interpolate2x2BlockByAny in
	/// BilinearScalerInternal_Arb.cpp with unrolled loops to geneate a 2 pixel
	/// row between the input texels using the given filtering coefficents
	/// \param tl Top Left source pixel
	/// \param bl Bottom Left source pixel
	/// \param tr Top Right source pixel
	/// \param br Bottom Right source pixel
	/// \param x1 horizontal filtering coefficent for left pixel (256 - 0)
	/// \param x2 horizontal filtering coefficent for right pixel (256 - 0)
	/// \param y vertical filtering coefficent for both pixels (256 - 0)
	BSI_FORCE_INLINE void Interpolate2x2BlockTo2x1(
			const uint8* const tl, const uint8* const bl, const uint8* const tr,
			const uint8* const br, const uint_fast32_t x1, uint_fast32_t x2,
			uint_fast32_t y, uint8* pixel, const limit_t limit = nullptr) {
		Interpolate2x2BlockTo1<uintX, Manip, uintS>(
				tl, bl, tr, br, x1, y, pixel, limit);
		Interpolate2x2BlockTo1<uintX, Manip, uintS>(
				tl, bl, tr, br, x2, y, pixel + sizeof(uintX), limit);
	}

	// Read 1 texel
	template <class Manip, typename texel_t, typename Ta>
	// Overload of Vardic ReadTexelsV fundion that reads one texel if clipping
	// is 1 or greater
	BSI_FORCE_INLINE void ReadTexelsV(
			const uint_fast8_t clipping, texel_t* texel, size_t, Ta& a) {
		// Read a if we can
		if (clipping >= 1) {
			Manip::split_source(*texel, a[0], a[1], a[2]);
		}
	}

	template <
			class Manip, typename texel_t, typename Ta, typename Tb,
			typename... Args>
	// Vardic function to read 2 or more texels Vertically with a clipping value
	// to specify how many can be read. If a texel can't be read it is  given
	// the same value as the previously read one. If all texels are clipped they
	// all get the value a had before reading was attempted
	BSI_FORCE_INLINE void ReadTexelsV(
			const uint_fast8_t clipping, texel_t* texel, size_t tpitch, Ta& a,
			Tb& b, Args&... more) {
		// Read a
		ReadTexelsV<Manip>(clipping, texel, tpitch, a);

		// Copy a to b if b can't be read
		if (clipping == 1) {
			b[0] = a[0];
			b[1] = a[1];
			b[2] = a[2];
			//  recursive call to copy b to the rest of the args
			ReadTexelsV<Manip>(0, texel + tpitch, tpitch, b, more...);
		}
		// Recursive call to read b and the rest of the args
		else {
			ReadTexelsV<Manip>(
					clipping - 1, texel + tpitch, tpitch, b, more...);
		}
	}

	// Bilinear scaler specialized for 2x scaling only
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_2x(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src);

	// Bilinear Scaler specialized for 2x horizontal 2.4x vertical scaling aka
	// aspect correct 2x.
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_X2Y24(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src);

	// Bilinear Scaler specialized for 1x horizontal 1.2x vertical scaling aka
	// aspect correction with no scaling.
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_X1Y12(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src);

	// Arbitrary Bilinear scaler capable of scaling by any integer or non
	// integer factor with no restrictions
	template <class uintX, class Manip, class uintS>
	bool BilinearScalerInternal_Arb(
			SDL_Surface* tex, uint_fast32_t sx, uint_fast32_t sy,
			uint_fast32_t sw, uint_fast32_t sh, uint8* pixel, uint_fast32_t dw,
			uint_fast32_t dh, uint_fast32_t pitch, bool clamp_src);

#ifdef COMPILE_GAMMA_CORRECT_SCALERS
#	define InstantiateFunc(func, a, b, c)                                 \
		template bool func<a, b, c>(                                       \
				SDL_Surface*, uint_fast32_t, uint_fast32_t, uint_fast32_t, \
				uint_fast32_t, uint8*, uint_fast32_t, uint_fast32_t,       \
				uint_fast32_t, bool);                                      \
		template bool func<a, b##_GC, c>(                                  \
				SDL_Surface*, uint_fast32_t, uint_fast32_t, uint_fast32_t, \
				uint_fast32_t, uint8*, uint_fast32_t, uint_fast32_t,       \
				uint_fast32_t, bool)
#else
#	define InstantiateFunc(func, a, b, c)                                 \
		template bool func<a, b, c>(                                       \
				SDL_Surface*, uint_fast32_t, uint_fast32_t, uint_fast32_t, \
				uint_fast32_t, uint8*, uint_fast32_t, uint_fast32_t,       \
				uint_fast32_t, bool)
#endif

#ifdef COMPILE_ALL_BILINEAR_SCALERS
#	define InstantiateBilinearScalerFunc(func)               \
		InstantiateFunc(func, uint32, Manip8to32, uint8);     \
		InstantiateFunc(func, uint32, Manip32to32, uint32);   \
		InstantiateFunc(func, uint16, Manip8to565, uint8);    \
		InstantiateFunc(func, uint16, Manip8to16, uint8);     \
		InstantiateFunc(func, uint16, Manip8to555, uint8);    \
		InstantiateFunc(func, uint16, Manip16to16, uint16);   \
		InstantiateFunc(func, uint16, Manip555to555, uint16); \
		InstantiateFunc(func, uint16, Manip565to565, uint16)
#else
#	define InstantiateBilinearScalerFunc(func)           \
		InstantiateFunc(func, uint32, Manip8to32, uint8); \
		InstantiateFunc(func, uint32, Manip32to32, uint32)
#endif
}}    // namespace Pentagram::BilinearScaler
