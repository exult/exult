/*
Copyright (C) 2005 The Pentagram Team
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
#ifndef AUDIOSAMPLE_H_INCLUDED
#define AUDIOSAMPLE_H_INCLUDED

#include "common_types.h"
#include "ignore_unused_variable_warning.h"

#include <memory>

class IDataSource;

namespace Pentagram {

	class AudioSample {
	protected:
		uint32 bits;
		int    frame_size;
		uint32 decompressor_size;
		uint32 decompressor_align;

		uint32                   buffer_limit;
		std::unique_ptr<uint8[]> buffer;

		uint32 refcount;

		// these are mutable so the const method initDecompressor can change
		// them as needed

		mutable uint32 sample_rate;
		mutable bool   stereo;
		mutable uint32 length;

	public:
		AudioSample(std::unique_ptr<uint8[]> buffer, uint32 size);
		virtual ~AudioSample() = default;

		inline uint32 getRate() const {
			return sample_rate;
		}

		inline uint32 getBits() const {
			return bits;
		}

		inline bool isStereo() const {
			return stereo;
		}

		inline uint32 getFrameSize() const {
			return frame_size;
		}

		inline uint32 getDecompressorDataSize() const {
			return decompressor_size;
		}

		inline uint32 getDecompressorAlignment() const {
			return decompressor_align;
		}

		//! get AudioSample length (in samples)
		inline uint32 getPlaybackLength() const {
			return length;
		}

		virtual void   initDecompressor(void* DecompData) const = 0;
		virtual uint32 decompressFrame(void* DecompData, void* samples) const
				= 0;

		virtual void rewind(void* DecompData) const {
			freeDecompressor(DecompData);
			initDecompressor(DecompData);
		}

		virtual void freeDecompressor(void* DecompData) const {
			ignore_unused_variable_warning(DecompData);
		}

		void IncRef() {
			refcount++;
		}

		void Release() {
			if (!--refcount) {
				delete this;
			}
		}

		uint32 getRefCount() {
			return refcount;
		}

		virtual bool isVocSample() const {
			return false;
		}

		static AudioSample* createAudioSample(
				std::unique_ptr<uint8[]> data, uint32 size);
	};

}    // namespace Pentagram

#endif    // AUDIOSAMPLE_H_INCLUDED
