/*
Copyright (C) 2010-2013 The Exult Team

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
#include <SDL.h>

using namespace Pentagram;


WavAudioSample::WavAudioSample(std::unique_ptr<uint8[]> buffer_, uint32 size) : RawAudioSample(std::move(buffer_),0,0,false,false)
{
	uint32 pos_fmt = 0;
	uint32 size_fmt = 0;

	uint32 pos_data = 0;
	uint32 size_data = 0;

	IBufferDataView ds(buffer,size);

	char buf[4];

	ds.seek(0);

	ds.read(buf,4);
	if (std::memcmp(buf,"RIFF",4) != 0) return;

	uint32 riff_size = ds.read4();
	if (riff_size != size-8) return;

	ds.read(buf,4);
	if (std::memcmp(buf,"WAVE",4) != 0) return;

	while (ds.getPos() < riff_size+8)
	{
		ds.read(buf,4);
		uint32 chunk_size = ds.read4();

		if (!std::memcmp(buf,"fmt ",4)) 
		{
			pos_fmt = ds.getPos();
			size_fmt = chunk_size;

			if (pos_data) break;
		}
		else if (!std::memcmp(buf,"data",4)) 
		{
			pos_data = ds.getPos();
			size_data = chunk_size;

			if (pos_fmt) break;
		}

		ds.skip(chunk_size);
	}

	if (!pos_fmt || !pos_data || size_fmt < 0x10) return;

	ds.seek(pos_fmt);
	uint16 format_tag = ds.read2(); 
	uint16 channels = ds.read2();
	sample_rate = ds.read4();
	//uint32 bytes_per_second = ds.read4();
	//uint16 block_align = ds.read2();
	ds.skip(6);
	uint16 bits_per_sample = ds.read2();
	//uint16 extra_bytes = ds.read2();
	ds.skip(2);

	if (format_tag != 1) return;
	if (channels != 1 && channels != 2) return;
	if (bits_per_sample != 16 && bits_per_sample != 8) return;

	stereo = channels == 2;
	bits = bits_per_sample ;

	buffer_size = pos_data+size_data;
	start_pos = pos_data;

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	byte_swap = false;
#else
	byte_swap = true;
#endif

	signeddata = bits == 16;
}

bool WavAudioSample::isThis(IDataSource *ds)
{
	char buf[4];
	ds->seek(0);

	ds->read(buf,4);
	if (std::memcmp(buf,"RIFF",4) != 0) return false;

	uint32 riff_size = ds->read4();

	if (riff_size != ds->getSize()-8) return false;

	ds->read(buf,4);
	if (std::memcmp(buf,"WAVE",4) != 0) return false;

	uint32 pos_fmt = 0;
	uint32 size_fmt = 0;

	uint32 pos_data = 0;
	//uint32 size_data = 0;

	while (ds->getPos() < riff_size+8)
	{
		ds->read(buf,4);
		uint32 chunk_size = ds->read4();

		if (!std::memcmp(buf,"fmt ",4)) 
		{
			pos_fmt = ds->getPos();
			size_fmt = chunk_size;

			if (pos_data) break;
		}
		else if (!std::memcmp(buf,"data",4)) 
		{
			if (pos_data) return false;

			pos_data = ds->getPos();
			//size_data = chunk_size;

			if (pos_fmt) break;
		}

		ds->skip(chunk_size);
	}

	if (!pos_fmt || !pos_data || size_fmt < 0x10) return false;

	ds->seek(pos_fmt);

	uint16 format_tag = ds->read2(); 
	uint16 channels = ds->read2();
	//uint32 sample_rate = ds->read4();
	//uint32 bytes_per_second = ds->read4();;
	//uint16 block_align = ds->read2();
	ds->skip(10);
	uint16 bits_per_sample = ds->read2();
	//uint16 extra_bytes = ds->read2();
	ds->skip(2);

	if (format_tag != 1) return false;
	if (channels != 1 && channels != 2) return false;
	if (bits_per_sample != 16 && bits_per_sample != 8) return false;
	
	return true;
}
