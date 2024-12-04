/*
 *  Copyright (C) 2000-2022  The Exult Team
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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "XMLEntity.h"

class Configuration {
public:
	Configuration()
			: xmltree(std::make_unique<XMLnode>("config")), rootname("config") {
	}

	Configuration(const std::string& fname, std::string_view root)
			: xmltree(std::make_unique<XMLnode>(root)), rootname(root) {
		if (!fname.empty()) {
			read_config_file(fname);
		}
	}

	bool read_config_file(
			const std::string& input_filename, std::string_view root = "");
	bool read_abs_config_file(
			std::string_view input_filename, std::string_view root = "");

	bool read_config_string(std::string_view s);

	void value(
			std::string_view key, std::string& ret,
			std::string_view defaultvalue = "") const;

	void value(
			std::string_view key, bool& ret, bool defaultvalue = false) const;

	void value(std::string_view key, int& ret, int defaultvalue = 0) const;

	bool key_exists(std::string_view key) const;

	void set(std::string_view key, std::string_view value, bool write_out);
	void set(std::string_view key, int value, bool write_out);

	void remove(std::string_view key, bool write_out);

	// Return a list of keys that are subsidiary to the supplied key
	std::vector<std::string> listkeys(
			std::string_view key, bool longformat = true) const;

	std::string   dump();    // Assembles a readable representation
	std::ostream& dump(std::ostream& o, std::string_view indentstr);

	void write_back();

	void clear(std::string_view new_root = "");

	using KeyType     = XMLnode::KeyType;
	using KeyTypeList = XMLnode::KeyTypeList;

	void getsubkeys(KeyTypeList& ktl, std::string_view basekey);

private:
	std::unique_ptr<XMLnode> xmltree;
	std::string              rootname;
	std::string              filename;
	bool                     is_file = false;
};

// Global Config
extern Configuration* config;

#endif
