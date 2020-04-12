/*
Copyright (C) 2005 The Pentagram team
Copyright (C) 2010 The Exult team

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
#ifndef VocAudioSample_H
#define VocAudioSample_H

#include "AudioSample.h"

namespace Pentagram {

class VocAudioSample : public AudioSample
{
public:
	VocAudioSample(std::unique_ptr<uint8[]> buffer, uint32 size);

	void initDecompressor(void *DecompData) const override;
	uint32 decompressFrame(void *DecompData, void *samples) const override;
	void freeDecompressor(void *DecompData) const override;

	static bool isThis(IDataSource *ds);
protected:

	struct VocDecompData {
		uint32	pos;
		int		compression;
		int		adpcm_reference;
		int		adpcm_scale;
		int		chunk_remain;	// Bytes remaining in chunk
		int		cur_type;
	};

	static inline int decode_ADPCM_4_sample(uint8 sample,
									 int& reference,
									 int& scale);

	static void decode_ADPCM_4(uint8* inBuf,	
							  int bufSize,				// Size of inbuf
							  uint8* outBuf,			// Size is 2x bufsize
							  int& reference,			// ADPCM reference value
							  int& scale);

	bool advanceChunk(void *DecompData) const;
};

}

#endif
