/*
 *  Copyright (C) 2003-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

// Includes Pentagram headers so we must include pent_include.h
#include "pent_include.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "Audio.h"
#include "AudioMixer.h"
#include "AudioOptions_gump.h"
#include "Configuration.h"
#include "Enabled_button.h"
#include "Gump_ToggleButton.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "MidiDriver.h"
#include "Mixer_gump.h"
#include "XMidiFile.h"
#include "exult.h"
#include "exult_constants.h"
#include "exult_flx.h"
#include "font.h"
#include "game.h"
#include "gamewin.h"
#include "mouse.h"

#include <iostream>
#include <sstream>

using namespace Pentagram;

static const int rowy[]
		= {5, 17, 29, 41, 56, 68, 80, 92, 104, 116, 131, 143, 158, 173};
static const int colx[] = {35, 50, 134};

static const char* oktext     = "OK";
static const char* canceltext = "CANCEL";
static const char* helptext   = "HELP";
static const char* mixertext  = "VOLUME MIXER";

uint32 AudioOptions_gump::sample_rates[5]  = {11025, 22050, 44100, 48000, 0};
int    AudioOptions_gump::num_sample_rates = 0;

using AudioOptions_button = CallbackTextButton<AudioOptions_gump>;
using AudioTextToggle     = CallbackToggleTextButton<AudioOptions_gump>;
using AudioEnabledToggle  = CallbackEnabledButton<AudioOptions_gump>;

void AudioOptions_gump::mixer() {
	auto* vol_mix = new Mixer_gump();
	gumpman->do_modal_gump(vol_mix, Mouse::hand);
	delete vol_mix;
}

void AudioOptions_gump::close() {
	save_settings();
	done = true;
}

void AudioOptions_gump::cancel() {
	done = true;
}

void AudioOptions_gump::help() {
	SDL_OpenURL("https://exult.info/docs.php#audio_gump");
}

void AudioOptions_gump::toggle_sfx_pack(int state) {
	if (have_digital_sfx() && sfx_enabled == 1) {
		sfx_package = state;
#ifdef ENABLE_MIDISFX
	} else if (sfx_enabled && have_midi_pack) {
		if (state == 1) {
			sfx_conversion = XMIDIFILE_CONVERT_GS127_TO_GS;
		} else {
			sfx_conversion = XMIDIFILE_CONVERT_NOCONVERSION;
		}
#endif
	}
}

static void strip_path(std::string& file) {
	if (file.empty()) {
		return;
	}
	size_t sep = file.rfind('/');
	if (sep != std::string::npos) {
		sep++;
		file = file.substr(sep);
	}
}

void AudioOptions_gump::rebuild_buttons() {
	// skip ok, cancel, and audio enabled settings
	for (int i = id_sample_rate; i < id_count; i++) {
		buttons[i].reset();
	}

	if (!audio_enabled) {
		return;
	}

	std::vector<std::string> sampleRates = {"11025", "22050", "44100", "48000"};
	if (std::find(sampleRates.cbegin(), sampleRates.cend(), sample_rate_str)
		!= sampleRates.cend()) {
		sampleRates.push_back(sample_rate_str);
	}

	buttons[id_sample_rate] = std::make_unique<AudioTextToggle>(
			this, &AudioOptions_gump::toggle_sample_rate,
			std::move(sampleRates), sample_rate, colx[2], rowy[2], 59);

	std::vector<std::string> speaker_types = {"Mono", "Stereo"};
	buttons[id_speaker_type]               = std::make_unique<AudioTextToggle>(
            this, &AudioOptions_gump::toggle_speaker_type,
            std::move(speaker_types), speaker_type, colx[2], rowy[3], 59);

	// music on/off
	buttons[id_music_enabled] = std::make_unique<AudioEnabledToggle>(
			this, &AudioOptions_gump::toggle_music_enabled, midi_enabled,
			colx[2], rowy[4], 59);
	if (midi_enabled) {
		rebuild_midi_buttons();
	}

	// sfx on/off
	std::vector<std::string> sfx_options = {"Disabled"};
	if (have_digital_sfx()) {
		sfx_options.emplace_back("Digital");
	}
#ifdef ENABLE_MIDISFX
	if (have_midi_pack) {
		midi_state = sfx_options.size();
		sfx_options.emplace_back("Midi");
	} else {
		midi_state = -1;
	}
#endif

	buttons[id_sfx_enabled] = std::make_unique<AudioTextToggle>(
			this, &AudioOptions_gump::toggle_sfx_enabled,
			std::move(sfx_options), sfx_enabled, colx[2], rowy[10], 59);
	if (sfx_enabled) {
		rebuild_sfx_buttons();
	}

	// speech on/off
	std::vector<std::string> speech_options
			= {"Subtitles only", "Voice only", "Voice + Subtitles"};
	buttons[id_speech_enabled] = std::make_unique<AudioTextToggle>(
			this, &AudioOptions_gump::toggle_speech_enabled,
			std::move(speech_options), speech_option, colx[2] - 49, rowy[12],
			108);
}

void AudioOptions_gump::rebuild_midi_buttons() {
	for (int i = id_music_looping; i < id_sfx_enabled; i++) {
		buttons[i].reset();
	}

	if (!midi_enabled) {
		return;
	}

	// ogg enabled/disabled
	buttons[id_music_digital] = std::make_unique<AudioEnabledToggle>(
			this, &AudioOptions_gump::toggle_music_digital, midi_ogg_enabled,
			colx[2], rowy[6], 59);

	// looping type
	std::vector<std::string> looping_options
			= {"Never", "Limited", "Auto", "Endless"};
	buttons[id_music_looping] = std::make_unique<AudioTextToggle>(
			this, &AudioOptions_gump::change_music_looping,
			std::move(looping_options), static_cast<int>(midi_looping), colx[2],
			rowy[4], 59);
	rebuild_midi_driver_buttons();
}

void AudioOptions_gump::rebuild_midi_driver_buttons() {
	for (int i = id_midi_driver; i < id_sfx_enabled; i++) {
		buttons[i].reset();
	}

	if (midi_ogg_enabled) {
		return;
	}

	const unsigned int       num_midi_drivers = MidiDriver::getDriverCount();
	std::vector<std::string> midi_drivertext;
	for (unsigned int i = 0; i < num_midi_drivers; i++) {
		midi_drivertext.emplace_back(MidiDriver::getDriverName(i));
	}
	midi_drivertext.emplace_back("Default");

	// midi driver
	buttons[id_midi_driver] = std::make_unique<AudioTextToggle>(
			this, &AudioOptions_gump::toggle_midi_driver,
			std::move(midi_drivertext), midi_driver, colx[2] - 15, rowy[7], 74);

	rebuild_mididriveroption_buttons();
}

void AudioOptions_gump::rebuild_sfx_buttons() {
	buttons[id_sfx_pack].reset();

	if (!sfx_enabled) {
		return;
	} else if (
			sfx_enabled == 1 && have_digital_sfx()
			&& !gwin->is_in_exult_menu()) {
		std::vector<std::string> sfx_digitalpacks;
		if (have_roland_pack) {
			sfx_digitalpacks.emplace_back("Roland MT-32");
		}
		if (have_blaster_pack) {
			sfx_digitalpacks.emplace_back("Sound Blaster");
		}
		if (have_custom_pack) {
			sfx_digitalpacks.emplace_back("Custom");
		}
		buttons[id_sfx_pack] = std::make_unique<AudioTextToggle>(
				this, &AudioOptions_gump::toggle_sfx_pack,
				std::move(sfx_digitalpacks), sfx_package, colx[2] - 33,
				rowy[11], 92);
	}
#ifdef ENABLE_MIDISFX
	else if (sfx_enabled == midi_state) {
		std::vector<std::string> sfx_conversiontext = {"None", "GS"};

		// sfx conversion
		buttons[id_sfx_pack] = std::make_unique<AudioTextToggle>(
				this, &AudioOptions_gump::toggle_sfx_pack,
				std::move(sfx_conversiontext), sfx_conversion == 5 ? 1 : 0,
				colx[2], rowy[11], 59);
	}
#endif
}

void AudioOptions_gump::rebuild_mididriveroption_buttons() {
	std::string s = "Default";
	if (midi_driver != MidiDriver::getDriverCount()) {
		s = MidiDriver::getDriverName(midi_driver);
	}

	if (s != "FMOpl" && s != "MT32Emu" && s != "Disabled") {
#ifdef MACOSX
		int string_size = 5;
		if (s == "Default" || s == "CoreAudio") {
			if (midi_conversion > 3) {
				midi_conversion = 0;
			}
			string_size = 4;
		}
#else
		const int string_size = 5;
#endif
		std::vector<std::string> midi_conversiontext
				= {"Fake MT32", "GM", "GS", "GS127"};
		if (string_size == 5) {
			midi_conversiontext.emplace_back("MT32");
		}

		// midi conversion
		buttons[id_midi_conv] = std::make_unique<AudioTextToggle>(
				this, &AudioOptions_gump::toggle_midi_conv,
				std::move(midi_conversiontext), midi_conversion, colx[2] - 7,
				rowy[8], 66);
	} else {
		buttons[id_midi_conv].reset();
	}

	if (s != "FMOpl" && s != "Disabled") {
		std::vector<std::string> midi_reverbchorustext
				= {"Disabled", "Reverb", "Chorus", "Both"};

		// reverb/chorus combo
		buttons[id_midi_effects] = std::make_unique<AudioTextToggle>(
				this, &AudioOptions_gump::toggle_midi_effects,
				std::move(midi_reverbchorustext), midi_reverb_chorus, colx[2],
				rowy[9], 59);
	} else {
		buttons[id_midi_effects].reset();
	}
}

void AudioOptions_gump::load_settings() {
	std::string s;
	audio_enabled     = (Audio::get_ptr()->is_audio_enabled() ? 1 : 0);
	midi_enabled      = (Audio::get_ptr()->is_music_enabled() ? 1 : 0);
	const bool sfx_on = (Audio::get_ptr()->are_effects_enabled());
	speech_option
			= (Audio::get_ptr()->is_speech_enabled()
					   ? (Audio::get_ptr()->is_speech_with_subs()
								  ? speech_on_with_subtitles
								  : speech_on)
					   : speech_off);
	midi_looping = Audio::get_ptr()->get_music_looping();
	speaker_type = true;    // stereo
	sample_rate  = 44100;
	config->value("config/audio/stereo", speaker_type, speaker_type);
	config->value("config/audio/sample_rate", sample_rate, sample_rate);
	num_sample_rates = 4;
	o_sample_rate    = sample_rate;
	o_speaker_type   = speaker_type;
	if (sample_rate == 11025) {
		sample_rate = 0;
	} else if (sample_rate == 22050) {
		sample_rate = 1;
	} else if (sample_rate == 44100) {
		sample_rate = 2;
	} else if (sample_rate == 48000) {
		sample_rate = 3;
	} else {
		sample_rates[4] = sample_rate;
		std::ostringstream strStream;
		strStream << sample_rate;
		sample_rate_str  = strStream.str();
		sample_rate      = 4;
		num_sample_rates = 5;
	}

	MyMidiPlayer* midi = Audio::get_ptr()->get_midi();
	if (midi) {
		midi_conversion  = midi->get_music_conversion();
		midi_ogg_enabled = midi->get_ogg_enabled();

		s = midi->get_midi_driver();
		for (midi_driver = 0; midi_driver < MidiDriver::getDriverCount();
			 midi_driver++) {
			const std::string name = MidiDriver::getDriverName(midi_driver);
			if (!Pentagram::strcasecmp(name.c_str(), s.c_str())) {
				break;
			}
		}

#ifdef ENABLE_MIDISFX
		sfx_conversion = midi->get_effects_conversion();
#endif
	} else {
		// String for default value for driver type
		std::string driver_default = "default";

		config->value("config/audio/midi/convert", s, "gm");
		if (s == "gs") {
			midi_conversion = XMIDIFILE_CONVERT_MT32_TO_GS;
		} else if (s == "none" || s == "mt32") {
			midi_conversion = XMIDIFILE_CONVERT_NOCONVERSION;
		} else if (s == "gs127") {
			midi_conversion = XMIDIFILE_CONVERT_MT32_TO_GS127;
		} else if (s == "gs127drum") {
			midi_conversion = XMIDIFILE_CONVERT_MT32_TO_GS;
		} else if (s == "fakemt32") {
			midi_conversion = XMIDIFILE_CONVERT_GM_TO_MT32;
		} else {
			midi_conversion = XMIDIFILE_CONVERT_MT32_TO_GM;
			config->set("config/audio/midi/convert", "gm", true);

			driver_default = "s";
		}

		// OGG Vorbis support
		config->value("config/audio/midi/use_oggs", s, "no");
		midi_ogg_enabled = (s == "yes" ? 1 : 0);

		config->value("config/audio/midi/driver", s, driver_default.c_str());

		if (s == "digital") {
			midi_ogg_enabled = true;
			config->set("config/audio/midi/driver", "default", true);
			config->set("config/audio/midi/use_oggs", "yes", true);
			midi_driver = MidiDriver::getDriverCount();
		} else {
			for (midi_driver = 0; midi_driver < MidiDriver::getDriverCount();
				 midi_driver++) {
				const std::string name = MidiDriver::getDriverName(midi_driver);
				if (!Pentagram::strcasecmp(name.c_str(), s.c_str())) {
					break;
				}
			}
		}

#ifdef ENABLE_MIDISFX
		config->value("config/audio/effects/convert", s, "gs");
		if (s == "none" || s == "mt32") {
			sfx_conversion = XMIDIFILE_CONVERT_NOCONVERSION;
		} else if (s == "gs127") {
			sfx_conversion = XMIDIFILE_CONVERT_NOCONVERSION;
		} else {
			sfx_conversion = XMIDIFILE_CONVERT_GS127_TO_GS;
		}
#endif
	}

	config->value("config/audio/midi/reverb/enabled", s, "no");
	midi_reverb_chorus = (s == "yes" ? 1 : 0);

	config->value("config/audio/midi/chorus/enabled", s, "no");
	midi_reverb_chorus |= (s == "yes" ? 2 : 0);

	const std::string d
			= "config/disk/game/" + Game::get_gametitle() + "/waves";
	config->value(d.c_str(), s, "---");
	if (have_roland_pack && s == rolandpack) {
		sfx_package = 0;
	} else if (have_blaster_pack && s == blasterpack) {
		sfx_package = have_roland_pack ? 1 : 0;
	} else if (have_custom_pack) {
		sfx_package = nsfxpacks - 1;
	} else {    // This should *never* happen.
		sfx_package = 0;
	}
	if (!sfx_on) {
		sfx_enabled = 0;
	} else {
#ifdef ENABLE_MIDISFX
		config->value("config/audio/effects/midi", s, "no");
#else
		s = "no";
#endif
		if (s == "yes" && have_midi_pack) {
			sfx_enabled = 1 + have_digital_sfx();
		} else if (have_digital_sfx()) {
			sfx_enabled = 1;
		} else {
			// Actually disable sfx if no sfx packs and no midi sfx.
			// This is just in case -- it should not be needed.
			sfx_enabled = 0;
		}
	}
}

AudioOptions_gump::AudioOptions_gump()
		: Modal_gump(nullptr, EXULT_FLX_AUDIOOPTIONS_SHP, SF_EXULT_FLX) {
	set_object_area(TileRect(0, 0, 0, 0), 8, 187);    //++++++ ???
	const Exult_Game  game  = Game::get_game_type();
	const std::string title = Game::get_gametitle();
	have_config_pack        = Audio::have_config_sfx(title, &configpack);
	have_roland_pack        = Audio::have_roland_sfx(game, &rolandpack);
	have_blaster_pack       = Audio::have_sblaster_sfx(game, &blasterpack);
	have_midi_pack          = Audio::have_midi_sfx(&midipack);
	strip_path(configpack);
	strip_path(rolandpack);
	strip_path(blasterpack);
	strip_path(midipack);
	have_custom_pack = have_config_pack && configpack != rolandpack
					   && configpack != blasterpack;

	nsfxopts  = 1;    // For "Disabled".
	nsfxpacks = 0;
	if (have_digital_sfx()) {
		// Have digital sfx.
		nsfxopts++;
		nsfxpacks += static_cast<int>(have_roland_pack)
					 + static_cast<int>(have_blaster_pack)
					 + static_cast<int>(have_custom_pack);
		if (have_custom_pack) {
			sfx_custompack = configpack;
		}
	}
	if (have_midi_pack) {    // Midi SFX.
		nsfxopts++;
	}

	load_settings();

	rebuild_buttons();

	// Volume Mixer
	buttons[id_mixer] = std::make_unique<AudioOptions_button>(
			this, &AudioOptions_gump::mixer, mixertext, colx[1] + 15, rowy[0],
			95);
	// audio on/off
	buttons[id_audio_enabled] = std::make_unique<AudioEnabledToggle>(
			this, &AudioOptions_gump::toggle_audio_enabled, audio_enabled,
			colx[2], rowy[1], 59);
	// Ok
	buttons[id_ok] = std::make_unique<AudioOptions_button>(
			this, &AudioOptions_gump::close, oktext, colx[0] - 2, rowy[13], 50);
	// Cancel
	buttons[id_cancel] = std::make_unique<AudioOptions_button>(
			this, &AudioOptions_gump::cancel, canceltext, colx[2] + 9, rowy[13],
			50);
	// Help
	buttons[id_help] = std::make_unique<AudioOptions_button>(
			this, &AudioOptions_gump::help, helptext, colx[2] - 46, rowy[13],
			50);
}

void AudioOptions_gump::save_settings() {
	int           track_playing = -1;
	bool          looping       = false;
	MyMidiPlayer* midi          = Audio::get_ptr()->get_midi();
	if (midi) {
		track_playing = midi->get_current_track();
		looping       = midi->is_repeating();
	}
	config->set("config/audio/sample_rate", sample_rates[sample_rate], false);
	config->set("config/audio/stereo", speaker_type ? "yes" : "no", false);
	if (sample_rates[sample_rate] != static_cast<uint32>(o_sample_rate)
		|| speaker_type != o_speaker_type) {
		Audio::Destroy();
		Audio::Init();
		midi = Audio::get_ptr()->get_midi();    // old pointer got deleted
	}
	Audio::get_ptr()->set_audio_enabled(audio_enabled == 1);
	Audio::get_ptr()->set_music_enabled(midi_enabled == 1);
	if (!midi_enabled) {    // Stop what's playing.
		Audio::get_ptr()->stop_music();
	}
	Audio::get_ptr()->set_effects_enabled(sfx_enabled != 0);
	if (!sfx_enabled) {    // Stop what's playing.
		Audio::get_ptr()->stop_sound_effects();
	}
	Audio::get_ptr()->set_speech_enabled(speech_option != speech_off);
	Audio::get_ptr()->set_speech_with_subs(
			speech_option == speech_on_with_subtitles);
	Audio::get_ptr()->set_music_looping(midi_looping);

	config->set("config/audio/enabled", audio_enabled ? "yes" : "no", false);
	config->set(
			"config/audio/midi/enabled", midi_enabled ? "yes" : "no", false);
	config->set(
			"config/audio/effects/enabled", sfx_enabled ? "yes" : "no", false);
	config->set(
			"config/audio/speech/enabled",
			(speech_option != speech_off) ? "yes" : "no", false);
	config->set(
			"config/audio/speech/with_subs",
			(speech_option == speech_on_with_subtitles) ? "yes" : "no", false);
	config->set(
			"config/audio/midi/chorus/enabled",
			(midi_reverb_chorus & 2) ? "yes" : "no", false);
	config->set(
			"config/audio/midi/reverb/enabled",
			(midi_reverb_chorus & 1) ? "yes" : "no", false);

	const char* midi_looping_values[] = {"never", "limited", "auto", "endless"};
	config->set(
			"config/audio/midi/looping",
			midi_looping_values[static_cast<int>(midi_looping)], false);

	const std::string d
			= "config/disk/game/" + Game::get_gametitle() + "/waves";
	std::string waves;
	if (!gwin->is_in_exult_menu()) {
		int i = 0;
		if (have_roland_pack && sfx_package == i++) {
			waves = rolandpack;
		} else if (have_blaster_pack && sfx_package == i++) {
			waves = blasterpack;
		} else if (have_custom_pack && sfx_package == i++) {
			waves = sfx_custompack;
		}
		if (waves != sfx_custompack) {
			config->set(d.c_str(), waves, false);
		}
	}
#ifdef ENABLE_MIDISFX
	config->set(
			"config/audio/effects/midi",
			sfx_enabled == 1 + have_digital_sfx() ? "yes" : "no", false);
#endif
	if (midi) {
		std::string s = "default";
		if (midi_driver != MidiDriver::getDriverCount()) {
			s = MidiDriver::getDriverName(midi_driver);
		}
		midi->set_midi_driver(s, midi_ogg_enabled != 0);
		midi->set_music_conversion(midi_conversion);
#ifdef ENABLE_MIDISFX
		midi->set_effects_conversion(sfx_conversion);
#endif
	} else {
		switch (midi_conversion) {
		case XMIDIFILE_CONVERT_MT32_TO_GS:
			config->set("config/audio/midi/convert", "gs", false);
			break;
		case XMIDIFILE_CONVERT_NOCONVERSION:
			config->set("config/audio/midi/convert", "mt32", false);
			break;
		case XMIDIFILE_CONVERT_MT32_TO_GS127:
			config->set("config/audio/midi/convert", "gs127", false);
			break;
		case XMIDIFILE_CONVERT_GM_TO_MT32:
			config->set("config/audio/midi/convert", "fakemt32", false);
			break;
		default:
			config->set("config/audio/midi/convert", "gm", false);
			break;
		}

		if (midi_driver == MidiDriver::getDriverCount()) {
			config->set("config/audio/midi/driver", "default", false);
		} else {
			config->set(
					"config/audio/midi/driver",
					MidiDriver::getDriverName(midi_driver), false);
		}

#ifdef ENABLE_MIDISFX
		switch (sfx_conversion) {
		case XMIDIFILE_CONVERT_NOCONVERSION:
			config->set("config/audio/effects/convert", "mt32", false);
			break;
		default:
			config->set("config/audio/effects/convert", "gs", false);
			break;
		}
#endif
	}
	config->write_back();
	Audio::get_ptr()->Init_sfx();
	// restart music track if one was playing and isn't anymore
	if (midi && Audio::get_ptr()->is_music_enabled()
		&& midi->get_current_track() != track_playing
		&& (!gwin->is_background_track(track_playing) || midi->get_ogg_enabled()
			|| midi->is_mt32())) {
		if (gwin->is_in_exult_menu()) {
			Audio::get_ptr()->start_music(
					EXULT_FLX_MEDITOWN_MID, true, EXULT_FLX);
		} else {
			Audio::get_ptr()->start_music(track_playing, looping);
		}
	}
}

void AudioOptions_gump::paint() {
	Gump::paint();
	for (auto& btn : buttons) {
		if (btn != nullptr) {
			btn->paint();
		}
	}

	Font*          font = fontManager.get_font("SMALL_BLACK_FONT");
	Image_window8* iwin = gwin->get_win();

	font->paint_text(iwin->get_ib8(), "Audio:", x + colx[0], y + rowy[1] + 1);
	if (audio_enabled) {
		font->paint_text(
				iwin->get_ib8(), "sample rate", x + colx[1], y + rowy[2] + 1);
		font->paint_text(
				iwin->get_ib8(), "speaker type", x + colx[1], y + rowy[3] + 1);
		font->paint_text(
				iwin->get_ib8(), "Music:", x + colx[0], y + rowy[4] + 1);
		if (midi_enabled) {
			font->paint_text(
					iwin->get_ib8(), "looping", x + colx[1], y + rowy[5] + 1);
			font->paint_text(
					iwin->get_ib8(), "digital music", x + colx[1],
					y + rowy[6] + 1);
			if (!midi_ogg_enabled) {
				font->paint_text(
						iwin->get_ib8(), "midi driver", x + colx[1],
						y + rowy[7] + 1);
				if (buttons[id_midi_conv] != nullptr) {
					font->paint_text(
							iwin->get_ib8(), "device type", x + colx[1],
							y + rowy[8] + 1);
				}
				if (buttons[id_midi_effects] != nullptr) {
					font->paint_text(
							iwin->get_ib8(), "effects", x + colx[1],
							y + rowy[9] + 1);
				}
			}
		}
		font->paint_text(
				iwin->get_ib8(), "SFX:", x + colx[0], y + rowy[10] + 1);
		if (sfx_enabled == 1 && have_digital_sfx()
			&& !gwin->is_in_exult_menu()) {
			font->paint_text(
					iwin->get_ib8(), "pack", x + colx[1], y + rowy[11] + 1);
		}
#ifdef ENABLE_MIDISFX
		else if (sfx_enabled == midi_state) {
			font->paint_text(
					iwin->get_ib8(), "conversion", x + colx[1],
					y + rowy[11] + 1);
		}
#endif
		font->paint_text(
				iwin->get_ib8(), "Speech:", x + colx[0], y + rowy[12] + 1);
	}
	gwin->set_painted();
}

bool AudioOptions_gump::mouse_down(int mx, int my, MouseButton button) {
	// Only left and right buttons
	if (button != MouseButton::Left && button != MouseButton::Right) {
		return Modal_gump::mouse_down(mx, my, button);
	}

	// We'll eat the mouse down if we've already got a button down
	if (pushed) {
		return true;
	}

	// First try checkmark
	pushed = Gump::on_button(mx, my);

	// Try buttons at bottom.
	if (!pushed) {
		for (auto& btn : buttons) {
			if (btn != nullptr && btn->on_button(mx, my)) {
				pushed = btn.get();
				break;
			}
		}
	}

	if (pushed && !pushed->push(button)) {    // On a button?
		pushed = nullptr;
	}

	return pushed != nullptr || Modal_gump::mouse_down(mx, my, button);
}

bool AudioOptions_gump::mouse_up(int mx, int my, MouseButton button) {
	// Not Pushing a button?
	if (!pushed) {
		return Modal_gump::mouse_up(mx, my, button);
	}

	if (pushed->get_pushed() != button) {
		return button == MouseButton::Left;
	}

	bool res = false;
	pushed->unpush(button);
	if (pushed->on_button(mx, my)) {
		res = pushed->activate(button);
	}
	pushed = nullptr;
	return res;
}
