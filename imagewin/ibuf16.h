/*
 *  ibuf16.h - 16-bit image buffer.
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

#ifndef INCL_IBUF16
#define INCL_IBUF16 1

#include "imagebuf.h"
#include "ignore_unused_variable_warning.h"

/*
 *  A 16-bit image buffer:
 */
class Image_buffer16 : public Image_buffer {
	unsigned short *palette;    // Palette for 8-bit graphics.
	void create_default_palette();
	unsigned short *get_pixels() {  // Cast bits.
		return reinterpret_cast<unsigned short *>(bits);
	}
public:
	Image_buffer16(unsigned int w, unsigned int h, int dpth)
		: Image_buffer(w, h, dpth), palette(nullptr) {
		create_default_palette();
		bits = new unsigned char[w * h * 2];
	}
	~Image_buffer16() override {
		delete palette;
	}
	// Create pixel from r,g,b values.
	unsigned short rgb(unsigned short r, unsigned short g,
	                   unsigned short b) {
		return ((r & 0x1f) << 11) + ((g & 0x1f) << 6) + (b & 0x1f);
	}
	// Set a palette color.
	void set_palette_color(unsigned char num,
	                       unsigned short r, unsigned short g, unsigned short b) {
		palette[num] = rgb(r, g, b);
	}
	// Set palette.
	void set_palette(const unsigned char *rgbs, int maxval,
	                 int brightness = 100);
	// Rotate palette colors.
	void rotate_colors(int first, int num, int upd);
	/*
	 *  16-bit color methods.
	 */
	// Fill with given pixel.
	void fill16(unsigned short pix) override;
	// Fill rect. wth pixel.
	void fill16(unsigned short pix, int srcw, int srch,
	            int destx, int desty) override;
	// Fill line with pixel.
	virtual void fill_line16(unsigned short pix, int srcw,
	                         int destx, int desty);
	// Copy rectangle into here.
	void copy16(const unsigned short *src_pixels,
	            int srcw, int srch, int destx, int desty) override;
	/*
	 *  Depth-independent methods:
	 */
	std::unique_ptr<Image_buffer> create_another(int w, int h) override {
		return std::make_unique<Image_buffer16>(w, h, depth);
	}
	// Copy within itself.
	void copy(int srcx, int srcy, int srcw, int srch,
	          int destx, int desty) override;
	// Get rect. into another buf.
	void get(Image_buffer *dest, int srcx, int srcy) override;
	// Put rect. back.
	void put(Image_buffer *src, int destx, int desty) override;

	void fill_static(int black, int gray, int white) override;

	/*
	 *  8-bit color methods:
	 */
	// Fill with given (8-bit) value.
	void fill8(unsigned char val) override {
		Image_buffer16::fill16(palette[val]);
	}
	// Fill rect. wth pixel.
	void fill8(unsigned char val, int srcw, int srch,
	           int destx, int desty) override {
		Image_buffer16::fill16(
		    palette[val], srcw, srch, destx, desty);
	}
	// Fill line with pixel.
	void fill_line8(unsigned char val, int srcw,
	                int destx, int desty) override {
		Image_buffer16::fill_line16(palette[val], srcw,
		                            destx, desty);
	}
	// Copy rectangle into here.
	void copy8(const unsigned char *src_pixels,
	           int srcw, int srch, int destx, int desty) override;
	// Copy line to here.
	void copy_line8(const unsigned char *src_pixels, int srcw,
	                int destx, int desty) override;
	// Copy with translucency table.
	void copy_line_translucent8(
	    const unsigned char *src_pixels, int srcw,
	    int destx, int desty, int first_translucent,
	    int last_translucent, const Xform_palette *xforms) override {
		ignore_unused_variable_warning(first_translucent, last_translucent, xforms);
		copy_line8(src_pixels, srcw, destx, desty);
	}
	// Apply translucency to a line.
	void fill_line_translucent8(unsigned char val,
	                            int srcw, int destx, int desty, const Xform_palette &xform) override {
		ignore_unused_variable_warning(xform);
		fill_line8(val, srcw, destx, desty);
	}
	// Apply translucency to a rectangle
	void fill_translucent8(unsigned char val, int srcw, int srch,
	                       int destx, int desty, const Xform_palette &xform) override {
		ignore_unused_variable_warning(xform);
		fill8(val, srcw, srch, destx, desty);
	}
	// Copy rect. with transp. color.
	void copy_transparent8(const unsigned char *src_pixels, int srcw,
	                       int srch, int destx, int desty) override;
};

#endif
