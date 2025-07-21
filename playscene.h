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

#ifndef PLAYSCENE_H
#define PLAYSCENE_H

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

class Game_window;
class Font;

struct FlicCommand {
	int index;
	int delay = 0;
};

struct AudioCommand {
	enum class Type {
		TRACK,
		SFX,
		OGG,
		WAVE,
		VOC
	};
	Type audio_type;
	int  index;
	int  start_frame;
	int  stop_condition;    // e.g., 0 for stop at end
};

struct SubtitleCommand {
	int         text_file_index;
	int         start_frame;
	int         line_number;
	int         font_type;
	int         color;
	int         duration_frames;
	std::string text;
	int         alignment;
};

using SceneCommand = std::variant<FlicCommand, AudioCommand, SubtitleCommand>;

struct TextEntry {
	std::string text;
	bool        centered;    // true for \C, false for \L
};

struct TextSection {
	std::vector<TextEntry>    entries;
	std::vector<SceneCommand> audio_commands;
	bool                      is_scrolling;
	int                       color;
	int                       font_type;
	int                       delay_ms;
};

struct OrderedSection {
	enum Type {
		SCENE,
		TEXT
	};

	Type                      type;
	std::vector<SceneCommand> scene_commands;
	TextSection               text_section;
};

class ScenePlayer {
private:
	enum class SkipAction {
		NONE,
		NEXT_SECTION,
		EXIT_SCENE
	};

	Game_window* gwin;
	std::string  scene_name;
	std::string  flx_path;
	std::string  info_path;

	std::vector<OrderedSection> ordered_sections;
	bool                        subtitles;
	bool                        speech_enabled;
	bool                        parse_scene_section(
								   const std::string& content, std::vector<SceneCommand>& commands);
	bool parse_text_section(const std::string& content, TextSection& section);
	bool load_text_from_flx(int index, std::string& text_content);
	SkipAction check_break();
	void       play_flic_with_audio(
				  const std::vector<SceneCommand>& commands,
				  std::vector<int>&                audio_ids);
	void show_scrolling_text(const TextSection& section);
	void show_delay_text(const TextSection& section);
	void show_text_section(
			const TextSection& section, std::vector<int>& active_audio_ids);
	void wait_for_audio_completion();
	void finish_scene();
	void start_audio_by_type(
			const AudioCommand& cmd, std::vector<int>& audio_ids,
			std::map<int, std::vector<int>>& command_audio_ids,
			size_t                           command_index);
	void stop_audio_by_type(
			const AudioCommand& cmd, const std::vector<int>& command_ids);
	AudioCommand parse_audio_command(
			const std::string& type_str, const std::vector<std::string>& parts);
	std::vector<int> active_audio_ids;
	bool             load_subtitle_from_file(
						int index, int line_number, std::string& out_text,
						int& out_alignment);
	void display_subtitle(const SubtitleCommand& cmd);

	struct ParsedTextLine {
		std::string text;
		int alignment;    // 0=left, 1=center, 2=left-right, 3=right-left
	};

	ParsedTextLine parse_text_formatting(const std::string& line);
	int            calculate_text_x_position(
					   int alignment, const std::string& text, std::shared_ptr<Font> font,
					   int screen_center);
	std::shared_ptr<Font> get_font_by_type(int font_type);
	void                  load_palette_by_color(int color);
	void                  show_single_text_page(
							 const TextEntry& entry, const TextSection& section);
	void process_section_content(
			const std::string& section_name, const std::string& content);
	int prev_palette = -1;

public:
	ScenePlayer(
			Game_window* gw, const std::string& scene_name, bool use_subtitles,
			bool use_speech);
	~ScenePlayer();

	bool scene_available() const;
	bool parse_info_file();
	void play_scene();
};

bool scene_available(const std::string& scene_name);
void play_scene(const std::string& scene_name);

#endif    // PLAYSCENE_H