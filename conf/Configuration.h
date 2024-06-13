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

#ifndef _Configuration_h_
#define _Configuration_h_

#include "XMLEntity.h"

class Configuration {
public:
	Configuration() : xmltree(new XMLnode("config")), rootname("config") {}

	Configuration(const std::string& fname, const std::string& root)
			: xmltree(new XMLnode(root)), rootname(root) {
		if (!fname.empty()) {
			read_config_file(fname);
		}
	}

	~Configuration() {
		delete xmltree;
	}

	bool read_config_file(
			const std::string& input_filename,
			const std::string& root = std::string());
	bool read_abs_config_file(
			const std::string& input_filename,
			const std::string& root = std::string());

	bool read_config_string(const std::string&);

	void value(
			const std::string& key, std::string& ret,
			const std::string& defaultvalue) const;
	void value(
			const std::string& key, bool& ret, bool defaultvalue = false) const;
	void value(const std::string& key, int& ret, int defaultvalue = 0) const;

	void value(
			const std::string& key, std::string& ret,
			const char* defaultvalue = "") const {
		value(key, ret, std::string(defaultvalue));
	}

	void value(const char* key, std::string& ret, const char* defaultvalue = "")
			const {
		value(std::string(key), ret, defaultvalue);
	}

	void value(const char* key, bool& ret, bool defaultvalue = false) const {
		value(std::string(key), ret, defaultvalue);
	}

	void value(const char* key, int& ret, int defaultvalue = 0) const {
		value(std::string(key), ret, defaultvalue);
	}

	bool key_exists(const std::string& key) const;

	void set(const std::string& key, const std::string& value, bool write_out);
	void set(const char* key, const char* value, bool write_out);
	void set(const char* key, const std::string& value, bool write_out);
	void set(const char* key, int, bool write_out);
	void set(const std::string& key, int, bool write_out);

	void remove(const std::string& key, bool write_out);

	// Return a list of keys that are subsidiary to the supplied key
	std::vector<std::string> listkeys(
			const std::string& key, bool longformat = true);
	std::vector<std::string> listkeys(const char* key, bool longformat = true);

	std::string   dump();    // Assembles a readable representation
	std::ostream& dump(std::ostream& o, const std::string& indentstr);

	void write_back();

	void clear(const std::string& new_root = std::string());

	using KeyType     = XMLnode::KeyType;
	using KeyTypeList = XMLnode::KeyTypeList;

	void getsubkeys(KeyTypeList& ktl, const std::string& basekey);

private:
	XMLnode*    xmltree;
	std::string rootname;
	std::string filename;
	bool        is_file = false;
};

// Global Config
extern Configuration* config;

#endif
