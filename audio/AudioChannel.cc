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

#include "AudioChannel.h"

#include "AudioSample.h"
#ifdef DEBUG
#	ifdef __GNUC__
#		pragma GCC diagnostic push
#		pragma GCC diagnostic ignored "-Wold-style-cast"
#		pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#		if !defined(__llvm__) && !defined(__clang__)
#			pragma GCC diagnostic ignored "-Wuseless-cast"
#		endif
#	endif    // __GNUC__
#	include <SDL3/SDL.h>
#	ifdef __GNUC__
#		pragma GCC diagnostic pop
#	endif    // __GNUC__
#endif

#include <cstring>

namespace Pentagram {

	sint16 ReadSample(uint8* src) {
		sint16 val;
		std::memcpy(&val, src, sizeof(sint16));
		return val;
	}

// We divide the data by 2, to prevent overshots. Imagine this sample pattern:
// 0, 65535, 65535, 0. Now you want to compute a value between the two 65535.
// Obviously, it will be *bigger* than 65535 (it can get to about 80,000).
// It is possibly to clamp it, but that leads to a distored wave form. Compare
// this to turning up the volume of your stereo to much, it will start to sound
// bad at a certain level (depending on the power of your stereo, your speakers
// etc, this can be quite loud, though ;-). Hence we reduce the original range.
// A factor of roughly 1/1.2 = 0.8333 is sufficient. Since we want to avoid
// floating point, we approximate that by 27/32
#define RANGE_REDUX(x) (((x) * 27) >> 5)

	AudioChannel::AudioChannel(uint32 sample_rate_, bool stereo_)
			: sample_rate(sample_rate_), stereo(stereo_) {}

	AudioChannel::~AudioChannel() {
		if (sample && playdata) {
			sample->freeDecompressor(decomp);
		}
		if (sample) {
			sample->Release();
		}
		playdata.reset();
		sample = nullptr;
	}

#ifdef DEBUG
	inline char* formatTicks() {
		static char formattedTicks[32];
		uint64      ticks = SDL_GetTicks();
		snprintf(
				formattedTicks, 32, "[ %5u.%03u ] ",
				static_cast<uint32>(ticks / 1000),
				static_cast<uint32>(ticks % 1000));
		return formattedTicks;
	}
#endif

	void AudioChannel::playSample(
			AudioSample* sample_, int loop_, int priority_, bool paused_,
			uint32 pitch_shift_, int lvol_, int rvol_, sint32 instance_id_) {
#ifdef DEBUG
		std::cout << formatTicks() << "AudioChannel::playSample, volume left "
				  << lvol_ << " right " << rvol_ << " sample rate "
				  << sample_->getRate() << " Hz -> " << sample_rate << " Hz"
				  << std::endl;
#endif
		stop();
		sample = sample_;

		loop             = loop_;
		priority         = priority_;
		lvol             = lvol_;
		rvol             = rvol_;
		paused           = paused_;
		pitch_shift      = pitch_shift_;
		instance_id      = instance_id_;
		distance         = 0;
		balance          = 0;
		overall_position = 0;

		if (!sample) {
			return;
		}
		sample->IncRef();

		// Lambda to round size up to next multiple of maximum alignment size,
		// if needed.
		auto round_up = [](size_t val) {
			constexpr const size_t maxalign = alignof(std::max_align_t);
			return ((val + maxalign - 1) / maxalign) * maxalign;
		};
		// Persistent data for the decompressor
		const size_t decompressor_size
				= round_up(sample->getDecompressorDataSize());
		const size_t frame_size         = round_up(sample->getFrameSize());
		const size_t decompressor_align = sample->getDecompressorAlignment();
		const size_t new_size
				= decompressor_size + frame_size * 2 + decompressor_align;

		// Setup buffers
		if (new_size > playdata_size) {
			playdata_size = new_size;
			playdata      = std::make_unique<uint8[]>(playdata_size);
		}

		size_t avail_space = playdata_size;
		decomp             = playdata.get();
		// Align decomp to required alignment.
		std::align(decompressor_align, decompressor_size, decomp, avail_space);
		uint8* frameptr = static_cast<uint8*>(decomp) + decompressor_size;
		frames[0]       = frameptr;
		// Second frame should be aligned for free.
		frames[1] = frameptr + frame_size;

		// Init the sample decompressor
		sample->initDecompressor(decomp);

		// Reset counter and stuff
		frame_evenodd = 0;
		position      = 0;
		fp_pos        = 0;
		fp_speed      = (pitch_shift * sample->getRate()) / sample_rate;

		// Decompress frame 0
		frame0_size = sample->decompressFrame(decomp, frames[0]);

		// Decompress frame 1
		DecompressNextFrame();

		// Setup resampler
		if (sample->getBits() == 8 && !sample->isStereo()) {
			uint8* src = frames[0];
			int    a   = *(src + 0);
			a          = (a | (a << 8)) - 32768;
			int b      = *(src + 1);
			b          = (a | (b << 8)) - 32768;
			int c      = *(src + 2);
			c          = (a | (c << 8)) - 32768;

			interp_l.init(RANGE_REDUX(a), RANGE_REDUX(b), RANGE_REDUX(c));
		}
	}

	void AudioChannel::resampleAndMix(sint16* stream, uint32 bytes) {
		if (!sample || paused) {
			return;
		}

		// Update fp_speed
		fp_speed = (pitch_shift * sample->getRate()) / sample_rate;
		// Get and Mix data
		do {
			int startpos = position;

			// 8 bit resampling
			if (sample->getBits() == 8) {
				if (!sample->isStereo() && stereo) {
					resampleFrameM8toS(
							stream, bytes);    // Mono Sample to Stereo Output
				} else if (!sample->isStereo() && !stereo) {
					resampleFrameM8toM(
							stream, bytes);    // Mono Sample to Stereo Output
				} else if (sample->isStereo() && !stereo) {
					resampleFrameS8toM(
							stream, bytes);    // Stereo Sample to Mono Output
				} else {
					resampleFrameS8toS(
							stream, bytes);    // Stereo Sample to Stereo Output
				}
			} else if (sample->getBits() == 16) {
				if (!sample->isStereo() && stereo) {
					resampleFrameM16toS(
							stream, bytes);    // Mono Sample to Stereo Output
				} else if (!sample->isStereo() && !stereo) {
					resampleFrameM16toM(
							stream, bytes);    // Mono Sample to Mono Output
				} else if (sample->isStereo() && !stereo) {
					resampleFrameS16toM(
							stream, bytes);    // Stereo Sample to Mono Output
				} else {
					resampleFrameS16toS(
							stream, bytes);    // Stereo Sample to Stereo Output
				}
			}
			overall_position
					+= (position - startpos)
					   / (sample->getBits() / (sample->isStereo() ? 4 : 8));
			// We ran out of data
			if (bytes || (position == frame0_size)) {
				// No more data
				if (!frame1_size) {
					if (sample) {
						sample->Release();
					}
					sample = nullptr;
					return;
				}

				// Invert evenodd
				frame_evenodd = 1 - frame_evenodd;

				// Set frame1 to be frame0
				frame0_size = frame1_size;
				position    = 0;

				DecompressNextFrame();
			}

		} while (bytes != 0);
	}

	uint32 AudioChannel::getPlaybackLength() const {
		if (!sample) {
			return UINT32_MAX;
		}
		return ((sample->getPlaybackLength()) * UINT64_C(1000))
			   / sample->getRate();
	}

	uint32 AudioChannel::getPlaybackPosition() const {
		if (!sample) {
			return UINT32_MAX;
		}
		return (overall_position * UINT64_C(1000)) / sample->getRate();
	}

	// Decompress a frame
	void AudioChannel::DecompressNextFrame() {
		// Get next frame of data
		uint8* src2 = frames[1 - frame_evenodd];
		frame1_size = sample->decompressFrame(decomp, src2);

		// No stream, go back to beginning and get first frame
		if (!frame1_size && loop) {
			if (loop != -1) {
				loop--;
			}
			overall_position = 0;
			sample->rewind(decomp);
			frame1_size = sample->decompressFrame(decomp, src2);
		} else if (!frame1_size) {
			sample->freeDecompressor(decomp);
		}
	}

	//
	// 8 Bit
	//

	// Resample a frame of mono 8bit unsigned to Stereo 16bit
	void AudioChannel::resampleFrameM8toS(sint16*& stream, uint32& bytes) {
		uint8* src  = frames[frame_evenodd];
		uint8* src2 = frames[1 - frame_evenodd];

		uint8* src_end  = src + frame0_size;
		uint8* src2_end = src2 + frame1_size;

		src += position;

		int lvol = this->lvol;
		int rvol = this->rvol;

		calculate2DVolume(lvol, rvol);

		do {
			// Add a new src sample (if required)
			if (fp_pos >= 0x10000) {
				if (src + 2 < src_end) {
					int c = *(src + 2);
					c     = (c | (c << 8)) - 32768;
					interp_l.feedData(c);
				} else if (src2 < src2_end) {
					int c = *(src2);
					c     = (c | (c << 8)) - 32768;
					interp_l.feedData(c);
					src2++;
				} else {
					interp_l.feedData();
				}
				src++;
				fp_pos -= 0x10000;
			}

			if (fp_pos < 0x10000) {
				do {
					// Do the interpolation
					const int result = interp_l.interpolate(fp_pos);

					int lresult = *(stream + 0) + (result * lvol) / 256;
					int rresult = *(stream + 1) + (result * rvol) / 256;

					// Enforce range in case of an "overshot". Shouldn't happen
					// since we scale down already, but safe is safe.
					if (lresult < -32768) {
						lresult = -32768;
					} else if (lresult > 32767) {
						lresult = 32767;
					}

					if (rresult < -32768) {
						rresult = -32768;
					} else if (rresult > 32767) {
						rresult = 32767;
					}

					*stream++ = lresult;
					*stream++ = rresult;
					bytes -= 4;
					fp_pos += fp_speed;

				} while (fp_pos < 0x10000 && bytes != 0);
			}

		} while (bytes != 0 && src != src_end);

		position = frame0_size - (src_end - src);
	}

	// Resample a frame of mono 8bit unsigned to Mono 16bit
	void AudioChannel::resampleFrameM8toM(sint16*& stream, uint32& bytes) {
		uint8* src  = frames[frame_evenodd];
		uint8* src2 = frames[1 - frame_evenodd];

		uint8* src_end  = src + frame0_size;
		uint8* src2_end = src2 + frame1_size;

		src += position;

		int lvol = this->lvol;
		int rvol = this->rvol;

		calculate2DVolume(lvol, rvol);

		const int volume = (rvol + lvol) / 2;

		do {
			// Add a new src sample (if required)
			if (fp_pos >= 0x10000) {
				if (src + 2 < src_end) {
					int c = *(src + 2);
					c     = (c | (c << 8)) - 32768;
					interp_l.feedData(c);
				} else if (src2 < src2_end) {
					int c = *(src2);
					c     = (c | (c << 8)) - 32768;
					interp_l.feedData(c);
					src2++;
				} else {
					interp_l.feedData();
				}
				src++;
				fp_pos -= 0x10000;
			}

			if (fp_pos < 0x10000) {
				do {
					// Do the interpolation
					int result = (interp_l.interpolate(fp_pos) * volume) / 256;

					result += *stream;

					// Enforce range in case of an "overshot". Shouldn't happen
					// since we scale down already, but safe is safe.
					if (result < -32768) {
						result = -32768;
					} else if (result > 32767) {
						result = 32767;
					}

					*stream++ = result;
					bytes -= 2;
					fp_pos += fp_speed;

				} while (fp_pos < 0x10000 && bytes != 0);
			}

		} while (bytes != 0 && src != src_end);

		position = frame0_size - (src_end - src);
	}

	// Resample a frame of stereo 8bit unsigned to Mono 16bit
	void AudioChannel::resampleFrameS8toM(sint16*& stream, uint32& bytes) {
		uint8* src  = frames[frame_evenodd];
		uint8* src2 = frames[1 - frame_evenodd];

		uint8* src_end  = src + frame0_size;
		uint8* src2_end = src2 + frame1_size;

		src += position;

		int lvol = this->lvol;
		int rvol = this->rvol;

		calculate2DVolume(lvol, rvol);

		do {
			// Add a new src sample (if required)
			if (fp_pos >= 0x10000) {
				if (src + 4 < src_end) {
					const int c  = *(src + 4);
					const int c2 = *(src + 5);
					interp_l.feedData((c | (c << 8)) - 32768);
					interp_r.feedData((c2 | (c2 << 8)) - 32768);
				} else if (src2 < src2_end) {
					const int c  = *(src2);
					const int c2 = *(src2 + 1);
					interp_l.feedData((c | (c << 8)) - 32768);
					interp_r.feedData((c2 | (c2 << 8)) - 32768);
					src2 += 2;
				} else {
					interp_l.feedData();
					interp_r.feedData();
				}
				src += 2;
				fp_pos -= 0x10000;
			}

			if (fp_pos < 0x10000) {
				do {
					// Do the interpolation
					int result = ((interp_l.interpolate(fp_pos) * lvol
								   + interp_r.interpolate(fp_pos) * rvol))
								 / 512;

					result += *stream;

					// Enforce range in case of an "overshot". Shouldn't happen
					// since we scale down already, but safe is safe.
					if (result < -32768) {
						result = -32768;
					} else if (result > 32767) {
						result = 32767;
					}

					*stream++ = result;
					bytes -= 2;
					fp_pos += fp_speed;

				} while (fp_pos < 0x10000 && bytes != 0);
			}

		} while (bytes != 0 && src != src_end);

		position = frame0_size - (src_end - src);
	}

	// Resample a frame of stereo 8bit unsigned to Stereo 16bit
	void AudioChannel::resampleFrameS8toS(sint16*& stream, uint32& bytes) {
		uint8* src  = frames[frame_evenodd];
		uint8* src2 = frames[1 - frame_evenodd];

		uint8* src_end  = src + frame0_size;
		uint8* src2_end = src2 + frame1_size;

		src += position;

		int lvol = this->lvol;
		int rvol = this->rvol;

		calculate2DVolume(lvol, rvol);

		do {
			// Add a new src sample (if required)
			if (fp_pos >= 0x10000) {
				if (src + 4 < src_end) {
					const int c  = *(src + 4);
					const int c2 = *(src + 5);
					interp_l.feedData((c | (c << 8)) - 32768);
					interp_r.feedData((c2 | (c2 << 8)) - 32768);
				} else if (src2 < src2_end) {
					const int c  = *(src2);
					const int c2 = *(src2 + 1);
					interp_l.feedData((c | (c << 8)) - 32768);
					interp_r.feedData((c2 | (c2 << 8)) - 32768);
					src2 += 2;
				} else {
					interp_l.feedData();
					interp_r.feedData();
				}
				src += 2;
				fp_pos -= 0x10000;
			}

			if (fp_pos < 0x10000) {
				do {
					// Do the interpolation
					int lresult = *(stream + 0)
								  + (interp_l.interpolate(fp_pos) * lvol) / 256;
					int rresult = *(stream + 1)
								  + (interp_r.interpolate(fp_pos) * rvol) / 256;

					// Enforce range in case of an "overshot". Shouldn't happen
					// since we scale down already, but safe is safe.
					if (lresult < -32768) {
						lresult = -32768;
					} else if (lresult > 32767) {
						lresult = 32767;
					}

					if (rresult < -32768) {
						rresult = -32768;
					} else if (rresult > 32767) {
						rresult = 32767;
					}

					*stream++ = lresult;
					*stream++ = rresult;
					bytes -= 4;
					fp_pos += fp_speed;

				} while (fp_pos < 0x10000 && bytes != 0);
			}

		} while (bytes != 0 && src != src_end);

		position = frame0_size - (src_end - src);
	}

	//
	// 16 Bit
	//

	// Resample a frame of mono 16bit unsigned to Stereo 16bit
	void AudioChannel::resampleFrameM16toS(sint16*& stream, uint32& bytes) {
		uint8* src  = frames[frame_evenodd];
		uint8* src2 = frames[1 - frame_evenodd];

		uint8* src_end  = src + frame0_size;
		uint8* src2_end = src2 + frame1_size;

		src += position;

		int lvol = this->lvol;
		int rvol = this->rvol;

		calculate2DVolume(lvol, rvol);

		do {
			// Add a new src sample (if required)
			if (fp_pos >= 0x10000) {
				if (src + 4 < src_end) {
					const int c = ReadSample(src + 4);
					interp_l.feedData(c);
				} else if (src2 < src2_end) {
					const int c = ReadSample(src2);
					interp_l.feedData(c);
					src2 += 2;
				} else {
					interp_l.feedData();
				}
				src += 2;
				fp_pos -= 0x10000;
			}

			if (fp_pos < 0x10000) {
				do {
					// Do the interpolation
					const int result = interp_l.interpolate(fp_pos);

					int lresult = *(stream + 0) + (result * lvol) / 256;
					int rresult = *(stream + 1) + (result * rvol) / 256;

					// Enforce range in case of an "overshot". Shouldn't happen
					// since we scale down already, but safe is safe.
					if (lresult < -32768) {
						lresult = -32768;
					} else if (lresult > 32767) {
						lresult = 32767;
					}

					if (rresult < -32768) {
						rresult = -32768;
					} else if (rresult > 32767) {
						rresult = 32767;
					}

					*stream++ = lresult;
					*stream++ = rresult;
					bytes -= 4;
					fp_pos += fp_speed;

				} while (fp_pos < 0x10000 && bytes != 0);
			}

		} while (bytes != 0 && src != src_end);

		position = frame0_size - (src_end - src);
	}

	// Resample a frame of mono 16bit unsigned to Mono 16bit
	void AudioChannel::resampleFrameM16toM(sint16*& stream, uint32& bytes) {
		uint8* src  = frames[frame_evenodd];
		uint8* src2 = frames[1 - frame_evenodd];

		uint8* src_end  = src + frame0_size;
		uint8* src2_end = src2 + frame1_size;

		src += position;

		int lvol = this->lvol;
		int rvol = this->rvol;

		calculate2DVolume(lvol, rvol);

		const int volume = (rvol + lvol) / 2;

		do {
			// Add a new src sample (if required)
			if (fp_pos >= 0x10000) {
				if (src + 4 < src_end) {
					const int c = ReadSample(src + 4);
					interp_l.feedData(c);
				} else if (src2 < src2_end) {
					const int c = ReadSample(src2);
					interp_l.feedData(c);
					src2 += 2;
				} else {
					interp_l.feedData();
				}
				src += 2;
				fp_pos -= 0x10000;
			}

			if (fp_pos < 0x10000) {
				do {
					// Do the interpolation
					int result = (interp_l.interpolate(fp_pos) * volume) / 256;

					result += *stream;

					// Enforce range in case of an "overshot". Shouldn't happen
					// since we scale down already, but safe is safe.
					if (result < -32768) {
						result = -32768;
					} else if (result > 32767) {
						result = 32767;
					}

					*stream++ = result;
					bytes -= 2;
					fp_pos += fp_speed;

				} while (fp_pos < 0x10000 && bytes != 0);
			}

		} while (bytes != 0 && src != src_end);

		position = frame0_size - (src_end - src);
	}

	// Resample a frame of stereo 16bit unsigned to Mono 16bit
	void AudioChannel::resampleFrameS16toM(sint16*& stream, uint32& bytes) {
		uint8* src  = frames[frame_evenodd];
		uint8* src2 = frames[1 - frame_evenodd];

		uint8* src_end  = src + frame0_size;
		uint8* src2_end = src2 + frame1_size;

		src += position;

		int lvol = this->lvol;
		int rvol = this->rvol;

		calculate2DVolume(lvol, rvol);

		do {
			// Add a new src sample (if required)
			if (fp_pos >= 0x10000) {
				if (src + 8 < src_end) {
					const int c  = ReadSample(src + 8);
					const int c2 = ReadSample(src + 10);
					interp_l.feedData(c);
					interp_r.feedData(c2);
				} else if (src2 < src2_end) {
					const int c  = ReadSample(src2);
					const int c2 = ReadSample(src2 + 2);
					interp_l.feedData(c);
					interp_r.feedData(c2);
					src2 += 4;
				} else {
					interp_l.feedData();
					interp_r.feedData();
				}
				src += 4;
				fp_pos -= 0x10000;
			}

			if (fp_pos < 0x10000) {
				do {
					// Do the interpolation
					int result = ((interp_l.interpolate(fp_pos) * lvol
								   + interp_r.interpolate(fp_pos) * rvol))
								 / 512;

					result += *stream;

					// Enforce range in case of an "overshot". Shouldn't happen
					// since we scale down already, but safe is safe.
					if (result < -32768) {
						result = -32768;
					} else if (result > 32767) {
						result = 32767;
					}

					*stream++ = result;
					bytes -= 2;
					fp_pos += fp_speed;

				} while (fp_pos < 0x10000 && bytes != 0);
			}

		} while (bytes != 0 && src != src_end);

		position = frame0_size - (src_end - src);
	}

	// Resample a frame of stereo 16bit unsigned to Stereo 16bit
	void AudioChannel::resampleFrameS16toS(sint16*& stream, uint32& bytes) {
		uint8* src  = frames[frame_evenodd];
		uint8* src2 = frames[1 - frame_evenodd];

		uint8* src_end  = src + frame0_size;
		uint8* src2_end = src2 + frame1_size;

		src += position;

		int lvol = this->lvol;
		int rvol = this->rvol;

		calculate2DVolume(lvol, rvol);

		do {
			// Add a new src sample (if required)
			if (fp_pos >= 0x10000) {
				if (src + 8 < src_end) {
					const int c  = ReadSample(src + 8);
					const int c2 = ReadSample(src + 10);
					interp_l.feedData(c);
					interp_r.feedData(c2);
				} else if (src2 < src2_end) {
					const int c  = ReadSample(src2);
					const int c2 = ReadSample(src2 + 2);
					interp_l.feedData(c);
					interp_r.feedData(c2);
					src2 += 4;
				} else {
					interp_l.feedData();
					interp_r.feedData();
				}
				src += 4;
				fp_pos -= 0x10000;
			}

			if (fp_pos < 0x10000) {
				do {
					// Do the interpolation
					int lresult = *(stream + 0)
								  + (interp_l.interpolate(fp_pos) * lvol) / 256;
					int rresult = *(stream + 1)
								  + (interp_r.interpolate(fp_pos) * rvol) / 256;

					// Enforce range in case of an "overshot". Shouldn't happen
					// since we scale down already, but safe is safe.
					if (lresult < -32768) {
						lresult = -32768;
					} else if (lresult > 32767) {
						lresult = 32767;
					}

					if (rresult < -32768) {
						rresult = -32768;
					} else if (rresult > 32767) {
						rresult = 32767;
					}

					*stream++ = lresult;
					*stream++ = rresult;
					bytes -= 4;
					fp_pos += fp_speed;

				} while (fp_pos < 0x10000 && bytes != 0);
			}

		} while (bytes != 0 && src != src_end);

		position = frame0_size - (src_end - src);
	}

	void AudioChannel::calculate2DVolume(int& lvol, int& rvol) {
		if (distance > 255) {
			lvol = 0;
			rvol = 0;
			return;
		}

		int lbal = 256;
		int rbal = 256;

		if (balance < 0) {
			if (balance < -256) {
				rbal = 0;
			} else {
				rbal = balance + 256;
			}
		} else if (balance > 0) {
			if (balance > 256) {
				lbal = 0;
			} else {
				lbal = 256 - balance;
			}
		}

		lvol = lvol * (256 - distance) * lbal / 65536;
		rvol = rvol * (256 - distance) * rbal / 65536;
	}

}    // namespace Pentagram
