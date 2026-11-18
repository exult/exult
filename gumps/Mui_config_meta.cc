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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Mui_config_meta.h"

#include "databuf.h"
#include "exult_flx.h"
#include "fnames.h"
#include "items.h"
#include "utils.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace {
	class Strings {
	public:
		static auto YesOrNo() {
			return get_text_msg(0xC87 - msg_file_start);
		}

		static auto OneOf() {
			return get_text_msg(0xC88 - msg_file_start);
		}

		static auto WholeNumber() {
			return get_text_msg(0xC89 - msg_file_start);
		}

		static auto Number() {
			return get_text_msg(0xC8A - msg_file_start);
		}

		static auto RangeFrom() {
			return get_text_msg(0xC8B - msg_file_start);
		}

		static auto RangeTo() {
			return get_text_msg(0xC8C - msg_file_start);
		}
	};

	std::string trim(const std::string& s) {
		const size_t b = s.find_first_not_of(" \t\r\n");
		if (b == std::string::npos) {
			return {};
		}
		const size_t e = s.find_last_not_of(" \t\r\n");
		return s.substr(b, e - b + 1);
	}

	std::string lower(std::string s) {
		for (auto& c : s) {
			c = char(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	}

	std::vector<std::string> split(const std::string& s, char sep, size_t max_parts = 0) {
		std::vector<std::string> out;
		size_t                   start = 0;
		while (true) {
			const size_t at = s.find(sep, start);
			if (at == std::string::npos || (max_parts && out.size() + 1 == max_parts)) {
				out.push_back(s.substr(start));
				return out;
			}
			out.push_back(s.substr(start, at - start));
			start = at + 1;
		}
	}

	bool parse_long(const std::string& s, long& v) {
		const std::string t = trim(s);
		if (t.empty()) {
			return false;
		}
		char* end = nullptr;
		v         = std::strtol(t.c_str(), &end, 10);
		return end && *end == 0;
	}

	bool parse_double(const std::string& s, double& v) {
		const std::string t = trim(s);
		if (t.empty()) {
			return false;
		}
		char* end = nullptr;
		v         = std::strtod(t.c_str(), &end);
		return end && *end == 0 && std::isfinite(v);
	}

	std::string number_text(double v) {
		std::ostringstream ss;
		ss << v;
		return ss.str();
	}

	// Literal characters of a pattern (for picking the most specific one).
	size_t literal_length(const std::string& pattern) {
		size_t n = 0;
		for (const char c : pattern) {
			n += c != '*' ? 1 : 0;
		}
		return n;
	}

	bool match_from(const char* pat, const char* str) {
		while (*pat) {
			if (*pat == '*') {
				// '*' takes any run of characters within one path segment.
				for (const char* s = str;; s++) {
					if (match_from(pat + 1, s)) {
						return true;
					}
					if (*s == 0 || *s == '/') {
						return false;
					}
				}
			}
			if (*pat != *str) {
				return false;
			}
			pat++;
			str++;
		}
		return *str == 0;
	}
}    // namespace

/*
 *  Mui_config_key
 */

int Mui_config_key::choice_index(const std::string& value) const {
	const std::string v = lower(trim(value));
	for (size_t i = 0; i < choices.size(); i++) {
		if (lower(choices[i]) == v) {
			return int(i);
		}
	}
	return -1;
}

bool Mui_config_key::is_default(const std::string& value) const {
	switch (type) {
	case Type::boolean:
	case Type::enumeration:
		return lower(trim(value)) == lower(def);
	case Type::integer:
	case Type::real: {
		double a = 0;
		double b = 0;
		if (parse_double(value, a) && parse_double(def, b)) {
			return std::fabs(a - b) < 1e-9;
		}
		return trim(value) == def;
	}
	case Type::path:
	case Type::string:
		break;
	}
	return value == def;
}

std::string Mui_config_key::validate(const std::string& value) const {
	auto range_text = [this] {
		return std::string(Strings::RangeFrom()) + " " + number_text(minv) + " " + Strings::RangeTo() + " " + number_text(maxv)
			   + ".";
	};
	switch (type) {
	case Type::boolean: {
		const std::string v = lower(trim(value));
		return v == "yes" || v == "no" ? std::string() : std::string(Strings::YesOrNo());
	}
	case Type::enumeration: {
		if (choice_index(value) >= 0) {
			return {};
		}
		std::string list;
		for (const auto& c : choices) {
			if (!c.empty()) {
				list += (list.empty() ? "" : ", ") + c;
			}
		}
		return std::string(Strings::OneOf()) + " " + list + ".";
	}
	case Type::integer: {
		long v = 0;
		if (!parse_long(value, v)) {
			return Strings::WholeNumber();
		}
		if (has_range && (double(v) < minv || double(v) > maxv)) {
			return range_text();
		}
		return {};
	}
	case Type::real: {
		double v = 0;
		if (!parse_double(value, v)) {
			return Strings::Number();
		}
		if (has_range && (v < minv || v > maxv)) {
			return range_text();
		}
		return {};
	}
	case Type::path:
	case Type::string:
		break;
	}
	return {};
}

/*
 *  Mui_config_meta
 */

const Mui_config_meta& Mui_config_meta::get() {
	static Mui_config_meta meta;
	static bool            loaded = false;
	if (!loaded) {
		loaded = true;
		try {
			IExultDataSource ds(File_spec(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX)), EXULT_FLX_MUI_CONFIG_KEYS_TXT);
			if (ds.good() && ds.getSize() > 0) {
				meta.parse(std::string(reinterpret_cast<const char*>(ds.getPtr()), ds.getSize()));
			}
		} catch (const std::exception& e) {
			std::cerr << "Mui_config_meta: cannot read the config key table: " << e.what() << std::endl;
		}
	}
	return meta;
}

size_t Mui_config_meta::parse(const std::string& text) {
	rows.clear();
	std::istringstream in(text);
	std::string        line;
	int                num = 0;
	while (std::getline(in, line)) {
		num++;
		const std::string t = trim(line);
		if (t.empty() || t[0] == '#') {
			continue;
		}
		const auto f = split(t, '|', 6);
		if (f.size() != 6) {
			std::cerr << "mui_config_keys.txt:" << num << ": expected 6 fields" << std::endl;
			continue;
		}
		Mui_config_key k;
		k.path              = trim(f[0]);
		const std::string y = trim(f[1]);
		k.def               = trim(f[2]);
		const std::string d = trim(f[3]);
		const std::string a = trim(f[4]);
		k.description       = trim(f[5]);
		if (y == "bool") {
			k.type = Mui_config_key::Type::boolean;
		} else if (y == "enum") {
			k.type = Mui_config_key::Type::enumeration;
		} else if (y == "int") {
			k.type = Mui_config_key::Type::integer;
		} else if (y == "float") {
			k.type = Mui_config_key::Type::real;
		} else if (y == "path") {
			k.type = Mui_config_key::Type::path;
		} else if (y == "string") {
			k.type = Mui_config_key::Type::string;
		} else {
			std::cerr << "mui_config_keys.txt:" << num << ": unknown type " << y << std::endl;
			continue;
		}
		k.apply = a == "now" ? Mui_config_key::Apply::now : Mui_config_key::Apply::restart;
		if (k.type == Mui_config_key::Type::enumeration) {
			for (const auto& c : split(d, ',')) {
				k.choices.push_back(trim(c));
			}
		} else if (!d.empty()) {
			const size_t dots = d.find("..");
			if (dots != std::string::npos && parse_double(d.substr(0, dots), k.minv) && parse_double(d.substr(dots + 2), k.maxv)) {
				k.has_range = true;
			}
		}
		if (k.path.rfind("config/", 0) != 0) {
			std::cerr << "mui_config_keys.txt:" << num << ": path must start with config/" << std::endl;
			continue;
		}
		rows.push_back(std::move(k));
	}
	return rows.size();
}

bool Mui_config_meta::matches(const std::string& pattern, const std::string& path) {
	return match_from(pattern.c_str(), path.c_str());
}

const Mui_config_key* Mui_config_meta::find(const std::string& path) const {
	const Mui_config_key* best     = nullptr;
	size_t                best_len = 0;
	for (const auto& k : rows) {
		if (!k.is_pattern()) {
			if (k.path == path) {
				return &k;
			}
			continue;
		}
		if (matches(k.path, path)) {
			const size_t n = literal_length(k.path);
			if (!best || n > best_len) {
				best     = &k;
				best_len = n;
			}
		}
	}
	return best;
}
