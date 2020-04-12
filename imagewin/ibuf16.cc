/*
 *  ibuf16.cc - 16-bit image buffer.
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

#include "ibuf16.h"
#include "ignore_unused_variable_warning.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>
using std::rotate;
using std::memmove;

/*
 *  Create a default palette for 8-bit graphics on a 16-bit display.
 */

void Image_buffer16::create_default_palette(
) {
	delete [] palette;         // Delete old.
	palette = new unsigned short[256];
	for (int i = 0; i < 256; i++)
		palette[i] = rgb(4 * ((i >> 5) & 0x7),
		                 4 * ((i >> 2) & 0x7), 9 * (i & 0x3));
}

/*
 *  Fill with a given 16-bit value.
 */

void Image_buffer16::fill16(
    unsigned short pix
) {
	unsigned short *pixels = get_pixels();
	int cnt = line_width * height;
	for (int i = 0; i < cnt; i++)
		*pixels++ = pix;
}

/*
 *  Fill a rectangle with a 16-bit value.
 */

void Image_buffer16::fill16(
    unsigned short pix,
    int srcw, int srch,
    int destx, int desty
) {
	int srcx = 0;
	int srcy = 0;
	// Constrain to window's space.
	if (!clip(srcx, srcy, srcw, srch, destx, desty))
		return;
	unsigned short *pixels = get_pixels() + desty * line_width + destx;
	int to_next = line_width - srcw;// # pixels to next line.
	while (srch--) {        // Do each line.
		for (int cnt = srcw; cnt; cnt--)
			*pixels++ = pix;
		pixels += to_next;  // Get to start of next line.
	}
}

/*
 *  Fill a line with a given 16-bit value.
 */

void Image_buffer16::fill_line16(
    unsigned short pix,
    int srcw,
    int destx, int desty
) {
	int srcx = 0;
	int srcy = 0;
	int srch = 1;
	// Constrain to window's space.
	if (!clip(srcx, srcy, srcw, srch, destx, desty))
		return;
	unsigned short *pixels = get_pixels() + desty * line_width + destx;
	for (int cnt = srcw; cnt; cnt--)
		*pixels++ = pix;
}

/*
 *  Copy another rectangle into this one.
 */

void Image_buffer16::copy16(
    const unsigned short *src_pixels, // Source rectangle pixels.
    int srcw, int srch,     // Dimensions of source.
    int destx, int desty
) {
	int srcx = 0;
	int srcy = 0;
	int src_width = srcw;       // Save full source width.
	// Constrain to window's space.
	if (!clip(srcx, srcy, srcw, srch, destx, desty))
		return;
	unsigned short *to = get_pixels() + desty * line_width + destx;
	const unsigned short *from = src_pixels + srcy * src_width + srcx;
	int to_next = line_width - srcw;// # pixels to next line.
	int from_next = src_width - srcw;
	while (srch--) {        // Do each line.
		for (int cnt = srcw; cnt; cnt--)
			*to++ = *from++;
		to += to_next;
		from += from_next;
	}
}

/*
 *  Copy an area of the image within itself.
 */

void Image_buffer16::copy(
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
	unsigned short *to = get_pixels() + yto * line_width + destx;
	unsigned short *from = get_pixels() + yfrom * line_width + srcx;
	// Go through lines.
	while (srch--) {
		memmove(to, from, srcw * 2);
		to += ynext;
		from += ynext;
	}
}

/*
 *  Get a rectangle from here into another Image_buffer.
 */

void Image_buffer16::get(
    Image_buffer *dest,     // Copy to here.
    int srcx, int srcy      // Upper-left corner of source rect.
) {
	int srcw = dest->get_width();
	int srch = dest->get_height();
	int destx = 0;
	int desty = 0;
	// Constrain to window's space. (Note
	//   convoluted use of clip().)
	if (!clip(destx, desty, srcw, srch, srcx, srcy))
		return;
	unsigned short *to = reinterpret_cast<unsigned short *>(dest->bits) +
	                     desty * dest->line_width + destx;
	unsigned short *from = get_pixels() + srcy * line_width + srcx;
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

void Image_buffer16::put(
    Image_buffer *src,      // Copy from here.
    int destx, int desty        // Copy to here.
) {
	Image_buffer16::copy16(reinterpret_cast<unsigned short *>(src->bits),
	                       src->get_width(), src->get_height(), destx, desty);
}

/*
 * Fill buffer with random static
 */
void Image_buffer16::fill_static(int black, int gray, int white) {
	unsigned short black16 = palette[black];
	unsigned short gray16 = palette[gray];
	unsigned short white16 = palette[white];
	for (int y = 0; y < height; ++y) {
		unsigned short *p = get_pixels() + y * line_width;
		for (int x = 0; x < width; ++x) {
			switch (std::rand() % 5) {
			case 0:
			case 1:
				*p++ = black16;
				break;
			case 2:
			case 3:
				*p++ = gray16;
				break;
			case 4:
				*p++ = white16;
				break;
			}
		}
	}
}

/*
 *  Convert rgb value.
 */

inline unsigned char Get_color16(
    unsigned char val,
    int maxval,
    int brightness          // 100=normal.
) {
	unsigned int c = (static_cast<unsigned int>(val) * brightness * 32) /
	                 (100 * (maxval + 1));
	return c < 32 ? c : 31;
}

/*
 *  Set palette.
 */

void Image_buffer16::set_palette(
    const unsigned char *rgbs,        // 256 3-byte entries.
    int maxval,         // Highest val. for each color.
    int brightness          // Brightness control (100 = normal).
) {
	// Get the colors.
	for (int i = 0; i < 3 * 256; i += 3) {
		unsigned char r = Get_color16(rgbs[i], maxval, brightness);
		unsigned char g = Get_color16(rgbs[i + 1], maxval, brightness);
		unsigned char b = Get_color16(rgbs[i + 2], maxval, brightness);
		set_palette_color(i / 3, r, g, b);
	}
}

/*
 *  Rotate a range of colors.
 */

void Image_buffer16::rotate_colors(
    int first,          // Palette index of 1st.
    int num,            // # in range.
    int upd             // 1 to update hardware now.
) {
	ignore_unused_variable_warning(upd);
	int cnt = abs(num);
	unsigned short *start = palette + first;
	unsigned short *finish = start + cnt;
	if (num > 0) {
		// Shift upward.
		rotate(start, finish - 1, finish);
	} else {
		// Shift downward.
		rotate(start, start + 1, finish);
	}
	// +++++upd?
}

/*
 *  Copy another rectangle into this one.
 */

void Image_buffer16::copy8(
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
	unsigned short *to = get_pixels() + desty * line_width + destx;
	const unsigned char *from = src_pixels + srcy * src_width + srcx;
	int to_next = line_width - srcw;// # pixels to next line.
	int from_next = src_width - srcw;
	while (srch--) {        // Do each line.
		for (int cnt = srcw; cnt; cnt--)
			*to++ = palette[*from++];
		to += to_next;
		from += from_next;
	}
}

/*
 *  Copy a line into this buffer.
 */

void Image_buffer16::copy_line8(
    const unsigned char *src_pixels,  // Source rectangle pixels.
    int srcw,           // Width to copy.
    int destx, int desty
) {
	int srcx = 0;
	int srcy = 0;
	int srch = 1;
	// Constrain to window's space.
	if (!clip(srcx, srcy, srcw, srch, destx, desty))
		return;
	unsigned short *to = get_pixels() + desty * line_width + destx;
	const unsigned char *from = src_pixels + srcx;
	for (int cnt = srcw; cnt; cnt--)
		*to++ = palette[*from++];
}

/*
 *  Copy another rectangle into this one, with 0 being the transparent
 *  color.
 */

void Image_buffer16::copy_transparent8(
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
	unsigned short *to = get_pixels() + desty * line_width + destx;
	const unsigned char *from = src_pixels + srcy * src_width + srcx;
	int to_next = line_width - srcw;// # pixels to next line.
	int from_next = src_width - srcw;
	while (srch--) {        // Do each line.
		for (int cnt = srcw; cnt; cnt--, to++) {
			int chr = *from++;
			if (chr)
				*to = palette[chr];
		}
		to += to_next;
		from += from_next;
	}
}

