/*
Copyright (C) 2001  The Exult Team

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
#  include <config.h>
#endif

#include "SDL_endian.h"

#include <cassert>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>

#include "common_types.h"

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define qtohl(x) (x)
#define qtohs(x) (x)
#else
#define qtohl(x) \
	((uint32)((((uint32)(x) & 0x000000ffU) << 24) | \
	          (((uint32)(x) & 0x0000ff00U) <<  8) | \
	          (((uint32)(x) & 0x00ff0000U) >>  8) | \
	          (((uint32)(x) & 0xff000000U) >> 24)))
#define qtohs(x) \
	((uint16)((((uint16)(x) & 0x00ff) << 8) | \
	          (((uint16)(x) & 0xff00) >> 8)))
#endif
#define htoql(x) qtohl(x)
#define htoqs(x) qtohs(x)


using std::cout;
using std::endl;
using std::cerr;
using std::string;
using std::strlen;
using std::memset;
using std::memcpy;

struct PCX_Header {
	uint8 manufacturer;
	uint8 version;
	uint8 compression;
	uint8 bpp;
	sint16 x1, y1;
	sint16 x2, y2;
	sint16 hdpi;
	sint16 vdpi;
	uint8 colormap[48];
	uint8 reserved;
	uint8 planes;
	sint16 bytesperline;
	sint16 color;
	uint8 filler[58];
};

struct u7frame {
	sint16 leftX;
	sint16 leftY;
	sint16 rightX;
	sint16 rightY;
	uint16 width;
	uint16 height;
	uint32 datalen;
	uint8 *pixels;
};

struct u7shape {
	int num_frames;
	struct u7frame *frames;
	int width, height;
};

unsigned int read1(FILE *f) {
	unsigned char b0;
	b0 = fgetc(f);
	return b0;
}

unsigned int read2(FILE *f) {
	unsigned char b0;
	unsigned char b1;
	b0 = fgetc(f);
	b1 = fgetc(f);
	return b0 + (b1 << 8);
}

/* Flauschepelz */
signed int read2signed(FILE *f) {
	unsigned char b0;
	unsigned char b1;
	signed int i0;
	b0 = fgetc(f);
	b1 = fgetc(f);
	i0 = b0 + (b1 << 8);
	if (i0 >= 32768) {
		i0 = i0 - 65536;
	}
	return i0;
}

unsigned int read4(FILE *f) {
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


u7shape *load_shape(char *filename) {
	FILE *fp;
	int file_size;
	int shape_size;
	int hdr_size;
	uint8 *pixptr/*, *eod*/;
	int frame_offset;
	int slice;
	int slice_type;
	int slice_length;
	int block_type;
	int block_length;
	int max_leftX = -1;
	int max_rightX = -1;
	int max_leftY = -1;
	int max_rightY = -1;
	int offsetX;
	int offsetY;
	uint8 block;
	uint8 pix;
	int i;
	int j;
	size_t err;

	int temp_int;

	fp = fopen(filename, "rb");
	if (!fp) {
		cerr << "Can't open " << filename << endl;
		return nullptr;
	}
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	u7shape *shape = new u7shape;
	u7frame *frame;

	shape_size = read4(fp);

	if (file_size != shape_size) { /* 8x8 tile */
		shape->num_frames = file_size / 64;
		fseek(fp, 0, SEEK_SET);     /* Return to start of file */
		cout << "num_frames = " << shape->num_frames << endl;
		shape->frames = new u7frame[shape->num_frames];
		max_leftX = 0;
		max_leftY = 0;
		max_rightX = 7;
		max_rightY = 7;

		for (i = 0; i < shape->num_frames; i++) {
			frame = &shape->frames[i];
			frame->width = 8;
			frame->height = 8;
			frame->leftX = 0;
			frame->leftY = 0;
			frame->pixels = new uint8[64];
			err = fread(frame->pixels, 1, 64, fp);
			assert(err == 64);
		}
	} else {
		hdr_size = read4(fp);
		shape->num_frames = (hdr_size - 4) / 4;

		cout << "num_frames = " << shape->num_frames << endl;
		shape->frames = new u7frame[shape->num_frames];

		for (i = 0; i < shape->num_frames; i++) {
			frame = &shape->frames[i];

			// Go to where frame offset is stored
			fseek(fp, (i + 1) * 4, SEEK_SET);
			frame_offset = read4(fp);
			fseek(fp, frame_offset, SEEK_SET);
			frame->rightX = read2(fp);
			frame->leftX = read2(fp);
			frame->leftY = read2(fp);
			frame->rightY = read2(fp);

			if (frame->leftX > max_leftX)
				max_leftX = frame->leftX;
			if (frame->rightX > max_rightX)
				max_rightX = frame->rightX;
			if (frame->leftY > max_leftY)
				max_leftY = frame->leftY;
			if (frame->rightY > max_rightY)
				max_rightY = frame->rightY;

			frame->width = frame->leftX + frame->rightX + 1;
			frame->height = frame->leftY + frame->rightY + 1;

			pixptr = new uint8[frame->width * frame->height];
			frame->pixels = pixptr;
			memset(pixptr, 0, frame->width * frame->height);

			//eod = frame->pixels+frame->width*frame->height;
			while ((slice = read2(fp)) != 0) {
				slice_type = slice & 0x1;
				slice_length = slice >> 1;

				offsetX = read2signed(fp);
				offsetY = read2signed(fp);

				temp_int = (frame->leftY + offsetY) * frame->width +
				           (frame->leftX + offsetX);

				pixptr = frame->pixels;
				pixptr = pixptr + temp_int;

				if (pixptr < frame->pixels)
					pixptr = frame->pixels;
				if (slice_type) {   // Compressed
					while (slice_length > 0) {
						block = read1(fp);
						block_type = block & 0x1;
						block_length = block >> 1;
						if (block_type) {
							pix = read1(fp);
							for (j = 0; j < block_length; j++) {
								*pixptr++ = pix;
							}
						} else {
							for (j = 0; j < block_length; j++) {
								pix = read1(fp);
								*pixptr++ = pix;
							}
						}
						slice_length -= block_length;
					}
				} else {        // Uncompressed
					// Just read the pixels
					for (j = 0; j < slice_length; j++) {
						pix = read1(fp);
						*pixptr++ = pix;
					}
				}
			}
		}
	}

	cout << "origin: x = " << max_leftX << ", y = " << max_leftY << endl;

	int width = max_leftX + max_rightX + 1;
	int height = max_leftY + max_rightY + 1;

	shape->width = width;
	shape->height = height;

	int srcx;
	int srcy;
	int dstx;
	int dsty;

	for (i = 0; i < shape->num_frames; i++) {
		frame = &shape->frames[i];
		pixptr = frame->pixels;

		frame->pixels = new uint8[width * height];
		memset(frame->pixels, 0, width * height);

		dsty = max_leftY - frame->leftY;

		for (srcy = 0; srcy < frame->height; srcy++, dsty++) {
			dstx = max_leftX - frame->leftX;
			srcx = 0;
			memcpy(frame->pixels + dsty * width + dstx, pixptr + srcy * frame->width + srcx, frame->width);
		}

		delete [] pixptr;
	}
	fclose(fp);
	return shape;
}

uint8 *load_palette(char *filename) {
	FILE *fp;
	long len;
	int i;

	fp = fopen(filename, "rb");
	if (!fp) {
		cerr << "Can't open " << filename << endl;
		return nullptr;
	}

	uint8 *palette = new uint8[768];

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (len == 768) {
		for (i = 0; i < 256; i++) {
			palette[i * 3] = read1(fp) << 2;
			palette[i * 3 + 1] = read1(fp) << 2;
			palette[i * 3 + 2] = read1(fp) << 2;
		}
	} else if (len == 1536) {
		for (i = 0; i < 256; i++) {
			palette[i * 3] = read1(fp) << 2;
			read1(fp);
			palette[i * 3 + 1] = read1(fp) << 2;
			read1(fp);
			palette[i * 3 + 2] = read1(fp) << 2;
			read1(fp);
		}
	} else {
		cerr << "Not a valid palette file: " << filename << endl;
		delete [] palette;
		fclose(fp);
		return nullptr;
	}
	fclose(fp);

	return palette;
}

static void writeline(FILE *dst, uint8 *buffer, int bytes) {
	uint8 value;
	uint8 count;
	uint8 tmp;
	uint8 *finish = buffer + bytes;

	while (buffer < finish) {

		value = *(buffer++);
		count = 1;

		while (buffer < finish && count < 63 && *buffer == value) {
			count++;
			buffer++;
		}

		if (value < 0xc0 && count == 1) {
			fputc(value, dst);
		} else {
			tmp = count + 0xc0;
			fputc(tmp, dst);
			fputc(value, dst);
		}
	}
}

void save_8(FILE *dst, int width, int height,
            int pitch, uint8 *buffer) {
	int row;

	for (row = 0; row < height; ++row) {
		writeline(dst, buffer, width);
		buffer += pitch;
	}
}


void save_image(uint8 *pixels, uint8 *palette, int width, int height, char *filename) {
	int pitch;
	PCX_Header header;

	FILE *fp = fopen(filename, "wb");
	if (!fp) {
		cerr << "Can't open " << filename << endl;
		return;
	}

	memset(&header, 0, sizeof(PCX_Header));
	pitch = width;

	header.manufacturer = 0x0a;
	header.version = 5;
	header.compression = 1;

	header.bpp = 8;
	header.bytesperline = htoqs(width);
	header.planes = 1;
	header.color = htoqs(1);

	header.x1 = 0;
	header.y1 = 0;
	header.x2 = htoqs(width - 1);
	header.y2 = htoqs(height - 1);

	header.hdpi = htoqs(300);
	header.vdpi = htoqs(300);
	header.reserved = 0;

	/* write header */
	fwrite(&header, sizeof(PCX_Header), 1, fp);

	save_8(fp, width, height, pitch, pixels);

	fputc(0x0c, fp);
	fwrite(palette, 3, 256, fp);
	fclose(fp);
}

int main(int argc, char *argv[]) {
	char *palfilename ;
	char *infilename;
	char *outprefix;
	char outfilename[255];
	u7shape *sh;
	uint8 *palette;

	if (argc < 4) {
		cout << "Usage: shp2pcx [input file] [output file prefix] [palette file]" << endl;
		return 0;
	}

	infilename = argv[1];
	outprefix = argv[2];
	palfilename = argv[3];

	if (strlen(outprefix) > 128) {
		cerr << "Output file prefix too long!" << endl;
		return -1;
	}

	sh = load_shape(infilename);
	if (!sh) return 1;
	palette = load_palette(palfilename);
	if (!palette) {
		delete sh;
		return 1;
	}

	for (int i = 0; i < sh->num_frames; i++) {
		sprintf(outfilename, "%s%02i.pcx", outprefix, i);
		cout << "Writing frame " << i << " to " << outfilename << "..." << endl;
		save_image(sh->frames[i].pixels, palette, sh->width, sh->height, outfilename);
	}

	delete sh;
	return 0;
}
