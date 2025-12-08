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

#ifndef GUMPINFO_H
#define GUMPINFO_H

#include <map>

class Gump_info {
	static std::map<int, Gump_info> gump_info_map;
	static bool                     any_modified;

	friend class Shapes_vga_file;

	bool container_from_patch;
	bool checkmark_from_patch;
	bool special_from_patch;

	bool container_modified;
	bool checkmark_modified;
	bool special_modified;

public:
	int  container_x;
	int  container_y;
	int  container_w;
	int  container_h;
	int  checkmark_x;
	int  checkmark_y;
	int  checkmark_shape;
	bool has_area;
	bool has_checkmark;
	bool is_checkmark;
	bool is_special;

	Gump_info();

	// Check if we need to write these sections
	bool is_container_dirty() const {
		return container_from_patch || container_modified;
	}

	bool is_checkmark_dirty() const {
		return checkmark_from_patch || checkmark_modified;
	}

	bool is_special_dirty() const {
		return special_from_patch || special_modified;
	}

	// Setters for patch flags
	void set_container_from_patch(bool tf) {
		container_from_patch = tf;
	}

	void set_checkmark_from_patch(bool tf) {
		checkmark_from_patch = tf;
	}

	void set_special_from_patch(bool tf) {
		special_from_patch = tf;
	}

	// Setters for modified flags
	void set_container_modified(bool v) {
		container_modified = v;
		if (v) {
			any_modified = true;
		}
	}

	void set_checkmark_modified(bool v) {
		checkmark_modified = v;
		if (v) {
			any_modified = true;
		}
	}

	void set_special_modified(bool v) {
		special_modified = v;
		if (v) {
			any_modified = true;
		}
	}

	static bool was_any_modified() {
		return any_modified;
	}

	static const Gump_info* get_gump_info(int shapenum);
	static Gump_info&       get_or_create_gump_info(int shapenum);
	static void             clear();
};

#endif