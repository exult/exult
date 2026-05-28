/*
 *  Copyright (C) 2026  The Exult Team
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

#include "font_map.h"

#include "databuf.h"
#include "exult_flx.h"
#include "fnames.h"
#include "msgfile.h"
#include "utils.h"

#include <charconv>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <vector>

using std::cerr;
using std::endl;

static std::unordered_map<std::string, std::string> utf8_to_font_special;
static std::unordered_map<std::string, std::string> utf8_to_font_ascii;
static bool                                         font_maps_initialized = false;

// Convert a hex string like "C387" to the corresponding bytes "\xC3\x87".
static std::string hex_to_bytes(std::string_view hex) {
	std::string result;
	result.reserve(hex.size() / 2);
	for (size_t i = 0; i + 1 < hex.size(); i += 2) {
		unsigned char byte = 0;
		std::from_chars(hex.data() + i, hex.data() + i + 2, byte, 16);
		result += static_cast<char>(byte);
	}
	return result;
}

// Set font_maps_initialized = true before constructing the inner
// Text_msg_file_reader so re-entrant translation calls become no-ops.
static void ensure_font_maps_loaded() {
	if (font_maps_initialized) {
		return;
	}
	font_maps_initialized = true;

	auto parse_reader = [](Text_msg_file_reader& reader) {
		std::vector<std::string> strings;

		// builtin section: entries are  <UTF8HEX>/<HEXBYTE>  e.g. C387/01
		reader.get_section_strings("builtin", strings);
		for (const auto& s : strings) {
			if (s.empty()) {
				continue;
			}
			const auto slash = s.find('/');
			if (slash == std::string::npos) {
				continue;
			}
			std::string   key = hex_to_bytes({s.data(), slash});
			unsigned char val = 0;
			std::from_chars(s.data() + slash + 1, s.data() + s.size(), val, 16);
			utf8_to_font_special[std::move(key)] = std::string(1, static_cast<char>(val));
		}

		// original section: entries are  <UTF8HEX>/<ASCIIREPLACEMENT>  e.g. C3BC/ue
		reader.get_section_strings("original", strings);
		for (const auto& s : strings) {
			if (s.empty()) {
				continue;
			}
			const auto slash = s.find('/');
			if (slash == std::string::npos) {
				continue;
			}
			std::string key = hex_to_bytes({s.data(), slash});
			utf8_to_font_ascii[std::move(key)] = s.substr(slash + 1);
		}
	};

	// Base font map: always loaded from exult.flx.
	{
		IExultDataSource ds(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX), EXULT_FLX_FONT_MAP_TXT);
		if (!ds.good()) {
			cerr << "Warning: could not load font_map.txt from exult.flx" << endl;
		} else {
			Text_msg_file_reader reader(ds);
			parse_reader(reader);
		}
	}

	// Patch font map (overlays the base entries).
	if (is_system_path_defined("<PATCH>") && U7exists(PATCH_FONT_MAP)) {
		IFileDataSource ds(PATCH_FONT_MAP, true);
		if (ds.good()) {
			Text_msg_file_reader reader(ds);
			parse_reader(reader);
		}
	}
}

void translate_utf8_to_font_hex(std::string& text, bool use_special_chars) {
	ensure_font_maps_loaded();
	const auto& utf8_map = use_special_chars ? utf8_to_font_special : utf8_to_font_ascii;

	std::string result;
	result.reserve(text.size());

	size_t i = 0;
	while (i < text.size()) {
		// 2-byte UTF-8 sequence (0xC0-0xDF followed by 0x80-0xBF).
		if (i + 1 < text.size() && (static_cast<unsigned char>(text[i]) & 0xE0) == 0xC0) {
			auto it = utf8_map.find(std::string(text.data() + i, 2));
			if (it != utf8_map.end()) {
				result += it->second;
				i += 2;
				continue;
			}
		}
		result += text[i];
		++i;
	}

	text = std::move(result);
}

// Mode used by translate_usecode_text(); kept in sync with the font config
// by Game::setup_text(). Defaults to true so usecode strings are translated
// against the special-char map even before setup_text() runs.
static bool font_map_use_special_chars = true;

void set_font_map_use_special_chars(bool value) {
	font_map_use_special_chars = value;
}

void translate_usecode_text(std::string& text) {
	translate_utf8_to_font_hex(text, font_map_use_special_chars);
}

void init_font_map() {
	// Just register the translator. The actual load is deferred to the first
	// translation call.
	set_text_msg_translator(&translate_utf8_to_font_hex);
}
