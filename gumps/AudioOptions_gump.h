/*
Copyright (C) 2003-2022 The Exult Team

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

#ifndef AUDIOOPTIONS_GUMP_H
#define AUDIOOPTIONS_GUMP_H

#include "Audio.h"
#include "Modal_gump.h"

#include <array>
#include <memory>
#include <string>

class Gump_button;

class AudioOptions_gump : public Modal_gump {
private:
	enum button_ids {
		id_first = 0,
		id_ok    = id_first,
		id_cancel,
		id_help,
		id_mixer,
		id_audio_enabled,
		id_sample_rate,
		id_speaker_type,
		id_music_enabled,
		id_music_looping,
		id_music_digital,
		id_midi_driver,
		id_midi_conv,
		id_midi_effects,
		id_sfx_enabled,
		id_sfx_pack,
		id_sfx_conv = id_sfx_pack,
		id_speech_enabled,
		id_count
	};

	std::array<std::unique_ptr<Gump_button>, id_count> buttons;

	typedef enum _audio_speech_state {
		speech_off,
		speech_on,
		speech_on_with_subtitles
	} audio_speech_state;

	bool               speaker_type;    // only mono and stereo atm
	bool               o_speaker_type;
	std::string        sample_rate_str;
	static uint32      sample_rates[5];
	static int         num_sample_rates;
	int                audio_enabled;
	int                sample_rate;
	int                o_sample_rate;
	int                midi_enabled;
	int                midi_conversion;
	int                midi_ogg_enabled;
	int                midi_driver;
	int                midi_reverb_chorus;
	Audio::LoopingType midi_looping;
	int                sfx_enabled;
	int                sfx_package;
#ifdef ENABLE_MIDISFX
	int sfx_conversion;
#endif
	audio_speech_state speech_option;

	// Auxiliary variables for digital SFX packages:
	int nsfxopts, nsfxpacks;
#ifdef ENABLE_MIDISFX
	int midi_state;
#endif
	bool have_config_pack, have_roland_pack, have_blaster_pack, have_midi_pack,
			have_custom_pack;
	std::string sfx_custompack;
	std::string configpack, rolandpack, blasterpack, midipack;

	bool have_digital_sfx() const {
		return have_roland_pack || have_blaster_pack || have_custom_pack;
	}

public:
	AudioOptions_gump();

	// Paint it and its contents.
	void paint() override;
	void close() override;

	// Handle events:
	bool mouse_down(int mx, int my, int button) override;
	bool mouse_up(int mx, int my, int button) override;

	void toggle(Gump_button* btn, int state);
	void rebuild_buttons();
	void rebuild_midi_buttons();
	void rebuild_midi_driver_buttons();
	void rebuild_mididriveroption_buttons();
	void rebuild_sfx_buttons();

	void load_settings();
	void save_settings();
	void cancel();
	void help();
	void mixer();
	
	void toggle_audio_enabled(int state) {
		audio_enabled = state;
		rebuild_buttons();
	}

	void toggle_sample_rate(int state) {
		sample_rate = state;
	}

	void toggle_speaker_type(int state) {
		speaker_type = state;
	}

	void toggle_music_enabled(int state) {
		midi_enabled = state;
		rebuild_midi_buttons();
	}

	void toggle_music_digital(int state) {
		midi_ogg_enabled = state;
		rebuild_midi_driver_buttons();
		paint();
	}

	void toggle_midi_driver(int state) {
		midi_driver = state;
		rebuild_mididriveroption_buttons();
		paint();
	}

	void change_music_looping(int state) {
		midi_looping = static_cast<Audio::LoopingType>(state);
	}

	void toggle_midi_conv(int state) {
		midi_conversion = state;
	}

	void toggle_midi_effects(int state) {
		midi_reverb_chorus = state;
	}

	void toggle_sfx_enabled(int state) {
		sfx_enabled = state;
		rebuild_sfx_buttons();
		paint();
	}

	void toggle_sfx_pack(int state);

	void toggle_speech_enabled(int state) {
		speech_option = static_cast<audio_speech_state>(state);
	}
};

#endif
