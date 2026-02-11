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

#include "common_types.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

using std::cerr;
using std::cout;
using std::endl;
using std::exit;
using std::memcpy;
using std::strcpy;
using std::string;
using std::strlen;
using std::strrchr;

struct u7frame {
	bool   tile;
	uint32 datalen;
	uint8* data;
};

struct u7shape {
	int      num_frames;
	u7frame* frames;
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

signed int read2signed(FILE* f) {
	unsigned char b0;
	unsigned char b1;
	signed int    i0;
	b0 = fgetc(f);
	b1 = fgetc(f);
	i0 = b0 + (b1 << 8);
	if (i0 >= 32768) {
		i0 -= 65536;
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

void write4(FILE* f, unsigned int b) {
	fputc(b & 0xFF, f);
	fputc((b >> 8) & 0xFF, f);
	fputc((b >> 16) & 0xFF, f);
	fputc((b >> 24) & 0xFF, f);
}

char* framefilename(char* shapefilename, int frame) {
	const size_t namelen = strlen(shapefilename) + 5;
	char*        fn      = new char[namelen];    //_xxx\0
	char*        dot     = strrchr(shapefilename, '.');
#ifdef _WIN32
	char* slash = strrchr(shapefilename, '\\');
#else
	char* slash = strrchr(shapefilename, '/');
#endif
	int dotpos;

	if (dot == nullptr || slash > dot) {
		dotpos = strlen(shapefilename);
	} else {
		dotpos = dot - shapefilename;
	}

	memcpy(fn, shapefilename, dotpos);
	snprintf(fn + dotpos, namelen - dotpos, "_%03i", frame);
	strcpy(fn + dotpos + 4, shapefilename + dotpos);

	return fn;
}

void split_shape(char* filename) {
	FILE* shpfile = fopen(filename, "rb");
	if (!shpfile) {
		cerr << "Can't open " << filename << endl;
		return;
	}
	fseek(shpfile, 0, SEEK_END);
	const int file_size = ftell(shpfile);
	fseek(shpfile, 0, SEEK_SET);

	const int shape_size = read4(shpfile);

	if (file_size != shape_size) { /* 8x8 tile */
		const int num_frames = file_size / 64;
		fseek(shpfile, 0, SEEK_SET); /* Return to start of file */
		cout << "num_frames = " << num_frames << endl;
		auto* data = new uint8[64];
		for (int i = 0; i < num_frames; i++) {
			char* framename = framefilename(filename, i);
			cout << "writing " << framename << "..." << endl;
			FILE*        framefile = fopen(framename, "wb");
			const size_t err       = fread(data, 1, 64, shpfile);
			assert(err == 64);
			fwrite(data, 1, 64, framefile);
			fclose(framefile);
			delete[] framename;
		}
		delete[] data;
	} else {
		const int hdr_size   = read4(shpfile);
		const int num_frames = (hdr_size - 4) / 4;

		cout << "num_frames = " << num_frames << endl;

		for (int i = 0; i < num_frames; i++) {
			char* framename = framefilename(filename, i);
			cout << "writing " << framename << "..." << endl;
			FILE* framefile = fopen(framename, "wb");

			// Go to where frame offset is stored
			fseek(shpfile, (i + 1) * 4, SEEK_SET);
			const int frame_offset = read4(shpfile);
			int       next_frame_offset;

			if (i + 1 < num_frames) {
				next_frame_offset = read4(shpfile);
			} else {
				next_frame_offset = shape_size;
			}

			fseek(shpfile, frame_offset, SEEK_SET);
			const size_t datalen = next_frame_offset - frame_offset;

			write4(framefile, datalen + 8);
			write4(framefile, 8);

			auto*        data = new uint8[datalen];
			const size_t err  = fread(data, 1, datalen, shpfile);
			assert(err == datalen);
			fwrite(data, 1, datalen, framefile);
			fclose(framefile);
			delete[] framename;
			delete[] data;
		}
	}
	cout << "done" << endl;

	fclose(shpfile);
}

void merge_frames(char* shapefile, char** framefiles, int numframefiles) {
	bool tiles = false;

	FILE* shpfile = fopen(shapefile, "wb");

	int total_size = 4 + 4 * numframefiles;

	for (int i = 0; i < numframefiles; i++) {
		FILE* framefile = fopen(framefiles[i], "rb");

		cout << "reading " << framefiles[i] << "..." << endl;

		fseek(framefile, 0, SEEK_END);
		const int file_size = ftell(framefile);
		fseek(framefile, 0, SEEK_SET);
		const int shape_size = read4(framefile);

		if (file_size != shape_size) {    // 8x8 tile
			if (i > 0 && !tiles) {
				cout << "Error: can't mix 8x8 tiles and non-tile shapes!" << endl;
				exit(1);
			}

			tiles = true;

			fseek(framefile, 0, SEEK_SET);
			fseek(shpfile, 64 * i, SEEK_SET);

			auto* data = new uint8[64];

			const size_t err = fread(data, 1, 64, framefile);
			assert(err == 64);
			fwrite(data, 1, 64, shpfile);
			fclose(framefile);

			delete[] data;
		} else {
			if (tiles) {
				cout << "Error: can't mix 8x8 tiles and non-tile shapes!" << endl;
				exit(1);
			}

			const int hdr_size = read4(framefile);

			size_t frame_size;
			if (hdr_size > 8) {
				frame_size = read4(framefile);
				frame_size -= hdr_size;
			} else {
				frame_size = shape_size - hdr_size;
			}

			auto* data = new uint8[frame_size];
			fseek(framefile, hdr_size, SEEK_SET);
			const size_t err = fread(data, 1, frame_size, framefile);
			assert(err == frame_size);
			fclose(framefile);

			fseek(shpfile, 4 + (i * 4), SEEK_SET);
			write4(shpfile, total_size);
			fseek(shpfile, total_size, SEEK_SET);
			fwrite(data, 1, frame_size, shpfile);

			total_size += frame_size;
			delete[] data;
		}
	}

	if (!tiles) {
		fseek(shpfile, 0, SEEK_SET);
		write4(shpfile, total_size);
	}

	cout << "done" << endl;

	fclose(shpfile);
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cout << "Usage: To split: splitshp [shape file]" << endl << "     or to pack: splitshp [shape file] [frame files]" << endl;
		return 0;
	}

	char* shapefile = argv[1];

	if (argc > 2) {
		const int numframefiles = argc - 2;
		char*     framefiles[255];
		for (int i = 0; i < numframefiles; i++) {
			framefiles[i] = argv[i + 2];
		}
		merge_frames(shapefile, framefiles, numframefiles);
	} else {
		split_shape(shapefile);
	}

	return 0;
}
