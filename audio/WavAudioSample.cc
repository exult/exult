/*
Copyright (C) 2010-2022 The Exult Team

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
#include "pent_include.h"

#include "WavAudioSample.h"

#include "databuf.h"

#include <cstring>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

using namespace Pentagram;

WavAudioSample::WavAudioSample(std::unique_ptr<uint8[]> buffer_, uint32 size)
		: RawAudioSample(std::move(buffer_), 0, 0, false, false) {
	uint32 pos_fmt  = 0;
	uint32 size_fmt = 0;

	uint32 pos_data  = 0;
	uint32 size_data = 0;

	IBufferDataView ds(buffer, size);

	char buf[4];

	ds.seek(0);

	ds.read(buf, 4);
	if (std::memcmp(buf, "RIFF", 4) != 0) {
		return;
	}

	const uint32 riff_size = ds.read4();
	if (riff_size != size - 8) {
		return;
	}

	ds.read(buf, 4);
	if (std::memcmp(buf, "WAVE", 4) != 0) {
		return;
	}

	while (ds.getPos() < riff_size + 8) {
		ds.read(buf, 4);
		const uint32 chunk_size = ds.read4();

		if (!std::memcmp(buf, "fmt ", 4)) {
			pos_fmt  = ds.getPos();
			size_fmt = chunk_size;

			if (pos_data) {
				break;
			}
		} else if (!std::memcmp(buf, "data", 4)) {
			pos_data  = ds.getPos();
			size_data = chunk_size;

			if (pos_fmt) {
				break;
			}
		}

		ds.skip(chunk_size);
	}

	if (!pos_fmt || !pos_data || size_fmt < 0x10) {
		return;
	}

	ds.seek(pos_fmt);
	const uint16 format_tag = ds.read2();
	const uint16 channels   = ds.read2();
	sample_rate             = ds.read4();
	// uint32 bytes_per_second = ds.read4();
	// uint16 block_align = ds.read2();
	ds.skip(6);
	const uint16 bits_per_sample = ds.read2();
	// uint16 extra_bytes = ds.read2();
	ds.skip(2);

	if (format_tag != 1) {
		return;
	}
	if (channels != 1 && channels != 2) {
		return;
	}
	if (bits_per_sample != 16 && bits_per_sample != 8) {
		return;
	}

	stereo = channels == 2;
	bits   = bits_per_sample;

	buffer_limit = pos_data + size_data;
	start_pos    = pos_data;

	length = size_data / (bits_per_sample / (stereo ? 4 : 8));

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	byte_swap = false;
#else
	byte_swap = true;
#endif

	signeddata = bits == 16;
}

bool WavAudioSample::isThis(IDataSource* ds) {
	char buf[4];
	ds->seek(0);

	ds->read(buf, 4);
	if (std::memcmp(buf, "RIFF", 4) != 0) {
		return false;
	}

	const uint32 riff_size = ds->read4();

	if (riff_size != ds->getAvail()) {
		return false;
	}

	ds->read(buf, 4);
	if (std::memcmp(buf, "WAVE", 4) != 0) {
		return false;
	}

	uint32 pos_fmt  = 0;
	uint32 size_fmt = 0;

	uint32 pos_data = 0;
	// uint32 size_data = 0;

	while (ds->getAvail() > 0) {
		ds->read(buf, 4);
		const uint32 chunk_size = ds->read4();

		if (!std::memcmp(buf, "fmt ", 4)) {
			pos_fmt  = ds->getPos();
			size_fmt = chunk_size;

			if (pos_data) {
				break;
			}
		} else if (!std::memcmp(buf, "data", 4)) {
			if (pos_data) {
				return false;
			}

			pos_data = ds->getPos();
			// size_data = chunk_size;

			if (pos_fmt) {
				break;
			}
		}

		ds->skip(chunk_size);
	}

	if (!pos_fmt || !pos_data || size_fmt < 0x10) {
		return false;
	}

	ds->seek(pos_fmt);

	const uint16 format_tag = ds->read2();
	const uint16 channels   = ds->read2();
	// uint32 sample_rate = ds->read4();
	// uint32 bytes_per_second = ds->read4();;
	// uint16 block_align = ds->read2();
	ds->skip(10);
	const uint16 bits_per_sample = ds->read2();
	// uint16 extra_bytes = ds->read2();
	ds->skip(2);

	if (format_tag != 1) {
		return false;
	}
	if (channels != 1 && channels != 2) {
		return false;
	}
	if (bits_per_sample != 16 && bits_per_sample != 8) {
		return false;
	}

	return true;
}
