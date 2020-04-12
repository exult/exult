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

#ifndef INCL_SCALE_XBR_H
#define INCL_SCALE_XBR_H    1

#ifdef USE_XBR_SCALER

#include "ignore_unused_variable_warning.h"
#include <cstddef>
#include <cstdlib>

#define XBR_VARIANT 4   // Tweaked xBR-z (Zenju's version)
//#define XBR_VARIANT 3  // xBR-C: Hyllian's "squared flavor" version
//#define XBR_VARIANT 2  // xBR-B: Hyllian's "semi-rounded flavor" version
//#define XBR_VARIANT 1  // xBR-A: Hyllian's "rounded flavor" version

enum RotationDegree { //clock-wise
    ROT_0,
    ROT_90,
    ROT_180,
    ROT_270
};

template <class Dest_pixel, size_t N, RotationDegree rotDeg>
class OutputMatrix;

//fill block of size n * n with the given color
template <class Buffer_Type>
static inline void fill_block(Buffer_Type *trg, Buffer_Type const &col, int n) {
	for (int y = 0; y < n; ++y)
		for (int x = 0; x < n; ++x)
			*trg++ = col;
}

//copy block of size n * n
template <class Dest_pixel, class Buffer_Type, class Manip_pixels>
static inline void copy_block(Dest_pixel *trg, int dline_pixels, Buffer_Type const *src, int n, const Manip_pixels &manip) {
	for (int y = 0; y < n; ++y, trg += dline_pixels - n) {
		for (int x = 0; x < n; ++x) {
			*trg++ = src->copy(manip);
			src++;
		}
	}
}

template <class Manip_pixels, int tol>
struct RGBColor {
	unsigned int  r,  g,  b;
	unsigned int dr, dg, db;
	RGBColor() = default;
	RGBColor(unsigned char c, const Manip_pixels &manip) {
		set(c, manip);
	}
	void fix() {
		dr = 3 * r;
		dg = 4 * g;
		db = 2 * b;
	}
	void set(unsigned char c, const Manip_pixels &manip) {
		manip.split_source(c, r, g, b);
		fix();
	}
	RGBColor<Manip_pixels, tol> &operator=(RGBColor<Manip_pixels, tol> const &other) {
		if (this != &other) {
			r = other.r;
			g = other.g;
			b = other.b;
			dr = other.dr;
			dg = other.dg;
			db = other.db;
		}
		return *this;
	}
	bool operator==(RGBColor<Manip_pixels, tol> const &other) const {
		return r == other.r && g == other.g && b == other.b;
	}
	bool operator!=(RGBColor<Manip_pixels, tol> const &other) const {
		return !(*this == other);
	}
	// See http://www.compuphase.com/cmetric.htm
	int dist(RGBColor<Manip_pixels, tol> const &other) const {
		return std::abs(static_cast<int>(dr - other.dr)) +
		       std::abs(static_cast<int>(dg - other.dg)) +
		       std::abs(static_cast<int>(db - other.db));
	}
	bool equals(RGBColor<Manip_pixels, tol> const &other) const {
		return std::abs(static_cast<int>(dr - other.dr)) <= (tol *  9)
		       && std::abs(static_cast<int>(dg - other.dg)) <= (tol * 16)
		       && std::abs(static_cast<int>(db - other.db)) <= (tol *  4);
	}
	template <unsigned int N, unsigned int M>
	inline void blend(RGBColor<Manip_pixels, tol> const &other) {
		r = (other.r * N + r * (M - N)) / M;
		g = (other.g * N + g * (M - N)) / M;
		b = (other.b * N + b * (M - N)) / M;
		// Not needed.
		//fix();
	}
	typename Manip_pixels::uintD copy(const Manip_pixels &manip) const {
		return manip.rgb(r, g, b);
	}
};

#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#elif defined __GNUC__
#define FORCE_INLINE __attribute__((always_inline)) inline
#else
#define FORCE_INLINE inline
#endif

/*
Input area naming convention:
     ----------------
     | A1 | B1 | C1 |
-----|----|----|----|-----
| A0 | A  | B  | C  | C4 |
-----|----|----|----|-----
| D0 | D  | E  | F  | F4 |
-----|----|----|----|-----
| G0 | G  | H  | I  | I4 |
-----|----|----|----|-----
     | G5 | G5 | I5 |
     ----------------
*/

template <class Scaler, class OutputMatrix, class Buffer_Type, class Manip_pixels>
static FORCE_INLINE //perf: quite worth it!
void scalePixel(
    Buffer_Type const &a1, Buffer_Type const &b1, Buffer_Type const &c1,
    Buffer_Type const &a0, Buffer_Type const &a,  Buffer_Type const &b,  Buffer_Type const &c, Buffer_Type const &c4,
    Buffer_Type const &d0, Buffer_Type const &d,  Buffer_Type const &e,  Buffer_Type const &f, Buffer_Type const &f4,
    Buffer_Type const &g0, Buffer_Type const &g,  Buffer_Type const &h,  Buffer_Type const &i, Buffer_Type const &i4,
    Buffer_Type const &g5, Buffer_Type const &h5, Buffer_Type const &i5,
    OutputMatrix &out, const Manip_pixels &manip
) {
	ignore_unused_variable_warning(a1, a0, manip);
	int const weight = 4;
	double const detectSteepWeight = 2.2;

	if (e == h || e == f)
		return;

	const int hf = e.dist(c) + e.dist(g)  + i.dist(h5) + i.dist(f4) + weight * h.dist(f);
	const int ei = h.dist(d) + h.dist(i5) + f.dist(i4) + f.dist(b)  + weight * e.dist(i);

	Buffer_Type const &px = e.dist(f) <= e.dist(h) ? f : h;

#if XBR_VARIANT == 4
	double const detectDirectionWeight = 3.6;
	const int dh = d0.dist(g) + g.dist(h5) + a.dist(e) + e.dist(i)  + weight * d.dist(h);
	const int eg = b.dist(d)  + d.dist(g0) + f.dist(h) + h.dist(g5) + weight * e.dist(g);

	const int ec = d.dist(b) + b.dist(c1) + h.dist(f)  + f.dist(c4) + weight * e.dist(c);
	const int bf = a.dist(e) + e.dist(i)  + b1.dist(c) + c.dist(f4) + weight * b.dist(f);


	const bool doBlendLine =
	    // make sure there is no second blending in an adjacent rotation for this pixel
	    ((eg < dh || e.equals(d) || e.equals(h)) &&
	     (ec < bf || e.equals(b) || e.equals(f)) &&
	     // for L-shapes: blend corner only (handles insular pixels and "mario mushroom eyes")
	     !(g.equals(h) && h.equals(i) && i.equals(f) && f.equals(c) && !e.equals(i))) ||
	    // U-shape
	    (d.equals(h) && h.equals(f) && a.equals(e) && e.equals(c) && !h.equals(e)) ||
	    (h.equals(f) && f.equals(b) && g.equals(e) && e.equals(a) && !f.equals(e)) ||
	    detectDirectionWeight * hf < ei;
#elif XBR_VARIANT == 3
	const bool doBlendLine = (!f.equals(b) && !f.equals(c)) ||
	                         (!h.equals(d) && !h.equals(g)) ||
	                         (e.equals(i) && ((!f.equals(f4) && !f.equals(i4)) || (!h.equals(h5) && !h.equals(i5)))) ||
	                         e.equals(g) ||
	                         e.equals(c);
#elif XBR_VARIANT == 2
	const bool doBlendLine = (!f.equals(b) && !h.equals(d)) ||
	                         (e.equals(i) && !f.equals(i4) && !h.equals(i5)) ||
	                         e.equals(g) ||
	                         e.equals(c);
#elif XBR_VARIANT == 1
	const bool doBlendLine = true;
#endif
	if (hf < ei) {
		if (doBlendLine) {
			const int fg = f.dist(g);
			const int hc = h.dist(c);
			bool ex2 = e != c && b != c;
			bool ex3 = e != g && d != g;

			if (detectSteepWeight * fg <= hc && ex3) {
				if (fg >= detectSteepWeight * hc && ex2)
					Scaler::blendLineSteepAndShallow(out, px);
				else
					Scaler::blendLineShallow(out, px);
			} else {
				if (fg >= detectSteepWeight * hc && ex2)
					Scaler::blendLineSteep(out, px);
				else
					Scaler::blendLineDiagonal(out, px);
			}
		} else
			Scaler::blendCorner(out, px);
	}
}

// fill `row' with the the disassembled color components from the original
// pixel values in `from'; if we run out of source pixels, just keep copying
// the last one we got
template <class Source_pixel, class Manip_pixels, class Buffer_Type>
static inline void fill_rgbcolor_row(
    Source_pixel *from,
    Buffer_Type *row,
    int width,      // number of pixels to write into 'row'
    const Manip_pixels &manip
) {
	Buffer_Type *stop = row + width;
	while (row < stop) {
		row->set(*from++, manip);
		row++;
	}
}

template <class Buffer_Type>
static inline void cycle_buffers(
    Buffer_Type *&b1,
    Buffer_Type *&b2,
    Buffer_Type *&b3,
    Buffer_Type *&b4,
    Buffer_Type *&b5
) {
	Buffer_Type *b0 = b1;
	b1 = b2;
	b2 = b3;
	b3 = b4;
	b4 = b5;
	b5 = b0;
}

template <class Dest_pixel, class Manip_pixels, class Scaler>
void Scale_xBR(
    unsigned char *source,      // ->source pixels.
    int srcx, int srcy,     // Start of rectangle within src.
    int srcw, int srch,     // Dims. of rectangle.
    int sline_pixels,       // Pixels (words)/line for source.
    int sheight,            // Source height.
    Dest_pixel *dest,       // ->dest pixels.
    int dline_pixels,       // Pixels (words)/line for dest.
    const Manip_pixels &manip   // Manipulator methods.
) {
	// the following are static because we don't want to be freeing and
	// reallocating space on each call, as new[]s are usually very
	// expensive; we do allow it to grow though
	static int buff_size = 0;
	static RGBColor<Manip_pixels, 2> *rgb_row_minus_2  = nullptr;
	static RGBColor<Manip_pixels, 2> *rgb_row_minus_1  = nullptr;
	static RGBColor<Manip_pixels, 2> *rgb_row_current  = nullptr;
	static RGBColor<Manip_pixels, 2> *rgb_row_plus_1   = nullptr;
	static RGBColor<Manip_pixels, 2> *rgb_row_plus_2   = nullptr;
	if (buff_size < sline_pixels) {
		delete [] rgb_row_minus_2;
		delete [] rgb_row_minus_1;
		delete [] rgb_row_current;
		delete [] rgb_row_plus_1;
		delete [] rgb_row_plus_2;
		buff_size = sline_pixels;
		rgb_row_minus_2 = new RGBColor<Manip_pixels, 2>[buff_size];
		rgb_row_minus_1 = new RGBColor<Manip_pixels, 2>[buff_size];
		rgb_row_current = new RGBColor<Manip_pixels, 2>[buff_size];
		rgb_row_plus_1  = new RGBColor<Manip_pixels, 2>[buff_size];
		rgb_row_plus_2  = new RGBColor<Manip_pixels, 2>[buff_size];
	}

	int from_x = srcx >= 2 ? srcx - 2 : 0;
	int to_x = (srcx + srcw + 2 <= sline_pixels) ? (srcx + srcw + 2) : sline_pixels;
	int from_width = to_x - from_x;
	if (sline_pixels < from_width)
		from_width = sline_pixels;

	unsigned char *ptr2 = source + srcy * sline_pixels + from_x; //center
	unsigned char *ptr1 = ptr2 - sline_pixels;
	unsigned char *ptr0 = ptr1 - sline_pixels;
	unsigned char *ptr3 = ptr2 + sline_pixels;
	unsigned char *ptr4 = ptr3 + sline_pixels;
	// Initial buffer fill.
	fill_rgbcolor_row(ptr2, rgb_row_current + from_x, from_width, manip);
	if (srcy >= 1) {
		fill_rgbcolor_row(ptr1, rgb_row_minus_1 + from_x, from_width, manip);
		if (srcy > 1)
			fill_rgbcolor_row(ptr0, rgb_row_minus_2 + from_x, from_width, manip);
	}
	if (srcy + 1 < sheight)
		fill_rgbcolor_row(ptr3, rgb_row_plus_1 + from_x, from_width, manip);

	for (int y = srcy; y < srcy + srch; ++y, ptr4 += sline_pixels) {
		Dest_pixel *out = dest + Scaler::scale * (y * dline_pixels + srcx);

		RGBColor<Manip_pixels, 2> const *sa2 = rgb_row_current + srcx; //center
		RGBColor<Manip_pixels, 2> const *sa0;
		RGBColor<Manip_pixels, 2> const *sa1;
		RGBColor<Manip_pixels, 2> const *sa3;
		RGBColor<Manip_pixels, 2> const *sa4;

		if (y <= 1) {
			if (y == 1)
				sa0 = sa1 = rgb_row_minus_1 + srcx;
			else
				sa0 = sa1 = sa2;
		} else {
			sa0 = rgb_row_minus_2 + srcx;
			sa1 = rgb_row_minus_1 + srcx;
		}
		if (y >= sheight - 2) {
			if (y == sheight - 2)
				sa4 = sa3 = rgb_row_plus_1 + srcx;
			else
				sa4 = sa3 = sa2;
		} else {
			sa3 = rgb_row_plus_1 + srcx;
			sa4 = rgb_row_plus_2 + srcx;
			fill_rgbcolor_row(ptr4, rgb_row_plus_2 + from_x, from_width, manip);
		}

		for (int x = srcx; x < srcx + srcw; ++x, ++sa0, ++sa1, ++sa2, ++sa3, ++sa4,
		        out += Scaler::scale) {
			RGBColor<Manip_pixels, 2> const *a0;
			RGBColor<Manip_pixels, 2> const *d0;
			RGBColor<Manip_pixels, 2> const *g0;
			RGBColor<Manip_pixels, 2> const *a1;
			RGBColor<Manip_pixels, 2> const *a;
			RGBColor<Manip_pixels, 2> const *d;
			RGBColor<Manip_pixels, 2> const *g;
			RGBColor<Manip_pixels, 2> const *g5;

			//all those bounds checks have only insignificant impact on performance!
			if (x <= 1) {
				if (x == 0) {
					a0 = &sa1[0];
					d0 = &sa2[0];
					g0 = &sa3[0];

					a1 = &sa0[0];
					a  = &sa1[0];
					d  = &sa2[0];
					g  = &sa3[0];
					g5 = &sa4[0];
				} else {
					a0 = &sa1[-1];
					d0 = &sa2[-1];
					g0 = &sa3[-1];

					a1 = &sa0[-1];
					a  = &sa1[-1];
					d  = &sa2[-1];
					g  = &sa3[-1];
					g5 = &sa4[-1];
				}
			} else {
				a0 = &sa1[-2];
				d0 = &sa2[-2];
				g0 = &sa3[-2];

				a1 = &sa0[-1];
				a  = &sa1[-1];
				d  = &sa2[-1];
				g  = &sa3[-1];
				g5 = &sa4[-1];
			}

			RGBColor<Manip_pixels, 2> const *b1 = &sa0[0];
			RGBColor<Manip_pixels, 2> const *b = &sa1[0];
			RGBColor<Manip_pixels, 2> const *e = &sa2[0];
			RGBColor<Manip_pixels, 2> const *h = &sa3[0];
			RGBColor<Manip_pixels, 2> const *h5 = &sa4[0];
			RGBColor<Manip_pixels, 2> const *c1;
			RGBColor<Manip_pixels, 2> const *c;
			RGBColor<Manip_pixels, 2> const *f;
			RGBColor<Manip_pixels, 2> const *i;
			RGBColor<Manip_pixels, 2> const *i5;
			RGBColor<Manip_pixels, 2> const *c4;
			RGBColor<Manip_pixels, 2> const *f4;
			RGBColor<Manip_pixels, 2> const *i4;

			if (x >= sline_pixels - 2) {
				if (x == sline_pixels - 1) {
					c1 = &sa0[0];
					c  = &sa1[0];
					f  = &sa2[0];
					i  = &sa3[0];
					i5 = &sa4[0];

					c4 = &sa1[0];
					f4 = &sa2[0];
					i4 = &sa3[0];
				} else {
					c1 = &sa0[1];
					c  = &sa1[1];
					f  = &sa2[1];
					i  = &sa3[1];
					i5 = &sa4[1];

					c4 = &sa1[1];
					f4 = &sa2[1];
					i4 = &sa3[1];
				}
			} else {
				c1 = &sa0[1];
				c  = &sa1[1];
				f  = &sa2[1];
				i  = &sa3[1];
				i5 = &sa4[1];

				c4 = &sa1[2];
				f4 = &sa2[2];
				i4 = &sa3[2];
			}

			//fill block of size n * n with the given color
			RGBColor<Manip_pixels, 2> buffer[Scaler::scale * Scaler::scale];
			fill_block(buffer, *e, Scaler::scale);
			//perf: no disadvantage compared to hand written assign!

			OutputMatrix<RGBColor<Manip_pixels, 2>, Scaler::scale, ROT_0> om0(buffer, Scaler::scale);
			scalePixel<Scaler>(
			    *a1, *b1, *c1,
			    *a0, *a,  *b,  *c,  *c4,
			    *d0, *d,  *e,  *f,  *f4,
			    *g0, *g,  *h,  *i,  *i4,
			    *g5, *h5, *i5,
			    om0, manip);

			OutputMatrix<RGBColor<Manip_pixels, 2>, Scaler::scale, ROT_90> om90(buffer, Scaler::scale);
			scalePixel<Scaler>(
			    *g0, *d0, *a0,
			    *g5, *g,  *d,  *a,  *a1,
			    *h5, *h,  *e,  *b,  *b1,
			    *i5, *i,  *f,  *c,  *c1,
			    *i4, *f4, *c4,
			    om90, manip);

			OutputMatrix<RGBColor<Manip_pixels, 2>, Scaler::scale, ROT_180> om180(buffer, Scaler::scale);
			scalePixel<Scaler>(
			    *i5, *h5, *g5,
			    *i4, *i,  *h,  *g,  *g0,
			    *f4, *f,  *e,  *d,  *d0,
			    *c4, *c,  *b,  *a,  *a0,
			    *c1, *b1, *a1,
			    om180, manip);

			OutputMatrix<RGBColor<Manip_pixels, 2>, Scaler::scale, ROT_270> om270(buffer, Scaler::scale);
			scalePixel<Scaler>(
			    *c4, *f4, *i4,
			    *c1, *c,  *f,  *i,  *i5,
			    *b1, *b,  *e,  *h,  *h5,
			    *a1, *a,  *d,  *g,  *g5,
			    *a0, *d0, *g0,
			    om270, manip);

			copy_block(out, dline_pixels, buffer, Scaler::scale, manip);
		}
		cycle_buffers(rgb_row_minus_2, rgb_row_minus_1, rgb_row_current,
		              rgb_row_plus_1 , rgb_row_plus_2);
	}
}

struct Scaler2xBR;
struct Scaler3xBR;
struct Scaler4xBR;

#endif //USE_XBR_SCALER

#endif //INCL_SCALE_XBR_H
