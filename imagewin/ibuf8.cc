/*
 *  ibuf8.cc - 8-bit image buffer.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "ibuf8.h"

#include "common_types.h"
#include "endianio.h"
#include "ignore_unused_variable_warning.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

using std::cerr;
using std::endl;

/*
 *  Copy an area of the image within itself.
 */

void Image_buffer8::copy(
		int srcx, int srcy,     // Where to start.
		int srcw, int srch,     // Dimensions to copy.
		int destx, int desty    // Where to copy to.
) {
	int ynext;
	int yfrom;
	int yto;                // Figure y stuff.
	if (srcy >= desty) {    // Moving up?
		ynext = line_width;
		yfrom = srcy;
		yto   = desty;
	} else {    // Moving down.
		ynext = -line_width;
		yfrom = srcy + srch - 1;
		yto   = desty + srch - 1;
	}
	unsigned char* to   = bits + yto * line_width + destx;
	unsigned char* from = bits + yfrom * line_width + srcx;
	// Go through lines.
	while (srch--) {
		std::memmove(to, from, srcw);
		to += ynext;
		from += ynext;
	}
}

/*
 *  Get a rectangle from here into another Image_buffer.
 */

void Image_buffer8::get(
		Image_buffer* dest,    // Copy to here.
		int srcx, int srcy     // Upper-left corner of source rect.
) {
	int srcw  = dest->width;
	int srch  = dest->height;
	int destx = 0;
	int desty = 0;
	// Constrain to window's space. (Note
	//   convoluted use of clip().)
	if (!clip(destx, desty, srcw, srch, srcx, srcy)) {
		return;
	}
	unsigned char* to   = dest->bits + desty * dest->line_width + destx;
	unsigned char* from = bits + srcy * line_width + srcx;
	// Figure # pixels to next line.
	const int to_next   = dest->line_width - srcw;
	const int from_next = line_width - srcw;
	while (srch--) {    // Do each line.
		for (int cnt = srcw; cnt; cnt--) {
			Write1(to, Read1(from));
		}
		to += to_next;
		from += from_next;
	}
}

/*
 *  Retrieve data from another buffer.
 */

void Image_buffer8::put(
		Image_buffer* src,      // Copy from here.
		int destx, int desty    // Copy to here.
) {
	Image_buffer8::copy8(
			src->bits, src->get_width(), src->get_height(), destx, desty);
}

/*
 * Fill buffer with random static
 */
void Image_buffer8::fill_static(int black, int gray, int white) {
	for (int y = 0; y < height; ++y) {
		unsigned char* p = bits + (y - offset_y) * line_width - offset_x;
		for (int x = 0; x < width; ++x) {
			switch (std::rand() % 5) {
			case 0:
			case 1:
				Write1(p, black);
				break;
			case 2:
			case 3:
				Write1(p, gray);
				break;
			case 4:
				Write1(p, white);
				break;
			}
		}
	}
}

/*
 *  Fill with a given 8-bit value.
 */

void Image_buffer8::fill8(unsigned char pix) {
	unsigned char* pixels = bits - offset_y * line_width - offset_x;
	const int      cnt    = line_width * height;
	for (int i = 0; i < cnt; i++) {
		Write1(pixels, pix);
	}
}

/*
 *  Fill a rectangle with an 8-bit value.
 */

void Image_buffer8::fill8(
		unsigned char pix, int srcw, int srch, int destx, int desty) {
	int srcx = 0;
	int srcy = 0;
	// Constrain to window's space.
	if (!clip(srcx, srcy, srcw, srch, destx, desty)) {
		return;
	}
	unsigned char* pixels  = bits + desty * line_width + destx;
	const int      to_next = line_width - srcw;    // # pixels to next line.
	while (srch--) {                               // Do each line.
		for (int cnt = srcw; cnt; cnt--) {
			Write1(pixels, pix);
		}
		pixels += to_next;    // Get to start of next line.
	}
}

/*
 *  Fill a line with a given 8-bit value.
 */

void Image_buffer8::fill_hline8(
		unsigned char pix, int srcw, int destx, int desty) {
	int srcx = 0;
	// Constrain to window's space.
	if (!clip_x(srcx, srcw, destx, desty)) {
		return;
	}
	unsigned char* pixels = bits + desty * line_width + destx;
	std::memset(pixels, pix, srcw);
}

// this function is used to clip a line so dimension 0 is clipped to range
// [startc,endc] the order of points doesn't matter
static bool clipline(
		int* start0, int* end0, int* start1, int* end1, int startc, int endc) {
	// Order points so start is before end
	if (*end0 < *start0) {
		std::swap(end0, start0);
		std::swap(end1, start1);
	}
	if (*start0 > endc) {
		return false;
	}
	if (*end0 < startc) {
		return false;
	}
	if (*start0 < startc) {
		// int offscreen = startc - *start0;

		int olddelta0 = *end0 - *start0;
		int newdelta0 = *end0 - startc;

		int olddelta1 = *end1 - *start1;
		int newdelta1 = (olddelta1 * newdelta0) / olddelta0;

		*start0 = startc;
		*start1 = *end1 - newdelta1;
	}
	if (*end0 > endc) {
		// int offscreen = *end0 - endc;

		int olddelta0 = *end0 - *start0;
		int newdelta0 = endc - *start0;

		int olddelta1 = *end1 - *start1;

		if (olddelta0 == 0 && (olddelta1 == 0 || newdelta0 == 0)) {
			*end0 = endc;
			*end1 = *start1;
		} else if (olddelta0 == 0) {
			return false;
		}
		int newdelta1 = (olddelta1 * newdelta0) / olddelta0;

		*end0 = endc;
		*end1 = *start1 + newdelta1;
	}
	return true;
}

constexpr static int point_side_of_line(
		int startx, int starty, int endx, int endy, int pointx, int pointy) {
	return (endx - startx) * (pointy - starty)
		   - (endy - starty) * (pointx - startx);
}

constexpr static bool isoob(int x, int y, int cx, int cw, int cy, int ch) {
	return x < cx || x > cx + cw || y < cy || y > cy + ch;
}

void Image_buffer8::draw_line8(
		unsigned char val, int startx, int starty, int endx, int endy,
		const Xform_palette* xform) {
	// 16:16 fixed point
	typedef uint32 fixedu1616;

	int cx, cy, cw, ch;
	// Clipping
	get_clip(cx, cy, cw, ch);
	// shrink clip by 1 pixel
	cw--;
	ch--;
	// check if entirely outside clip region
	if ((startx > cx + cw && endx > cx + cw) || (endx < cx && startx < cx)
		|| (starty > cy + ch && endy > cy + ch) || (endy < cy && starty < cy)) {
		// do nothing
		return;
	}

	// If both points are oob it might be off screen but not always so make sure
	if (isoob(startx, starty, cx, cw, cy, ch)
		&& isoob(endx, endy, cx, cw, cy, ch)) {
		// Check what side of the line each clip point is
		// if all the same sign then the line is off screen

		int tl = point_side_of_line(startx, starty, endx, endy, clipx, clipy);
		int tr = point_side_of_line(
				startx, starty, endx, endy, clipx, clipy + cliph);
		int bl = point_side_of_line(startx, starty, endx, endy, clipx, clipy);
		int br = point_side_of_line(
				startx, starty, endx, endy, clipx, clipy + cliph);

		// negative upbove or left of all points
		if (tl < 0 && tr < 0 && br < 0 && bl < 0) {
			return;
		}
		// positive below or right of all points
		if (tl > 0 && tr > 0 && br > 0 && bl > 0) {
			return;
		}
		// zero means points are on the line and on screen

	}
	// clip x
	;
	if (!clipline(&startx, &endx, &starty, &endy, cx, cx + cw)) {
		// do nothing
		return;
	}    // clip y
	;
	if (!clipline(&starty, &endy, &startx, &endx, cy, cy + ch)) {
		// do nothing
		return;
	}

	int end0   = endx;
	int start0 = startx;
	int inc0   = 1;
	int end1   = endy;
	int start1 = starty;
	int inc1   = line_width;

	// More vertical than horizontal (slope not in range [-1.0,1.0])
	// swap the dimensions to prevent gaps
	if (std::abs(endx - startx) < std::abs(endy - starty)) {
		std::swap(start0, start1);
		std::swap(end0, end1);
		std::swap(inc0, inc1);
	}

	// order points in dim0
	if (end0 < start0) {
		std::swap(end0, start0);
		std::swap(end1, start1);
	}

	if (end1 < start1) {
		// Invert dim1 values to prevent slope being negative in calculations
		inc1   = -inc1;
		end1   = -end1;
		start1 = -start1;
	}

	int delta0 = end0 - start0;
	int delta1 = end1 - start1;

	// change in dim1 for each increment of dim0
	const fixedu1616 slope
			= (delta1 << 16)
			  / std::max(
					  delta0,
					  1);    // prevent div by zero if line is only 1 pixel long

	// relative dim1 of current pixel. offset by rounding error from when
	// calculating slope to ensure endpoint pixel is actually drawn
	fixedu1616 cur1 = (delta1 << 16) - slope * delta0;

	// draw the line
	unsigned char* to  = bits + start0 * inc0 + start1 * inc1;
	unsigned char* end = bits + end0 * inc0 + end1 * inc1;
	if (xform == nullptr) {
		for (;;) {
			*to = val;
			// if last pixel was drawn, break out of loop before incrementing
			// pointer to next pixel
			if (to == end) {
				break;
			}
			cur1 += slope;
			to += inc0 + inc1 * static_cast<int>(cur1 >> 16);
			cur1 &= 0xffff;
		}
	} else {
		auto xf = *xform;
		for (;;) {
			*to = xf[*to];
			if (to == end) {
				break;
			}
			cur1 += slope;
			to += inc0 + inc1 * static_cast<int>(cur1 >> 16);
			cur1 &= 0xffff;
		}
	}
}

/*
 *  Copy another rectangle into this one.
 */

void Image_buffer8::copy8(
		const unsigned char* src_pixels,    // Source rectangle pixels.
		int srcw, int srch,                 // Dimensions of source.
		int destx, int desty) {
	if (!src_pixels) {
		cerr << "WTF! src_pixels in Image_buffer8::copy8 was 0!" << endl;
		return;
	}

	int       srcx      = 0;
	int       srcy      = 0;
	const int src_width = srcw;    // Save full source width.
	// Constrain to window's space.
	if (!clip(srcx, srcy, srcw, srch, destx, desty)) {
		return;
	}

	uint8*       to   = bits + desty * line_width + destx;
	const uint8* from = src_pixels + srcy * src_width + srcx;
	while (srch--) {
		std::memcpy(to, from, srcw);
		from += src_width;
		to += line_width;
	}
}

/*
 *  Copy a line into this buffer.
 */

void Image_buffer8::copy_hline8(
		const unsigned char* src_pixels,    // Source rectangle pixels.
		int                  srcw,          // Width to copy.
		int destx, int desty) {
	int srcx = 0;
	// Constrain to window's space.
	if (!clip_x(srcx, srcw, destx, desty)) {
		return;
	}
	unsigned char*       to   = bits + desty * line_width + destx;
	const unsigned char* from = src_pixels + srcx;
	std::memcpy(to, from, srcw);
}

/*
 *  Copy a line into this buffer where some of the colors are translucent.
 */

void Image_buffer8::copy_hline_translucent8(
		const unsigned char* src_pixels,    // Source rectangle pixels.
		int                  srcw,          // Width to copy.
		int destx, int desty,
		int first_translucent,         // Palette index of 1st trans. color.
		int last_translucent,          // Index of last trans. color.
		const Xform_palette* xforms    // Transformers.  Need same # as
									   //   (last_translucent -
									   //    first_translucent + 1).
) {
	int srcx = 0;
	// Constrain to window's space.
	if (!clip_x(srcx, srcw, destx, desty)) {
		return;
	}
	unsigned char*       to   = bits + desty * line_width + destx;
	const unsigned char* from = src_pixels + srcx;
	for (int i = srcw; i; i--) {
		// Get char., and transform.
		unsigned char c = Read1(from);
		if (c >= first_translucent && c <= last_translucent) {
			// Use table to shift existing pixel.
			c = xforms[c - first_translucent][*to];
		}
		Write1(to, c);
	}
}

/*
 *  Apply a translucency table to a line.
 */

void Image_buffer8::fill_hline_translucent8(
		unsigned char val,    // Ignored for this method.
		int srcw, int destx, int desty,
		const Xform_palette& xform    // Transform table.
) {
	ignore_unused_variable_warning(val);
	int srcx = 0;
	// Constrain to window's space.
	if (!clip_x(srcx, srcw, destx, desty)) {
		return;
	}
	unsigned char* pixels = bits + desty * line_width + destx;
	while (srcw--) {
		*pixels = xform[*pixels];
		pixels++;
	}
}

/*
 *  Apply a translucency table to a rectangle.
 */

void Image_buffer8::fill_translucent8(
		unsigned char /* val */,    // Not used.
		int srcw, int srch, int destx, int desty,
		const Xform_palette& xform    // Transform table.
) {
	int srcx = 0;
	int srcy = 0;
	// Constrain to window's space.
	if (!clip(srcx, srcy, srcw, srch, destx, desty)) {
		return;
	}
	unsigned char* pixels  = bits + desty * line_width + destx;
	const int      to_next = line_width - srcw;    // # pixels to next line.
	while (srch--) {                               // Do each line.
		for (int cnt = srcw; cnt; cnt--, pixels++) {
			*pixels = xform[*pixels];
		}
		pixels += to_next;    // Get to start of next line.
	}
}

/*
 *  Copy another rectangle into this one, with 0 being the transparent
 *  color.
 */

void Image_buffer8::copy_transparent8(
		const unsigned char* src_pixels,    // Source rectangle pixels.
		int srcw, int srch,                 // Dimensions of source.
		int destx, int desty) {
	int       srcx      = 0;
	int       srcy      = 0;
	const int src_width = srcw;    // Save full source width.
	// Constrain to window's space.
	if (!clip(srcx, srcy, srcw, srch, destx, desty)) {
		return;
	}
	unsigned char*       to   = bits + desty * line_width + destx;
	const unsigned char* from = src_pixels + srcy * src_width + srcx;
	const int to_next         = line_width - srcw;    // # pixels to next line.
	const int from_next       = src_width - srcw;
	while (srch--) {    // Do each line.
		for (int cnt = srcw; cnt; cnt--, to++) {
			const int chr = Read1(from);
			if (chr) {
				*to = chr;
			}
		}
		to += to_next;
		from += from_next;
	}
}

// Slightly Optimized RLE Painter
void Image_buffer8::paint_rle(int xoff, int yoff, const unsigned char* inptr) {
	const uint8* in = inptr;
	int          scanlen;
	const int    right  = clipx + clipw;
	const int    bottom = clipy + cliph;

	while ((scanlen = little_endian::Read2(in)) != 0) {
		// Get length of scan line.
		const int encoded = scanlen & 1;    // Is it encoded?
		scanlen           = scanlen >> 1;
		int       scanx = xoff + static_cast<sint16>(little_endian::Read2(in));
		const int scany = yoff + static_cast<sint16>(little_endian::Read2(in));

		// Is there somthing on screen?
		bool on_screen = true;
		if (scanx >= right || scany >= bottom || scany < clipy
			|| scanx + scanlen < clipx) {
			on_screen = false;
		}

		if (!encoded) {    // Raw data?
			// Only do the complex calcs if we think it could be on screen
			if (on_screen) {
				// Do we need to skip pixels at the start?
				if (scanx < clipx) {
					const int delta = clipx - scanx;
					in += delta;
					scanlen -= delta;
					scanx = clipx;
				}

				// Do we need to skip pixels at the end?
				int skip = scanx + scanlen - right;
				if (skip < 0) {
					skip = 0;
				}

				// Is there anything to put on the screen?
				if (skip < scanlen) {
					unsigned char* dest = bits + scany * line_width + scanx;
					const unsigned char* end = in + scanlen - skip;
					while (in < end) {
						Write1(dest, Read1(in));
					}
					in += skip;
					continue;
				}
			}
			in += scanlen;
			continue;
		} else {    // Encoded
			unsigned char* dest = bits + scany * line_width + scanx;

			while (scanlen) {
				unsigned char bcnt = Read1(in);
				// Repeat next char. if odd.
				const int repeat = bcnt & 1;
				bcnt             = bcnt >> 1;    // Get count.

				// Only do the complex calcs if we think it could be on screen
				if (on_screen && scanx < right && scanx + bcnt > clipx) {
					if (repeat) {    // Const Colour
						// Do we need to skip pixels at the start?
						if (scanx < clipx) {
							const int delta = clipx - scanx;
							dest += delta;
							bcnt -= delta;
							scanlen -= delta;
							scanx = clipx;
						}

						// Do we need to skip pixels at the end?
						int skip = scanx + bcnt - right;
						if (skip < 0) {
							skip = 0;
						}

						// Is there anything to put on the screen?
						if (skip < bcnt) {
							const unsigned char col = Read1(in);
							unsigned char*      end = dest + bcnt - skip;
							while (dest < end) {
								Write1(dest, col);
							}

							// dest += skip; - Don't need it
							scanx += bcnt;
							scanlen -= bcnt;
							continue;
						}

						// Make sure all the required values get
						// properly updated

						// dest += bcnt; - Don't need it
						scanx += bcnt;
						scanlen -= bcnt;
						++in;
						continue;
					} else {
						// Do we need to skip pixels at the start?
						if (scanx < clipx) {
							const int delta = clipx - scanx;
							dest += delta;
							in += delta;
							bcnt -= delta;
							scanlen -= delta;
							scanx = clipx;
						}

						// Do we need to skip pixels at the end?
						int skip = scanx + bcnt - right;
						if (skip < 0) {
							skip = 0;
						}

						// Is there anything to put on the screen?
						if (skip < bcnt) {
							unsigned char* end = dest + bcnt - skip;
							while (dest < end) {
								Write1(dest, Read1(in));
							}
							// dest += skip; - Don't need it
							in += skip;
							scanx += bcnt;
							scanlen -= bcnt;
							continue;
						}

						// Make sure all the required values get
						// properly updated

						// dest += skip; - Don't need it
						scanx += bcnt;
						scanlen -= bcnt;
						in += bcnt;
						continue;
					}
				}

				// Make sure all the required values get
				// properly updated

				dest += bcnt;
				scanx += bcnt;
				scanlen -= bcnt;
				if (!repeat) {
					in += bcnt;
				} else {
					++in;
				}
			}
		}
	}
}

// Slightly Optimized RLE Painter
void Image_buffer8::paint_rle_remapped(
		int xoff, int yoff, const unsigned char* inptr,
		const unsigned char*& trans) {
	const uint8* in = inptr;
	int          scanlen;
	const int    right  = clipx + clipw;
	const int    bottom = clipy + cliph;

	while ((scanlen = little_endian::Read2(in)) != 0) {
		// Get length of scan line.
		const int encoded = scanlen & 1;    // Is it encoded?
		scanlen           = scanlen >> 1;
		int       scanx = xoff + static_cast<sint16>(little_endian::Read2(in));
		const int scany = yoff + static_cast<sint16>(little_endian::Read2(in));

		// Is there somthing on screen?
		bool on_screen = true;
		if (scanx >= right || scany >= bottom || scany < clipy
			|| scanx + scanlen < clipx) {
			on_screen = false;
		}

		if (!encoded) {    // Raw data?
			// Only do the complex calcs if we think it could be on screen
			if (on_screen) {
				// Do we need to skip pixels at the start?
				if (scanx < clipx) {
					const int delta = clipx - scanx;
					in += delta;
					scanlen -= delta;
					scanx = clipx;
				}

				// Do we need to skip pixels at the end?
				int skip = scanx + scanlen - right;
				if (skip < 0) {
					skip = 0;
				}

				// Is there anything to put on the screen?
				if (skip < scanlen) {
					unsigned char* dest = bits + scany * line_width + scanx;
					const unsigned char* end = in + scanlen - skip;
					while (in < end) {
						Write1(dest, trans[Read1(in)]);
					}
					in += skip;
					continue;
				}
			}
			in += scanlen;
			continue;
		} else {    // Encoded
			unsigned char* dest = bits + scany * line_width + scanx;

			while (scanlen) {
				unsigned char bcnt = Read1(in);
				// Repeat next char. if odd.
				const int repeat = bcnt & 1;
				bcnt             = bcnt >> 1;    // Get count.

				// Only do the complex calcs if we think it could be on screen
				if (on_screen && scanx < right && scanx + bcnt > clipx) {
					if (repeat) {    // Const Colour
						// Do we need to skip pixels at the start?
						if (scanx < clipx) {
							const int delta = clipx - scanx;
							dest += delta;
							bcnt -= delta;
							scanlen -= delta;
							scanx = clipx;
						}

						// Do we need to skip pixels at the end?
						int skip = scanx + bcnt - right;
						if (skip < 0) {
							skip = 0;
						}

						// Is there anything to put on the screen?
						if (skip < bcnt) {
							const unsigned char col = Read1(in);
							unsigned char*      end = dest + bcnt - skip;
							while (dest < end) {
								Write1(dest, trans[col]);
							}

							// dest += skip; - Don't need it
							scanx += bcnt;
							scanlen -= bcnt;
							continue;
						}

						// Make sure all the required values get
						// properly updated

						// dest += bcnt; - Don't need it
						scanx += bcnt;
						scanlen -= bcnt;
						++in;
						continue;
					} else {
						// Do we need to skip pixels at the start?
						if (scanx < clipx) {
							const int delta = clipx - scanx;
							dest += delta;
							in += delta;
							bcnt -= delta;
							scanlen -= delta;
							scanx = clipx;
						}

						// Do we need to skip pixels at the end?
						int skip = scanx + bcnt - right;
						if (skip < 0) {
							skip = 0;
						}

						// Is there anything to put on the screen?
						if (skip < bcnt) {
							unsigned char* end = dest + bcnt - skip;
							while (dest < end) {
								Write1(dest, trans[Read1(in)]);
							}
							// dest += skip; - Don't need it
							in += skip;
							scanx += bcnt;
							scanlen -= bcnt;
							continue;
						}

						// Make sure all the required values get
						// properly updated

						// dest += skip; - Don't need it
						scanx += bcnt;
						scanlen -= bcnt;
						in += bcnt;
						continue;
					}
				}

				// Make sure all the required values get
				// properly updated

				dest += bcnt;
				scanx += bcnt;
				scanlen -= bcnt;
				if (!repeat) {
					in += bcnt;
				} else {
					++in;
				}
			}
		}
	}
}

void Image_buffer8::draw_beveled_box(
		int x, int y, int w, int h, int depth, uint8 colfill, uint8 coltop,
		uint8 coltr, uint8 colbottom, uint8 colbl) {
	// Need to shrink by 1 so lines are drawn in the right places
	w--;
	h--;
	// draw beveled edge

	while (depth--) {
		// corner pixels
		if (colfill != 0xff) {
			// top left
			put_pixel8(colfill, x, y);
			// bottom right
			put_pixel8(colfill, x + w, y + h);
		}
		// top right
		if (coltr != 0xff) {
			put_pixel8(coltr, x + w, y);
		}

		// bottomleft
		if (colbl != 0xff) {
			put_pixel8(colbl, x, y + h);
		}

		// Lines

		if (coltop != 0xff) {
			// top
			fill_hline8(coltop, w - 1, x + 1, y);
			// right
			draw_line8(coltop, x + w, y + 1, x + w, y + h - 1, nullptr);
		}

		if (colbottom != 0xff) {
			// bottom
			fill_hline8(colbottom, w - 1, x + 1, y + h);
			// left
			draw_line8(colbottom, x, y + 1, x, (y + h) - 1, nullptr);
		}

		// adjust the box size to be inside of what we just drew
		x++;
		y++;
		w -= 2;
		h -= 2;
	}
	// expand by 1 undoing what was done at the top
	w++;
	h++;
	// Fill centre
	if (colfill != 0xff) {
		fill8(colfill, w, h, x, y);
	}
}
