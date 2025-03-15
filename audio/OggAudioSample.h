/*
Copyright (C) 2010-2022 The Exult team

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
#ifndef OggAudioSample_H
#define OggAudioSample_H

#include "AudioSample.h"

#include <memory>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wunused-variable"
#	else
#		if __clang_major__ >= 16
#			pragma GCC diagnostic ignored "-Wcast-function-type-strict"
#		endif
#	endif
#endif    // __GNUC__
#include <vorbis/vorbisfile.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

namespace Pentagram {

	// FIXME - OggAudioSample doesn't support playing the same sample on more
	// than one channel at the same time. Its really intended only for music
	// playing support

	class OggAudioSample : public AudioSample {
	public:
		OggAudioSample(std::unique_ptr<IDataSource> oggdata);
		OggAudioSample(std::unique_ptr<uint8[]> buffer, uint32 size);

		void   initDecompressor(void* DecompData) const override;
		uint32 decompressFrame(void* DecompData, void* samples) const override;
		void   freeDecompressor(void* DecompData) const override;
		void   rewind(void* DecompData) const override;
		static ov_callbacks callbacks;

		static size_t read_func(
				void* ptr, size_t size, size_t nmemb, void* datasource);
		static int  seek_func(void* datasource, ogg_int64_t offset, int whence);
		static long tell_func(void* datasource);

		static bool isThis(IDataSource* oggdata);

	protected:
		struct OggDecompData {
			OggVorbis_File ov;
			int            bitstream;
			int            last_rate;
			bool           last_stereo;
			IDataSource*   datasource;
			bool           freed;
		};

		std::unique_ptr<IDataSource> oggdata;
		mutable bool                 locked;
	};

}    // namespace Pentagram

#endif
