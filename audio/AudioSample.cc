/*
Copyright (C) 2005 The Pentagram team
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

#include "AudioSample.h"

#include "OggAudioSample.h"
#include "VocAudioSample.h"
#include "WavAudioSample.h"

namespace Pentagram {

	AudioSample::AudioSample(std::unique_ptr<uint8[]> buffer_, uint32 size_)
			: bits(0), frame_size(0), decompressor_size(0),
			  decompressor_align(0), buffer_limit(size_),
			  buffer(std::move(buffer_)), refcount(1), sample_rate(0),
			  stereo(false), length(0) {}

	AudioSample* AudioSample::createAudioSample(
			std::unique_ptr<uint8[]> data, uint32 size) {
		IBufferDataView ds(data, size);

		if (VocAudioSample::isThis(&ds)) {
			return new VocAudioSample(std::move(data), size);
		} else if (WavAudioSample::isThis(&ds)) {
			return new WavAudioSample(std::move(data), size);
		} else if (OggAudioSample::isThis(&ds)) {
			return new OggAudioSample(std::move(data), size);
		}
		return nullptr;
	}

}    // namespace Pentagram
