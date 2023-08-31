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
#include "AudioMixer.h"
#include "Configuration.h"

#include "AudioChannel.h"

#include "MidiDriver.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include <algorithm>
#include <iostream>
#include <limits>
#include <mutex>

#include "Midi.h"

namespace Pentagram {
	class SDLAudioDevice {
	public:
		SDLAudioDevice(SDL_AudioStream *stream_) :
		    dev(SDL_GetAudioStreamDevice(stream_)), stream(stream_) {}
		~SDLAudioDevice() {
			SDL_CloseAudioDevice(dev);
		}

		void pause() {
			SDL_PauseAudioDevice(dev);
		}

		void unpause() {
			SDL_ResumeAudioDevice(dev);
		}

		void lock() {
			SDL_LockAudioStream(stream);
		}

		void unlock() {
			SDL_UnlockAudioStream(stream);
		}

	private:
		SDL_AudioDeviceID dev;
		SDL_AudioStream  *stream;
	};
}

using namespace Pentagram;

AudioMixer *AudioMixer::the_audio_mixer = nullptr;

AudioMixer::AudioMixer(int sample_rate_, bool stereo_, int num_channels_) :
audio_ok(false),
sample_rate(sample_rate_), stereo(stereo_),
midi(nullptr), midi_volume(255),
id_counter(0)
{
	the_audio_mixer = this;

	std::cout << "Creating AudioMixer..." << std::endl;

	SDL_AudioSpec desired{};
	SDL_AudioSpec obtained;

	desired.format = SDL_AUDIO_S16SYS;
	desired.freq = sample_rate_;
	desired.channels = stereo_?2:1;

	// Set update rate to 5 Hz, or there abouts. This should be more than adequate for everyone
	// Note: setting this to 1 Hz (/1) causes Exult to hang on MacOS.
	int samples = 1;                                     // No need to pass it to SDL3
	while ( samples <= desired.freq / 5 ) samples <<= 1; // No need to pass it to SDL3

	// Open SDL Audio (even though we may not need it)
	SDL_InitSubSystem(SDL_INIT_AUDIO);
	stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &desired, sdlAudioCallback, this);
	audio_ok = (stream != nullptr);

	if (audio_ok) {
		SDL_GetAudioStreamFormat(stream, &obtained, nullptr);
		pout << "Audio opened using format: " << obtained.freq << " Hz from desired " << sample_rate_ << " Hz "
		     << static_cast<int>(obtained.channels) << " Channels" <<  std::endl;
		device = std::make_unique<SDLAudioDevice>(stream);
		{
			const std::lock_guard<SDLAudioDevice> lock(*device);
			sample_rate = obtained.freq;
			stereo = obtained.channels == 2;

			internal_buffer . resize( samples * 2 );     // No need to pass it to SDL3
			for (int i = 0; i < num_channels_; i++)
				channels.emplace_back(sample_rate, stereo);
		}

		// GO GO GO!
		device->unpause();
	}
}

AudioMixer::~AudioMixer()
{
	std::cout << "Destroying AudioMixer..." << std::endl;

	closeMidiOutput();

	if (stream) {
		SDL_DestroyAudioStream(stream);
		stream = nullptr;
	}
	SDL_QuitSubSystem(SDL_INIT_AUDIO);

	the_audio_mixer = nullptr;
}

void AudioMixer::reset()
{
	if (!audio_ok) return;

	std::cout << "Resetting AudioMixer..." << std::endl;

	midi->stop_music();

	const std::lock_guard<SDLAudioDevice> lock(*device);
	for (auto& channel : channels) channel.stop();

	if (stream) {
		SDL_FlushAudioStream(stream);
	}
}

sint32 AudioMixer::playSample(AudioSample *sample, int loop, int priority, bool paused, uint32 pitch_shift_, int lvol, int rvol)
{
	if (!audio_ok || channels.empty()) return -1;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	auto it = std::find_if(channels.begin(), channels.end(), [](auto& channel) {
		return !channel.isPlaying();
	});
	if (it == channels.end()) {
		it = std::min_element(channels.begin(), channels.end(), [](auto& c1, auto& c2) {
			return c1.getPriority() < c2.getPriority();
		});
	}
	if (it != channels.end() && (!it->isPlaying() || it->getPriority() < priority)) {
		if (id_counter == std::numeric_limits<decltype(id_counter)>::max()) {
			id_counter = 0;
		} else {
			++id_counter;
		}
		it->playSample(sample, loop, priority, paused, pitch_shift_, lvol, rvol, id_counter);
		return id_counter;
	}
	return -1;
}

bool AudioMixer::isPlaying(sint32 instance_id) const
{
	if (instance_id < 0 || channels.empty() || !audio_ok) return false;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	auto it = std::find_if(channels.cbegin(), channels.cend(), [instance_id](auto& channel) {
		return channel.getInstanceId() == instance_id;
	});
	return it != channels.end() && it->isPlaying();
}

bool AudioMixer::isPlaying(AudioSample *sample) const
{
	if (!sample || channels.empty() || !audio_ok) return false;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	auto it = std::find_if(channels.cbegin(), channels.cend(), [sample](auto& channel) {
		return channel.getSample() == sample;
	});
	return it != channels.end();
}

bool AudioMixer::isPlayingVoice() const
{
	if (channels.empty() || !audio_ok) return false;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	auto it = std::find_if(channels.cbegin(), channels.cend(), [](auto& channel) {
		return channel.isPlaying() && channel.getSample()->isVocSample();
	});
	return it != channels.end();
}

void AudioMixer::stopSample(sint32 instance_id)
{
	if (instance_id < 0 || channels.empty() || !audio_ok) return;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	auto it = std::find_if(channels.begin(), channels.end(), [instance_id](auto& channel) {
		return channel.getInstanceId() == instance_id;
	});
	if (it != channels.end()) {
		it->stop();
	}
}

void AudioMixer::stopSample(AudioSample *sample)
{
	if (!sample || channels.empty() || !audio_ok) return;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	for (auto& channel : channels)
	{
		if (channel.getSample() == sample)
			channel.stop();
	}
}

void AudioMixer::setPaused(sint32 instance_id, bool paused)
{
	if (instance_id < 0 || channels.empty() || !audio_ok) return;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	auto it = std::find_if(channels.begin(), channels.end(), [instance_id](auto& channel) {
		return channel.getInstanceId() == instance_id;
	});
	if (it != channels.end()) {
		it->setPaused(paused);
	}
}

bool AudioMixer::isPaused(sint32 instance_id) const
{
	if (instance_id < 0 || channels.empty() || !audio_ok) return false;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	auto it = std::find_if(channels.cbegin(), channels.cend(), [instance_id](auto& channel) {
		return channel.getInstanceId() == instance_id;
	});
	return it != channels.end() && it->isPaused();
}

void AudioMixer::setPausedAll(bool paused)
{
	if (channels.empty() || !audio_ok) return;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	for (auto& channel : channels)
		channel.setPaused(paused);
}

void AudioMixer::setVolume(sint32 instance_id, int lvol, int rvol)
{
	if (instance_id < 0 || channels.empty() || !audio_ok) return;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	auto it = std::find_if(channels.begin(), channels.end(), [instance_id](auto& channel) {
		return channel.getInstanceId() == instance_id;
	});
	if (it != channels.end()) {
		it->setVolume(lvol, rvol);
	}
}

void AudioMixer::getVolume(sint32 instance_id, int &lvol, int &rvol) const
{
	if (instance_id < 0 || channels.empty() || !audio_ok) return;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	auto it = std::find_if(channels.cbegin(), channels.cend(), [instance_id](auto& channel) {
		return channel.getInstanceId() == instance_id;
	});
	if (it != channels.end()) {
		it->getVolume(lvol, rvol);
	}
}


bool AudioMixer::set2DPosition(sint32 instance_id, int distance, int angle)
{
	if (instance_id < 0 || channels.empty() || !audio_ok) return false;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	auto it = std::find_if(channels.begin(), channels.end(), [instance_id](auto& channel) {
		return channel.getInstanceId() == instance_id;
	});
	if (it != channels.end()) {
		it->set2DPosition(distance, angle);
		return it->isPlaying();
	}
	return false;
}

void AudioMixer::get2DPosition(sint32 instance_id, int &distance, int &angle) const
{
	if (instance_id < 0 || channels.empty() || !audio_ok) return;

	const std::lock_guard<SDLAudioDevice> lock(*device);
	auto it = std::find_if(channels.cbegin(), channels.cend(), [instance_id](auto& channel) {
		return channel.getInstanceId() == instance_id;
	});
	if (it != channels.end()) {
		it->get2DPosition(distance, angle);
	}
}

void AudioMixer::sdlAudioCallback(void *userdata, SDL_AudioStream *stream, int len)
{
	auto *mixer = static_cast<AudioMixer *>(userdata);
	// Unfortunately, SDL does not guarantee that stream will be aligned to
	// the correct alignment for sint16.
	// There is no real solution except using an aligned buffer and copying.
	const size_t newlen = size_t(len + 1) / 2;
	// This should never be needed, as we set the vector length
	// based on information provided by SDL. Lets leave it in anyway
	// just in case...
	if (newlen > mixer->internal_buffer.size()) {
		mixer->internal_buffer.resize(newlen);
	}
	if (len == 0) return;
	mixer->MixAudio(mixer->internal_buffer.data(), mixer->internal_buffer.size()*2); // len);
	SDL_PutAudioStreamData(stream, mixer->internal_buffer.data(), mixer->internal_buffer.size()*2); // len);
}

void AudioMixer::MixAudio(sint16 *stream, uint32 bytes)
{
	if (!audio_ok) return;
	std::memset(stream, 0, bytes);
	if (midi) midi->produceSamples(stream, bytes);
	for (auto& channel : channels)
		if (channel.isPlaying()) channel.resampleAndMix(stream,bytes);
}

void AudioMixer::openMidiOutput()
{
	if (midi) return;
	if (!audio_ok) return;

	auto * new_driver = new MyMidiPlayer();

	{
		const std::lock_guard<SDLAudioDevice> lock(*device);
		midi = new_driver;
	}
	midi->load_timbres();
	//midi_driver->setGlobalVolume(midi_volume);
}

void AudioMixer::closeMidiOutput()
{
	if (!midi) return;
	std::cout << "Destroying MidiDriver..." << std::endl;

	midi->stop_music(true);
	midi->destroyMidiDriver();

	const std::lock_guard<SDLAudioDevice> lock(*device);
	delete midi;
	midi = nullptr;
}
