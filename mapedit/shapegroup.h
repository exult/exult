/**
 ** A group of shapes/chunks that can be used as a 'palette'.
 **
 ** Written: 1/22/02 - JSF
 **/

#ifndef INCL_SHAPEGROUP
#define INCL_SHAPEGROUP 1

/*
Copyright (C) 2002-2025 The Exult Team

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

#include <string>
#include <vector>

class Shape_group_file;
class ODataSource;

/*
 *  A group of shape/chunk #'s:
 */
class Shape_group : std::vector<int> {    // Not public on purpose.
	std::string       name;               // What this group is called.
	Shape_group_file* file;               // Where this comes from.
	int               builtin;            // -1 if not builtin, 0-14 if
										  //  a Shape_info::Shape_class, 100-103
										  //  if Special_builtin.
	bool modified;

public:
	friend class Shape_group_file;
	Shape_group(const char* nm, Shape_group_file* f, int built = -1);
	~Shape_group() = default;

	bool is_builtin() const {
		return builtin != -1;
	}

	Shape_group_file* get_file() {
		return file;
	}

	const char* get_name() const {
		return name.c_str();
	}

	void set_name(char* nm) {
		name = nm;
	}

	int size() const {
		return std::vector<int>::size();
	}

	int& operator[](int i) {
		return std::vector<int>::operator[](i);
	}

	void del(int i);
	void swap(int i);    // Swap entries i and i+1.
	void add(int id);    // Add ID, checking for duplicate 1st.
	void write(ODataSource& out);

	void set_modified() {
		modified = true;
	}

	enum Special_builtin {
		first_group = 20,
		ammo_group  = first_group,
		armour_group,
		monsters_group,
		weapons_group,
		containers_group,
		mountain_top,
		animation_group,
		barge_group,
		body_group,
		explosion_group,
		field_group,
		npcflags_group,
		npcpaperdoll_group,
		sfx_group,
		content_rules_group,
		frameflags_group,
		framehps_group,
		framenames_group,
		frameusecode_group,
		objpaperdoll_group,
		warmth_group,
		light_group,
		last_builtin_group
	};
};

/*
 *  This class represents the file containing groups for a given shape
 *  or chunks file.  It is read from the 'patch' or 'static' directory,
 *  and saved to the 'patch' directory.
 */
class Shape_group_file {
	std::string               name;        // Base filename.
	std::vector<Shape_group*> groups;      // List of groups from the file.
	std::vector<Shape_group*> builtins;    // Builtin groups, created
	//  on demand.
	bool modified;    // Changed since last save.
public:
	friend class Shape_group;
	Shape_group_file(const char* nm);
	~Shape_group_file();

	int size() {
		return groups.size();
	}

	Shape_group* get(int i) {
		return groups[i];
	}

	void clear() {    // Clears list (but doesn't delete).
		groups.resize(0);
	}

	void add(Shape_group* grp) {    // Add a new group.
		groups.push_back(grp);
		modified = true;
	}

	void set(Shape_group* grp, int i) {
		groups[i] = grp;
		modified  = true;
	}

	void insert(Shape_group* grp, int i) {
		groups.insert(groups.begin() + i, grp);
		modified = true;
	}

	void set_modified() {
		modified = true;
	}

	void set_clean() {
		modified = false;
	}

	bool is_modified() {
		return modified;
	}

	int find(const char* nm);    // Find group with given name.
	// Remove and delete group.
	void         remove(int index, bool del = true);
	Shape_group* get_builtin(int menindex, const char* nm);
	void         write();    // Write out (to 'patch' directory).
};

#endif
