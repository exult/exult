/*
 *  Copyright (C) 2009 Exult Team
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
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 */

#ifndef INCL_SCALE_BILINEAR_H
#define INCL_SCALE_BILINEAR_H   1

/**
 ** Note: This file should only be included by source files that use the
 ** templates below; the templates will only be instantiated when they
 ** are used anyway.
 **/

using COMPONENT = unsigned int;


// fill `row' with the the disassembled color components from the original
// pixel values in `from'; if we run out of source pixels, just keep copying
// the last one we got
template <class Source_pixel, class Manip_pixels>
inline void fill_rgb_row(
    Source_pixel *from,
    int src_width,  // number of pixels to read from 'from'
    COMPONENT *row,
    int width,      // number of pixels to write into 'row'
    const Manip_pixels &manip
) {
	COMPONENT *copy_start = row + src_width * 3;
	COMPONENT *all_stop = row + width * 3;
	while (row < copy_start) {
		COMPONENT &r = *row++;
		COMPONENT &g = *row++;
		COMPONENT &b = *row++;
		manip.split_source(*from++, r, g, b);
	}
	// any remaining elements to be written to 'row' are a replica of the
	// preceding pixel
	COMPONENT *p = row - 3;
	while (row < all_stop) {
		// we're guaranteed three elements per pixel; could unroll the loop
		// further, especially with a Duff's Device, but the gains would be
		// probably limited (judging by profiler output)
		*row++ = *p++;
		*row++ = *p++;
		*row++ = *p++;
	}
}


template <class Source_pixel, class Dest_pixel, class Manip_pixels>
void Scale_2xBilinear(
    Source_pixel *source,       // ->source pixels.
    int srcx, int srcy,     // Start of rectangle within src.
    int srcw, int srch,     // Dims. of rectangle.
    int sline_pixels,       // Pixels (words)/line for source.
    int sheight,            // Source height.
    Dest_pixel *dest,       // ->dest pixels.
    int dline_pixels,       // Pixels (words)/line for dest.
    const Manip_pixels &manip   // Manipulator methods.
) {
	Source_pixel *from = source + srcy * sline_pixels + srcx;
	Dest_pixel *to = dest + 2 * srcy * dline_pixels + 2 * srcx;
	Dest_pixel *to_odd = to + dline_pixels;

	// the following are static because we don't want to be freeing and
	// reallocating space on each call, as new[]s are usually very
	// expensive; we do allow it to grow though
	static int buff_size = 0;
	static COMPONENT *rgb_row_cur  = nullptr;
	static COMPONENT *rgb_row_next = nullptr;
	if (buff_size < sline_pixels + 1) {
		delete [] rgb_row_cur;
		delete [] rgb_row_next;
		buff_size = sline_pixels + 1;
		rgb_row_cur  = new COMPONENT[buff_size * 3];
		rgb_row_next = new COMPONENT[buff_size * 3];
	}

	int from_width = sline_pixels - srcx;
	if (srcw + 1 < from_width)
		from_width = srcw + 1;

	fill_rgb_row(from, from_width, rgb_row_cur, srcw + 1, manip);

	for (int y = 0; y < srch; y++) {
		Source_pixel *from_orig = from;
		Dest_pixel *to_orig = to;

		if (y + 1 < sheight)
			fill_rgb_row(from + sline_pixels, from_width, rgb_row_next,
			             srcw + 1, manip);
		else
			fill_rgb_row(from, from_width, rgb_row_next, srcw + 1, manip);

		// every pixel in the src region, is extended to 4 pixels in the
		// destination, arranged in a square 'quad'; if the current src
		// pixel is 'a', then in what follows 'b' is the src pixel to the
		// right, 'c' is the src pixel below, and 'd' is the src pixel to
		// the right and down
		COMPONENT *cur_row  = rgb_row_cur;
		COMPONENT *next_row = rgb_row_next;
		COMPONENT *ar = cur_row++;
		COMPONENT *ag = cur_row++;
		COMPONENT *ab = cur_row++;
		COMPONENT *cr = next_row++;
		COMPONENT *cg = next_row++;
		COMPONENT *cb = next_row++;
		for (int x = 0; x < srcw; x++) {
			COMPONENT *br = cur_row++;
			COMPONENT *bg = cur_row++;
			COMPONENT *bb = cur_row++;
			COMPONENT *dr = next_row++;
			COMPONENT *dg = next_row++;
			COMPONENT *db = next_row++;

			// upper left pixel in quad: just copy it in
			*to++ = manip.rgb(*ar, *ag, *ab);

			// upper right
			*to++ = manip.rgb((*ar + *br) >> 1, (*ag + *bg) >> 1, (*ab + *bb) >> 1);

			// lower left
			*to_odd++ = manip.rgb((*ar + *cr) >> 1, (*ag + *cg) >> 1, (*ab + *cb) >> 1);

			// lower right
			*to_odd++ = manip.rgb((*ar + *br + *cr + *dr) >> 2,
			                      (*ag + *bg + *cg + *dg) >> 2,
			                      (*ab + *bb + *cb + *db) >> 2);

			// 'b' becomes 'a', 'd' becomes 'c'
			ar = br;
			ag = bg;
			ab = bb;
			cr = dr;
			cg = dg;
			cb = db;
		}

		// the "next" rgb row becomes the current; the old current rgb row is
		// recycled and serves as the new "next" row
		COMPONENT *temp;
		temp = rgb_row_cur;
		rgb_row_cur = rgb_row_next;
		rgb_row_next = temp;

		// update the pointers for start of next pair of lines
		from = from_orig + sline_pixels;
		to = to_orig + 2 * dline_pixels;
		to_odd = to + dline_pixels;
	}
}


template <class Source_pixel, class Dest_pixel, class Manip_pixels>
void Scale_2xBilinearPlus(
    Source_pixel *source,       // ->source pixels.
    int srcx, int srcy,     // Start of rectangle within src.
    int srcw, int srch,     // Dims. of rectangle.
    int sline_pixels,       // Pixels (words)/line for source.
    int sheight,            // Source height.
    Dest_pixel *dest,       // ->dest pixels.
    int dline_pixels,       // Pixels (words)/line for dest.
    const Manip_pixels &manip   // Manipulator methods.
) {
	Source_pixel *from = source + srcy * sline_pixels + srcx;
	Dest_pixel *to = dest + 2 * srcy * dline_pixels + 2 * srcx;
	Dest_pixel *to_odd = to + dline_pixels;

	// the following are static because we don't want to be freeing and
	// reallocating space on each call, as new[]s are usually very
	// expensive; we do allow it to grow though
	static int buff_size = 0;
	static COMPONENT *rgb_row_cur  = nullptr;
	static COMPONENT *rgb_row_next = nullptr;
	if (buff_size < sline_pixels + 1) {
		delete [] rgb_row_cur;
		delete [] rgb_row_next;
		buff_size = sline_pixels + 1;
		rgb_row_cur  = new COMPONENT[buff_size * 3];
		rgb_row_next = new COMPONENT[buff_size * 3];
	}

	int from_width = sline_pixels - srcx;
	if (srcw + 1 < from_width)
		from_width = srcw + 1;

	fill_rgb_row(from, from_width, rgb_row_cur, srcw + 1, manip);

	for (int y = 0; y < srch; y++) {
		Source_pixel *from_orig = from;
		Dest_pixel *to_orig = to;

		if (y + 1 < sheight)
			fill_rgb_row(from + sline_pixels, from_width, rgb_row_next,
			             srcw + 1, manip);
		else
			fill_rgb_row(from, from_width, rgb_row_next, srcw + 1, manip);

		// every pixel in the src region, is extended to 4 pixels in the
		// destination, arranged in a square 'quad'; if the current src
		// pixel is 'a', then in what follows 'b' is the src pixel to the
		// right, 'c' is the src pixel below, and 'd' is the src pixel to
		// the right and down
		COMPONENT *cur_row  = rgb_row_cur;
		COMPONENT *next_row = rgb_row_next;
		COMPONENT *ar = cur_row++;
		COMPONENT *ag = cur_row++;
		COMPONENT *ab = cur_row++;
		COMPONENT *cr = next_row++;
		COMPONENT *cg = next_row++;
		COMPONENT *cb = next_row++;
		for (int x = 0; x < srcw; x++) {
			COMPONENT *br = cur_row++;
			COMPONENT *bg = cur_row++;
			COMPONENT *bb = cur_row++;
			COMPONENT *dr = next_row++;
			COMPONENT *dg = next_row++;
			COMPONENT *db = next_row++;

			// upper left pixel in quad: just copy it in
			//*to++ = manip.rgb(*ar, *ag, *ab);
#ifdef USE_ORIGINAL_BILINEAR_PLUS
			*to++ = manip.rgb(
			            (((*ar) << 2) + ((*ar)) + (*cr + *br + *br)) >> 3,
			            (((*ag) << 2) + ((*ag)) + (*cg + *bg + *bg)) >> 3,
			            (((*ab) << 2) + ((*ab)) + (*cb + *bb + *bb)) >> 3);
#else
			*to++ = manip.rgb(
			            (((*ar) << 3) + ((*ar) << 1) + (*cr + *br + *br + *cr)) >> 4,
			            (((*ag) << 3) + ((*ag) << 1) + (*cg + *bg + *bg + *cg)) >> 4,
			            (((*ab) << 3) + ((*ab) << 1) + (*cb + *bb + *bb + *cb)) >> 4);
#endif

			// upper right
			*to++ = manip.rgb((*ar + *br) >> 1, (*ag + *bg) >> 1, (*ab + *bb) >> 1);

			// lower left
			*to_odd++ = manip.rgb((*ar + *cr) >> 1, (*ag + *cg) >> 1, (*ab + *cb) >> 1);

			// lower right
			*to_odd++ = manip.rgb((*ar + *br + *cr + *dr) >> 2,
			                      (*ag + *bg + *cg + *dg) >> 2,
			                      (*ab + *bb + *cb + *db) >> 2);

			// 'b' becomes 'a', 'd' becomes 'c'
			ar = br;
			ag = bg;
			ab = bb;
			cr = dr;
			cg = dg;
			cb = db;
		}

		// the "next" rgb row becomes the current; the old current rgb row is
		// recycled and serves as the new "next" row
		COMPONENT *temp;
		temp = rgb_row_cur;
		rgb_row_cur = rgb_row_next;
		rgb_row_next = temp;

		// update the pointers for start of next pair of lines
		from = from_orig + sline_pixels;
		to = to_orig + 2 * dline_pixels;
		to_odd = to + dline_pixels;
	}
}

#endif
