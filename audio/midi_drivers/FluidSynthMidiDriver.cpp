/* 
 * Copyright (C) 2001-2005 The ScummVM project
 * Copyright (C) 2005 The Pentagram Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#include "pent_include.h"
#include "FluidSynthMidiDriver.h"
#include <string>
#include <vector>

#ifdef USE_FLUIDSYNTH_MIDI

//#include <cstring>

const MidiDriver::MidiDriverDesc FluidSynthMidiDriver::desc = 
		MidiDriver::MidiDriverDesc ("FluidSynth", createInstance);

// MidiDriver method implementations

void FluidSynthMidiDriver::setInt(const char *name, int val) {
	//char *name2 = strdup(name);
	char *name2 = const_cast<char*>(name);

	fluid_settings_setint(_settings, name2, val);
	//std::free(name2);
}

void FluidSynthMidiDriver::setNum(const char *name, double val) {
	//char *name2 = strdup(name);
	char *name2 = const_cast<char*>(name);

	fluid_settings_setnum(_settings, name2, val);
	//std::free(name2);
}

void FluidSynthMidiDriver::setStr(const char *name, const char *val) {
	//char *name2 = strdup(name);
	//char *val2 = strdup(val);
	char *name2 = const_cast<char*>(name);
	char *val2 = const_cast<char*>(val);

	fluid_settings_setstr(_settings, name2, val2);
	//std::free(name2);
	//std::free(val2);
}

int FluidSynthMidiDriver::open() {

	if (!stereo) {
		perr << "FluidSynth only works with Stereo output" << std::endl;
		return -1;
	}

	std::string sfsetting = "fluidsynth_soundfont";
	std::vector<std::string> soundfonts;
	std::string soundfont;
	for (size_t i = 0; i < 10; i++) {
		std::string settingkey = sfsetting + static_cast<char>(i + '0');
		soundfont = getConfigSetting(settingkey, "");
		if (!soundfont.empty())
			soundfonts.push_back(soundfont);
	}
	soundfont = getConfigSetting(sfsetting, "");
	if (!soundfont.empty())
		soundfonts.push_back(soundfont);

	if (soundfonts.empty()) {
		perr << "FluidSynth requires a 'fluidsynth_soundfont' setting" << std::endl;
		return -2;
	}

	_settings = new_fluid_settings();

	// The default gain setting is ridiculously low, but we can't set it
	// too high either or sound will be clipped. This may need tuning...

	setNum("synth.gain", 2.1);
	setNum("synth.sample-rate", sample_rate);

	_synth = new_fluid_synth(_settings);

	// In theory, this ought to reduce CPU load... but it doesn't make any
	// noticeable difference for me, so disable it for now.

	// fluid_synth_set_interp_method(_synth, -1, FLUID_INTERP_LINEAR);
	// fluid_synth_set_reverb_on(_synth, 0);
	// fluid_synth_set_chorus_on(_synth, 0);

	int numloaded = 0;
	for (std::vector<std::string>::const_iterator it = soundfonts.begin();
	     it != soundfonts.end(); ++it)
	{
		int soundFont = fluid_synth_sfload(_synth, it->c_str(), 1);
		if (soundFont == -1) {
			perr << "Failed loading custom sound font '" << *it << "'" << std::endl;
		} else {
			perr << "Loaded custom sound font '" << *it << "'" << std::endl;
			_soundFont.push(soundFont);
			numloaded++;
		}
		
	}
	if (numloaded == 0) {
		perr << "Failed to load any custom sound fonts; giving up." << std::endl;
		return -3;
	}

	return 0;
}

void FluidSynthMidiDriver::close() {
	while (!_soundFont.empty()) {
		int soundfont = _soundFont.top();
		_soundFont.pop();
		fluid_synth_sfunload(_synth, soundfont, 1);
	}

	delete_fluid_synth(_synth);
	_synth = nullptr;
	delete_fluid_settings(_settings);
	_settings = nullptr;
}

void FluidSynthMidiDriver::send(uint32 b) {
	//uint8 param3 = static_cast<uint8>((b >> 24) & 0xFF);
	uint32 param2 = static_cast<uint8>((b >> 16) & 0xFF);
	uint32 param1 = static_cast<uint8>((b >>  8) & 0xFF);
	uint8 cmd     = static_cast<uint8>(b & 0xF0);
	uint8 chan    = static_cast<uint8>(b & 0x0F);

	switch (cmd) {
	case 0x80:	// Note Off
		fluid_synth_noteoff(_synth, chan, param1);
		break;
	case 0x90:	// Note On
		fluid_synth_noteon(_synth, chan, param1, param2);
		break;
	case 0xA0:	// Aftertouch
		break;
	case 0xB0:	// Control Change
		fluid_synth_cc(_synth, chan, param1, param2);
		break;
	case 0xC0:	// Program Change
		fluid_synth_program_change(_synth, chan, param1);
		break;
	case 0xD0:	// Channel Pressure
		break;
	case 0xE0:	// Pitch Bend
		fluid_synth_pitch_bend(_synth, chan, (param2 << 7) | param1);
		break;
	case 0xF0:	// SysEx
		// We should never get here! SysEx information has to be
		// sent via high-level semantic methods.
		perr << "FluidSynthMidiDriver: Receiving SysEx command on a send() call" << std::endl;
		break;
	default:
		perr << "FluidSynthMidiDriver: Unknown send() command 0x" << std::hex << cmd << std::dec << std::endl;
		break;
	}
}

void FluidSynthMidiDriver::lowLevelProduceSamples(sint16 *samples, uint32 num_samples) {
	fluid_synth_write_s16(_synth, num_samples, samples, 0, 2, samples, 1, 2);
}

#endif
