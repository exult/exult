/*
Copyright (C) 2005 The Pentagram team
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
#ifndef AUDIOMIXER_H_INCLUDED
#define AUDIOMIXER_H_INCLUDED

#include "common_types.h"

#include <memory>
#include <vector>

class MyMidiPlayer;
struct SDL_AudioStream;

#define AUDIO_MAX_VOLUME 256
#define AUDIO_DEF_PITCH  0x10000

namespace Pentagram {
	class AudioChannel;
	class AudioSample;
	class SDLAudioDevice;

	class AudioMixer {
	public:
		AudioMixer(int sample_rate, bool stereo, int num_channels);
		~AudioMixer();

		static AudioMixer* get_instance() {
			return the_audio_mixer;
		}

		void reset();

		sint32 playSample(
				AudioSample* sample, int loop, int priority,
				bool paused = false, uint32 pitch_shift = AUDIO_DEF_PITCH,
				int lvol = AUDIO_MAX_VOLUME, int rvol = AUDIO_MAX_VOLUME);
		bool   isPlaying(sint32 instance_id) const;
		bool   isPlaying(AudioSample* sample) const;
		bool   isPlayingVoice() const;
		void   stopSample(sint32 instance_id);
		void   stopSample(AudioSample* sample);
		sint32 getLoop(sint32 instance_id) const;
		void   setLoop(sint32 instance_id, sint32 newloop);

		void setPaused(sint32 instance_id, bool paused);
		bool isPaused(sint32 instance_id) const;

		void setPausedAll(bool paused);

		void setVolume(sint32 instance_id, int lvol, int rvol);
		void getVolume(sint32 instance_id, int& lvol, int& rvol) const;

		bool set2DPosition(sint32 instance_id, int distance, int angle);
		void get2DPosition(sint32 instance_id, int& distance, int& angle) const;

		// Get playback length in MS or UIN32_MAX on error
		uint32 GetPlaybackLength(sint32 instance_id);
		// Get playback position in MS or UIN32_MAX on error
		uint32 GetPlaybackPosition(sint32 instance_id);
		void   openMidiOutput();
		void   closeMidiOutput();

		MyMidiPlayer* getMidiPlayer() const {
			return midi;
		}

		uint32 getSampleRate() const {
			return sample_rate;
		}

		bool getStereo() const {
			return stereo;
		}

	private:
		bool                audio_ok;
		uint32              sample_rate;
		bool                stereo;
		MyMidiPlayer*       midi;
		int                 midi_volume;
		std::vector<sint16> internal_buffer;

		std::vector<AudioChannel> channels;
		sint32                    id_counter;

		std::unique_ptr<SDLAudioDevice> device;

		void        init_midi();
		static void sdlAudioCallback(
				void* userdata, SDL_AudioStream* stream, int len, int maxlen);
		SDL_AudioStream* stream;

		void MixAudio(sint16* stream, uint32 bytes);

		static AudioMixer* the_audio_mixer;
	};

}    // namespace Pentagram

#endif    // AUDIOMIXER_H_INCLUDED
