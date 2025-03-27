/*
Copyright (C) 2001-2022  The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

// This is the only place in Exult where a sub include of SDL
//    is not promoted to full SDL.h :
// 1- shp2pcx has no use for SDL, it only uses SDL_endian.h
//    to get the SDL_BYTEORDER macro for Endianness.
// 2- Including SDL.h in full redirects main() to SDL_main()
//    when the code is a stand alone executable, which shp2pcx is.
#include "common_types.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL_endian.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#	define qtohl(x) (x)
#	define qtohs(x) (x)
#else
#	define qtohl(x)                                   \
		((uint32)((((uint32)(x) & 0x000000ffU) << 24)  \
				  | (((uint32)(x) & 0x0000ff00U) << 8) \
				  | (((uint32)(x) & 0x00ff0000U) >> 8) \
				  | (((uint32)(x) & 0xff000000U) >> 24)))
#	define qtohs(x)                            \
		((uint16)((((uint16)(x) & 0x00ff) << 8) \
				  | (((uint16)(x) & 0xff00) >> 8)))
#endif
#define htoql(x) qtohl(x)
#define htoqs(x) qtohs(x)

using std::cerr;
using std::cout;
using std::endl;
using std::memcpy;
using std::memset;
using std::string;
using std::strlen;

struct PCX_Header {
	uint8  manufacturer;
	uint8  version;
	uint8  compression;
	uint8  bpp;
	sint16 x1, y1;
	sint16 x2, y2;
	sint16 hdpi;
	sint16 vdpi;
	uint8  colormap[48];
	uint8  reserved;
	uint8  planes;
	sint16 bytesperline;
	sint16 color;
	uint8  filler[58];
};

struct u7frame {
	sint16 leftX;
	sint16 leftY;
	sint16 rightX;
	sint16 rightY;
	uint16 width;
	uint16 height;
	uint32 datalen;
	uint8* pixels;
};

struct u7shape {
	int      num_frames;
	u7frame* frames;
	int      width, height;
};

unsigned int read1(FILE* f) {
	unsigned char b0;
	b0 = fgetc(f);
	return b0;
}

unsigned int read2(FILE* f) {
	unsigned char b0;
	unsigned char b1;
	b0 = fgetc(f);
	b1 = fgetc(f);
	return b0 + (b1 << 8);
}

/* Flauschepelz */
signed int read2signed(FILE* f) {
	unsigned char b0;
	unsigned char b1;
	signed int    i0;
	b0 = fgetc(f);
	b1 = fgetc(f);
	i0 = b0 + (b1 << 8);
	if (i0 >= 32768) {
		i0 = i0 - 65536;
	}
	return i0;
}

unsigned int read4(FILE* f) {
	unsigned char b0;
	unsigned char b1;
	unsigned char b2;
	unsigned char b3;
	b0 = fgetc(f);
	b1 = fgetc(f);
	b2 = fgetc(f);
	b3 = fgetc(f);
	return b0 + (b1 << 8) + (b2 << 16) + (b3 << 24);
}

u7shape* load_shape(char* filename) {
	int max_leftX  = -1;
	int max_rightX = -1;
	int max_leftY  = -1;
	int max_rightY = -1;

	FILE* fp = fopen(filename, "rb");
	if (!fp) {
		cerr << "Can't open " << filename << endl;
		return nullptr;
	}
	fseek(fp, 0, SEEK_END);
	const int file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	auto*    shape = new u7shape;
	u7frame* frame;

	const int shape_size = read4(fp);

	if (file_size != shape_size) { /* 8x8 tile */
		shape->num_frames = file_size / 64;
		fseek(fp, 0, SEEK_SET); /* Return to start of file */
		cout << "num_frames = " << shape->num_frames << endl;
		shape->frames = new u7frame[shape->num_frames];
		max_leftX     = 0;
		max_leftY     = 0;
		max_rightX    = 7;
		max_rightY    = 7;

		for (int i = 0; i < shape->num_frames; i++) {
			frame            = &shape->frames[i];
			frame->width     = 8;
			frame->height    = 8;
			frame->leftX     = 0;
			frame->leftY     = 0;
			frame->pixels    = new uint8[64];
			const size_t err = fread(frame->pixels, 1, 64, fp);
			assert(err == 64);
		}
	} else {
		const int hdr_size = read4(fp);
		shape->num_frames  = (hdr_size - 4) / 4;

		cout << "num_frames = " << shape->num_frames << endl;
		shape->frames = new u7frame[shape->num_frames];

		for (int i = 0; i < shape->num_frames; i++) {
			frame = &shape->frames[i];

			// Go to where frame offset is stored
			fseek(fp, (i + 1) * 4, SEEK_SET);
			const int frame_offset = read4(fp);
			fseek(fp, frame_offset, SEEK_SET);
			frame->rightX = read2(fp);
			frame->leftX  = read2(fp);
			frame->leftY  = read2(fp);
			frame->rightY = read2(fp);

			if (frame->leftX > max_leftX) {
				max_leftX = frame->leftX;
			}
			if (frame->rightX > max_rightX) {
				max_rightX = frame->rightX;
			}
			if (frame->leftY > max_leftY) {
				max_leftY = frame->leftY;
			}
			if (frame->rightY > max_rightY) {
				max_rightY = frame->rightY;
			}

			frame->width  = frame->leftX + frame->rightX + 1;
			frame->height = frame->leftY + frame->rightY + 1;

			frame->pixels = new uint8[frame->width * frame->height];
			memset(frame->pixels, 0, frame->width * frame->height);

			// eod = frame->pixels+frame->width*frame->height;
			int slice;
			while ((slice = read2(fp)) != 0) {
				const int slice_type   = slice & 0x1;
				int       slice_length = slice >> 1;

				const int offsetX = read2signed(fp);
				const int offsetY = read2signed(fp);

				const int temp_int = (frame->leftY + offsetY) * frame->width
									 + (frame->leftX + offsetX);

				uint8* pixptr = frame->pixels;
				pixptr        = pixptr + temp_int;

				if (pixptr < frame->pixels) {
					pixptr = frame->pixels;
				}
				if (slice_type) {    // Compressed
					while (slice_length > 0) {
						const uint8 block        = read1(fp);
						const int   block_type   = block & 0x1;
						const int   block_length = block >> 1;
						if (block_type) {
							const uint8 pix = read1(fp);
							for (int j = 0; j < block_length; j++) {
								*pixptr++ = pix;
							}
						} else {
							for (int j = 0; j < block_length; j++) {
								const uint8 pix = read1(fp);
								*pixptr++       = pix;
							}
						}
						slice_length -= block_length;
					}
				} else {    // Uncompressed
					// Just read the pixels
					for (int j = 0; j < slice_length; j++) {
						const uint8 pix = read1(fp);
						*pixptr++       = pix;
					}
				}
			}
		}
	}

	cout << "origin: x = " << max_leftX << ", y = " << max_leftY << endl;

	const int width  = max_leftX + max_rightX + 1;
	const int height = max_leftY + max_rightY + 1;

	shape->width  = width;
	shape->height = height;

	for (int i = 0; i < shape->num_frames; i++) {
		frame         = &shape->frames[i];
		uint8* pixptr = frame->pixels;

		frame->pixels = new uint8[width * height];
		memset(frame->pixels, 0, width * height);

		int dsty = max_leftY - frame->leftY;

		for (int srcy = 0; srcy < frame->height; srcy++, dsty++) {
			const int dstx = max_leftX - frame->leftX;
			const int srcx = 0;
			memcpy(frame->pixels + dsty * width + dstx,
				   pixptr + srcy * frame->width + srcx, frame->width);
		}

		delete[] pixptr;
	}
	fclose(fp);
	return shape;
}

uint8* load_palette(char* filename) {
	FILE* fp = fopen(filename, "rb");
	if (!fp) {
		cerr << "Can't open " << filename << endl;
		return nullptr;
	}

	auto* palette = new uint8[768];

	fseek(fp, 0, SEEK_END);
	const long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (len == 768) {
		for (int i = 0; i < 256; i++) {
			palette[i * 3]     = read1(fp) << 2;
			palette[i * 3 + 1] = read1(fp) << 2;
			palette[i * 3 + 2] = read1(fp) << 2;
		}
	} else if (len == 1536) {
		for (int i = 0; i < 256; i++) {
			palette[i * 3] = read1(fp) << 2;
			read1(fp);
			palette[i * 3 + 1] = read1(fp) << 2;
			read1(fp);
			palette[i * 3 + 2] = read1(fp) << 2;
			read1(fp);
		}
	} else {
		cerr << "Not a valid palette file: " << filename << endl;
		delete[] palette;
		fclose(fp);
		return nullptr;
	}
	fclose(fp);

	return palette;
}

static void writeline(FILE* dst, uint8* buffer, int bytes) {
	uint8* finish = buffer + bytes;

	while (buffer < finish) {
		const uint8 value = *(buffer++);
		uint8       count = 1;

		while (buffer < finish && count < 63 && *buffer == value) {
			count++;
			buffer++;
		}

		if (value < 0xc0 && count == 1) {
			fputc(value, dst);
		} else {
			const uint8 tmp = count + 0xc0;
			fputc(tmp, dst);
			fputc(value, dst);
		}
	}
}

void save_8(FILE* dst, int width, int height, int pitch, uint8* buffer) {
	for (int row = 0; row < height; ++row) {
		writeline(dst, buffer, width);
		buffer += pitch;
	}
}

void save_image(
		uint8* pixels, uint8* palette, int width, int height, char* filename) {
	int        pitch;
	PCX_Header header;

	FILE* fp = fopen(filename, "wb");
	if (!fp) {
		cerr << "Can't open " << filename << endl;
		return;
	}

	memset(&header, 0, sizeof(PCX_Header));
	pitch = width;

	header.manufacturer = 0x0a;
	header.version      = 5;
	header.compression  = 1;

	header.bpp          = 8;
	header.bytesperline = htoqs(width);
	header.planes       = 1;
	header.color        = htoqs(1);

	header.x1 = 0;
	header.y1 = 0;
	header.x2 = htoqs(width - 1);
	header.y2 = htoqs(height - 1);

	header.hdpi     = htoqs(300);
	header.vdpi     = htoqs(300);
	header.reserved = 0;

	/* write header */
	fwrite(&header, sizeof(PCX_Header), 1, fp);

	save_8(fp, width, height, pitch, pixels);

	fputc(0x0c, fp);
	fwrite(palette, 3, 256, fp);
	fclose(fp);
}

int main(int argc, char* argv[]) {
	if (argc < 4) {
		cout << "Usage: shp2pcx [input file] [output file prefix] [palette "
				"file]"
			 << endl;
		return 0;
	}

	char* infilename  = argv[1];
	char* outprefix   = argv[2];
	char* palfilename = argv[3];

	if (strlen(outprefix) > 128) {
		cerr << "Output file prefix too long!" << endl;
		return -1;
	}

	u7shape* sh = load_shape(infilename);
	if (!sh) {
		return 1;
	}
	uint8* palette = load_palette(palfilename);
	if (!palette) {
		delete sh;
		return 1;
	}

	for (int i = 0; i < sh->num_frames; i++) {
		char outfilename[255];
		snprintf(outfilename, sizeof(outfilename), "%s%02i.pcx", outprefix, i);
		cout << "Writing frame " << i << " to " << outfilename << "..." << endl;
		save_image(
				sh->frames[i].pixels, palette, sh->width, sh->height,
				outfilename);
	}

	delete sh;
	return 0;
}
