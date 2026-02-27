/*
 *  Copyright (C) 2009-2022 Exult Team
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

#ifndef INCL_SCALE_POINT_H
#define INCL_SCALE_POINT_H 1

#include "ignore_unused_variable_warning.h"

/**
 ** Note: This file should only be included by source files that use the
 ** templates below; the templates will only be instantiated when they
 ** are used anyway.
 **/

//
// Point Sampling Scaler; adapted from Pentagram.
//

template <class Source_pixel, class Dest_pixel, class Manip_pixels>
void Scale_point(
		Source_pixel* source,                // ->source pixels.
		int srcx, int srcy,                  // Start of rectangle within src.
		int srcw, int srch,                  // Dims. of rectangle.
		int                 sline_pixels,    // Pixels (words)/line for source.
		int                 sheight,         // Source height.
		Dest_pixel*         dest,            // ->dest pixels.
		int                 dline_pixels,    // Pixels (words)/line for dest.
		const Manip_pixels& manip,           // Manipulator methods.
		const int           factor           // Scale factor
) {
	ignore_unused_variable_warning(sheight);
	// Source buffer pointers
	Source_pixel* from    = source + srcy * sline_pixels + srcx;
	Source_pixel* limit_x = from + srcw;
	Source_pixel* limit_y = from + srch * sline_pixels;
	const int     sdiff   = sline_pixels - srcw;

	// Dest buffer pointers
	Dest_pixel* to = (dest + factor * srcy * dline_pixels + factor * srcx);

	if (factor == 2) {
		Dest_pixel* to2   = to + dline_pixels;
		const int   pdiff = 2 * dline_pixels - 2 * srcw;
		// Src loop Y
		do {
			// Src loop X
			do {
				Dest_pixel p = manip.copy(*from++);
				*(to + 0)    = p;
				*(to + 1)    = p;
				*(to2 + 0)   = p;
				*(to2 + 1)   = p;
				to += 2;
				to2 += 2;
			} while (from != limit_x);
			to += pdiff;
			to2 += pdiff;

			from += sdiff;
			limit_x += sline_pixels;
		} while (from != limit_y);
	} else {
		Dest_pixel* px_end = to + factor;
		Dest_pixel* py_end = to + factor * dline_pixels;

		const int block_h     = dline_pixels * factor;
		const int block_xdiff = dline_pixels - factor;
		const int pdiff       = block_h - factor * srcw;
		// Src loop Y
		do {
			// Src loop X
			do {
				Dest_pixel p = manip.copy(*from++);
				// Inner loops
				// Dest loop Y
				do {
					// Dest loop X
					do {
						*to++ = p;
					} while (to != px_end);
					to += block_xdiff;
					px_end += dline_pixels;
				} while (to != py_end);

				to += factor - block_h;
				px_end += factor - block_h;
				py_end += factor;
			} while (from != limit_x);
			to += pdiff;
			py_end += pdiff;
			px_end += pdiff;

			from += sdiff;
			limit_x += sline_pixels;
		} while (from != limit_y);
	}
}

// 8-bit Point Sampling Scaler
void Scale_point(
		const unsigned char* source,       // ->source pixels.
		const int srcx, const int srcy,    // Start of rectangle within src.
		const int srcw, const int srch,    // Dims. of rectangle.
		const int      sline_pixels,       // Pixels (words)/line for source.
		const int      sheight,            // Source height.
		unsigned char* dest,               // ->dest pixels.
		const int      dline_pixels,       // Pixels (words)/line for dest.
		const int      factor              // Scale factor
);

#endif
