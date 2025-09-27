/**
 ** Msgfile.h - Read in text message file.
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

#ifndef INCL_MSGFILE_H
#define INCL_MSGFILE_H

#include "common_types.h"

#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <vector>

class IDataSource;

class Text_msg_file_reader {
	using Section_data  = std::vector<std::optional<std::string_view>>;
	using Text_msg_data = std::map<std::string_view, Section_data, std::less<>>;
	using Text_msg_first = std::map<std::string_view, uint32, std::less<>>;
	std::string    contents;
	Section_data   global_section;
	uint32         global_first;
	Text_msg_data  items;
	Text_msg_first firsts;

	bool parse_contents();

public:
	Text_msg_file_reader();
	Text_msg_file_reader(IDataSource& in);

	[[nodiscard]] const Section_data& get_global_section() const {
		return global_section;
	}

	uint32 get_global_section_strings(std::vector<std::string>& strings) const {
		strings.clear();
		for (const auto& s : global_section) {			
			strings.emplace_back(s.value_or(std::string()));
		}
		return global_first;
	}
	uint32 get_global_section_strings(std::vector<std::optional<std::string>>& strings) const {
		strings.clear();
		for (const auto& s : global_section) {
			strings.emplace_back(s);
		}
		return global_first;
	}

	[[nodiscard]] const Section_data* get_section(
			std::string_view sectionName, int& firstMsg) const {
		auto section = items.find(sectionName);
		if (section != items.end()) {
			firstMsg = firsts.at(sectionName);
			return std::addressof(section->second);
		}
		firstMsg = -1;
		return nullptr;
	}

	uint32 get_section_strings(
			std::string_view          sectionName,
			std::vector<std::string>& strings) const {
		strings.clear();
		if (has_section(sectionName)) {
			for (const auto& s : items.at(sectionName)) {
				strings.emplace_back(s.value_or(std::string()));
			}
			return firsts.at(sectionName);
		}
		return 0;
	}


	uint32 get_section_strings(
			std::string_view          sectionName,
			std::vector<std::optional<std::string>>& strings) const {
		strings.clear();
		if (has_section(sectionName)) {
			for (const auto& s : items.at(sectionName)) {
				strings.emplace_back(s);
			}
			return firsts.at(sectionName);
		}
		return 0;
	}

	const Text_msg_data& get_sections() const {
		return items;
	}

	[[nodiscard]] bool has_section(std::string_view sectionName) const {
		return items.find(sectionName) != items.end();
	}

	[[nodiscard]] std::optional<int> get_version() const;
};

void Write_msg_file_section(
		std::ostream& out, const char* section,
		std::vector<std::string>& items);

#endif
