/*
 * Scale2X algorithm by Andrea Mazzoleni.
 *
 * Copyright (C) 2001-2011 Andrea Mazzoleni
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 */

#ifndef INCL_SCALE_2X_H
#define INCL_SCALE_2X_H 1

/**
 ** Note: This file should only be included by source files that use the
 ** templates below; the templates will only be instantiated when they
 ** are used anyway.
 **/

void Scale2x_noblur(
    const unsigned char *src1,  // ->source pixels.
    const int srcx, const int srcy, // Start of rectangle within src.
    const int srcw, const int srch, // Dims. of rectangle.
    const int sline_pixels,     // Pixels (words)/line for source.
    const int sheight,      // Source height.
    unsigned char *dest,        // ->dest pixels.
    const int dline_pixels      // Pixels (words)/line for dest.
);

template <class Dest_pixel>
static inline void cycle_buffers(
    Dest_pixel *&b1,
    Dest_pixel *&b2,
    Dest_pixel *&b3) {
	Dest_pixel *b0 = b1;
	b1 = b2;
	b2 = b3;
	b3 = b0;
}

// Expand `row' into the destination pixel format.
template <class Source_pixel, class Dest_pixel, class Manip_pixels>
static inline void fill_with_rgb(
    Source_pixel *from,
    Dest_pixel *row,
    int width,      // number of pixels to write into 'row'
    const Manip_pixels &manip
) {
	Dest_pixel *stop = row + width;
	while (row < stop)
		*row++ = manip.copy(*from++);
}

/*
 *  Scale2x modification. This version is optimized for the cases where the
 *  Source_pixel and Dest_pixel pixels have different sizes. Instead of using
 *  manip each time a pixel write is needed, and considering that each pixel
 *  will likely be used at least once, and possibly up to 9 times, each pixel
 *  row in the source is expanded to the Dest_pixel format in a triple-row
 *  buffer. These buffers rotated as each row is finished, meaning that each
 *  pixel is decoded *once*.
 */
template <class Source_pixel, class Dest_pixel, class Manip_pixels>
void Scale2x_noblur(
    Source_pixel *source,       // ->source pixels.
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
	static Dest_pixel *rgb_row_prev  = nullptr;
	static Dest_pixel *rgb_row_cur  = nullptr;
	static Dest_pixel *rgb_row_next = nullptr;
	if (buff_size < sline_pixels) {
		delete [] rgb_row_prev;
		delete [] rgb_row_cur;
		delete [] rgb_row_next;
		buff_size = sline_pixels;
		rgb_row_prev  = new Dest_pixel[buff_size];
		rgb_row_cur  = new Dest_pixel[buff_size];
		rgb_row_next = new Dest_pixel[buff_size];
	}

	// These are used for checking height limitations
	Source_pixel *ptr1 = source + srcy * sline_pixels + srcx;
	Source_pixel *ptr0 = ptr1 - sline_pixels;   // ->prev. row.
	Source_pixel *ptr2 = ptr1 + sline_pixels;   // ->next row.
	// The limiting line desired.
	Source_pixel *limit_y = ptr1 + srch * sline_pixels;
	// Very end of source surface:
	Source_pixel *end_src = source + sheight * sline_pixels;

	// Check for edge pixels.
	int to_right_edge = srcx + srcw == sline_pixels ? 1 : 0;
	int edge_off = srcx == 0 ? 0 : 1;
	int copy_width = srcw + 1 - to_right_edge + edge_off;
	// Initial fill of the buffers.
	if (ptr0 >= source)     // But don't go before row 0.
		fill_with_rgb(ptr0 - edge_off, rgb_row_prev, copy_width, manip);
	fill_with_rgb(ptr1 - edge_off, rgb_row_cur, copy_width, manip);
	fill_with_rgb(ptr2 - edge_off, rgb_row_next, copy_width, manip);

	// Point to start of dest area.
	dest += srcy * 2 * dline_pixels + srcx * 2;
	Dest_pixel *dest0 = dest;
	Dest_pixel *dest1 = dest + dline_pixels;


	// These are used for the actual algorithm. They require a pixel to the
	// left of the source rectangle except when this rectangle starts at the
	// left edge of the screen.
	Dest_pixel *src1 = rgb_row_cur + edge_off;
	Dest_pixel *src0 = (ptr0 < source ? rgb_row_cur : rgb_row_prev) + edge_off;
	Dest_pixel *src2 = rgb_row_next + edge_off;

	// This is the line limit; if going to edge, stop 1 pixel before it.
	Dest_pixel *limit_x = src1 + srcw - to_right_edge;
	while (ptr1 < limit_y) {
		if (!edge_off) {    // First pixel.
			dest0[0] = dest1[0] = src1[0];
			if (src1[1] == src0[0] && src2[0] != src0[0])
				dest0[1] = src0[0];
			else
				dest0[1] = src1[0];
			if (src1[1] == src2[0] && src0[0] != src2[0])
				dest1[1] = src2[0];
			else
				dest1[1] = src1[0];
			++src0;
			++src1;
			++src2;
			dest0 += 2;
			dest1 += 2;
		}
		// Middle pixels.
		while (src1 < limit_x) {
			if (src1[-1] == src0[0] && src2[0] != src0[0] &&
			        src1[1] != src0[0])
				dest0[0] = src0[0];
			else
				dest0[0] = src1[0];
			if (src1[1] == src0[0] && src2[0] != src0[0] &&
			        src1[-1] != src0[0])
				dest0[1] = src0[0];
			else
				dest0[1] = src1[0];
			if (src1[-1] == src2[0] && src0[0] != src2[0] &&
			        src1[1] != src2[0])
				dest1[0] = src2[0];
			else
				dest1[0] = src1[0];
			if (src1[1] == src2[0] && src0[0] != src2[0] &&
			        src1[-1] != src2[0])
				dest1[1] = src2[0];
			else
				dest1[1] = src1[0];
			++src0;
			++src1;
			++src2;
			dest0 += 2;
			dest1 += 2;
		}
		if (to_right_edge) {
			// End pixel in row.
			if (src1[-1] == src0[0] && src2[0] != src0[0])
				dest0[0] = src0[0];
			else
				dest0[0] = src1[0];
			if (src1[-1] == src2[0] && src0[0] != src2[0])
				dest1[0] = src2[0];
			else
				dest1[0] = src1[0];
			dest0[1] = src1[0];
			dest1[1] = src1[0];
			++src0;
			++src1;
			++src2;
			dest0 += 2;
			dest1 += 2;
		}
		ptr1 += sline_pixels;
		ptr2 += sline_pixels;
		// Here, we rotate the 3 buffers and only fill the one corresponding
		// to the now unused previous row.
		cycle_buffers(rgb_row_prev, rgb_row_cur, rgb_row_next);
		src0 = rgb_row_prev + edge_off;
		src1 = rgb_row_cur + edge_off;
		if (ptr2 > end_src)
			src2 = src1;    // On last row.
		else {
			fill_with_rgb(ptr2 - edge_off, rgb_row_next, copy_width, manip);
			src2 = rgb_row_next + edge_off;
		}
		limit_x = src1 + srcw - to_right_edge;
		dest1 += dline_pixels - 2 * srcw;
		dest0 = dest1;
		dest1 += dline_pixels;
	}
}

/*
 *  Scale2x. This version is optimized for the cases where the Source_pixel
 *  and Dest_pixel pixels have the same size/format; but it isn't a requirement.
 */
template <class Pixel, class Manip_pixels>
void Scale2x_noblur(
    Pixel *source,      // ->source pixels.
    int srcx, int srcy,     // Start of rectangle within src.
    int srcw, int srch,     // Dims. of rectangle.
    int sline_pixels,       // Pixels (words)/line for source.
    int sheight,            // Source height.
    Pixel *dest,        // ->dest pixels.
    int dline_pixels,       // Pixels (words)/line for dest.
    const Manip_pixels &manip   // Manipulator methods.
) {
	dest += srcy * 2 * dline_pixels + srcx * 2;
	Pixel *dest0 = dest;
	Pixel *dest1 = dest + dline_pixels;
	// ->current row.
	Pixel *src1 = source + srcy * sline_pixels + srcx;
	Pixel *src0 = src1 - sline_pixels;  // ->prev. row.
	Pixel *src2 = src1 + sline_pixels;  // ->next row.
	Pixel *limit_y = src1 + srch * sline_pixels;
	Pixel *limit_x = src1 + srcw;
	// Very end of source surface:
	Pixel *end_src = source + sheight * sline_pixels;

	if (src0 < source)
		src0 = src1;        // Don't go before row 0.
	if (srcx + srcw == sline_pixels)    // Going to right edge?
		limit_x--;      // Stop 1 pixel before it.
	while (src1 < limit_y) {
		if (src2 > end_src)
			src2 = src1;    // On last row.
		if (srcx == 0) {    // First pixel.
			dest0[0] = dest1[0] = manip.copy(src1[0]);
			if (src1[1] == src0[0] && src2[0] != src0[0])
				dest0[1] = manip.copy(src0[0]);
			else
				dest0[1] = manip.copy(src1[0]);
			if (src1[1] == src2[0] && src0[0] != src2[0])
				dest1[1] = manip.copy(src2[0]);
			else
				dest1[1] = manip.copy(src1[0]);
			++src0;
			++src1;
			++src2;
			dest0 += 2;
			dest1 += 2;
		}
		// Middle pixels.
		while (src1 < limit_x) {
			if (src1[-1] == src0[0] && src2[0] != src0[0] &&
			        src1[1] != src0[0])
				dest0[0] = manip.copy(src0[0]);
			else
				dest0[0] = manip.copy(src1[0]);
			if (src1[1] == src0[0] && src2[0] != src0[0] &&
			        src1[-1] != src0[0])
				dest0[1] = manip.copy(src0[0]);
			else
				dest0[1] = manip.copy(src1[0]);
			if (src1[-1] == src2[0] && src0[0] != src2[0] &&
			        src1[1] != src2[0])
				dest1[0] = manip.copy(src2[0]);
			else
				dest1[0] = manip.copy(src1[0]);
			if (src1[1] == src2[0] && src0[0] != src2[0] &&
			        src1[-1] != src2[0])
				dest1[1] = manip.copy(src2[0]);
			else
				dest1[1] = manip.copy(src1[0]);
			++src0;
			++src1;
			++src2;
			dest0 += 2;
			dest1 += 2;
		}
		if (srcx + srcw == sline_pixels) {
			// End pixel in row.
			if (src1[-1] == src0[0] && src2[0] != src0[0])
				dest0[0] = manip.copy(src0[0]);
			else
				dest0[0] = manip.copy(src1[0]);
			if (src1[-1] == src2[0] && src0[0] != src2[0])
				dest1[0] = manip.copy(src2[0]);
			else
				dest1[0] = manip.copy(src1[0]);
			dest0[1] = manip.copy(src1[0]);
			dest1[1] = manip.copy(src1[0]);
			++src0;
			++src1;
			++src2;
			dest0 += 2;
			dest1 += 2;
		}
		src0 += sline_pixels - srcw;
		src1 += sline_pixels - srcw;
		src2 += sline_pixels - srcw;
		dest1 += dline_pixels - 2 * srcw;
		if (src0 == src1)   // End of first row?
			src0 -= sline_pixels;
		limit_x += sline_pixels;
		dest0 = dest1;
		dest1 += dline_pixels;
	}
}

#endif
