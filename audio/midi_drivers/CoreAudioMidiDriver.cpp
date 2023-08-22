/*
Code originally written by Max Horn for ScummVM,
later improvements by Matthew Hoops,
minor tweaks by various other people of the ScummVM, Pentagram
and Exult teams.

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
#include "CoreAudioMidiDriver.h"
#include "Configuration.h"
#include "exceptions.h"

#ifdef USE_CORE_AUDIO_MIDI

#include <pthread.h>
#include <sched.h>
#include <iostream>

class   CoreAudioException : public exult_exception {
	OSStatus _err;
	unsigned long _line;
public:
	CoreAudioException(OSStatus err, unsigned long line):
		exult_exception("CoreAudio initialization failed"), _err(err), _line(line) {  }
	OSStatus get_err() const {
		return _err;
	}
	unsigned long get_line() const {
		return _line;
	}
};

// A macro to simplify error handling a bit.
#define RequireNoErr_Inner(error,location) \
	do { \
		err = error; \
		if (err != noErr) \
			throw CoreAudioException(err, location); \
	} while (false)
#define RequireNoErr(error) RequireNoErr_Inner(error,__LINE__)

const MidiDriver::MidiDriverDesc CoreAudioMidiDriver::desc =
    MidiDriver::MidiDriverDesc("CoreAudio", createInstance);

CoreAudioMidiDriver::CoreAudioMidiDriver() :
	_auGraph(nullptr) {
}

int CoreAudioMidiDriver::open() {
	OSStatus err = noErr;

	if (_auGraph)
		return 1;

	try {
		// Open the Music Device.
		RequireNoErr(NewAUGraph(&_auGraph));
		AUNode outputNode, synthNode;
		AudioComponentDescription desc;

		// The default output device
		desc.componentType = kAudioUnitType_Output;
#ifdef __IOS__
		desc.componentSubType = kAudioUnitSubType_RemoteIO;
#else
		desc.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif
		desc.componentManufacturer = kAudioUnitManufacturer_Apple;
		desc.componentFlags = 0;
		desc.componentFlagsMask = 0;

		RequireNoErr(AUGraphAddNode(_auGraph, &desc, &outputNode));

		// The built-in default (softsynth) music device
		desc.componentType = kAudioUnitType_MusicDevice;
#ifdef __IOS__
		desc.componentSubType = kAudioUnitSubType_MIDISynth;
#else
		desc.componentSubType = kAudioUnitSubType_DLSSynth;
#endif
		desc.componentManufacturer = kAudioUnitManufacturer_Apple;

		RequireNoErr(AUGraphAddNode(_auGraph, &desc, &synthNode));

		// Connect the softsynth to the default output
		RequireNoErr(AUGraphConnectNodeInput(_auGraph, synthNode, 0, outputNode, 0));

		// Open and initialize the whole graph
		RequireNoErr(AUGraphOpen(_auGraph));
		RequireNoErr(AUGraphInitialize(_auGraph));

		// Get the music device from the graph.
		RequireNoErr(AUGraphNodeInfo(_auGraph, synthNode, nullptr, &_synth));

#ifdef __IOS__
		// on iOS we make sure there is a soundfont loaded for CoreAudio to work
		if (!config->key_exists("config/audio/midi/coreaudio_soundfont"))
			config->set("config/audio/midi/coreaudio_soundfont", "gs_instruments.dls", true);
#endif
		// Load custom soundfont, if specified
		if (config->key_exists("config/audio/midi/coreaudio_soundfont")) {
			std::string soundfont = getConfigSetting("coreaudio_soundfont", "");
			if (!soundfont.empty()) {
				// is the full path entered or is it in the App bundle or the data folder
				std::string options[] = {"", "<BUNDLE>", "<DATA>"};
				for (auto& d : options) {
					std::string f;
					if (!d.empty()) {
						if (!is_system_path_defined(d)) {
							continue;
						}
						f = get_system_path(d);
						f += '/';
						f += soundfont;
					} else {
						f = soundfont;
					}
					if (U7exists(f.c_str()))
						soundfont = f;
				}
				std::cout << "Loading SoundFont '" << soundfont << "'" << std::endl;
				if (!soundfont.empty() && U7exists(soundfont.c_str())) {
					OSErr err;
					CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
					               reinterpret_cast<const UInt8 *>(soundfont.c_str()),
					               soundfont.size(), false);
					if (url) {
						err = AudioUnitSetProperty(
						          _synth, kMusicDeviceProperty_SoundBankURL,
						          kAudioUnitScope_Global, 0, &url, sizeof(url));
						CFRelease(url);
					} else {
						std::cout << "Failed to allocate CFURLRef from '" << soundfont << "'" << std::endl;
						// after trying and failing to load a soundfont it's better
						// to fail initializing the CoreAudio driver or it might crash
						return 1;
					}
					if (!err) {
						std::cout << "Loaded!" << std::endl;
					} else {
						std::cout << "Error loading SoundFont '" << soundfont << "'" << std::endl;
						// after trying and failing to load a soundfont it's better
						// to fail initializing the CoreAudio driver or it might crash
						return 1;
					}
				} else {
					std::cout << "Path to SoundFont '" << soundfont << "' not found. Continuing without." << std::endl;
				}
			}
		}

		// Finally: Start the graph!
		RequireNoErr(AUGraphStart(_auGraph));
	} catch (CoreAudioException const &error) {
#ifdef DEBUG
		std::cerr << error.what() << " at " << __FILE__ << ":" << error.get_line()
		          << " with error code " << static_cast<int>(error.get_err())
		          << std::endl;
#endif
		if (_auGraph) {
			AUGraphStop(_auGraph);
			DisposeAUGraph(_auGraph);
			_auGraph = nullptr;
		}
	}
	return 0;
}

void CoreAudioMidiDriver::close() {
	// Stop the output
	if (_auGraph) {
		AUGraphStop(_auGraph);
		DisposeAUGraph(_auGraph);
		_auGraph = nullptr;
	}
}

void CoreAudioMidiDriver::send(uint32 message) {
	uint8 status_byte = (message & 0x000000FF);
	uint8 first_byte = (message & 0x0000FF00) >> 8;
	uint8 second_byte = (message & 0x00FF0000) >> 16;
	// you need to preload the soundfont patches by setting
	// kAUMIDISynthProperty_EnablePreload to true, load the patch
	// and then turn off kAUMIDISynthProperty_EnablePreload
	uint32 enabled = 1;
	uint32 disabled = 0;
	assert(_auGraph != nullptr);
	AudioUnitSetProperty(
	          _synth, kAUMIDISynthProperty_EnablePreload,
	          kAudioUnitScope_Global, 0, &enabled, sizeof(enabled));
	MusicDeviceMIDIEvent(_synth, uint32(0xC0 | status_byte), first_byte, 0, 0);
	AudioUnitSetProperty(
	          _synth, kAUMIDISynthProperty_EnablePreload,
	          kAudioUnitScope_Global, 0, &disabled, sizeof(disabled));
	auto cmd = static_cast<uint32>(status_byte & 0xF0);
	switch (cmd){
		case 0x80:
			MusicDeviceMIDIEvent(_synth, status_byte, first_byte, 0, 0);
			break;
		case 0x90:	// Note On
			MusicDeviceMIDIEvent(_synth, status_byte, first_byte, second_byte, 0);
			break;
		case 0xA0:	// Aftertouch
			break;
		case 0xB0:	// Control Change
			MusicDeviceMIDIEvent(_synth, status_byte, first_byte, second_byte, 0);
			break;
		case 0xC0:	// Program Change
			MusicDeviceMIDIEvent(_synth, status_byte, first_byte, 0, 0);
			break;
		case 0xD0:	// Channel Pressure
			break;
		case 0xE0:	// Pitch Bend
			MusicDeviceMIDIEvent(_synth, status_byte, first_byte, second_byte, 0);
			break;
		case 0xF0:	// SysEx
			// We should never get here! SysEx information has to be
			// sent via high-level semantic methods.
			std::cout << "CoreAudioMidiDriver: Receiving SysEx command on a send() call" << std::endl;
			break;
		default:
			std::cout << "CoreAudioMidiDriver: Unknown send() command 0x" << std::hex << cmd << std::dec << std::endl;
			break;
	}
}

void CoreAudioMidiDriver::send_sysex(uint8 status, const uint8 *msg, uint16 length) {
	uint8 buf[384];

	assert(sizeof(buf) >= static_cast<size_t>(length) + 2);
	assert(_auGraph != nullptr);

	// Add SysEx frame
	buf[0] = status;
	memcpy(buf + 1, msg, length);
	buf[length + 1] = 0xF7;

	MusicDeviceSysEx(_synth, buf, length + 2);
}

void CoreAudioMidiDriver::increaseThreadPriority() {
	pthread_t self;
	int policy;
	struct sched_param param;

	self = pthread_self();
	pthread_getschedparam(self, &policy, &param);
	param.sched_priority = sched_get_priority_max(policy);
	pthread_setschedparam(self, policy, &param);
}

void CoreAudioMidiDriver::yield() {
	sched_yield();
}

#endif //USE_CORE_AUDIO_MIDI
