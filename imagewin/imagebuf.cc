/**
 ** Imagebuf.cc - A window to blit images into.
 **
 ** Written: 8/13/98 - JSF
 **/

/*
Copyright (C) 1998 Jeffrey S. Freedman

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "imagebuf.h"

/*
 *  Create buffer.
 */

Image_buffer::Image_buffer(
    unsigned int w,         // Desired width, height.
    unsigned int h,
    int dpth            // Depth (bits/pixel).
) : width(w), height(h),
	offset_x(0), offset_y(0),
	depth(dpth), bits(nullptr), line_width(w),
	clipx(0), clipy(0),
	clipw(w), cliph(h) {
	switch (depth) {        // What depth?
	case 8:
		pixel_size = 1;
		break;
	case 15:
	case 16:
		pixel_size = 2;
		break;
	case 32:
		pixel_size = 4;
		break;
	}
}


