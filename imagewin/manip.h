/*
 *   Copyright (C) 2009-2022 Exult Team
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the
 *   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *   Boston, MA  02111-1307, USA.
 */

#ifndef INCL_MANIP_H
#define INCL_MANIP_H 1

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "common_types.h"

#include <cstdlib>
#include <cstring>

class color_8 {
public:
	using T = uint8;
};

class color_16 {
public:
	using T = uint16;
};

class color_555 : public color_16 {};

class color_565 : public color_16 {};

class color_32 {
public:
	using T = uint32;
};

class ManipBase {
protected:
	static const SDL_PixelFormatDetails*
					 fmt;    // Format of dest. pixels (and src for rgb src).
	static SDL_Color colors[256];    // Palette for source window.

	ManipBase(SDL_Color* c, const SDL_PixelFormatDetails* f) {
		fmt = f;
		if (c) {
			std::memcpy(colors, c, sizeof(colors));
		}
	}

public:
	static SDL_Color* get_colors() {
		return colors;
	}
};

// Generic RGB dest
template <typename color_d>
class ManipBaseDest : public ManipBase {
protected:
	ManipBaseDest(SDL_Color* c, const SDL_PixelFormatDetails* f)
			: ManipBase(c, f) {}

public:
	using uintD = typename color_d::T;

	static uintD rgb(unsigned int r, unsigned int g, unsigned int b) {
		return ((r >> (8 - fmt->Rbits)) << fmt->Rshift)
			   | ((g >> (8 - fmt->Gbits)) << fmt->Gshift)
			   | ((b >> (8 - fmt->Bbits)) << fmt->Bshift);
	}

	static void split_dest(
			uintD pix, unsigned int& r, unsigned int& g, unsigned int& b) {
		r = ((pix & fmt->Rmask) >> fmt->Rshift) << (8 - fmt->Rbits);
		g = ((pix & fmt->Gmask) >> fmt->Gshift) << (8 - fmt->Gbits);
		b = ((pix & fmt->Bmask) >> fmt->Bshift) << (8 - fmt->Bbits);
	}
};

// 555 RGB dest
template <>
class ManipBaseDest<color_555> : public ManipBase {
protected:
	ManipBaseDest(SDL_Color* c, const SDL_PixelFormatDetails* f)
			: ManipBase(nullptr, f) {
		if (c) {
			if (fmt->Rmask == 0x7c00 && fmt->Gmask == 0x03e0
				&& fmt->Bmask == 0x001f) {
				std::memcpy(colors, c, sizeof(colors));
			} else {
				SDL_Color*       dst = colors;
				SDL_Color* const end = dst + 256;
				while (dst != end) {
					dst->b = c->r;
					dst->g = c->g;
					dst->r = c->b;
					++dst;
					++c;
				}
			}
		}
	}

public:
	using uintD = color_555::T;

	static uintD rgb(unsigned int r, unsigned int g, unsigned int b) {
		return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
	}

	static void split_dest(
			uintD pix, unsigned int& r, unsigned int& g, unsigned int& b) {
		r = (pix >> 10) << 3;
		g = (pix >> 5) << 3;
		b = (pix) << 3;
	}
};

// 565 RGB dest
template <>
class ManipBaseDest<color_565> : public ManipBase {
protected:
	ManipBaseDest(SDL_Color* c, const SDL_PixelFormatDetails* f)
			: ManipBase(nullptr, f) {
		if (c) {
			if (fmt->Rmask == 0xf800 && fmt->Gmask == 0x7e0
				&& fmt->Bmask == 0x1f) {
				std::memcpy(colors, c, sizeof(colors));
			} else {
				SDL_Color*       dst = colors;
				SDL_Color* const end = dst + 256;
				while (dst != end) {
					dst->b = c->r;
					dst->g = c->g;
					dst->r = c->b;
					++dst;
					++c;
				}
			}
		}
	}

public:
	using uintD = color_565::T;

	static uintD rgb(unsigned int r, unsigned int g, unsigned int b) {
		return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
	}

	static void split_dest(
			uintD pix, unsigned int& r, unsigned int& g, unsigned int& b) {
		r = (pix >> 11) << 3;
		g = (pix >> 5) << 2;
		b = (pix) << 3;
	}
};

// Paletted dest
template <>
class ManipBaseDest<uint8> : public ManipBase {
protected:
	ManipBaseDest(SDL_Color* c, const SDL_PixelFormatDetails* f)
			: ManipBase(c, f) {}

public:
	using uintD = color_8::T;

	static int get_error(int r, int g, int b, int index) {
		const SDL_Color& color = colors[index];
		return std::abs(r - color.r) + std::abs(g - color.g)
			   + std::abs(b - color.b);
	}

	// This is god awful slow
	static uintD rgb(unsigned int r, unsigned int g, unsigned int b) {
		uintD best       = 0;
		int   best_error = get_error(r, g, b, 0);
		for (int i = 0; i < 256; i++) {
			const int error = get_error(r, g, b, i);
			if (error < best_error) {
				best       = i;
				best_error = error;
			}
		}
		return best;
	}

	static void split_dest(
			uintD pix, unsigned int& r, unsigned int& g, unsigned int& b) {
		const SDL_Color& color = colors[pix];
		r                      = color.r;
		g                      = color.g;
		b                      = color.b;
	}
};

// Generic RGB Src
template <typename color_s, typename color_d>
class ManipBaseSrc : public ManipBaseDest<color_d> {
protected:
	ManipBaseSrc(SDL_Color* c, const SDL_PixelFormatDetails* f)
			: ManipBaseDest<color_d>(c, f) {}

public:
	using uintS = typename color_s::T;

	static void split_source(
			uintS pix, unsigned int& r, unsigned int& g, unsigned int& b) {
		r = ((pix & ManipBase::fmt->Rmask) >> ManipBase::fmt->Rshift)
			<< (8 - ManipBase::fmt->Rbits);
		g = ((pix & ManipBase::fmt->Gmask) >> ManipBase::fmt->Gshift)
			<< (8 - ManipBase::fmt->Gbits);
		b = ((pix & ManipBase::fmt->Bmask) >> ManipBase::fmt->Bshift)
			<< (8 - ManipBase::fmt->Bbits);
	}

	static void split_source(uintS pix, uint8& r, uint8& g, uint8& b) {
		r = ((pix & ManipBase::fmt->Rmask) >> ManipBase::fmt->Rshift)
			<< (8 - ManipBase::fmt->Rbits);
		g = ((pix & ManipBase::fmt->Gmask) >> ManipBase::fmt->Gshift)
			<< (8 - ManipBase::fmt->Gbits);
		b = ((pix & ManipBase::fmt->Bmask) >> ManipBase::fmt->Bshift)
			<< (8 - ManipBase::fmt->Bbits);
	}
};

// 555 RGB Src
template <typename color_d>
class ManipBaseSrc<color_555, color_d> : public ManipBaseDest<color_d> {
protected:
	ManipBaseSrc(SDL_Color* c, const SDL_PixelFormatDetails* f)
			: ManipBaseDest<color_d>(c, f) {}

public:
	using uintS = typename color_555::T;

	static void split_source(
			uintS pix, unsigned int& r, unsigned int& g, unsigned int& b) {
		r = (pix >> 10) << 3;
		g = (pix >> 5) << 3;
		b = (pix) << 3;
	}

	static void split_source(uintS pix, uint8& r, uint8& g, uint8& b) {
		r = (pix >> 10) << 3;
		g = (pix >> 5) << 3;
		b = (pix) << 3;
	}
};

// 565 RGB Src
template <typename color_d>
class ManipBaseSrc<color_565, color_d> : public ManipBaseDest<color_d> {
protected:
	ManipBaseSrc(SDL_Color* c, const SDL_PixelFormatDetails* f)
			: ManipBaseDest<color_d>(c, f) {}

public:
	using uintS = typename color_565::T;

	static void split_source(
			uintS pix, unsigned int& r, unsigned int& g, unsigned int& b) {
		r = (pix >> 11) << 3;
		g = (pix >> 5) << 2;
		b = (pix) << 3;
	}

	static void split_source(uintS pix, uint8& r, uint8& g, uint8& b) {
		r = (pix >> 11) << 3;
		g = (pix >> 5) << 2;
		b = (pix) << 3;
	}
};

// Paletted Src
template <typename color_d>
class ManipBaseSrc<color_8, color_d> : public ManipBaseDest<color_d> {
protected:
	ManipBaseSrc(SDL_Color* c, const SDL_PixelFormatDetails* f)
			: ManipBaseDest<color_d>(c, f) {}

public:
	using uintS = typename color_8::T;

	static void split_source(
			uintS pix, unsigned int& r, unsigned int& g, unsigned int& b) {
		const SDL_Color& color = ManipBase::colors[pix];
		r                      = color.r;
		g                      = color.g;
		b                      = color.b;
	}

	static void split_source(uintS pix, uint8& r, uint8& g, uint8& b) {
		const SDL_Color& color = ManipBase::colors[pix];
		r                      = color.r;
		g                      = color.g;
		b                      = color.b;
	}
};

// Src and dest are different
template <typename color_s, typename color_d>
class ManipStoD : public ManipBaseSrc<color_s, color_d> {
public:
	ManipStoD(SDL_Color* c, const SDL_PixelFormatDetails* f)
			: ManipBaseSrc<color_s, color_d>(c, f) {}

	using uintS = typename color_s::T;
	using uintD = typename color_d::T;

	static uintD copy(uintS src) {
		unsigned int r;
		unsigned int g;
		unsigned int b;
		ManipBaseSrc<color_s, color_d>::split_source(src, r, g, b);
		return ManipBaseDest<color_d>::rgb(r, g, b);
	}

	static void copy(uintD& dest, uintS src) {
		dest = copy(src);
	}

	template <unsigned int N, unsigned int M>
	static inline void blend(
			uintD& dest, unsigned int rs, unsigned int gs, unsigned int bs) {
		unsigned int rd;
		unsigned int gd;
		unsigned int bd;
		ManipBaseDest<color_d>::split_dest(dest, rd, gd, bd);
		dest = ManipBaseDest<color_d>::rgb(
				(rs * N + rd * (M - N)) / M, (gs * N + gd * (M - N)) / M,
				(bs * N + bd * (M - N)) / M);
	}
};

// Src and dest are the same
template <typename colorSD>
class ManipStoD<colorSD, colorSD> : public ManipBaseSrc<colorSD, colorSD> {
public:
	ManipStoD(SDL_Color* c, const SDL_PixelFormatDetails* f)
			: ManipBaseSrc<colorSD, colorSD>(c, f) {}

	using uintS = typename colorSD::T;
	using uintD = typename colorSD::T;

	static uintD copy(uintS src) {
		return src;
	}

	static void copy(uintD& dest, uintS src) {
		dest = src;
	}
};

using Manip8to8   = ManipStoD<color_8, color_8>;
using Manip8to16  = ManipStoD<color_8, color_16>;
using Manip8to555 = ManipStoD<color_8, color_555>;
using Manip8to565 = ManipStoD<color_8, color_565>;
using Manip8to32  = ManipStoD<color_8, color_32>;

using Manip16to16   = ManipStoD<color_16, color_16>;
using Manip555to555 = ManipStoD<color_555, color_555>;
using Manip565to565 = ManipStoD<color_565, color_565>;
using Manip32to32   = ManipStoD<color_32, color_32>;

inline void increase_area(
		int& x, int& y, int& w, int& h, int left, int right, int top,
		int bottom, int buf_width, int buf_height) {
	x -= left;
	w += left + right;
	y -= top;
	h += top + bottom;

	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}

	if (x + w > buf_width) {
		w = buf_width - x;
	}
	if (y + h > buf_height) {
		h = buf_height - y;
	}
}

#endif
