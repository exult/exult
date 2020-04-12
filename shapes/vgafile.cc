/*
 *  vgafile.cc - Handle access to one of the xxx.vga files.
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
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

#include <cstring>

#include "utils.h"
#include "rect.h"
#include "ibuf8.h"
#include "vgafile.h"
#include "databuf.h"
#include "Flex.h"
#include "U7file.h"
#include "exceptions.h"
#include "palette.h"

#include <cassert>
#include <vector>
#include <string>
#include <utility>
#include <map>

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::ios;
using std::memcpy;
using std::memset;
using std::ostream;
using std::vector;
using std::string;
using std::pair;
using std::unique_ptr;
using std::make_unique;

Image_buffer8 *Shape_frame::scrwin = nullptr;

#include <iomanip>
#include "items.h"

/*
 *  +++++Debugging
 */
inline void Check_file(
    ifstream &shapes
) {
	if (!shapes.good()) {
		cout << "VGA file is bad!" << endl;
		shapes.clear();
	}
}

/*
 *  Create a new frame by reflecting across a line running NW to SE.
 *
 *  May return 0.
 */

unique_ptr<Shape_frame> Shape_frame::reflect(
) {
	if (!data)
		return nullptr;
	int w = get_width();
	int h = get_height();
	if (w < h)
		w = h;
	else
		h = w;          // Use max. dim.
	auto reflected = make_unique<Shape_frame>();
	reflected->rle = true;      // Set data.
	reflected->xleft = yabove;
	reflected->yabove = xleft;
	reflected->xright = ybelow;
	reflected->ybelow = xright;
	// Create drawing area.
	Image_buffer8 ibuf(h, w);
	ibuf.fill8(255);       // Fill with 'transparent' pixel.
	// Figure origin.
	int xoff = reflected->xleft;
	int yoff = reflected->yabove;
	const uint8 *in = data.get();   // Point to data, and draw.
	int scanlen;
	while ((scanlen = Read2(in)) != 0) {
		// Get length of scan line.
		int encoded = scanlen & 1; // Is it encoded?
		scanlen = scanlen >> 1;
		short scanx = Read2(in);
		short scany = Read2(in);
		if (!encoded) {     // Raw data?
			ibuf.copy8(in, 1, scanlen,
			            xoff + scany, yoff + scanx);
			in += scanlen;
			continue;
		}
		for (int b = 0; b < scanlen;) {
			unsigned char bcnt = *in++;
			// Repeat next char. if odd.
			int repeat = bcnt & 1;
			bcnt = bcnt >> 1; // Get count.
			if (repeat) {
				unsigned char pix = *in++;
				ibuf.fill8(pix, 1, bcnt,
				            xoff + scany, yoff + scanx + b);
			} else {    // Get that # of bytes.
				ibuf.copy8(in, 1, bcnt,
				            xoff + scany, yoff + scanx + b);
				in += bcnt;
			}
			b += bcnt;
		}
	}
	reflected->create_rle(ibuf.get_bits(), w, h);
	return reflected;
}

/*
 *  Skip transparent pixels.
 *
 *  Output: Index of first non-transparent pixel (w if no more).
 *      Pixels is incremented by (delta x).
 */

static int Skip_transparent(
    unsigned char  *&pixels,    // 8-bit pixel scan line.
    int x,              // X-coord. of pixel to start with.
    int w               // Remaining width of pixels.
) {
	while (x < w && *pixels == 255) {
		x++;
		pixels++;
	}
	return x;
}

/*
 *  Split a line of pixels into runs, where a run
 *  consists of different pixels, or a repeated pixel.
 *
 *  Output: Index of end of scan line.
 */

static int Find_runs(
    unsigned short *runs,           // Each run's length is returned.
    // For each byte, bit0==repeat.
    // List ends with a 0.
    unsigned char *pixels,      // Scan line (8-bit color).
    int x,              // X-coord. of pixel to start with.
    int w               // Remaining width of pixels.
) {
	int runcnt = 0;         // Counts runs.
	while (x < w && *pixels != 255) { // Stop at first transparent pixel.
		int run = 0;        // Look for repeat.
		while (x < w - 1 && pixels[0] == pixels[1]) {
			x++;
			pixels++;
			run++;
		}
		if (run) {      // Repeated?  Count 1st, shift, flag.
			run = ((run + 1) << 1) | 1;
			x++;        // Also pass the last one.
			pixels++;
		} else do {     // Pass non-repeated run of
				//   andy length.
				x++;
				pixels++;
				run += 2;   // So we don't have to shift.
			} while (x < w && *pixels != 255 &&
			         (x == w - 1 || pixels[0] != pixels[1]));
		// Store run length.
		runs[runcnt++] = run;
	}
	runs[runcnt] = 0;       // 0-delimit list.
	return x;
}

unsigned char Shape_frame::get_topleft_pix(
    unsigned char def
) const {
	if (!data)
		return def;
	else if (!rle)
		return data[0];
	unsigned char *ptr = data.get();
	int scanlen = (ptr[1] << 8) | ptr[0];
	if (!scanlen)
		return def;
	ptr += 2;
	if (*ptr++ || *ptr++ || *ptr++ || *ptr++)
		return def;
	if ((scanlen & 1) == 0)
		return *ptr;
	return ptr[1];
}

/*
 *  Writes the frame to the given stream.
 */

void Shape_frame::write(ODataSource& out) const {
	if (rle) {
		out.write2(xright);
		out.write2(xleft);
		out.write2(yabove);
		out.write2(ybelow);
	}
	out.write(data.get(), datalen);   // The frame data.
}

void Shape_frame::save(ODataSource *shape_source) const {
	shape_source->write2(xright);
	shape_source->write2(xleft);
	shape_source->write2(yabove);
	shape_source->write2(ybelow);
	shape_source->write(data.get(), get_size());
}

/*
 *  Encode an 8-bit image into an RLE frame.
 *
 *  Output: Data is set to compressed image.
 */

void Shape_frame::create_rle(
    unsigned char *pixels,      // 8-bit uncompressed data.
    int w, int h            // Width, height.
) {
	data = encode_rle(pixels, w, h, xleft, yabove, datalen);
}

/*
 *  Encode an 8-bit image into an RLE frame.
 *
 *  Output: ->allocated RLE data.
 */

unique_ptr<unsigned char[]> Shape_frame::encode_rle(
    unsigned char *pixels,      // 8-bit uncompressed data.
    int w, int h,           // Width, height.
    int xoff, int yoff,     // Origin (xleft, yabove).
    int &datalen            // Length of RLE data returned.
) {
	// Create an oversized buffer.
	uint8 *buf = new uint8[w * h * 2 + 16 * h];
	uint8 *out = buf;
	int newx;           // Gets new x at end of a scan line.
	for (int y = 0; y < h; y++) // Go through rows.
		for (int x = 0; (x = Skip_transparent(pixels, x, w)) < w;
		        x = newx) {
			unsigned short runs[200];// Get runs.
			newx = Find_runs(runs, pixels, x, w);
			// Just 1 non-repeated run?
			if (!runs[1] && !(runs[0] & 1)) {
				int len = runs[0] >> 1;
				Write2(out, runs[0]);
				// Write position.
				Write2(out, x - xoff);
				Write2(out, y - yoff);
				memcpy(out, pixels, len);
				pixels += len;
				out += len;
				continue;
			}
			// Encoded, so write it with bit0==1.
			Write2(out, ((newx - x) << 1) | 1);
			// Write position.
			Write2(out, x - xoff);
			Write2(out, y - yoff);
			// Go through runs.
			for (int i = 0; runs[i]; i++) {
				int len = runs[i] >> 1;
				// Check for repeated run.
				if (runs[i] & 1) {
					while (len) {
						int c = len > 127
						        ? 127 : len;
						*out++ = (c << 1) | 1;
						*out++ = *pixels;
						pixels += c;
						len -= c;
					}
				} else while (len > 0) {
						int c = len > 127 ? 127 : len;
						*out++ = c << 1;
						memcpy(out, pixels, c);
						out += c;
						pixels += c;
						len -= c;
					}
			}
		}
	Write2(out, 0);         // End with 0 length.
	datalen = out - buf;        // Create buffer of correct size.
#ifdef DEBUG
	if (datalen > w * h * 2 + 16 * h)
		cout << "create_rle: datalen: " << datalen << " w: " << w
		     << " h: " << h << endl;
#endif
	auto data = make_unique<unsigned char[]>(datalen);
	memcpy(data.get(), buf, datalen);
	delete [] buf;
	return data;
}

/*
 *  Create from data.
 */

Shape_frame::Shape_frame(
    unsigned char *pixels,      // (A copy is made.)
    int w, int h,           // Dimensions.
    int xoff, int yoff,     // Xleft, yabove.
    bool setrle         // Run-length-encode.
) : xleft(xoff), xright(w - xoff - 1), yabove(yoff), ybelow(h - yoff - 1),
	rle(setrle) {
	if (!rle) {
		assert(w == c_tilesize && h == c_tilesize);
		datalen = c_num_tile_bytes;
		data = make_unique<unsigned char[]>(c_num_tile_bytes);
		memcpy(data.get(), pixels, c_num_tile_bytes);
	} else
		data = encode_rle(pixels, w, h, xleft, yabove, datalen);
}

/*
 *  Create from data.
 */

Shape_frame::Shape_frame(
    unique_ptr<unsigned char[]> pixels,      // (Gets stolen.)
    int w, int h,           // Dimensions.
    int xoff, int yoff,     // Xleft, yabove.
    bool setrle         // Run-length-encode.
) : xleft(xoff), xright(w - xoff - 1), yabove(yoff), ybelow(h - yoff - 1),
	rle(setrle) {
	if (!rle) {
		assert(w == c_tilesize && h == c_tilesize);
		datalen = c_num_tile_bytes;
		data = std::move(pixels);
	} else
		data = encode_rle(pixels.get(), w, h, xleft, yabove, datalen);
}

/*
 *  Read in a desired shape.
 *
 *  Output: # of frames.
 */

unsigned int Shape_frame::read(
    IDataSource *shapes,     // Shapes data source to read.
    uint32 shapeoff,        // Offset of shape in file.
    uint32 shapelen,        // Length expected for detecting RLE.
    int frnum           // Frame #.
) {
	int framenum = frnum;
	rle = false;
	if (!shapelen && !shapeoff) return 0;
	// Get to actual shape.
	shapes->seek(shapeoff);
	uint32 dlen = shapes->read4();
	uint32 hdrlen = shapes->read4();
	if (dlen == shapelen) {
		rle = true;     // It's run-length-encoded.
		// Figure # frames.
		int nframes = (hdrlen - 4) / 4;
		if (framenum >= nframes)// Bug out if bad frame #.
			return nframes;
		// Get frame offset, lengeth.
		uint32 frameoff;
		uint32 framelen;
		if (framenum == 0) {
			frameoff = hdrlen;
			framelen = nframes > 1 ? shapes->read4() - frameoff :
			           dlen - frameoff;
		} else {
			shapes->skip((framenum - 1) * 4);
			frameoff = shapes->read4();
			// Last frame?
			if (framenum == nframes - 1)
				framelen = dlen - frameoff;
			else
				framelen = shapes->read4() - frameoff;
		}
		// Get compressed data.
		get_rle_shape(shapes, shapeoff + frameoff, framelen);
		// Return # frames.
		return nframes;
	}
	framenum &= 31;         // !!!Guessing here.
	xleft = yabove = c_tilesize;        // Just an 8x8 bitmap.
	xright = ybelow = -1;
	shapes->seek(shapeoff + framenum * c_num_tile_bytes);
	datalen = c_num_tile_bytes;
	data = shapes->readN(c_num_tile_bytes);
	return shapelen / c_num_tile_bytes;   // That's how many frames.
}

/*
 *  Read in a Run-Length_Encoded shape.
 */

void Shape_frame::get_rle_shape(
    IDataSource *shapes,     // Shapes data source to read.
    long filepos,           // Position in file.
    long len            // Length of entire frame data.
) {
	shapes->seek(filepos);      // Get to extents.
	xright = shapes->read2();
	xleft = shapes->read2();
	yabove = shapes->read2();
	ybelow = shapes->read2();
	len -= 8;           // Subtract what we just read.
	datalen = len + 2;
	data = make_unique<unsigned char[]>(datalen);
	shapes->read(data.get(), len);
	data[len] = 0;          // 0-delimit.
	data[len + 1] = 0;
	rle = true;
}

/*
 *  Show a Run-Length_Encoded shape.
 */

void Shape_frame::paint_rle(
    Image_buffer8 *win,     // Buffer to paint in.
    int xoff, int yoff      // Where to show in iwin.
) {
	assert(rle);

	int w = get_width();
	int h = get_height();
	if (w >= c_tilesize || h >= c_tilesize)     // Big enough to check?  Off screen?
		if (!win->is_visible(xoff - xleft, yoff - yabove, w, h))
			return;

	win->paint_rle(xoff, yoff, data.get());
}

/*
 *  Show a Run-Length_Encoded shape mapped to a different palette.
 */

void Shape_frame::paint_rle_remapped(
    Image_buffer8 *win,     // Buffer to paint in.
    int xoff, int yoff,     // Where to show in iwin.
    const unsigned char *trans
) {
	assert(rle);

	int w = get_width();
	int h = get_height();
	if (w >= c_tilesize || h >= c_tilesize)     // Big enough to check?  Off screen?
		if (!win->is_visible(xoff - xleft, yoff - yabove, w, h))
			return;

	win->paint_rle_remapped(xoff, yoff, data.get(), trans);
}

/*
 *  Paint either type of shape.
 */

void Shape_frame::paint(
    Image_buffer8 *win,     // Buffer to paint in.
    int xoff, int yoff      // Where to show in iwin.
) {
	if (rle)
		paint_rle(win, xoff, yoff);
	else
		win->copy8(data.get(), c_tilesize, c_tilesize,
		           xoff - c_tilesize, yoff - c_tilesize);
}

/*
 *  Show a Run-Length_Encoded shape with translucency.
 */

void Shape_frame::paint_rle_translucent(
    Image_buffer8 *win,     // Buffer to paint in.
    int xoff, int yoff,     // Where to show in iwin.
    const Xform_palette *xforms,      // Transforms translucent colors
    int xfcnt           // Number of xforms.
) {
	assert(rle);

	int w = get_width();
	int h = get_height();
	if (w >= c_tilesize || h >= c_tilesize)     // Big enough to check?  Off screen?
		if (!win->is_visible(xoff - xleft,
		                     yoff - yabove, w, h))
			return;
	// First pix. value to transform.
	const int xfstart = 0xff - xfcnt;
	const uint8 *in = data.get();
	int scanlen;
	while ((scanlen = Read2(in)) != 0) {
		// Get length of scan line.
		int encoded = scanlen & 1; // Is it encoded?
		scanlen = scanlen >> 1;
		short scanx = Read2(in);
		short scany = Read2(in);
		if (!encoded) {     // Raw data?
			win->copy_line_translucent8(in, scanlen,
			                            xoff + scanx, yoff + scany,
			                            xfstart, 0xfe, xforms);
			in += scanlen;
			continue;
		}
		for (int b = 0; b < scanlen;) {
			unsigned char bcnt = *in++;
			// Repeat next char. if odd.
			int repeat = bcnt & 1;
			bcnt = bcnt >> 1; // Get count.
			if (repeat) {
				unsigned char pix = *in++;
				if (pix >= xfstart && pix <= 0xfe)
					win->fill_line_translucent8(pix, bcnt,
					                            xoff + scanx + b, yoff + scany,
					                            xforms[pix - xfstart]);
				else
					win->fill_line8(pix, bcnt,
					                xoff + scanx + b, yoff + scany);
			} else {    // Get that # of bytes.
				win->copy_line_translucent8(in, bcnt,
				                            xoff + scanx + b, yoff + scany,
				                            xfstart, 0xfe, xforms);
				in += bcnt;
			}
			b += bcnt;
		}
	}
}

/*
 *  Paint a shape purely by translating the pixels it occupies.  This is
 *  used for invisible NPC's.
 */

void Shape_frame::paint_rle_transformed(
    Image_buffer8 *win,     // Buffer to paint in.
    int xoff, int yoff,     // Where to show in iwin.
    const Xform_palette &xform        // Use to transform pixels.
) {
	assert(rle);

	int w = get_width();
	int h = get_height();
	if (w >= c_tilesize || h >= c_tilesize)     // Big enough to check?  Off screen?
		if (!win->is_visible(xoff - xleft,
		                     yoff - yabove, w, h))
			return;
	const uint8 *in = data.get();
	int scanlen;
	while ((scanlen = Read2(in)) != 0) {
		// Get length of scan line.
		int encoded = scanlen & 1; // Is it encoded?
		scanlen = scanlen >> 1;
		short scanx = Read2(in);
		short scany = Read2(in);
		if (!encoded) {     // Raw data?
			// (Note: 1st parm is ignored).
			win->fill_line_translucent8(0, scanlen,
			                            xoff + scanx, yoff + scany, xform);
			in += scanlen;
			continue;
		}
		for (int b = 0; b < scanlen;) {
			unsigned char bcnt = *in++;
			// Repeat next char. if odd.
			int repeat = bcnt & 1;
			bcnt = bcnt >> 1; // Get count.
			in += repeat ? 1 : bcnt;
			win->fill_line_translucent8(0, bcnt,
			                            xoff + scanx + b, yoff + scany, xform);
			b += bcnt;
		}
	}
}

/*
 *  Paint outline around a shape.
 */

void Shape_frame::paint_rle_outline(
    Image_buffer8 *win,     // Buffer to paint in.
    int xoff, int yoff,     // Where to show in win.
    unsigned char color     // Color to use.
) {
	assert(rle);

	int w = get_width();
	int h = get_height();
	if (w >= c_tilesize || h >= c_tilesize)     // Big enough to check?  Off screen?
		if (!win->is_visible(xoff - xleft,
		                     yoff - yabove, w, h))
			return;
	int firsty = -10000;        // Finds first line.
	int lasty = -10000;
	const uint8 *in = data.get();
	int scanlen;
	while ((scanlen = Read2(in)) != 0) {
		// Get length of scan line.
		int encoded = scanlen & 1; // Is it encoded?
		scanlen = scanlen >> 1;
		short scanx = Read2(in);
		short scany = Read2(in);
		int x = xoff + scanx;
		int y = yoff + scany;
		if (firsty == -10000) {
			firsty = y;
			lasty = y + h - 1;
		}
		// Put pixel at both ends.
		win->put_pixel8(color, x, y);
		win->put_pixel8(color, x + scanlen - 1, y);

		if (!encoded) {     // Raw data?
			if (y == firsty ||  // First line?
			        y == lasty)     // Last line?
				win->fill_line8(color, scanlen, x, y);
			in += scanlen;
			continue;
		}
		for (int b = 0; b < scanlen;) {
			unsigned char bcnt = *in++;
			// Repeat next char. if odd.
			int repeat = bcnt & 1;
			bcnt = bcnt >> 1; // Get count.
			if (repeat) // Pass repetition byte.
				in++;
			else        // Skip that # of bytes.
				in += bcnt;
			if (y == firsty ||  // First line?
			        y == lasty)     // Last line?
				win->fill_line8(color, bcnt, x + b, y);
			b += bcnt;
		}
	}
}

/*
 *  See if a point, relative to the shape's 'origin', actually within the
 *  shape.
 */

bool Shape_frame::has_point(
    int x, int y            // Relative to origin of shape.
) const {
	if (!rle) {         // 8x8 flat?
		return x >= -xleft && x < xright &&
		       y >= -yabove && y < ybelow;
	}
	const uint8 *in = data.get();       // Point to data.
	int scanlen;
	while ((scanlen = Read2(in)) != 0) {
		// Get length of scan line.
		int encoded = scanlen & 1; // Is it encoded?
		scanlen = scanlen >> 1;
		short scanx = Read2(in);
		short scany = Read2(in);
		// Be liberal by 1 pixel.
		if (y == scany && x >= scanx - 1 && x <= scanx + scanlen)
			return true;
		if (!encoded) {     // Raw data?
			in += scanlen;
			continue;
		}
		for (int b = 0; b < scanlen;) {
			unsigned char bcnt = *in++;
			// Repeat next char. if odd.
			int repeat = bcnt & 1;
			bcnt = bcnt >> 1; // Get count.
			if (repeat)
				in++;   // Skip pixel to repeat.
			else        // Skip that # of bytes.
				in += bcnt;
			b += bcnt;
		}
	}
	return false;         // Never found it.
}

/*
 *  Set new offset, assuming dimensions are unchanged.
 */

void Shape_frame::set_offset(
    int new_xright, int new_ybelow
) {
	if (!rle)
		return;         // Can do it for 8x8 tiles.
	int w = get_width();
	int h = get_height();
	if (new_xright > w)     // Limit to left edge.
		new_xright = w;
	if (new_ybelow > h)
		new_ybelow = h;
	int deltax = new_xright - xright;   // Get changes.
	int deltay = new_ybelow - ybelow;
	xright = new_xright;
	ybelow = new_ybelow;
	xleft = w - xright - 1;     // Update other dims.
	yabove = h - ybelow - 1;
	uint8 *in = data.get();       // Got to update all scan lines!
	int scanlen;
	while ((scanlen = MRead2(in)) != 0) {
		// Get length of scan line.
		int encoded = scanlen & 1; // Is it encoded?
		scanlen = scanlen >> 1;
		short scanx = MRead2(in);
		in -= 2;
		Write2(in, scanx + deltax);
		short scany = MRead2(in);
		in -= 2;
		Write2(in, scany + deltay);
		// Just need to scan past EOL.
		if (!encoded)       // Raw data?
			in += scanlen;
		else for (int b = 0; b < scanlen;) {
				unsigned char bcnt = *in++;
				// Repeat next char. if odd.
				int repeat = bcnt & 1;
				bcnt = bcnt >> 1; // Get count.
				if (repeat)
					in++;   // Skip pixel to repeat.
				else        // Skip that # of bytes.
					in += bcnt;
				b += bcnt;
			}
	}
}

/*
 *  Create the reflection of a shape.
 */

Shape_frame *Shape::reflect(
    vector<pair<unique_ptr<IDataSource>, bool>> const &shapes,    // shapes data source to read.
    int shapenum,           // Shape #.
    int framenum,           // Frame # without the 'reflect' bit.
    vector<int> const &counts
) {
	// Get normal frame.
	Shape_frame *normal = get(shapes, shapenum, framenum, counts);
	if (!normal)
		return nullptr;
	// Reflect it.
	unique_ptr<Shape_frame> reflected(normal->reflect());
	if (!reflected)
		return nullptr;
	framenum |= 32;         // Put back 'reflect' flag.
	// Expand list if necessary.
	if (static_cast<unsigned>(framenum) >= frames.size() - 1) {
		frames.resize(framenum + 1);
	}
	frames[framenum] = std::move(reflected);   // Store new frame.
	return frames[framenum].get();
}

/*
 *  Resize list.  This is for outside clients, and it sets num_frames.
 */

void Shape::resize(
    int newsize
) {
	if (newsize < 0 || size_t(newsize) == frames.size())
		return;
	frames.resize(newsize);
	num_frames = newsize;
	modified = true;
}

/*
 *  Call this to set num_frames, frames_size and create 'frames' list.
 */

inline void Shape::create_frames_list(
    int nframes
) {
	num_frames = nframes;
	frames.resize(nframes);
}

/*
 *  Read in a frame, or convert an existing one if reflection is
 *  desired.
 *
 *  Output: ->frame, or 0 if failed.
 */

Shape_frame *Shape::read(
    vector<pair<unique_ptr<IDataSource>, bool>> const &shapes, // Shapes data source to read.
    int shapenum,           // Shape #.
    int framenum,           // Frame # within shape.
    vector<int> const &counts,      // Number of shapes in files.
    int src
) {
	IDataSource *shp = nullptr;
	// Figure offset in "shapes.vga".
	uint32 shapeoff = 0x80 + shapenum * 8;
	uint32 shapelen = 0;

	// Check backwards for the shape file to use.
	int i = counts.size();
	if (src < 0) {
		for (auto it = shapes.crbegin(); it != shapes.crend(); ++it) {
			i--;
			if (shapenum < counts[i]) {
				IDataSource *ds = it->first.get();
				ds->seek(shapeoff);
				// Get location, length.
				int s = ds->read4();
				shapelen = ds->read4();

				if (s && shapelen) {
					shapeoff = s;
					shp = ds;
					from_patch = it->second;
					break;
				}
			}
		}
	} else if (shapenum < counts[src]) {
		IDataSource *ds = shapes[src].first.get();
		ds->seek(shapeoff);
		// Get location, length.
		int s = ds->read4();
		shapelen = ds->read4();

		if (s && shapelen) {
			shapeoff = s;
			shp = ds;
			from_patch = shapes[src].second;
		}
	}
	// The shape was not found anywhere, so leave.
	if (shp == nullptr) {
		std::cerr << "Shape num out of range: " << shapenum << std::endl;
		return nullptr;
	}
	// Read it in and get frame count.
	auto frame = make_unique<Shape_frame>();
	int nframes = frame->read(shp, shapeoff, shapelen, framenum);
	if (!num_frames)        // 1st time?
		create_frames_list(nframes);
	if (!frame->is_rle())
		framenum &= 31;     // !!Guessing.
	if (framenum >= nframes &&  // Compare against #frames in file.
	        (framenum & 32)) {  // Reflection desired?
		return reflect(shapes, shapenum, framenum & 0x1f, counts);
	}
	return store_frame(std::move(frame), framenum);
}

/*
 *  Write a shape's frames as an entry in an Exult .vga file.  Note that
 *  a .vga file is a .FLX file with one shape/entry.
 *
 *  NOTE:  This should only be called if all frames have been read.
 */

void Shape::write(
    ODataSource& out            // What to write to.
) const {
	if (!num_frames)
		return;         // Empty.
	assert(!frames.empty() && frames[0]);
	bool flat = !frames[0]->is_rle();
	// Save starting position.
	size_t startpos = out.getPos();

	size_t frnum;
	if (!flat) {
		out.write4(0);     // Place-holder for total length.
		// Also for frame locations.
		for (frnum = 0; frnum < num_frames; frnum++)
			out.write4(0);
	}
	for (frnum = 0; frnum < num_frames; frnum++) {
		Shape_frame *frame = frames[frnum].get();
		assert(frame != nullptr); // Better all be the same type.
		assert(flat == !frame->is_rle());
		if (frame->is_rle()) {
			// Get position of frame.
			size_t pos = out.getPos();
			out.seek(startpos + (frnum + 1) * 4);
			out.write4(pos - startpos);    // Store pos.
			out.seek(pos);         // Get back.
		}
		frame->write(out);
	}
	if (!flat) {
		size_t pos = out.getPos();// Ending position.
		out.seek(startpos);        // Store total length.
		out.write4(pos - startpos);
		out.seek(pos);         // And get back to end.
	}
}

/*
 *  Store frame that was read.
 *
 *  Output: ->frame, or 0 if not valid.
 */

Shape_frame *Shape::store_frame(
    unique_ptr<Shape_frame> frame,     // Frame that was read.
    int framenum            // It's frame #.
) {
	if (framenum < 0) { // Something fishy?
		cerr << "Shape::store_frame:  framenum < 0 ("
		     << framenum << " >= " << frames.size()
		     << ")" << endl;
		return nullptr;
	} else if (size_t(framenum) >= frames.size()) { // Something fishy?
		cerr << "Shape::store_frame:  framenum >= frames.size() ("
		     << framenum << " >= " << frames.size()
		     << ")" << endl;
		return nullptr;
	}
	if (frames.empty()) {  // First one?
		frames.resize(num_frames);
	}
	frames[framenum] = std::move(frame);
	return frames[framenum].get();
}

/*
 *  Create with a single frame.
 */

Shape::Shape(unique_ptr<Shape_frame> fr) : num_frames(1) {
	frames.push_back(std::move(fr));
}

/*
 *  Create with space for a given number of frames.
 */

Shape::Shape(
    int n               // # frames.
) {
	create_frames_list(n);
}

void Shape::reset() {
	frames.clear();
	num_frames = 0;
}

/*
 *  Load all frames for a single shape.  (Assumes RLE-type shape.)
 */

void Shape::load(
    IDataSource *shape_source        // datasource.
) {
	reset();
	auto frame = make_unique<Shape_frame>();
	size_t location = shape_source->getPos();
	uint32 shapelen = shape_source->read4();
	// Read frame 0 & get frame count.
	create_frames_list(frame->read(shape_source, location, shapelen, 0));
	store_frame(std::move(frame), 0);
	// Get the rest.
	for (size_t i = 1; i < num_frames; i++) {
		auto frame = make_unique<Shape_frame>();
		frame->read(shape_source, location, shapelen, i);
		store_frame(std::move(frame), i);
	}
}

/*
 *  Set desired frame.
 */

void Shape::set_frame(
    unique_ptr<Shape_frame> frame,     // Must be allocated.
    int framenum
) {
	assert(framenum >= 0 && static_cast<unsigned>(framenum) < num_frames);
	frames[framenum] = std::move(frame);
	modified = true;
}

/*
 *  Add/insert a frame.
 */

void Shape::add_frame(
    unique_ptr<Shape_frame> frame,     // Must be allocated.
    int framenum            // Insert here.
) {
	assert(framenum >= 0 && static_cast<unsigned>(framenum) <= num_frames); // Can append.
	frames.emplace(frames.begin() + framenum, std::move(frame));
	num_frames++;
	modified = true;
}

/*
 *  Delete a frame.
 */

void Shape::del_frame(
    int framenum
) {
	assert(framenum >= 0 && static_cast<unsigned>(framenum) < num_frames);
	frames.erase(frames.begin() + framenum);
	num_frames--;
	modified = true;
}

/*
 *  Read in all shapes from a single-shape file.
 */

Shape_file::Shape_file(
    const char *nm          // Path to file.
) {
	load(nm);
}

/*
 *  Read in all shapes from a single-shape file.
 */

void Shape_file::load(
    const char *nm          // Path to file.
) {
	IFileDataSource shape_source(nm);
	Shape::load(&shape_source);
}

/*
 *  Read in all shapes from a single-shape file.
 */

Shape_file::Shape_file(
    IDataSource *shape_source        // datasource.
) {
	Shape::load(shape_source);
}



// NOTE: Only works on shapes other than the special 8x8 tile-shapes
int Shape_file::get_size() const {
	int size = 4;
	for (size_t i = 0; i < num_frames; i++)
		size += frames[i]->get_size() + 4 + 8;
	return size;
}

// NOTE: Only works on shapes other than the special 8x8 tile-shapes
void Shape_file::save(ODataSource *shape_source) const {
	auto offsets = make_unique<int[]>(num_frames);
	int size;
	offsets[0] = 4 + num_frames * 4;
	size_t i;   // Blame MSVC
	for (i = 1; i < num_frames; i++)
		offsets[i] = offsets[i - 1] + frames[i - 1]->get_size() + 8;
	size = offsets[num_frames - 1] + frames[num_frames - 1]->get_size() + 8;
	shape_source->write4(size);
	for (i = 0; i < num_frames; i++)
		shape_source->write4(offsets[i]);
	for (i = 0; i < num_frames; i++) {
		frames[i]->save(shape_source);
	}
}

/*
 *  Open file.
 */

Vga_file::Vga_file(
    const char *nm,         // Path to file.
    int u7drag,         // # from u7drag.h, or -1.
    const char *nm2         // Patch file, or null.
) : u7drag_type(u7drag) {
	load(nm, nm2);
}

Vga_file::Vga_file() = default;

Vga_file::Vga_file(
    vector<pair<string, int>> const &sources,
    int u7drag      // # from u7drag.h, or -1
) : u7drag_type(u7drag) {
	load(sources);
}

/*
 *  Open file.
 */

bool Vga_file::load(
    const char *nm,
    const char *nm2,
    bool resetimports
) {
	vector<pair<string, int>> src;
	src.push_back(pair<string, int>(string(nm), -1));
	if (nm2 != nullptr)
		src.push_back(pair<string, int>(string(nm2), -1));
	return load(src, resetimports);
}

IDataSource *Vga_file::U7load(
    pair<string, int> const &resource,
    vector<pair<unique_ptr<IDataSource>, bool>> &shps
) {
	unique_ptr<IDataSource> source;
	bool is_patch = false;
	if (resource.second < 0) {
		// It is a file.
		source = make_unique<IFileDataSource>(resource.first);
		is_patch = !resource.first.compare(0, 7, "<PATCH>");
	} else {
		// It is a resource.
		source = make_unique<IExultDataSource>(resource.first, resource.second);
		is_patch = false;
	}
	if (!source->good()) {
		CERR("Resource '" << resource.first << "' not found.");
		return nullptr;
	} else {
		IDataSource *ds = source.get();
		shps.emplace_back(std::move(source), is_patch);
		return ds;
	}
}

bool Vga_file::load(
    vector<pair<string, int>> const &sources,
    bool resetimports
) {
	reset();
	if (resetimports)
		reset_imports();
	int count = sources.size();
	shape_sources.reserve(count);
	shape_cnts.reserve(count);
	bool is_good = true;
	size_t num_shapes = shapes.size();
	if (!U7exists(sources[0].first.c_str()))
		is_good = false;
	for (auto& src : sources) {
		IDataSource *source = U7load(src, shape_sources);
		if (source) {
			flex = Flex::is_flex(source);
			if (flex) {
				source->seek(0x54); // Get # of shapes.
				size_t cnt = source->read4();
				num_shapes = num_shapes > cnt ? num_shapes : cnt;
				shape_cnts.push_back(cnt);
			}
		}
	}
	if (shape_sources.empty())
		throw file_open_exception(get_system_path(sources[0].first));
	if (!flex) {        // Just one shape, which we preload.
		shape_cnts.clear();
		shape_cnts.push_back(1);
		shapes.emplace_back();
		shapes[0].load(shape_sources[shape_sources.size() - 1].first.get());
		return true;
	}
	// Set up lists of pointers.
	shapes.resize(num_shapes);
	return is_good;
}

bool Vga_file::is_shape_imported(int shnum) {
	std::map<int, imported_map>::iterator it =
	    imported_shape_table.find(shnum);
	return it != imported_shape_table.end();
}

bool Vga_file::get_imported_shape_data(int shnum, imported_map &data) {
	std::map<int, imported_map>::iterator it =
	    imported_shape_table.find(shnum);
	if (it != imported_shape_table.end()) {
		data = (*it).second;
		return true;
	}
	return false;
}

bool Vga_file::import_shapes(
    pair<string, int> const &source,
    vector<pair<int, int>> const &imports
) {
	reset_imports();
	IDataSource *ds = U7load(source, imported_sources);
	if (ds) {
		ds->seek(0x54); // Get # of shapes.
		int cnt = ds->read4();
		imported_cnts.push_back(cnt);
		flex = Flex::is_flex(ds);
		assert(flex);
		imported_shapes.reserve(imported_shapes.size() + imports.size());
		for (vector<pair<int, int>>::const_iterator it = imports.begin();
		        it != imports.end(); ++it) {
			int shpsize = imported_shapes.size();
			int srcsize = imported_sources.size() - 1;
			imported_map data = {
				(*it).second,   // The real shape
				shpsize,    // The index of the data pointer.
				srcsize
			};   // The data source index.
			imported_shape_table[(*it).first] = data;
			imported_shapes.emplace_back();
		}
		return true;
	} else {
		// Set up the import table anyway.
		for (vector<pair<int, int>>::const_iterator it = imports.begin();
		        it != imports.end(); ++it) {
			imported_map data = {(*it).second, -1, -1};
			imported_shape_table[(*it).first] = data;
		}
	}
	return false;
}

void Vga_file::reset() {
	shapes.clear();
	shape_sources.clear();
	shape_cnts.clear();
}

void Vga_file::reset_imports() {
	imported_shapes.clear();
	imported_sources.clear();
	imported_cnts.clear();
	imported_shape_table.clear();
}

// Out-of-line definition to avoid more dependencies on databuf.h.
Vga_file::~Vga_file() noexcept = default;

/*
 *  Make a spot for a new shape, and delete frames in existing shape.
 *
 *  Output: ->shape, or 0 if invalid shapenum.
 */

Shape *Vga_file::new_shape(
    int shapenum
) {
	if (shapenum < 0 || shapenum >= c_max_shapes)
		return nullptr;
	if (size_t(shapenum) < shapes.size()) {
		shapes[shapenum].reset();
		shapes[shapenum].set_modified();
	} else {            // Enlarge list.
		if (!flex)
			return nullptr;   // 1-shape file.
		shapes.resize(shapenum + 1);
	}
	return &shapes[shapenum];
}

