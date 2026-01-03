/*
 *  contain.h - Container objects.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
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

#ifndef INCL_CONTAIN
#define INCL_CONTAIN 1

#include "ignore_unused_variable_warning.h"
#include "iregobjs.h"

/*
 *  A container object:
 */
class Container_game_object : public Ireg_game_object {
	int  volume_used = 0;    // Amount of volume occupied.
	char resistance  = 0;    // Resistance to attack.
protected:
	Object_list objects = nullptr;    // ->first object.
	int         get_max_volume() const;
	int         gumpX    = 0;
	int         gumpY    = 0;
	bool        gumpInit = false;

public:
	void setGumpXY(int x, int y) {
		gumpX    = x;
		gumpY    = y;
		gumpInit = true;
	}

	int getGumpX() {
		return gumpX;
	}

	int getGumpY() {
		return gumpY;
	}

	bool validGumpXY() {
		return gumpInit;
	}

	Container_game_object(
			int shapenum, int framenum, unsigned int tilex, unsigned int tiley,
			unsigned int lft, char res = 0)
			: Ireg_game_object(shapenum, framenum, tilex, tiley, lft),
			  resistance(res), objects(nullptr) {}

	Container_game_object() = default;

	Object_list& get_objects() {
		return objects;
	}

	// For when an obj's quantity changes:
	void modify_volume_used(int delta) {
		volume_used += delta;
	}

	// Room for this object?
	bool has_room(Game_object* obj) const {
		return get_max_volume() <= 0
			   || obj->get_volume() + volume_used <= get_max_volume();
	}

	// Remove an object.
	virtual void remove(Game_object* obj);
	// Add an object.
	bool add(
			Game_object* obj, bool dont_check = false, bool combine = false,
			bool noset = false) override;
	// Change member shape.
	virtual void change_member_shape(Game_object* obj, int newshape);

	// Find object's spot.
	virtual int find_readied(Game_object* obj) {
		ignore_unused_variable_warning(obj);
		return -1;
	}

	virtual Game_object* get_readied(int index) const {
		ignore_unused_variable_warning(index);
		return nullptr;
	}

	virtual void call_readied_usecode(
			int index, Game_object* obj, int eventid) {
		ignore_unused_variable_warning(index, obj, eventid);
	}

	// Add/remove quantities of objs.
	int add_quantity(
			int delta, int shapenum, int qual = c_any_qual,
			int framenum = c_any_framenum, bool dontcreate = false,
			bool temporary = false) override;
	int create_quantity(
			int delta, int shnum, int qual, int frnum,
			bool temporary = false) override;
	int remove_quantity(
			int delta, int shapenum, int qual, int framenum) override;
	Game_object* find_item(int shapenum, int qual, int framenum) override;
	bool         show_gump(int event = 1);
	// Run usecode function.
	void activate(int event = 1) override;
	bool edit() override;            // Edit in ExultStudio.
	bool edit_basic_properties();    // Edit basic properties in ExultStudio.
	// Saved from ExultStudio.
	static void update_from_studio(unsigned char* data, int datalen);
	int         get_weight() override;
	// Drop another onto this.
	bool drop(Game_object* obj) override;
	// Count contained objs.
	int count_objects(
			int shapenum, int qual = c_any_qual,
			int framenum = c_any_framenum) override;
	// Get contained objs.
	int get_objects(
			Game_object_vector& vec, int shapenum, int qual,
			int framenum) override;
	void set_flag_recursively(int flag) override;
	// Write out to IREG file.
	void write_ireg(ODataSource* out) override;
	// Get size of IREG. Returns -1 if can't write to buffer
	int get_ireg_size() override;
	// Write contents in IREG format.
	virtual void write_contents(ODataSource* out);

	int get_obj_hp() const override {
		return resistance;
	}

	void set_obj_hp(int hp) override {
		resistance = static_cast<char>(hp);
	}

	Game_object* find_weapon_ammo(
			int weapon, int needed = 1, bool recursive = false) override;

	bool extract_contents(Container_game_object* targ);

	void delete_contents() override;

	void remove_this(Game_object_shared* keep = nullptr) override;

	Container_game_object* as_container() override {
		return this;
	}

	const Container_game_object* as_container() const override {
		return this;
	}
};

#endif
