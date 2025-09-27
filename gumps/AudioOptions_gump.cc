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
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "AdvancedOptions_gump.h"
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
#include "Yesno_gump.h"
#include "exult.h"
#include "exult_constants.h"
#include "exult_flx.h"
#include "font.h"
#include "game.h"
#include "gamewin.h"
#include "items.h"
#include "mouse.h"

#include <iostream>
#include <sstream>

using namespace Pentagram;


class Strings : public GumpStrings {
public:
	static auto VOLUMEMIXER() {
		return get_text_msg(0x600 - msg_file_start);
	}

	static auto SETTINGS() {
		return get_text_msg(0x601 - msg_file_start);
	}

	static auto Advancedsettingsfor() {
		return get_text_msg(0x602 - msg_file_start);
	}

	static auto mididriver_() {
		return get_text_msg(0x603 - msg_file_start);
	}

	static auto Mono() {
		return get_text_msg(0x604 - msg_file_start);
	}

	static auto Stereo() {
		return get_text_msg(0x605 - msg_file_start);
	}

	static auto Digital() {
		return get_text_msg(0x606 - msg_file_start);
	}

	static auto Subtitlesonly() {
		return get_text_msg(0x607 - msg_file_start);
	}

	static auto Voiceonly() {
		return get_text_msg(0x608 - msg_file_start);
	}

	static auto VoiceSubtitles() {
		return get_text_msg(0x609 - msg_file_start);
	}

	static auto Never() {
		return get_text_msg(0x60A - msg_file_start);
	}

	static auto Limited() {
		return get_text_msg(0x60B - msg_file_start);
	}

	static auto Auto() {
		return get_text_msg(0x60C - msg_file_start);
	}

	static auto Endless() {
		return get_text_msg(0x60D - msg_file_start);
	}

	static auto RolandMT32() {
		return get_text_msg(0x60E - msg_file_start);
	}

	static auto SoundBlaster() {
		return get_text_msg(0x60F - msg_file_start);
	}

	static auto Custom() {
		return get_text_msg(0x610 - msg_file_start);
	}

	static auto Themod() {
		return get_text_msg(0x611 - msg_file_start);
	}

	static auto requiresDigitalMusic_() {
		return get_text_msg(0x612 - msg_file_start);
	}

	static auto Disableanyway_() {
		return get_text_msg(0x613 - msg_file_start);
	}

	static auto Audio_() {
		return get_text_msg(0x614 - msg_file_start);
	}

	static auto samplerate() {
		return get_text_msg(0x615 - msg_file_start);
	}

	static auto speakertype() {
		return get_text_msg(0x616 - msg_file_start);
	}

	static auto Music_() {
		return get_text_msg(0x617 - msg_file_start);
	}

	static auto looping() {
		return get_text_msg(0x618 - msg_file_start);
	}

	static auto digitalmusic() {
		return get_text_msg(0x619 - msg_file_start);
	}

	static auto mididriver() {
		return get_text_msg(0x61A - msg_file_start);
	}

	static auto SFX_() {
		return get_text_msg(0x61B - msg_file_start);
	}

	static auto pack() {
		return get_text_msg(0x61C - msg_file_start);
	}

	static auto Speech_() {
		return get_text_msg(0x61D - msg_file_start);
	}

	static auto conversion() {
		return get_text_msg(0x61E - msg_file_start);
	}
};

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

void AudioOptions_gump::advanced() {
	// this shouldnever happen if we got here

	std::string midi_driver_name = "default";
	if (midi_driver != MidiDriver::getDriverCount()) {
		midi_driver_name = MidiDriver::getDriverName(midi_driver);
	}
	AdvancedOptions_gump aog(
			&advancedsettings,
			Strings::Advancedsettingsfor() + ("\n" + midi_driver_name + " ")
					+ Strings::mididriver_(),
			"https://exult.info/docs.html#advanced_midi_gump",
			[midi_driver_name] {
				MyMidiPlayer* midi = Audio::get_ptr()->get_midi();
				if (!midi || !Audio::get_ptr()->is_music_enabled()) {
					return;
				}
				// Only reinit midi driver if the diver we changed settings for
				// is the courrenyly used driver
				if (midi->get_midi_driver() != midi_driver_name) {
					return;
				}
				auto track_playing = midi->get_current_track();
				auto looping       = midi->is_repeating();
				midi->destroyMidiDriver();
				midi->can_play_midi();
				if (!gwin->is_background_track(track_playing)
					|| midi->get_ogg_enabled() || midi->is_mt32()
					|| gwin->is_in_exult_menu()) {
					if (gwin->is_in_exult_menu()) {
						Audio::get_ptr()->start_music(
								EXULT_FLX_MEDITOWN_MID, true,
								MyMidiPlayer::Force_None, EXULT_FLX);
					} else {
						Audio::get_ptr()->start_music(track_playing, looping);
					}
				}
			});

	gumpman->do_modal_gump(&aog, Mouse::hand);
}

void AudioOptions_gump::close() {
	//	save_settings();
	done = true;
}

void AudioOptions_gump::cancel() {
	done = true;
}

void AudioOptions_gump::help() {
	SDL_OpenURL("https://exult.info/docs.html#audio_gump");
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
	if (!sample_rate_str.empty()
		&& std::find(sampleRates.cbegin(), sampleRates.cend(), sample_rate_str)
				   == sampleRates.cend()) {
		sampleRates.push_back(sample_rate_str);
	}

	buttons[id_sample_rate] = std::make_unique<AudioTextToggle>(
			this, &AudioOptions_gump::toggle_sample_rate,
			std::move(sampleRates), sample_rate,
			get_button_pos_for_label(Strings::samplerate()), yForRow(2),
			59);

	std::vector<std::string> speaker_types
			= {Strings::Mono(), Strings::Stereo()};
	buttons[id_speaker_type] = std::make_unique<AudioTextToggle>(
			this, &AudioOptions_gump::toggle_speaker_type,
			std::move(speaker_types), speaker_type,
			get_button_pos_for_label(Strings::speakertype()),
			yForRow(3), 59);

	// music on/off
	buttons[id_music_enabled] = std::make_unique<AudioEnabledToggle>(
			this, &AudioOptions_gump::toggle_music_enabled, midi_enabled,
			get_button_pos_for_label(Strings::Music_()), yForRow(4), 59);
	if (midi_enabled) {
		rebuild_midi_buttons();
	}

	// sfx on/off
	std::vector<std::string> sfx_options = {Strings::Disabled()};
	if (have_digital_sfx()) {
		sfx_options.emplace_back(Strings::Digital());
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
			std::move(sfx_options), sfx_enabled,
			get_button_pos_for_label(Strings::SFX_()), yForRow(10), 59);
	if (sfx_enabled) {
		rebuild_sfx_buttons();
	}

	// speech on/off
	std::vector<std::string> speech_options
			= {Strings::Subtitlesonly(), Strings::Voiceonly(),
			   Strings::VoiceSubtitles()};
	buttons[id_speech_enabled] = std::make_unique<AudioTextToggle>(
			this, &AudioOptions_gump::toggle_speech_enabled,
			std::move(speech_options), speech_option,
			get_button_pos_for_label(Strings::Speech_()), yForRow(12),
			108);
	do_arrange();
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
			get_button_pos_for_label(Strings::digitalmusic()), yForRow(6), 59);

	// looping type
	std::vector<std::string> looping_options
			= {Strings::Never(), Strings::Limited(), Strings::Auto(),
			   Strings::Endless()};
	buttons[id_music_looping] = std::make_unique<AudioTextToggle>(
			this, &AudioOptions_gump::change_music_looping,
			std::move(looping_options), static_cast<int>(midi_looping),
			get_button_pos_for_label(Strings::looping()),
			yForRow(5), 59);
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
	midi_drivertext.emplace_back(Strings::Default());

	// midi driver
	buttons[id_midi_driver] = std::make_unique<AudioTextToggle>(
			this, &AudioOptions_gump::toggle_midi_driver,
			std::move(midi_drivertext), midi_driver, get_button_pos_for_label(Strings::mididriver()), yForRow(7), 74);

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
			sfx_digitalpacks.emplace_back(Strings::RolandMT32());
		}
		if (have_blaster_pack) {
			sfx_digitalpacks.emplace_back(Strings::SoundBlaster());
		}
		if (have_custom_pack) {
			sfx_digitalpacks.emplace_back(Strings::Custom());
		}
		buttons[id_sfx_pack] = std::make_unique<AudioTextToggle>(
				this, &AudioOptions_gump::toggle_sfx_pack,
				std::move(sfx_digitalpacks), sfx_package,
				get_button_pos_for_label(Strings::pack()),
				yForRow(11), 92);
	}
#ifdef ENABLE_MIDISFX
	else if (sfx_enabled == midi_state) {
		std::vector<std::string> sfx_conversiontext = {"None", "GS"};

		// sfx conversion
		buttons[id_sfx_pack] = std::make_unique<AudioTextToggle>(
				this, &AudioOptions_gump::toggle_sfx_pack,
				std::move(sfx_conversiontext), sfx_conversion == 5 ? 1 : 0,
				get_button_pos_for_label(Strings::conversion()), yForRow(11), 59);
	}
#endif
	do_arrange();
}

void AudioOptions_gump::do_arrange() {
	// Risize to fit all
	ResizeWidthToFitWidgets(
			tcb::span(buttons.data() + id_first, id_count));

	HorizontalArrangeWidgets(
			tcb::span(buttons.data() + id_apply, 3));

	// Centre mixer button
	buttons[id_mixer]->set_pos(
			(procedural_background.w - buttons[id_mixer]->get_rect().w) / 2,
			buttons[id_mixer]->get_y());

	// Right align other setting buttons
	RightAlignWidgets(tcb::span(
			buttons.data() + id_first_setting, id_count - id_first_setting));
}

void AudioOptions_gump::rebuild_mididriveroption_buttons() {
	std::string s = Strings::Default();
	if (midi_driver != MidiDriver::getDriverCount()) {
		s = MidiDriver::getDriverName(midi_driver);
	}

	advancedsettings = MidiDriver::get_midi_driver_settings(
			MidiDriver::getDriverName(midi_driver));

	// put advanced setting button on now empty row 13
	if (!advancedsettings.empty()) {
		buttons[id_advanced] = std::make_unique<AudioOptions_button>(
				this, &AudioOptions_gump::advanced, Strings::SETTINGS(),
				100, yForRow(8), 50);
	} else {
		buttons[id_advanced].reset();
	}
	do_arrange();

	// force repaint of screen
	gwin->set_all_dirty();
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
		: Modal_gump(nullptr, -1) {
	const int bottomrow_gap = 0;
	SetProceduralBackground(
			TileRect(0, 0, 100, yForRow(14) + 2 * bottomrow_gap), -1);

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

	// Volume Mixer
	buttons[id_mixer] = std::make_unique<AudioOptions_button>(
			this, &AudioOptions_gump::mixer, Strings::VOLUMEMIXER(),
			50, yForRow(0), 95);
	// audio on/off
	buttons[id_audio_enabled] = std::make_unique<AudioEnabledToggle>(
			this, &AudioOptions_gump::toggle_audio_enabled, audio_enabled,
			procedural_background.w-59, yForRow(1), 59);
	// Apply
	buttons[id_apply] = std::make_unique<AudioOptions_button>(
			this, &AudioOptions_gump::save_settings, Strings::APPLY(), 25,
			yForRow(13) + bottomrow_gap, 50);
	// Help
	buttons[id_help] = std::make_unique<AudioOptions_button>(
			this, &AudioOptions_gump::help, Strings::HELP(), 50,
			yForRow(13) + bottomrow_gap, 50);
	// Cancel
	buttons[id_cancel] = std::make_unique<AudioOptions_button>(
			this, &AudioOptions_gump::cancel, Strings::CANCEL(), 100,
			yForRow(13) + bottomrow_gap, 50);

	rebuild_buttons();

	// always recentre here
	set_pos();
}

void AudioOptions_gump::save_settings() {
	int           track_playing = -1;
	bool          looping       = false;
	MyMidiPlayer* midi          = Audio::get_ptr()->get_midi();
	if (midi) {
		track_playing = midi->get_current_track();
		looping       = midi->is_repeating();
	}

	// Check if mod forces digital music but user is trying to disable it
	if (midi_ogg_enabled == 0) {
		if (!Game::get_modtitle().empty()) {
			std::string mod_cfg_path = get_system_path("<MODS>") + "/"
									   + Game::get_modtitle() + ".cfg";
			if (U7exists(mod_cfg_path)) {
				Configuration modconfig(mod_cfg_path, "modinfo");
				std::string   force_digital_music_str;
				bool          has_force_digital_music
						= modconfig.key_exists("mod_info/force_digital_music");
				if (has_force_digital_music) {
					modconfig.value(
							"mod_info/force_digital_music",
							force_digital_music_str);
					bool force_digital_music
							= (force_digital_music_str == "yes"
							   || force_digital_music_str == "true");
					if (force_digital_music) {
						std::string warning_message
								= Strings::Themod()
								  + ((" \"" + Game::get_modtitle() + "\" ")
									 + Strings::requiresDigitalMusic_())
								  + "\n" + Strings::Disableanyway_();
						if (!Yesno_gump::ask(
									warning_message.c_str(), nullptr,
									"TINY_BLACK_FONT")) {
							return;
						}
					}
				}
			}
		}
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
#ifdef ENABLE_MIDISFX
		midi->set_effects_conversion(sfx_conversion);
#endif
	} else {
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
					EXULT_FLX_MEDITOWN_MID, true, MyMidiPlayer::Force_None,
					EXULT_FLX);
		} else {
			Audio::get_ptr()->start_music(track_playing, looping);
		}
	}
}

void AudioOptions_gump::paint() {
	Modal_gump::paint();
	for (auto& btn : buttons) {
		if (btn != nullptr) {
			btn->paint();
		}
	}

	Image_window8*        iwin = gwin->get_win();

	font->paint_text(
			iwin->get_ib8(), Strings::Audio_(), x + label_margin, y + yForRow(1) + 1);
	if (audio_enabled) {
		font->paint_text(
				iwin->get_ib8(), Strings::samplerate(), x + label_margin,
				y + yForRow(2) + 1);
		font->paint_text(
				iwin->get_ib8(), Strings::speakertype(), x + label_margin,
				y + yForRow(3) + 1);
		font->paint_text(
				iwin->get_ib8(), Strings::Music_(), x + label_margin,
				y + yForRow(4) + 1);
		if (midi_enabled) {
			font->paint_text(
					iwin->get_ib8(), Strings::looping(), x + label_margin,
					y + yForRow(5) + 1);
			font->paint_text(
					iwin->get_ib8(), Strings::digitalmusic(), x + label_margin,
					y + yForRow(6) + 1);
			if (!midi_ogg_enabled) {
				font->paint_text(
						iwin->get_ib8(), Strings::mididriver(), x + label_margin,
						y + yForRow(7) + 1);
			}
		}
		font->paint_text(
				iwin->get_ib8(), Strings::SFX_(), x + label_margin,
				y + yForRow(10) + 1);
		if (sfx_enabled == 1 && have_digital_sfx()
			&& !gwin->is_in_exult_menu()) {
			font->paint_text(
					iwin->get_ib8(), Strings::pack(), x + label_margin,
					y + yForRow(11) + 1);
		}
#ifdef ENABLE_MIDISFX
		else if (sfx_enabled == midi_state) {
			font->paint_text(
					iwin->get_ib8(), Strings::conversion(), x + label_margin,
					y + yForRow(11) + 1);
		}
#endif
		font->paint_text(
				iwin->get_ib8(), Strings::Speech_(), x + label_margin,
				y + yForRow(12) + 1);
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
