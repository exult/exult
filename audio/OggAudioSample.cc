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

#include "OggAudioSample.h"

#include "databuf.h"
#include "headers/exceptions.h"

#include <new>

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

namespace Pentagram {

	ov_callbacks OggAudioSample::callbacks
			= {&read_func, &seek_func, nullptr, &tell_func};

	OggAudioSample::OggAudioSample(std::unique_ptr<IDataSource> oggdata_)
			: AudioSample(nullptr, 0), oggdata(std::move(oggdata_)) {
		frame_size         = 4096;
		decompressor_size  = sizeof(OggDecompData);
		decompressor_align = alignof(OggDecompData);
		bits               = 16;
		locked             = false;
		// we set this in initDecompressor
		length = 0;
	}

	OggAudioSample::OggAudioSample(std::unique_ptr<uint8[]> buffer, uint32 size)
			: AudioSample(std::move(buffer), size), oggdata(nullptr) {
		frame_size         = 4096;
		decompressor_size  = sizeof(OggDecompData);
		decompressor_align = alignof(OggDecompData);
		bits               = 16;
		locked             = false;
		// we set this in initDecompressor
		length = 0;
	}

	size_t OggAudioSample::read_func(
			void* ptr, size_t size, size_t nmemb, void* datasource) {
		auto* ids = static_cast<IDataSource*>(datasource);
		// if (ids->eof()) return 0;
		const size_t limit = ids->getAvail();
		if (limit == 0) {
			return 0;
		} else if (limit < size * nmemb) {
			nmemb = limit / size;
		}
		ids->read(ptr, size * nmemb);
		return nmemb;
	}

	int OggAudioSample::seek_func(
			void* datasource, ogg_int64_t offset, int whence) {
		auto* ids = static_cast<IDataSource*>(datasource);
		switch (whence) {
		case SEEK_SET:
			ids->seek(static_cast<size_t>(offset));
			return 0;
		case SEEK_END:
			ids->seek(ids->getSize() - static_cast<size_t>(offset));
			return 0;
		case SEEK_CUR:
			ids->skip(static_cast<size_t>(offset));
			return 0;
		}
		return -1;
	}

	long OggAudioSample::tell_func(void* datasource) {
		auto* ids = static_cast<IDataSource*>(datasource);
		return ids->getPos();
	}

	bool OggAudioSample::isThis(IDataSource* oggdata) {
		OggVorbis_File vf;
		oggdata->seek(0);
		const int res = ov_test_callbacks(oggdata, &vf, nullptr, 0, callbacks);
		ov_clear(&vf);

		return res == 0;
	}

	void OggAudioSample::initDecompressor(void* DecompData) const {
		auto* decomp = new (DecompData) OggDecompData;

		if (locked) {
			throw exult_exception("Attempted to play OggAudioSample on more "
								  "than one channel at the same time.");
		}

		if (this->oggdata) {
			locked             = true;
			decomp->datasource = this->oggdata.get();
		} else {
			decomp->datasource = new IBufferDataView(buffer, buffer_limit);
		}

		decomp->datasource->seek(0);
		ov_open_callbacks(
				decomp->datasource, &decomp->ov, nullptr, 0, callbacks);
		decomp->bitstream = 0;

		vorbis_info* info = ov_info(&decomp->ov, -1);
		sample_rate = decomp->last_rate = info->rate;
		stereo = decomp->last_stereo = info->channels == 2;

		// Set the sample length if needed
		if (!length) {
			// seek to beginning as there seems to be a bug that sometimes
			// ov_pcm_total can report the wrong value if the state is not
			// reset to 0 see https://stackoverflow.com/a/72482773
			ov_raw_seek(&decomp->ov, 0);
			length = ov_pcm_total(&decomp->ov, -1);
		}
		// seek to 0 for good measure here
		ov_raw_seek(&decomp->ov, 0);

		decomp->freed = false;
	}

	void OggAudioSample::freeDecompressor(void* DecompData) const {
		auto* decomp = static_cast<OggDecompData*>(DecompData);
		if (decomp->freed) {
			return;
		}
		decomp->freed = true;
		ov_clear(&decomp->ov);

		if (this->oggdata) {
			locked = false;
		} else {
			delete decomp->datasource;
		}

		decomp->datasource = nullptr;
		decomp->~OggDecompData();
	}

	void OggAudioSample::rewind(void* DecompData) const {
		auto* decomp = static_cast<OggDecompData*>(DecompData);
		if (ov_raw_seek(&decomp->ov, 0) != 0) {
			// If raw seek fails just call themuch more expensive method in the
			// base class
			AudioSample::rewind(DecompData);
		}
	}

	uint32 OggAudioSample::decompressFrame(
			void* DecompData, void* samples) const {
		auto* decomp = static_cast<OggDecompData*>(DecompData);

		vorbis_info* info = ov_info(&decomp->ov, -1);

		if (info == nullptr) {
			return 0;
		}

		sample_rate         = decomp->last_rate;
		stereo              = decomp->last_stereo;
		decomp->last_rate   = info->rate;
		decomp->last_stereo = info->channels == 2;

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		const int bigendianp = 0;
#else
		const int bigendianp = 1;
#endif

		const long count = ov_read(
				&decomp->ov, static_cast<char*>(samples), frame_size,
				bigendianp, 2, 1, &decomp->bitstream);

		// if (count == OV_EINVAL || count == 0) {
		if (count <= 0) {
			return 0;
		}
		// else if (count < 0) {
		//	*(uint32*)samples = 0;
		//	return 1;
		// }

		return count;
	}

}    // namespace Pentagram
