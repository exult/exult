/*
 *  Copyright (C) 2025  The Exult Team
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

#include "playscene.h"

#include "Audio.h"
#include "Configuration.h"
#include "OggAudioSample.h"
#include "VocAudioSample.h"
#include "WavAudioSample.h"
#include "data/exult_flx.h"
#include "exceptions.h"
#include "files/U7file.h"
#include "files/utils.h"
#include "flic/playfli.h"
#include "font.h"
#include "game.h"
#include "gamewin.h"
#include "palette.h"
#include "txtscroll.h"
#include "vgafile.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

using std::make_unique;
using std::string;
using std::unique_ptr;
using std::vector;

namespace {
	int safe_stoi(
			const std::vector<std::string>& parts, size_t index,
			int default_value = 0) {
		if (index < parts.size()) {
			try {
				return std::stoi(parts[index]);
			} catch (const std::invalid_argument&) {
			}
		}
		return default_value;
	}

	string trim(const string& str) {
		size_t first = str.find_first_not_of(" \t\r\n");
		if (first == string::npos) {
			return "";
		}
		size_t last = str.find_last_not_of(" \t\r\n");
		return str.substr(first, (last - first + 1));
	}

	vector<string> split(const string& str, char delimiter) {
		vector<string>    tokens;
		std::stringstream ss(str);
		string            token;
		while (std::getline(ss, token, delimiter)) {
			tokens.push_back(token);
		}
		return tokens;
	}
}    // namespace

ScenePlayer::ScenePlayer(
		Game_window* gw, const string& scene_name, bool use_subtitles,
		bool use_speech)
		: gwin(gw), scene_name(scene_name), subtitles(use_subtitles),
		  speech_enabled(use_speech) {
	flx_path  = "<PATCH>/" + scene_name + ".flx";
	info_path = "<PATCH>/" + scene_name + "_info.txt";

	// Initialize fonts
	const char* fname = BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX);
	fontManager.add_font("CREDITS_FONT", fname, EXULT_FLX_FONT_SHP, 1);
	fontManager.add_font("HOT_FONT", fname, EXULT_FLX_FONTON_SHP, 1);
}

ScenePlayer::~ScenePlayer() = default;

bool ScenePlayer::scene_available() const {
	return U7exists(flx_path.c_str()) && U7exists(info_path.c_str());
}

bool ScenePlayer::parse_info_file() {
	std::unique_ptr<std::istream> file = U7open_in(info_path.c_str());
	if (!file || !file->good()) {
		return false;
	}

	ordered_sections.clear();

	string line;
	string current_section_name;
	string section_content;

	while (std::getline(*file, line)) {
		string trimmed_line = trim(line);

		if (current_section_name.empty()
			&& (trimmed_line.empty() || trimmed_line[0] == '#')) {
			continue;
		}

		if (trimmed_line.substr(0, 10) == "%%section ") {
			process_section_content(current_section_name, section_content);

			current_section_name = trimmed_line.substr(10);
			section_content.clear();
			continue;
		}

		if (trimmed_line == "%%endsection") {
			process_section_content(current_section_name, section_content);

			current_section_name.clear();
			section_content.clear();
			continue;
		}

		if (current_section_name == "version") {
			continue;
		}

		if (!current_section_name.empty()) {
			if (!section_content.empty()) {
				section_content += "\n";
			}
			section_content += trimmed_line;
		}
	}

	process_section_content(current_section_name, section_content);

	return true;
}

bool ScenePlayer::parse_scene_section(
		const string& content, vector<SceneCommand>& commands) {
	std::istringstream iss(content);
	string             line;

	while (std::getline(iss, line)) {
		line = trim(line);
		if (line.empty() || line[0] != ':') {
			continue;
		}

		vector<string> parts = split(line.substr(1), '/');
		if (parts.empty()) {
			continue;
		}

		const string& type_str = parts[0];

		if (type_str == "flic") {
			int index           = safe_stoi(parts, 1);
			int fade_in_frames  = safe_stoi(parts, 2);
			int fade_out_frames = safe_stoi(parts, 3);
			commands.emplace_back(
					FlicCommand{index, fade_in_frames, fade_out_frames});
		} else if (type_str == "subtitle") {
			if (parts.size() < 6) {
				continue;
			}

			SubtitleCommand cmd;
			cmd.text_file_index = safe_stoi(parts, 1);
			cmd.start_frame     = safe_stoi(parts, 2);
			cmd.line_number     = safe_stoi(parts, 3);
			cmd.font_type       = safe_stoi(parts, 4);
			cmd.color           = safe_stoi(parts, 5);
			cmd.duration_frames
					= safe_stoi(parts, 6, 15);    // Default duration 15 frames

			if (load_subtitle_from_file(
						cmd.text_file_index, cmd.line_number, cmd.text,
						cmd.alignment)) {
				commands.emplace_back(cmd);
			}
		} else if (
				type_str == "sfx" || type_str == "track" || type_str == "ogg"
				|| type_str == "wave" || type_str == "voc") {
			AudioCommand::Type audio_type;
			if (type_str == "sfx") {
				audio_type = AudioCommand::Type::SFX;
			} else if (type_str == "track") {
				audio_type = AudioCommand::Type::TRACK;
			} else if (type_str == "ogg") {
				audio_type = AudioCommand::Type::OGG;
			} else if (type_str == "wave") {
				audio_type = AudioCommand::Type::WAVE;
			} else {
				audio_type = AudioCommand::Type::VOC;
			}

			int index          = safe_stoi(parts, 1);
			int start_frame    = safe_stoi(parts, 2);
			int stop_condition = safe_stoi(parts, 3);

			commands.emplace_back(AudioCommand{
					audio_type, index, start_frame, stop_condition});
		}
	}
	return true;
}

bool ScenePlayer::parse_text_section(
		const string& content, TextSection& section) {
	std::istringstream iss(content);
	string             line;

	while (std::getline(iss, line)) {
		line = trim(line);
		if (line.empty() || line[0] != ':') {
			continue;
		}

		// Remove the leading ':'
		line                 = line.substr(1);
		vector<string> parts = split(line, '/');

		if (parts.empty()) {
			continue;
		}

		string type_str = parts[0];

		if (type_str == "text") {
			if (parts.size() < 5) {
				std::cerr << "Play_Scene Error: Invalid text command format: "
						  << line << std::endl;
				continue;
			}

			int index = std::stoi(parts[1]);
			int font_type
					= std::stoi(parts[2]);      // 0=CREDITS_FONT, 1=HOT_FONT
			int color = std::stoi(parts[3]);    // Color index
			int mode  = std::stoi(
                    parts[4]);    // 0=scroll, 1=delay(default), >1=delay(ms)

			string text_content;
			if (!load_text_from_flx(index, text_content)) {
				std::cerr << "Play_Scene Error: Could not load text " << index
						  << " from FLX file" << std::endl;
				continue;
			}

			section.is_scrolling = (mode == 0);
			if (mode == 1) {
				section.delay_ms = 3000;    // Default 3-second delay
			} else if (mode > 1) {
				section.delay_ms = mode;    // Custom delay in ms
			} else {
				section.delay_ms = 0;    // Not applicable for scrolling
			}

			section.color     = color;
			section.font_type = font_type;

			if (section.is_scrolling) {
				std::istringstream text_iss(text_content);
				string             text_line;

				while (std::getline(text_iss, text_line)) {
					if (text_line.empty()) {
						// Add empty line as separate entry
						TextEntry empty_entry;
						empty_entry.centered = false;
						empty_entry.text     = "";
						section.entries.push_back(empty_entry);
					} else {
						// Keep the original line with formatting codes intact
						TextEntry entry;
						entry.centered = false;
						entry.text     = text_line;
						section.entries.push_back(entry);
					}
				}
			} else {
				// For delay text, group lines until empty line (page break)
				std::istringstream text_iss(text_content);
				string             text_line;
				string             current_text_block;
				bool               in_text_block = false;

				while (std::getline(text_iss, text_line)) {
					if (text_line.empty()) {
						// Empty line = page break for delay text
						if (in_text_block && !current_text_block.empty()) {
							TextEntry entry;
							entry.centered = false;
							entry.text     = current_text_block;
							section.entries.push_back(entry);
							current_text_block.clear();
							in_text_block = false;
						}
					} else {
						if (!in_text_block) {
							current_text_block = text_line;
							in_text_block      = true;
						} else {
							if (!current_text_block.empty()) {
								current_text_block += "\n";
							}
							current_text_block += text_line;
						}
					}
				}

				if (in_text_block && !current_text_block.empty()) {
					TextEntry entry;
					entry.centered = false;
					entry.text     = current_text_block;
					section.entries.push_back(entry);
				}
			}
		} else if (
				type_str == "sfx" || type_str == "track" || type_str == "ogg"
				|| type_str == "wave" || type_str == "voc") {
			SceneCommand cmd = parse_audio_command(type_str, parts);
			section.audio_commands.push_back(cmd);
		}
	}

	return !section.entries.empty();
}

ScenePlayer::SkipAction ScenePlayer::check_break() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_KEY_DOWN) {
			switch (event.key.key) {
			case SDLK_ESCAPE:
				return SkipAction::EXIT_SCENE;
			case SDLK_SPACE:
			case SDLK_RETURN:
				return SkipAction::NEXT_SECTION;
			default:
				break;
			}
		}
	}
	return SkipAction::NONE;
}

void ScenePlayer::play_flic_with_audio(
		const vector<SceneCommand>& commands, std::vector<int>& audio_ids) {
	if (commands.empty()) {
		return;
	}

	gwin->clear_screen(true);

	const FlicCommand* flic_cmd = nullptr;
	for (const auto& command_variant : commands) {
		if (std::holds_alternative<FlicCommand>(command_variant)) {
			flic_cmd = &std::get<FlicCommand>(command_variant);
			break;
		}
	}

	Audio* audio = Audio::get_ptr();
	if (!audio) {
		return;
	}

	// Map to track which audio IDs belong to which commands
	std::map<int, std::vector<int>> command_audio_ids;

	std::vector<std::pair<SubtitleCommand, int>> active_subtitles;

	// Map frame numbers to a list of commands that start on that frame.
	std::map<int, std::vector<std::pair<SceneCommand, size_t>>> timed_commands;
	for (size_t i = 0; i < commands.size(); ++i) {
		const auto& cmd_variant = commands[i];
		std::visit(
				[&](auto&& cmd) {
					using T = std::decay_t<decltype(cmd)>;
					if constexpr (
							std::is_same_v<T, AudioCommand>
							|| std::is_same_v<T, SubtitleCommand>) {
						timed_commands[cmd.start_frame].push_back(
								{cmd_variant, i});
					}
				},
				cmd_variant);
	}

	try {
		gwin->clear_screen(true);

		playfli fli(flx_path.c_str(), flic_cmd->index);

		// Start audio tracks that should play immediately (frame 0)
		if (timed_commands.count(0)) {
			for (const auto& cmd_pair : timed_commands[0]) {
				std::visit(
						[&](auto&& cmd) {
							using T = std::decay_t<decltype(cmd)>;
							if constexpr (std::is_same_v<T, AudioCommand>) {
								start_audio_by_type(
										cmd, audio_ids, command_audio_ids,
										cmd_pair.second);
							}
						},
						cmd_pair.first);
			}
		}

		if (flic_cmd->fade_in_frames > 0) {
			gwin->get_pal()->fade_in(flic_cmd->fade_in_frames);
		}

		// Play the flic
		playfli::fliinfo finfo;
		fli.info(&finfo);

		uint32 next = SDL_GetTicks();

		for (unsigned int i = 0; i < static_cast<unsigned>(finfo.frames); i++) {
			next = fli.play(gwin->get_win(), i, i, next);

			// Check if any commands are scheduled for the current frame 'i'.
			if (timed_commands.count(i)) {
				for (const auto& cmd_pair : timed_commands[i]) {
					const auto&  command_variant = cmd_pair.first;
					const size_t original_index  = cmd_pair.second;

					std::visit(
							[&](auto&& cmd) {
								using T = std::decay_t<decltype(cmd)>;
								if constexpr (std::is_same_v<
													  T, SubtitleCommand>) {
									active_subtitles.push_back(
											{cmd, cmd.duration_frames});
								} else if constexpr (std::is_same_v<
															 T, AudioCommand>) {
									start_audio_by_type(
											cmd, audio_ids, command_audio_ids,
											original_index);
								}
							},
							command_variant);
				}
			}

			for (auto it = active_subtitles.begin();
				 it != active_subtitles.end();) {
				display_subtitle(it->first);
				it->second--;

				if (it->second <= 0) {
					it = active_subtitles.erase(it);
				} else {
					++it;
				}
			}

			gwin->get_win()->ShowFillGuardBand();

			switch (check_break()) {
			case SkipAction::EXIT_SCENE:
				throw UserSkipException();    // Will be caught by play_scene to
											  // exit
			case SkipAction::NEXT_SECTION:
				return;    // Exit this function to advance to the next section
			case SkipAction::NONE:
				break;
			}
		}

		if (flic_cmd->fade_out_frames > 0) {
			gwin->get_pal()->fade_out(flic_cmd->fade_out_frames);
		}

		gwin->clear_screen(true);

		// Stop audio that should stop with end of flic
		for (size_t i = 0; i < commands.size(); i++) {
			const auto& cmd_variant = commands[i];
			std::visit(
					[&](auto&& cmd) {
						using T = std::decay_t<decltype(cmd)>;
						if constexpr (std::is_same_v<T, AudioCommand>) {
							if (cmd.stop_condition == 0) {
								stop_audio_by_type(cmd, command_audio_ids[i]);
							}
						}
					},
					cmd_variant);
		}
	} catch (const UserSkipException&) {
		audio->stop_music();
		for (int id : audio_ids) {
			audio->stop_sound_effect(id);
		}
		gwin->clear_screen(true);
		throw;
	}

	gwin->clear_screen(true);
}

void ScenePlayer::show_delay_text(const TextSection& section) {
	try {
		std::shared_ptr<Font> font = get_font_by_type(section.font_type);
		if (!font) {
			std::cerr
					<< "Play_Scene Warning: Could not load font for delay text"
					<< std::endl;
			return;
		}

		Image_window8* win           = gwin->get_win();
		int            screen_center = gwin->get_width() / 2;

		const auto& entry = section.entries[0];
		gwin->clear_screen();

		std::istringstream          iss(entry.text);
		std::string                 line;
		std::vector<ParsedTextLine> parsed_lines;

		while (std::getline(iss, line)) {
			parsed_lines.push_back(parse_text_formatting(line));
		}

		int total_height = parsed_lines.size() * font->get_text_height();
		int start_y      = gwin->get_height() / 2 - total_height / 2;

		for (size_t i = 0; i < parsed_lines.size(); i++) {
			const auto& parsed = parsed_lines[i];
			int         x      = calculate_text_x_position(
                    parsed.alignment, parsed.text, font, screen_center);
			int y = start_y + i * font->get_text_height();

			font->draw_text(win->get_ib8(), x, y, parsed.text.c_str());
		}

		win->ShowFillGuardBand();
	} catch (const UserSkipException&) {
		// Stop all audio and re-throw to exit the scene player.
		Audio* audio = Audio::get_ptr();
		if (audio) {
			audio->stop_music();
			for (int id : active_audio_ids) {
				audio->stop_sound_effect(id);
			}
		}
		gwin->clear_screen(true);
		throw;
	}
}

void ScenePlayer::show_text_section(
		const TextSection& section, std::vector<int>& audio_ids) {
	std::map<int, std::vector<int>> command_audio_ids;
	size_t                          next_command_index = 0;

	// Sort audio commands by start time for easy processing
	std::vector<std::pair<SceneCommand, size_t>> sorted_commands;
	for (size_t i = 0; i < section.audio_commands.size(); i++) {
		const auto& cmd_variant = section.audio_commands[i];
		sorted_commands.push_back({cmd_variant, i});
	}

	std::sort(
			sorted_commands.begin(), sorted_commands.end(),
			[](const auto& a, const auto& b) {
				auto get_start_frame = [](auto&& arg) -> int {
					using T = std::decay_t<decltype(arg)>;
					if constexpr (
							std::is_same_v<T, AudioCommand>
							|| std::is_same_v<T, SubtitleCommand>) {
						return arg.start_frame;
					}
					return 0;
				};
				int frame_a = std::visit(get_start_frame, a.first);
				int frame_b = std::visit(get_start_frame, b.first);
				return frame_a < frame_b;
			});

	load_palette_by_color(section.color);

	if (section.is_scrolling) {
		// Start any audio that should play immediately (time 0)
		for (const auto& cmd_pair : sorted_commands) {
			std::visit(
					[&](auto&& cmd) {
						using T = std::decay_t<decltype(cmd)>;
						if constexpr (std::is_same_v<T, AudioCommand>) {
							if (cmd.start_frame == 0) {
								start_audio_by_type(
										cmd, audio_ids, command_audio_ids,
										cmd_pair.second);
							}
						}
					},
					cmd_pair.first);
		}

		show_scrolling_text(section);

	} else {
		size_t current_page_index = 0;

		while (current_page_index < section.entries.size()) {
			switch (check_break()) {
			case SkipAction::EXIT_SCENE:
				throw UserSkipException();
			case SkipAction::NEXT_SECTION:
				goto end_text_section;    // Use goto to break out of the nested
										  // loops
			case SkipAction::NONE:
				break;
			}

			// Display the current page
			TextSection single_page_section = section;
			single_page_section.entries = {section.entries[current_page_index]};
			show_delay_text(single_page_section);

			// Trigger any audio commands scheduled for this page index.
			while (next_command_index < sorted_commands.size()) {
				const auto& cmd_pair = sorted_commands[next_command_index];
				bool        command_triggered = false;

				std::visit(
						[&](auto&& cmd) {
							using T = std::decay_t<decltype(cmd)>;
							if constexpr (std::is_same_v<T, AudioCommand>) {
								if (cmd.start_frame
									== static_cast<int>(current_page_index)) {
									start_audio_by_type(
											cmd, audio_ids, command_audio_ids,
											cmd_pair.second);
									command_triggered = true;
								} else if (
										cmd.start_frame > static_cast<int>(
												current_page_index)) {
									return;
								}
							}
						},
						cmd_pair.first);

				if (command_triggered) {
					next_command_index++;
				} else {
					break;
				}
			}

			// Wait for a 3-second timeout or user input to advance the page.
			uint32 page_display_time = SDL_GetTicks();
			while (SDL_GetTicks() - page_display_time
				   < static_cast<uint32_t>(section.delay_ms)) {
				switch (check_break()) {
				case SkipAction::EXIT_SCENE:
					throw UserSkipException();
				case SkipAction::NEXT_SECTION:
					goto end_text_section;
					break;
				case SkipAction::NONE:
					break;
				}
				SDL_Delay(50);
			}

			current_page_index++;
		}
	}
end_text_section:;
	if (section.color != 0) {
		load_palette_by_color(0);
	}

	// Stop audio that should stop at the end of the text section
	for (size_t i = 0; i < section.audio_commands.size(); i++) {
		const auto& cmd_variant = section.audio_commands[i];
		std::visit(
				[&](auto&& cmd) {
					using T = std::decay_t<decltype(cmd)>;
					if constexpr (std::is_same_v<T, AudioCommand>) {
						if (cmd.stop_condition
							== 0) {    // Stop at end of section
							stop_audio_by_type(cmd, command_audio_ids[i]);
						}
					}
				},
				cmd_variant);
	}
}

void ScenePlayer::play_scene() {
	if (!scene_available() || !parse_info_file()) {
		return;
	}

	try {
		active_audio_ids.clear();

		gwin->clear_screen(true);

		for (size_t i = 0; i < ordered_sections.size(); i++) {
			const OrderedSection& section = ordered_sections[i];

			gwin->clear_screen(true);

			if (section.type == OrderedSection::SCENE) {
				play_flic_with_audio(section.scene_commands, active_audio_ids);
			} else if (section.type == OrderedSection::TEXT) {
				show_text_section(section.text_section, active_audio_ids);
			}

			gwin->clear_screen(true);

			if (check_break() != SkipAction::NONE) {
				throw UserSkipException();
			}
		}

		wait_for_audio_completion();

		finish_scene();
	} catch (const UserSkipException&) {
		finish_scene();
	}

	gwin->clear_screen(true);
}

void ScenePlayer::wait_for_audio_completion() {
	SDL_Delay(500);
}

AudioCommand ScenePlayer::parse_audio_command(
		const std::string& type_str, const std::vector<std::string>& parts) {
	AudioCommand::Type audio_type;
	if (type_str == "sfx") {
		audio_type = AudioCommand::Type::SFX;
	} else if (type_str == "track") {
		audio_type = AudioCommand::Type::TRACK;
	} else if (type_str == "ogg") {
		audio_type = AudioCommand::Type::OGG;
	} else if (type_str == "wave") {
		audio_type = AudioCommand::Type::WAVE;
	} else {
		audio_type = AudioCommand::Type::VOC;
	}

	int index          = safe_stoi(parts, 1);
	int start_frame    = safe_stoi(parts, 2);
	int stop_condition = safe_stoi(parts, 3);

	return AudioCommand{audio_type, index, start_frame, stop_condition};
}

void ScenePlayer::start_audio_by_type(
		const AudioCommand& cmd, std::vector<int>& audio_ids,
		std::map<int, std::vector<int>>& command_audio_ids,
		size_t                           command_index) {
	Audio* audio = Audio::get_ptr();
	if (!audio) {
		return;
	}

	if (cmd.audio_type == AudioCommand::Type::TRACK) {
		audio->start_music(cmd.index, false);
	} else if (cmd.audio_type == AudioCommand::Type::SFX) {
		int id = audio->play_sound_effect(cmd.index, 255);
		if (id != -1) {
			audio_ids.push_back(id);
			active_audio_ids.push_back(id);
			command_audio_ids[command_index].push_back(id);
		}
	} else if (
			cmd.audio_type == AudioCommand::Type::OGG
			|| cmd.audio_type == AudioCommand::Type::WAVE) {
		File_spec sfxfile(flx_path.c_str(), cmd.index);
		int       id = audio->play_sound_effect(sfxfile, 0, 255);
		if (id != -1) {
			audio_ids.push_back(id);
			active_audio_ids.push_back(id);
			command_audio_ids[command_index].push_back(id);
		}
	} else if (cmd.audio_type == AudioCommand::Type::VOC) {
		if (!speech_enabled) {
			return;
		}

		U7multiobject voc_obj(flx_path.c_str(), cmd.index);
		size_t        len;
		const std::unique_ptr<unsigned char[]> voc_data = voc_obj.retrieve(len);
		if (voc_data && len > 0) {
			int id = audio->copy_and_play_speech(
					voc_data.get(), len, false, 255);
			if (id != -1) {
				audio_ids.push_back(id);
				active_audio_ids.push_back(id);
				command_audio_ids[command_index].push_back(id);
			}
		}
	}
}

void ScenePlayer::stop_audio_by_type(
		const AudioCommand& cmd, const std::vector<int>& command_ids) {
	Audio* audio = Audio::get_ptr();
	if (!audio) {
		return;
	}

	if (cmd.audio_type == AudioCommand::Type::TRACK) {
		audio->stop_music();
	} else {
		for (int id : command_ids) {
			audio->stop_sound_effect(id);
		}
	}
}

void ScenePlayer::finish_scene() {
	Audio* audio = Audio::get_ptr();
	if (!audio) {
		return;
	}

	audio->stop_music();

	// go through active_audio_ids and stop the sound effects
	for (int id : active_audio_ids) {
		audio->stop_sound_effect(id);
	}
	active_audio_ids.clear();

	audio->cancel_streams();

	gwin->clear_screen(true);

	gwin->get_pal()->fade_out(30);    // 30 cycles fade out

	gwin->clear_screen(true);
}

bool ScenePlayer::load_text_from_flx(int index, std::string& out_text) {
	try {
		U7multiobject                          txtobj(flx_path.c_str(), index);
		size_t                                 len;
		const std::unique_ptr<unsigned char[]> txt = txtobj.retrieve(len);
		if (!txt) {
			return false;
		}
		out_text = std::string(reinterpret_cast<char*>(txt.get()), len);
		return true;
	} catch (const std::exception& e) {
		std::cerr << "Play_Scene Error: loading text from flx: " << e.what()
				  << std::endl;
		return false;
	}
}

bool ScenePlayer::load_subtitle_from_file(
		int index, int line_number, std::string& out_text, int& out_alignment) {
	try {
		U7multiobject                          txtobj(flx_path.c_str(), index);
		size_t                                 len;
		const std::unique_ptr<unsigned char[]> txt = txtobj.retrieve(len);
		if (!txt) {
			return false;
		}

		string target_prefix = std::to_string(line_number);
		target_prefix.insert(0, 3 - target_prefix.length(), '0');
		target_prefix += ":";

		std::istringstream iss(
				std::string(reinterpret_cast<char*>(txt.get()), len));
		std::string line;

		// Read line-by-line instead of loading the whole file content again.
		while (std::getline(iss, line)) {
			if (line.rfind(target_prefix, 0) == 0) {
				string text_content   = line.substr(target_prefix.length());
				ParsedTextLine parsed = parse_text_formatting(text_content);
				out_text              = parsed.text;
				out_alignment         = parsed.alignment;
				return true;
			}
		}
		return false;
	} catch (const std::exception& e) {
		std::cerr << "Play_Scene Error: loading subtitle: " << e.what()
				  << std::endl;
		return false;
	}
}

void ScenePlayer::display_subtitle(const SubtitleCommand& cmd) {
	if (!subtitles) {
		return;
	}

	try {
		std::shared_ptr<Font> font = get_font_by_type(cmd.font_type);
		if (!font) {
			std::cerr << "Play_Scene Warning: Could not load font for subtitle"
					  << std::endl;
			return;
		}

		Image_window8* win           = gwin->get_win();
		int            screen_center = gwin->get_width() / 2;
		int            y             = gwin->get_height() - 25;

		int alignment = cmd.alignment & 0xFF;
		int x         = calculate_text_x_position(
                alignment, cmd.text, font, screen_center);

		font->draw_text(win->get_ib8(), x, y, cmd.text.c_str());
	} catch (const std::exception& e) {
		std::cerr << "Play_Scene Error: displaying subtitle: " << e.what()
				  << std::endl;
	}
}

void ScenePlayer::show_scrolling_text(const TextSection& section) {
	std::ostringstream text_stream;
	for (size_t i = 0; i < section.entries.size(); i++) {
		const auto& entry = section.entries[i];
		text_stream << entry.text;
		if (i < section.entries.size() - 1) {
			text_stream << "\n";
		}
	}

	try {
		std::shared_ptr<Font> font = get_font_by_type(section.font_type);
		if (font) {
			TextScroller scroller(text_stream.str(), font, nullptr);
			(void)scroller.run(gwin);
		}
	} catch (const std::exception& e) {
		std::cerr << "Play_scene Error: showing scrolling text: " << e.what()
				  << std::endl;
	}
}

ScenePlayer::ParsedTextLine ScenePlayer::parse_text_formatting(
		const std::string& line) {
	ParsedTextLine result;
	result.alignment = 0;
	result.text      = line;

	if (line.size() >= 2) {
		if (line.substr(0, 2) == "\\C") {
			result.alignment = 1;    // Center
			result.text      = line.substr(2);
		} else if (line.substr(0, 2) == "\\L") {
			result.alignment = 2;    // Left aligned to right center
			result.text      = line.substr(2);
		} else if (line.substr(0, 2) == "\\R") {
			result.alignment = 3;    // Right aligned to left center
			result.text      = line.substr(2);
		}
	}

	return result;
}

int ScenePlayer::calculate_text_x_position(
		int alignment, const std::string& text, std::shared_ptr<Font> font,
		int screen_center) {
	if (alignment == 1) {
		// Center
		return screen_center - font->get_text_width(text.c_str()) / 2;
	} else if (alignment == 2) {
		// Left aligned to right center
		return screen_center + 10;
	} else if (alignment == 3) {
		// Right aligned to left center
		return screen_center - font->get_text_width(text.c_str()) - 10;
	} else {
		// Default left alignment
		return 10;
	}
}

std::shared_ptr<Font> ScenePlayer::get_font_by_type(int font_type) {
	if (font_type == 1) {
		return fontManager.get_font("HOT_FONT");
	} else {
		return fontManager.get_font("CREDITS_FONT");
	}
}

void ScenePlayer::load_palette_by_color(int color) {
	const str_int_pair& palette_resource
			= game->get_resource(("palettes/" + std::to_string(color)).c_str());
	if (palette_resource.str) {
		try {
			gwin->get_pal()->load(palette_resource.str, palette_resource.num);
			gwin->get_pal()->apply();
		} catch (const std::exception& e) {
			std::cerr << "Play_scene Error: loading palette " << color << ": "
					  << e.what() << std::endl;
			gwin->get_pal()->apply();
		}
	} else {
		gwin->get_pal()->apply();
	}
}

void ScenePlayer::process_section_content(
		const std::string& section_name, const std::string& content) {
	if (section_name.empty() || content.empty()) {
		return;
	}

	OrderedSection section;
	if (section_name == "scene") {
		section.type = OrderedSection::SCENE;
		parse_scene_section(content, section.scene_commands);
		ordered_sections.push_back(section);
	} else if (section_name == "text") {
		section.type = OrderedSection::TEXT;
		parse_text_section(content, section.text_section);
		ordered_sections.push_back(section);
	}
}

bool scene_available(const string& scene_name) {
	Game_window* gwin = Game_window::get_instance();
	ScenePlayer  player(gwin, scene_name, true, true);
	return player.scene_available();
}

void play_scene(const string& scene_name) {
	Game_window* gwin  = Game_window::get_instance();
	Audio*       audio = Audio::get_ptr();
	const bool   speech_is_on
			= audio && audio->is_audio_enabled() && audio->is_speech_enabled();
	const bool subtitles_are_on
			= !speech_is_on || (audio && audio->is_speech_with_subs());
	ScenePlayer player(gwin, scene_name, subtitles_are_on, speech_is_on);
	player.play_scene();
}
