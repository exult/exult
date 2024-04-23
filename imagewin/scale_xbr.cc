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
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to16 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to16, Scaler2xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

void Image_window::show_scaled8to555_2xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to555 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to555, Scaler2xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

void Image_window::show_scaled8to565_2xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to565 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to565, Scaler2xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

void Image_window::show_scaled8to32_2xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to32 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint32, Manip8to32, Scaler2xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint32*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

//
// 3xBR Filtering
//
void Image_window::show_scaled8to16_3xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to16 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to16, Scaler3xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

void Image_window::show_scaled8to555_3xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to555 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to555, Scaler3xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

void Image_window::show_scaled8to565_3xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to565 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to565, Scaler3xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

void Image_window::show_scaled8to32_3xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to32 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint32, Manip8to32, Scaler3xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint32*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

//
// 4xBR Filtering
//
void Image_window::show_scaled8to16_4xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to16 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to16, Scaler4xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

void Image_window::show_scaled8to555_4xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to555 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to555, Scaler4xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

void Image_window::show_scaled8to565_4xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to565 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint16, Manip8to565, Scaler4xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint16*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

void Image_window::show_scaled8to32_4xBR(
		int x, int y, int w, int h    // Area to show.
) {
	const SDL_PixelFormatDetails* inter_surface_format
			= SDL_GetPixelFormatDetails(inter_surface->format);
	SDL_Palette* paletted_surface_palette
			= SDL_GetSurfacePalette(paletted_surface);
	const Manip8to32 manip(
			paletted_surface_palette->colors, inter_surface_format);
	Scale_xBR<uint32, Manip8to32, Scaler4xBR>(
			static_cast<uint8*>(draw_surface->pixels), x + guard_band,
			y + guard_band, w, h, ibuf->line_width, ibuf->height + guard_band,
			static_cast<uint32*>(inter_surface->pixels),
			inter_surface->pitch / inter_surface_format->bytes_per_pixel,
			manip);
}

// calculate input matrix coordinates after rotation at compile time
template <RotationDegree rotDeg, size_t I, size_t J, size_t N>
struct MatrixRotation;

template <size_t I, size_t J, size_t N>
struct MatrixRotation<ROT_0, I, J, N> {
	enum {
		I_old = I,
		J_old = J
	};
};

template <
		RotationDegree rotDeg, size_t I, size_t J,
		size_t N>    //(i, j) = (row, col) indices, N = size of (square) matrix
struct MatrixRotation {
	enum {
		I_old = N - 1 - MatrixRotation
				< static_cast<RotationDegree>(rotDeg - 1),
		I,
		J,
		N > ::J_old,    // old coordinates before rotation!
		J_old = MatrixRotation < static_cast<RotationDegree>(rotDeg - 1),
		I,
		J,
		N > ::I_old
	};
};

template <class Dest_pixel, size_t N, RotationDegree rotDeg>
class OutputMatrix {
public:
	// access matrix area, top-left at position "out" for image with given width
	OutputMatrix(Dest_pixel* out, int outWidth)
			: out_(out), outWidth_(outWidth) {}

	template <size_t I, size_t J>
	Dest_pixel& get() const {
		static const size_t I_old = MatrixRotation<rotDeg, I, J, N>::I_old;
		static const size_t J_old = MatrixRotation<rotDeg, I, J, N>::J_old;
		return *(out_ + J_old + I_old * outWidth_);
	}

private:
	Dest_pixel* out_;
	const int   outWidth_;
};

struct Scaler2xBR {
	enum {
		scale = 2
	};

	template <class OutMatrix, class Buffer_Type>
	static void blendLineSteepAndShallow(
			OutMatrix& out, const Buffer_Type& col) {
		out.template get<1, 0>().template blend<1, 4>(col);
		out.template get<0, 1>().template blend<1, 4>(col);
		out.template get<1, 1>().template blend<5, 6>(
				col);    //[!] fixes 7/8 used in original
	}

	template <class OutMatrix, class Buffer_Type>
	static void blendLineShallow(OutMatrix& out, const Buffer_Type& col) {
		out.template get<1, 0>().template blend<1, 4>(col);
		out.template get<1, 1>().template blend<3, 4>(col);
	}

	template <class OutMatrix, class Buffer_Type>
	static void blendLineSteep(OutMatrix& out, const Buffer_Type& col) {
		out.template get<0, 1>().template blend<1, 4>(col);
		out.template get<1, 1>().template blend<3, 4>(col);
	}

	template <class OutMatrix, class Buffer_Type>
	static void blendLineDiagonal(OutMatrix& out, const Buffer_Type& col) {
		out.template get<1, 1>().template blend<1, 2>(col);
	}

	template <class OutMatrix, class Buffer_Type>
	static void blendCorner(OutMatrix& out, const Buffer_Type& col) {
#	if XBR_VARIANT == 4
		// model a round corner
		out.template get<1, 1>().template blend<21, 100>(
				col);    // exact: 1 - pi/4 = 0.2146018366
#	else
		out.template get<1, 1>().template blend<1, 2>(col);
#	endif
	}
};

struct Scaler3xBR {
	enum {
		scale = 3
	};

	template <class OutMatrix, class Buffer_Type>
	static void blendLineSteepAndShallow(
			OutMatrix& out, const Buffer_Type& col) {
		out.template get<2, 0>().template blend<1, 4>(col);
		out.template get<0, 2>().template blend<1, 4>(col);
		out.template get<2, 1>().template blend<3, 4>(col);
		out.template get<1, 2>().template blend<3, 4>(col);
		out.template get<2, 2>() = col;
	}

	template <class OutMatrix, class Buffer_Type>
	static void blendLineShallow(OutMatrix& out, const Buffer_Type& col) {
		out.template get<2, 1>().template blend<3, 4>(col);
		out.template get<1, 2>().template blend<1, 4>(col);
		out.template get<2, 0>().template blend<1, 4>(col);
		out.template get<2, 2>() = col;
	}

	template <class OutMatrix, class Buffer_Type>
	static void blendLineSteep(OutMatrix& out, const Buffer_Type& col) {
		out.template get<1, 2>().template blend<3, 4>(col);
		out.template get<2, 1>().template blend<1, 4>(col);
		out.template get<0, 2>().template blend<1, 4>(col);
		out.template get<2, 2>() = col;
	}

	template <class OutMatrix, class Buffer_Type>
	static void blendLineDiagonal(OutMatrix& out, const Buffer_Type& col) {
		out.template get<2, 2>().template blend<7, 8>(col);
		out.template get<1, 2>().template blend<1, 8>(col);
		out.template get<2, 1>().template blend<1, 8>(col);
	}

	template <class OutMatrix, class Buffer_Type>
	static void blendCorner(OutMatrix& out, const Buffer_Type& col) {
#	if XBR_VARIANT == 4
		// model a round corner
		out.template get<2, 2>().template blend<45, 100>(
				col);    // exact: 0.4545939598
		// out.template get<2, 1>().template blend<14, 1000>(col);
		// //0.01413008627 -> negligible out.template get<1, 2>().template
		// blend<14, 1000>(col); //0.01413008627
#	else
		out.template get<2, 2>().template blend<1, 2>(col);
#	endif
	}
};

struct Scaler4xBR {
	enum {
		scale = 4
	};

	template <class OutMatrix, class Buffer_Type>
	static void blendLineSteepAndShallow(
			OutMatrix& out, const Buffer_Type& col) {
		out.template get<3, 1>().template blend<3, 4>(col);
		out.template get<1, 3>().template blend<3, 4>(col);
		out.template get<3, 0>().template blend<1, 4>(col);
		out.template get<0, 3>().template blend<1, 4>(col);
		out.template get<2, 2>().template blend<1, 3>(
				col);    //[!] fixes 1/4 used in original
		out.template get<3, 3>()           = out.template get<3, 2>()
				= out.template get<2, 3>() = col;
	}

	template <class OutMatrix, class Buffer_Type>
	static void blendLineShallow(OutMatrix& out, const Buffer_Type& col) {
		out.template get<2, 3>().template blend<3, 4>(col);
		out.template get<3, 1>().template blend<3, 4>(col);
		out.template get<2, 2>().template blend<1, 4>(col);
		out.template get<3, 0>().template blend<1, 4>(col);
		out.template get<3, 2>() = out.template get<3, 3>() = col;
	}

	template <class OutMatrix, class Buffer_Type>
	static void blendLineSteep(OutMatrix& out, const Buffer_Type& col) {
		out.template get<3, 2>().template blend<3, 4>(col);
		out.template get<1, 3>().template blend<3, 4>(col);
		out.template get<2, 2>().template blend<1, 4>(col);
		out.template get<0, 3>().template blend<1, 4>(col);
		out.template get<2, 3>() = out.template get<3, 3>() = col;
	}

	template <class OutMatrix, class Buffer_Type>
	static void blendLineDiagonal(OutMatrix& out, const Buffer_Type& col) {
		out.template get<2, 3>().template blend<1, 2>(col);
		out.template get<3, 2>().template blend<1, 2>(col);
		out.template get<3, 3>() = col;
	}

	template <class OutMatrix, class Buffer_Type>
	static void blendCorner(OutMatrix& out, const Buffer_Type& col) {
#	if XBR_VARIANT == 4
		// model a round corner
		out.template get<3, 3>().template blend<68, 100>(
				col);    // exact: 0.6848532563
		out.template get<3, 2>().template blend<9, 100>(
				col);    // 0.08677704501
		out.template get<2, 3>().template blend<9, 100>(
				col);    // 0.08677704501
#	else
		out.template get<3, 3>().template blend<1, 2>(col);
#	endif
	}
};

#endif    // USE_XBR_SCALER
