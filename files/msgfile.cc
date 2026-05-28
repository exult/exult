/**
 ** Msgfile.cc - Read in text message file.
 **
 ** Written: 6/25/03
 **/

/*
 *  Copyright (C) 2002-2022  The Exult Team
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

#include "msgfile.h"

#include "databuf.h"
#include "exult_flx.h"
#include "fnames.h"
#include "ios_state.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using std::cerr;
using std::endl;
using std::hex;
using std::istream;
using std::ostream;
using std::string;
using std::stringstream;
using std::vector;

/*
 *  Translation tables for UTF-8 encoded special characters to font hex
 *  positions. Loaded lazily from font_map.txt (see FONT_MAP in fnames.h).
 *  The builtin map covers fonts with special character positions (original/serif).
 *  The original map covers fonts without them, using ASCII equivalents.
 */

static std::unordered_map<std::string, std::string> utf8_to_font_special;
static std::unordered_map<std::string, std::string> utf8_to_font_ascii;
static bool                                         font_maps_initialized = false;

// Converts a hex string like "C387" to the corresponding binary bytes "\xC3\x87".
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

// Loads both font maps from font_map.txt on first call, then overlays any
// patch entries from PATCH_FONT_MAP (if present) — same layering pattern as
// shape_info.txt.  Sets font_maps_initialized = true before creating the inner
// Text_msg_file_reader so that re-entrant calls (translate_utf8_to_font_hex on
// the font_map.txt itself) are no-ops and do not cause infinite recursion.
static void ensure_font_maps_loaded() {
	if (font_maps_initialized) {
		return;
	}
	font_maps_initialized = true;    // Must be set before the recursive constructor call below.

	// Parse one Text_msg_file_reader's sections into the two maps.
	// Entries in a later call override entries from an earlier call,
	// so patch entries naturally shadow base entries for the same key.
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

	// Patch font map (loaded on top when a patch directory is active).
	if (is_system_path_defined("<PATCH>") && U7exists(PATCH_FONT_MAP)) {
		IFileDataSource ds(PATCH_FONT_MAP, true);
		if (ds.good()) {
			Text_msg_file_reader reader(ds);
			parse_reader(reader);
		}
	}
}

/*
 *  Translate UTF-8 encoded special characters to font hex positions.
 *  This modifies the string in place, converting multi-byte UTF-8 sequences
 *  to single bytes (or multi-byte ASCII equivalents) based on font config.
 */
static void translate_utf8_to_font_hex(std::string& text, bool use_special_chars) {
	ensure_font_maps_loaded();
	const auto& utf8_map = use_special_chars ? utf8_to_font_special : utf8_to_font_ascii;

	std::string result;
	result.reserve(text.size());

	size_t i = 0;
	while (i < text.size()) {
		// Check for 2-byte UTF-8 sequence (0xC0-0xDF followed by 0x80-0xBF)
		if (i + 1 < text.size() && (static_cast<unsigned char>(text[i]) & 0xE0) == 0xC0) {
			auto it = utf8_map.find(std::string(text.data() + i, 2));
			if (it != utf8_map.end()) {
				result += it->second;
				i += 2;
				continue;
			}
		}
		// Not a recognized UTF-8 sequence, copy byte as-is
		result += text[i];
		++i;
	}

	text = std::move(result);
}

Text_msg_file_reader::Text_msg_file_reader() : global_first(0) {}

Text_msg_file_reader::Text_msg_file_reader(IDataSource& in, bool use_special_chars) : global_first(0) {
	in.read(contents, in.getAvail());
	// Translate UTF-8 special characters to font hex positions
	translate_utf8_to_font_hex(contents, use_special_chars);
	if (!parse_contents()) {
		cerr << "Error parsing text message file" << endl;
		global_section.clear();
		items.clear();
	}
}

/*
 *  Read in text, where each line is of the form "nnn:sssss", where nnn is
 *  to be the Flex entry #, and anything after the ':' is the string to
 *  store.
 *  NOTES:  Entry #'s may be skipped, and may be given in hex (0xnnn)
 *          or decimal.
 *      Max. text length is 1024.
 *      A line beginning with a '#' is a comment.
 *      A 'section' can be marked:
 *          %%section shapes
 *              ....
 *          %%endsection
 *  Output: true if successful, false if not.
 */

bool Text_msg_file_reader::parse_contents() {
	constexpr static const auto             NONEFOUND = std::numeric_limits<uint32>::max();
	constexpr static const std::string_view sectionStart("%%section");
	constexpr static const std::string_view sectionEnd("%%endsection");

	Section_data* current_section = &global_section;
	uint32*       current_first   = &global_first;
	*current_first                = NONEFOUND;

	current_section->reserve(1000);

	int    linenum    = 0;
	uint32 next_index = 0;    // For auto-indexing of lines

	enum class State : uint8 {
		None,
		InSection
	};
	std::string_view data(contents);
	State            state = State::None;
	while (!data.empty()) {
		++linenum;
		const auto lineEnd = data.find_first_of("\r\n");
		auto       line    = data.substr(0, lineEnd);
		// Skip data up to the start of the next line.
		if (lineEnd == std::string_view::npos) {
			data.remove_prefix(data.size());
		} else {
			data.remove_prefix(line.size());
		}
		const auto nextLine = data.find_first_not_of("\r\n");
		if (nextLine != std::string_view::npos) {
			data.remove_prefix(nextLine);
		} else {
			data.remove_prefix(data.size());
		}
		// Ignore leading whitespace.
		auto nonWs = line.find_first_not_of(" \t\b");
		line.remove_prefix(nonWs);
		if (line.empty()) {
			continue;    // Empty line.
		}

		if (line.compare(0, sectionStart.length(), sectionStart) == 0) {
			if (state == State::InSection) {
				cerr << "Line " << linenum << " has a section starting inside another section" << endl;
			}
			const auto namePos = line.find_first_not_of(" \t\b", sectionStart.length());
			line.remove_prefix(namePos);
			auto sectionName(line);
			if (sectionName.empty()) {
				cerr << "Line " << linenum << " has an empty section name" << endl;
				return false;
			}
			{
				auto [iter, inserted] = items.try_emplace(sectionName);
				if (!inserted) {
					cerr << "Line " << linenum << " has a duplicate section name: " << sectionName << endl;
					return false;
				}
				current_section = &iter->second;
				current_section->reserve(1000);
			}
			{
				auto [iter, inserted] = firsts.try_emplace(sectionName, NONEFOUND);
				if (!inserted) {
					cerr << "Line " << linenum << " has a duplicate section name: " << sectionName << endl;
					return false;
				}
				current_first = &iter->second;
			}
			state = State::InSection;
			continue;
		}

		if (line.compare(0, sectionEnd.length(), sectionEnd) == 0) {
			if (state != State::InSection) {
				cerr << "Line " << linenum << " has an endsection without a section" << endl;
			}
			// Reset to sane defaults.
			state           = State::None;
			current_section = &global_section;
			current_first   = &global_first;
			continue;
		}

		uint32           index;
		std::string_view lineVal;
		if (line[0] == ':') {
			// Auto-index lines missing an index.
			index   = next_index++;
			lineVal = line.substr(1);
		} else if (line[0] == '#') {
			continue;
		} else {
			// Get line# in decimal, hex, or oct.
			auto colon = line.find(':');
			if (colon == std::string_view::npos) {
				cerr << "Missing ':' in line " << linenum << ".  Ignoring line" << endl;
				continue;
			}
			int base = 10;
			if (line.size() > 2 && line[0] == '0' && (line[1] == 'x' || line[1] == 'X')) {
				base = 16;
				colon -= 2;
				line.remove_prefix(2);
			} else if (line[0] == '0') {
				base = 8;
			}
			const auto* start = line.data();
			const auto* end   = std::next(start, colon);
			auto [p, ec]      = std::from_chars(start, end, index, base);
			if (ec != std::errc() || p != end) {
				cerr << "Line " << linenum << " doesn't start with a number" << endl;
				return false;
			}
			lineVal = line.substr(colon + 1);
		}
		if (index >= current_section->size()) {
			current_section->resize(index + 1);
		}
		(*current_section)[index] = lineVal;
		*current_first            = std::min(index, *current_first);
	}
	return true;
}

[[nodiscard]] std::optional<int> Text_msg_file_reader::get_version() const {
	if (this->contents.empty()) {
		return std::nullopt;
	}
	constexpr static const std::string_view versionstr("version");
	int                                     firstMsg;
	const auto*                             data = get_section(versionstr, firstMsg);
	if (data == nullptr) {
		cerr << "No version number in text message file" << endl;
		return std::nullopt;
	}
	if (data->size() != 1) {
		cerr << "Invalid version number in text message file" << endl;
		return std::nullopt;
	}

	int         version;
	auto        versionStr = (*data)[0].value_or(std::string_view());
	const auto* start      = versionStr.data();
	const auto* end        = std::next(start, versionStr.size());
	if (std::from_chars(start, end, version).ec != std::errc()) {
		cerr << "Invalid version number in text message file" << endl;
		return std::nullopt;
	}
	return version;
}

/*
 *  Write one section.
 */

void Write_msg_file_section(ostream& out, const char* section, vector<string>& items) {
	const boost::io::ios_flags_saver flags(out);
	out << "%%section " << section << hex << endl;
	for (unsigned i = 0; i < items.size(); ++i) {
		if (!items[i].empty()) {
			out << "0x" << i << ':' << items[i] << endl;
		}
	}
	out << "%%endsection " << section << endl;
}
