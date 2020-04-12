/*
 *  ibuf8.cc - 8-bit image buffer.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2013  The Exult Team
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
#  include <config.h>
#endif

#include "ibuf8.h"

#include <string>
#include <iostream>
#include <cstring>
#include <cstdlib>

#include "common_types.h"
#include "utils.h"
#include "ignore_unused_variable_warning.h"

using std::cerr;
using std::endl;

/*
 *  Copy an area of the image within itself.
 */

void Image_buffer8::copy(
    int srcx, int srcy,     // Where to start.
    int srcw, int srch,     // Dimensions to copy.
    int destx, int desty        // Where to copy to.
) {
	int ynext;
	int yfrom;
	int yto;      // Figure y stuff.
	if (srcy >= desty) {    // Moving up?
		ynext = line_width;
		yfrom = srcy;
		yto = desty;
	} else {            // Moving down.
		ynext = -line_width;
		yfrom = srcy + srch - 1;
		yto = desty + srch - 1;
	}
	unsigned char *to = bits + yto * line_width + destx;
	unsigned char *from = bits + yfrom * line_width + srcx;
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
    Image_buffer *dest,     // Copy to here.
    int srcx, int srcy      // Upper-left corner of source rect.
) {
	int srcw = dest->width;
	int srch = dest->height;
	int destx = 0;
	int desty = 0;
	// Constrain to window's space. (Note
	//   convoluted use of clip().)
	if (!clip(destx, desty, srcw, srch, srcx, srcy))
		return;
	unsigned char *to = dest->bits + desty * dest->line_width + destx;
	unsigned char *from = bits + srcy * line_width + srcx;
	// Figure # pixels to next line.
	int to_next = dest->line_width - srcw;
	int from_next = line_width - srcw;
	while (srch--) {        // Do each line.
		for (int cnt = srcw; cnt; cnt--)
			*to++ = *from++;
		to += to_next;
		from += from_next;
	}
}

/*
 *  Retrieve data from another buffer.
 */

void Image_buffer8::put(
    Image_buffer *src,      // Copy from here.
    int destx, int desty        // Copy to here.
) {
	Image_buffer8::copy8(src->bits, src->get_width(), src->get_height(), destx, desty);
}

/*
 * Fill buffer with random static
 */
void Image_buffer8::fill_static(int black, int gray, int white) {
	for (int y = 0; y < height; ++y) {
		unsigned char *p = bits + (y - offset_y) * line_width - offset_x;
		for (int x = 0; x < width; ++x) {
			switch (std::rand() % 5) {
			case 0:
			case 1:
				*p++ = black;
				break;
			case 2:
			case 3:
				*p++ = gray;
				break;
			case 4:
				*p++ = white;
				break;
			}
		}
	}
}

/*
 *  Fill with a given 8-bit value.
 */

void Image_buffer8::fill8(
    unsigned char pix
) {
	unsigned char *pixels = bits - offset_y * line_width - offset_x;
	int cnt = line_width * height;
	for (int i = 0; i < cnt; i++)
		*pixels++ = pix;
}

/*
 *  Fill a rectangle with an 8-bit value.
 */

void Image_buffer8::fill8(
    unsigned char pix,
    int srcw, int srch,
    int destx, int desty
) {
	int srcx = 0;
	int srcy = 0;
	// Constrain to window's space.
	if (!clip(srcx, srcy, srcw, srch, destx, desty))
		return;
	unsigned char *pixels = bits + desty * line_width + destx;
	int to_next = line_width - srcw;// # pixels to next line.
	while (srch--) {        // Do each line.
		for (int cnt = srcw; cnt; cnt--)
			*pixels++ = pix;
		pixels += to_next;  // Get to start of next line.
	}
}

/*
 *  Fill a line with a given 8-bit value.
 */

void Image_buffer8::fill_line8(
    unsigned char pix,
    int srcw,
    int destx, int desty
) {
	int srcx = 0;
	// Constrain to window's space.
	if (!clip_x(srcx, srcw, destx, desty))
		return;
	unsigned char *pixels = bits + desty * line_width + destx;
	std::memset(pixels, pix, srcw);
}

/*
 *  Copy another rectangle into this one.
 */

void Image_buffer8::copy8(
    const unsigned char *src_pixels,  // Source rectangle pixels.
    int srcw, int srch,     // Dimensions of source.
    int destx, int desty
) {

	if (!src_pixels) {
		cerr << "WTF! src_pixels in Image_buffer8::copy8 was 0!" << endl;
		return;
	}

	int srcx = 0;
	int srcy = 0;
	int src_width = srcw;       // Save full source width.
	// Constrain to window's space.
	if (!clip(srcx, srcy, srcw, srch, destx, desty))
		return;

#if !defined(__sparc__)
	uint32 *to = reinterpret_cast<uint32 *>(bits + desty * line_width + destx);
	const uint32 *from = reinterpret_cast<const uint32 *>(src_pixels + srcy * src_width + srcx);
	int to_next = line_width - srcw;// # pixels to next line.
	int from_next = src_width - srcw;

	// Need to know if we end dword alligned
	int end_align = srcw % 4;
	// The actual aligned width in dwords
	int aligned = srcw / 4;

	uint8 *to8;
	const uint8 *from8;

	while (srch--) {        // Do each line.
		int counter = aligned;
		while (counter--) *to++ = *from++;

		to8 = reinterpret_cast<uint8 *>(to);
		from8 = reinterpret_cast<const uint8 *>(from);

		counter = end_align;
		while (counter--) *to8++ = *from8++;

		to = reinterpret_cast<uint32 *>(to8 + to_next);
		from = reinterpret_cast<const uint32 *>(from8 + from_next);
	}
#else
	uint8 *to = bits + desty * line_width + destx;
	uint8 *from = src_pixels + srcy * src_width + srcx;
	while (srch--) {
		std::memcpy(to, from, srcw);
		from += src_width;
		to += line_width;
	}
#endif
}

/*
 *  Copy a line into this buffer.
 */

void Image_buffer8::copy_line8(
    const unsigned char *src_pixels,  // Source rectangle pixels.
    int srcw,           // Width to copy.
    int destx, int desty
) {
	int srcx = 0;
	// Constrain to window's space.
	if (!clip_x(srcx, srcw, destx, desty))
		return;
	unsigned char *to = bits + desty * line_width + destx;
	const unsigned char *from = src_pixels + srcx;
	std::memcpy(to, from, srcw);
}

/*
 *  Copy a line into this buffer where some of the colors are translucent.
 */

void Image_buffer8::copy_line_translucent8(
    const unsigned char *src_pixels,  // Source rectangle pixels.
    int srcw,           // Width to copy.
    int destx, int desty,
    int first_translucent,      // Palette index of 1st trans. color.
    int last_translucent,       // Index of last trans. color.
    const Xform_palette *xforms       // Transformers.  Need same # as
    //   (last_translucent -
    //    first_translucent + 1).
) {
	int srcx = 0;
	// Constrain to window's space.
	if (!clip_x(srcx, srcw, destx, desty))
		return;
	unsigned char *to = bits + desty * line_width + destx;
	const unsigned char *from = src_pixels + srcx;
	for (int i = srcw; i; i--) {
		// Get char., and transform.
		unsigned char c = *from++;
		if (c >= first_translucent && c <= last_translucent)
			// Use table to shift existing pixel.
			c = xforms[c - first_translucent][*to];
		*to++ = c;
	}
}

/*
 *  Apply a translucency table to a line.
 */

void Image_buffer8::fill_line_translucent8(
    unsigned char val,      // Ignored for this method.
    int srcw,
    int destx, int desty,
    const Xform_palette &xform        // Transform table.
) {
	ignore_unused_variable_warning(val);
	int srcx = 0;
	// Constrain to window's space.
	if (!clip_x(srcx, srcw, destx, desty))
		return;
	unsigned char *pixels = bits + desty * line_width + destx;
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
    int srcw, int srch,
    int destx, int desty,
    const Xform_palette &xform        // Transform table.
) {
	int srcx = 0;
	int srcy = 0;
	// Constrain to window's space.
	if (!clip(srcx, srcy, srcw, srch, destx, desty))
		return;
	unsigned char *pixels = bits + desty * line_width + destx;
	int to_next = line_width - srcw;// # pixels to next line.
	while (srch--) {        // Do each line.
		for (int cnt = srcw; cnt; cnt--, pixels++)
			*pixels = xform[*pixels];
		pixels += to_next;  // Get to start of next line.
	}
}

/*
 *  Copy another rectangle into this one, with 0 being the transparent
 *  color.
 */

void Image_buffer8::copy_transparent8(
    const unsigned char *src_pixels,  // Source rectangle pixels.
    int srcw, int srch,     // Dimensions of source.
    int destx, int desty
) {
	int srcx = 0;
	int srcy = 0;
	int src_width = srcw;       // Save full source width.
	// Constrain to window's space.
	if (!clip(srcx, srcy, srcw, srch, destx, desty))
		return;
	unsigned char *to = bits + desty * line_width + destx;
	const unsigned char *from = src_pixels + srcy * src_width + srcx;
	int to_next = line_width - srcw;// # pixels to next line.
	int from_next = src_width - srcw;
	while (srch--) {        // Do each line.
		for (int cnt = srcw; cnt; cnt--, to++) {
			int chr = *from++;
			if (chr)
				*to = chr;
		}
		to += to_next;
		from += from_next;
	}
}

// Slightly Optimized RLE Painter
void Image_buffer8::paint_rle(int xoff, int yoff, const unsigned char *inptr) {
	const uint8 *in = inptr;
	int scanlen;
	const int right = clipx + clipw;
	const int bottom = clipy + cliph;

	while ((scanlen = Read2(in)) != 0) {
		// Get length of scan line.
		int encoded = scanlen & 1; // Is it encoded?
		scanlen = scanlen >> 1;
		int scanx = xoff + static_cast<sint16>(Read2(in));
		int scany = yoff + static_cast<sint16>(Read2(in));

		// Is there somthing on screen?
		bool on_screen = true;
		if (scanx >= right || scany >= bottom || scany < clipy || scanx + scanlen < clipx)
			on_screen = false;

		if (!encoded) { // Raw data?
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
				if (skip < 0) skip = 0;

				// Is there anything to put on the screen?
				if (skip < scanlen) {
					unsigned char *dest = bits + scany * line_width + scanx;
					const unsigned char *end = in + scanlen - skip;
					while (in < end) *dest++ = *in++;
					in += skip;
					continue;
				}
			}
			in += scanlen;
			continue;
		} else { // Encoded
			unsigned char *dest = bits + scany * line_width + scanx;

			while (scanlen) {
				unsigned char bcnt = *in++;
				// Repeat next char. if odd.
				int repeat = bcnt & 1;
				bcnt = bcnt >> 1; // Get count.

				// Only do the complex calcs if we think it could be on screen
				if (on_screen && scanx < right && scanx + bcnt > clipx) {
					if (repeat) { // Const Colour
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
						if (skip < 0) skip = 0;

						// Is there anything to put on the screen?
						if (skip < bcnt) {
							unsigned char col = *in++;
							unsigned char *end = dest + bcnt - skip;
							while (dest < end) *dest++ = col;

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
						if (skip < 0) skip = 0;

						// Is there anything to put on the screen?
						if (skip < bcnt) {
							unsigned char *end = dest + bcnt - skip;
							while (dest < end) *dest++ = *in++;
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
				if (!repeat) in += bcnt;
				else ++in;
			}
		}
	}
}

// Slightly Optimized RLE Painter
void Image_buffer8::paint_rle_remapped(
    int xoff, int yoff,
    const unsigned char *inptr,
    const unsigned char *&trans
) {
	const uint8 *in = inptr;
	int scanlen;
	const int right = clipx + clipw;
	const int bottom = clipy + cliph;

	while ((scanlen = Read2(in)) != 0) {
		// Get length of scan line.
		int encoded = scanlen & 1; // Is it encoded?
		scanlen = scanlen >> 1;
		int scanx = xoff + static_cast<sint16>(Read2(in));
		int scany = yoff + static_cast<sint16>(Read2(in));

		// Is there somthing on screen?
		bool on_screen = true;
		if (scanx >= right || scany >= bottom || scany < clipy || scanx + scanlen < clipx)
			on_screen = false;

		if (!encoded) { // Raw data?
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
				if (skip < 0) skip = 0;

				// Is there anything to put on the screen?
				if (skip < scanlen) {
					unsigned char *dest = bits + scany * line_width + scanx;
					const unsigned char *end = in + scanlen - skip;
					while (in < end) *dest++ = trans[*in++];
					in += skip;
					continue;
				}
			}
			in += scanlen;
			continue;
		} else { // Encoded
			unsigned char *dest = bits + scany * line_width + scanx;

			while (scanlen) {
				unsigned char bcnt = *in++;
				// Repeat next char. if odd.
				int repeat = bcnt & 1;
				bcnt = bcnt >> 1; // Get count.

				// Only do the complex calcs if we think it could be on screen
				if (on_screen && scanx < right && scanx + bcnt > clipx) {
					if (repeat) { // Const Colour
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
						if (skip < 0) skip = 0;

						// Is there anything to put on the screen?
						if (skip < bcnt) {
							unsigned char col = *in++;
							unsigned char *end = dest + bcnt - skip;
							while (dest < end) *dest++ = trans[col];

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
						if (skip < 0) skip = 0;

						// Is there anything to put on the screen?
						if (skip < bcnt) {
							unsigned char *end = dest + bcnt - skip;
							while (dest < end) *dest++ = trans[*in++];
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
				if (!repeat) in += bcnt;
				else ++in;
			}
		}
	}
}

/*
 *  Convert this image to 32-bit RGBA and return the allocated buffer.
 */

unsigned char *Image_buffer8::rgba(
    const unsigned char *pal,     // 3*256 bytes (rgbrgbrgb...).
    unsigned char transp,       // Transparent value.
    bool &rotate,               // Flag set to true if any of the colors rotate.
    int first_rotate,           // First color that rotates.
    int last_rotate,            // Last color that rotates.
    int first_translucent,      // Palette index of 1st trans. color.
    int last_translucent,       // Index of last trans. color.
    const Xform_palette *xforms,      // Transformers.  Need same # as
    //   (last_translucent -
    //    first_translucent + 1).
    int alpha       // What to use as alpha
) {
	int cnt = line_width * height;  // Allocate destination buffer.
	uint32 *buf32 = new uint32[cnt];
	uint32 *ptr32 = buf32;
	unsigned char *pixels = bits - offset_y * line_width - offset_x;
	rotate = false;
	for (int i = 0; i < cnt; i++) {
		unsigned char pix = *pixels++;
		if (pix == transp) { // Transparent?  Store Alpha=0.
			*ptr32++ = 0;
			continue;
		}
		unsigned char r;
		unsigned char g;
		unsigned char b;
		unsigned char a; // Pieces of the color.
		if (pix >= first_translucent && pix <= last_translucent) {
			// Get actual color & alpha from tbl.
			const Xform_palette &xf = xforms[pix - first_translucent];
			r = xf.r;
			g = xf.g;
			b = xf.b;
			a = xf.a;
		} else {
			if (pix >= first_rotate && pix <= last_rotate)
				rotate = true;
			r = pal[3 * pix];
			g = pal[3 * pix + 1];
			b = pal[3 * pix + 2];
			a = alpha;
		}
		*ptr32++ = (static_cast<uint32>(r) << 0) +
		           (static_cast<uint32>(g) << 8) +
		           (static_cast<uint32>(b) << 16) +
		           (static_cast<uint32>(a) << 24);
	}
	return reinterpret_cast<unsigned char *>(buf32);
}
