/*
Copyright (C) 2026 The Exult Team

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

#ifndef MUI_CONFIG_META_H
#define MUI_CONFIG_META_H

#include <string>
#include <vector>

/*
 *  Description of one exult.cfg key (a row of data/mui_config_keys.txt).
 */
struct Mui_config_key {
	enum class Type {
		boolean,
		enumeration,
		integer,
		real,
		path,
		string
	};

	enum class Apply {
		now,
		restart
	};

	std::string              path;    // '*' = one dynamic part of a segment.
	Type                     type = Type::string;
	std::string              def;        // Default value; may be empty.
	std::vector<std::string> choices;    // Enumeration values.
	bool                     has_range = false;
	double                   minv      = 0;
	double                   maxv      = 0;
	Apply                    apply     = Apply::restart;
	std::string              description;

	bool is_pattern() const {
		return path.find('*') != std::string::npos;
	}

	// Index of value in choices (ignoring case), or -1.
	int choice_index(const std::string& value) const;
	// Whether value is the default (same choice, number or text).
	bool is_default(const std::string& value) const;
	// Empty if value is valid for the key, else why not (translated).
	std::string validate(const std::string& value) const;
};

/*
 *  The table of known exult.cfg keys, read once from exult.flx
 *  (EXULT_FLX_MUI_CONFIG_KEYS_TXT). Rows are
 *  path|type|default|range-or-choices|apply|description; see the header
 *  of data/mui_config_keys.txt.
 */
class Mui_config_meta {
public:
	static const Mui_config_meta& get();

	// Parse table text, replacing the current rows. Bad rows are skipped
	// (and reported on stderr). Returns the number of rows read.
	size_t parse(const std::string& text);
	// The row for a key: an exact path first, else the matching pattern
	// with the most literal characters. nullptr for unknown keys.
	const Mui_config_key* find(const std::string& path) const;

	const std::vector<Mui_config_key>& keys() const {
		return rows;
	}

	// Whether a '*' pattern matches a key path ('*' never crosses a '/').
	static bool matches(const std::string& pattern, const std::string& path);

private:
	std::vector<Mui_config_key> rows;
};

#endif
