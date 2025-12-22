/*
Copyright (C) 2025 The Exult Team

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

#ifndef INCL_SHAPEPRESETS
#define INCL_SHAPEPRESETS 1

#include <map>
#include <string>
#include <vector>

class Shape_preset_file;
class IDataSource;
class ODataSource;

/*
 *  A preset that stores complete shape configuration data.
 */
class Shape_preset {
	std::string        name;    // Name of this preset.
	Shape_preset_file* file;    // Where this comes from.
	bool               modified;
	std::map<std::string, std::string>
			data;    // Key-value pairs for all shape data

public:
	friend class Shape_preset_file;
	Shape_preset(const char* nm, Shape_preset_file* f);
	~Shape_preset() = default;

	Shape_preset_file* get_file() {
		return file;
	}

	const char* get_name() const {
		return name.c_str();
	}

	void set_name(const char* nm) {
		name = nm;
	}

	// Get/set preset data
	void        set_value(const std::string& key, const std::string& value);
	std::string get_value(const std::string& key) const;
	bool        has_value(const std::string& key) const;

	// Data access
	const std::map<std::string, std::string>& get_data() const {
		return data;
	}

	void clear_data() {
		data.clear();
	}

	void write(ODataSource& out);
	void read(IDataSource& in);

	void set_modified() {
		modified = true;
	}

	bool is_modified() const {
		return modified;
	}
};

/*
 *  Manages a file of shape presets.
 */
class Shape_preset_file {
	std::string                filename;
	std::vector<Shape_preset*> presets;

public:
	bool modified;    // Changes not saved yet.

	Shape_preset_file(const char* nm = "");
	~Shape_preset_file();

	const char* get_name() const {
		return filename.c_str();
	}

	int size() const {
		return presets.size();
	}

	Shape_preset* get(int i) {
		return i >= 0 && i < static_cast<int>(presets.size()) ? presets[i]
															  : nullptr;
	}

	Shape_preset* find(const char* nm);
	int           find_index(const char* nm);
	Shape_preset* create(const char* nm);
	void          remove(Shape_preset* preset);
	bool          write();                 // Write out to file.
	bool          read(const char* nm);    // Read in from file nm.

	static Shape_preset_file* read_file(const char* pathname);

	static const char* get_default_filename() {
		return "shapes.vga.pre";
	}
};

#endif
