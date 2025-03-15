/*
 * Copyright (C) 2001-2005 The ScummVM project
 * Copyright (C) 2005 The Pentagram Team
 * Copyright (C) 2006-2022 The Exult Team
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

#ifdef USE_FLUIDSYNTH_MIDI
#	include "FluidSynthMidiDriver.h"

#	include <string>
#	include <vector>

// #include <cstring>

const MidiDriver::MidiDriverDesc FluidSynthMidiDriver::desc
		= MidiDriver::MidiDriverDesc("FluidSynth", createInstance);

// MidiDriver method implementations

std::vector<ConfigSetting_widget::Definition> FluidSynthMidiDriver::
		GetSettings() {
	ConfigSetting_widget::Definition soundfont{
			"Soundfont",                                  // label
			"config/audio/midi/fluidsynth_soundfont",     // config_setting
			10,                                           // additional
			true,                                         // required
			true,                                         // unique
			ConfigSetting_widget::Definition::dropdown    // setting_type
	};
	soundfont.add_filenames_to_choices("<BUNDLE>/*.sf2");
	soundfont.add_filenames_to_choices("<DATA>/*.sf2");
	soundfont.add_filenames_to_choices("<BUNDLE>/*.SF2");
	soundfont.add_filenames_to_choices("<DATA>/*.SF2");
	soundfont.default_value = "default.sf2";

	soundfont.sort_choices();
	auto settings = MidiDriver::GetSettings();
	settings.push_back(std::move(soundfont));
	return settings;
}

int FluidSynthMidiDriver::setInt(const char* name, int val) {
	return fluid_settings_setint(_settings, name, val);
}

int FluidSynthMidiDriver::setNum(const char* name, double val) {
	return fluid_settings_setnum(_settings, name, val);
}

int FluidSynthMidiDriver::setStr(const char* name, const char* val) {
	return fluid_settings_setstr(_settings, name, val);
}

int FluidSynthMidiDriver::getStr(const char* name, char* val, size_t size) {
	val[0] = 0;
#	ifdef USING_FLUIDSYNTH
	return fluid_settings_copystr(_settings, name, val, size);
#	else
	char* str;
	if (fluid_settings_getstr(_settings, name, &str) && str && *str) {
		std::strncpy(val, str, size);
		return FLUID_OK;
	}
#	endif
	return -1;
}

int FluidSynthMidiDriver::open() {
	if (!stereo) {
		perr << "FluidSynth only works with Stereo output" << std::endl;
		return -1;
	}

	const std::string        sfsetting = "fluidsynth_soundfont";
	std::vector<std::string> soundfonts;
	std::string              soundfont;
#	ifdef ANDROID
	// on Android only try <DATA>
	std::string options[] = {"<DATA>"};
#	else
	std::string options[] = {"", "<BUNDLE>", "<DATA>"};
#	endif
	soundfont = getConfigSetting(sfsetting, "default.sf2");
	if (!soundfont.empty()) {
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
			if (U7exists(f.c_str())) {
				soundfont = std::move(f);
			}
		}
		if (U7exists(soundfont.c_str())) {
			soundfonts.push_back(soundfont);
		}
	}
	for (size_t i = 0; i < 10; i++) {
		const std::string settingkey = sfsetting + static_cast<char>(i + '0');
		soundfont                    = getConfigSetting(settingkey, "");
		if (!soundfont.empty()) {
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
				if (U7exists(f.c_str())) {
					soundfont = std::move(f);
				}
			}
			soundfonts.push_back(soundfont);
		}
	}

	_settings = new_fluid_settings();

	if (soundfonts.empty()) {
		char default_soundfont[512];
		if (getStr("synth.default-soundfont", default_soundfont,
				   sizeof(default_soundfont))
					== FLUID_OK
			&& default_soundfont[0] != 0) {
			// try whether the FluidSynth default soundfont is in our paths
			if (!U7exists(default_soundfont)) {
				for (auto& d : options) {
					std::string f;
					if (!d.empty()) {
						if (!is_system_path_defined(d)) {
							continue;
						}
						f = get_system_path(d);
						f += '/';
						f += default_soundfont;
					} else {
						f = soundfont;
					}
					if (U7exists(f.c_str())) {
						soundfont = std::move(f);
					}
					soundfonts.emplace_back(soundfont);
				}
			} else {
				soundfonts.emplace_back(default_soundfont);
			}
			perr << "Setting 'fluidsynth_soundfont' missing in 'exult.cfg': "
					"enabling FluidSynth with FluidSynth default SoundFont"
				 << std::endl;
		} else {
			perr << "Setting 'fluidsynth_soundfont' missing in 'exult.cfg' or "
					"file unreadable and "
					"Fluidsynth without a default SoundFont: not enabling "
					"Fluidsynth"
				 << std::endl;
			return -2;
		}
	}

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
	perr << "Compiled with " << FLUID_VERSION
		 << ", using library: " << fluid_version_str() << std::endl;

	int numloaded = 0;
	for (auto& soundfont : soundfonts) {
		const int soundFont = fluid_synth_sfload(_synth, soundfont.c_str(), 1);
		if (soundFont == -1) {
			perr << "Failed loading sound font '" << soundfont << "'"
				 << std::endl;
		} else {
			perr << "Loaded sound font '" << soundfont << "'" << std::endl;
			_soundFont.push(soundFont);
			numloaded++;
		}
	}
	if (numloaded == 0) {
		perr << "Failed to load any custom sound fonts; giving up."
			 << std::endl;
		return -3;
	}

	return 0;
}

void FluidSynthMidiDriver::close() {
	//-- Do not unload the SoundFonts, this causes messages :
	//--   fluidsynth: warning: No preset found on channel # [bank=# prog=#]
	//-- And delete_fluid_synth unloads the SoundFonts anyway.
	while (!_soundFont.empty()) {
		_soundFont.pop();
	}

	delete_fluid_synth(_synth);
	_synth = nullptr;
	delete_fluid_settings(_settings);
	_settings = nullptr;
}

void FluidSynthMidiDriver::send(uint32 b) {
	// uint8 param3 = static_cast<uint8>((b >> 24) & 0xFF);
	const uint32 param2 = static_cast<uint8>((b >> 16) & 0xFF);
	const uint32 param1 = static_cast<uint8>((b >> 8) & 0xFF);
	auto         cmd    = static_cast<uint8>(b & 0xF0);
	auto         chan   = static_cast<uint8>(b & 0x0F);

	switch (cmd) {
	case 0x80:    // Note Off
		fluid_synth_noteoff(_synth, chan, param1);
		break;
	case 0x90:    // Note On
		fluid_synth_noteon(_synth, chan, param1, param2);
		break;
	case 0xA0:    // Aftertouch
		break;
	case 0xB0:    // Control Change
		fluid_synth_cc(_synth, chan, param1, param2);
		break;
	case 0xC0:    // Program Change
		fluid_synth_program_change(_synth, chan, param1);
		break;
	case 0xD0:    // Channel Pressure
		break;
	case 0xE0:    // Pitch Bend
		fluid_synth_pitch_bend(_synth, chan, (param2 << 7) | param1);
		break;
	case 0xF0:    // SysEx
		// We should never get here! SysEx information has to be
		// sent via high-level semantic methods.
		perr << "FluidSynthMidiDriver: Receiving SysEx command on a send() call"
			 << std::endl;
		break;
	default:
		perr << "FluidSynthMidiDriver: Unknown send() command 0x" << std::hex
			 << cmd << std::dec << std::endl;
		break;
	}
}

void FluidSynthMidiDriver::lowLevelProduceSamples(
		sint16* samples, uint32 num_samples) {
	fluid_synth_write_s16(_synth, num_samples, samples, 0, 2, samples, 1, 2);
}

#endif
