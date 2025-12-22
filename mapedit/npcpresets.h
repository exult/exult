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

#ifndef INCL_NPCPRESETS
#define INCL_NPCPRESETS 1

#include <map>
#include <string>
#include <vector>

class Npc_preset_file;
class IDataSource;
class ODataSource;

/*
 *  A preset that stores NPC number for quick reference.
 */
class Npc_preset {
	std::string      name;    // Name of this preset.
	Npc_preset_file* file;    // Where this comes from.
	bool             modified;
	std::map<std::string, std::string>
			data;    // Key-value pairs (mainly npc_number)

public:
	friend class Npc_preset_file;
	Npc_preset(const char* nm, Npc_preset_file* f);
	~Npc_preset() = default;

	Npc_preset_file* get_file() {
		return file;
	}

	const char* get_name() const {
		return name.c_str();
	}

	void set_name(const char* nm) {
		name     = nm;
		modified = true;
	}

	void set_modified();

	bool is_modified() const {
		return modified;
	}

	void        set_value(const std::string& key, const std::string& value);
	std::string get_value(const std::string& key) const;
	bool        has_value(const std::string& key) const;

	void clear_data() {
		data.clear();
		modified = true;
	}

	const std::map<std::string, std::string>& get_data() const {
		return data;
	}

	void write(ODataSource& out);
	void read(IDataSource& in);
};

/*
 *  Manages NPC presets file.
 */
class Npc_preset_file {
	std::string              filename;
	std::vector<Npc_preset*> presets;

public:
	friend class Npc_preset;
	bool modified;

	Npc_preset_file(const char* nm = nullptr);
	~Npc_preset_file();

	const char* get_filename() const {
		return filename.c_str();
	}

	int size() const {
		return static_cast<int>(presets.size());
	}

	Npc_preset* get(int i) {
		return i >= 0 && i < static_cast<int>(presets.size()) ? presets[i]
															  : nullptr;
	}

	Npc_preset*             find(const char* nm);
	int                     find_index(const char* nm);
	Npc_preset*             create(const char* nm);
	void                    remove(Npc_preset* preset);
	bool                    write();
	bool                    read(const char* nm);
	static Npc_preset_file* read_file(const char* pathname);

	static const char* get_default_filename() {
		return "npcs.pre";
	}
};

/*
 *  Inline method implementations that need complete type.
 */
inline void Npc_preset::set_modified() {
	modified = true;
	if (file) {
		file->modified = true;
	}
}

#endif
