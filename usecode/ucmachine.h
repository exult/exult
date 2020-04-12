/*
 *  ucmachine.cc - Interpreter for usecode.
 *
 *  Copyright (C) 1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2013  The Exult Team
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

#ifndef UCMACHINE_H
#define UCMACHINE_H

#include <iosfwd>

class Game_window;
class Usecode_machine;
class Conversation;
class Keyring;
class Game_object;
class Actor;
class Usecode_value;
class Usecode_class_symbol;
class Tile_coord;

#include "exceptions.h"
#include "singles.h"
#include "exult_constants.h"

/*
 *  Here's our virtual machine for running usecode.  The actual internals
 *  are in Usecode_internal.
 */
class Usecode_machine : public Game_singletons {
	UNREPLICATABLE_CLASS(Usecode_machine)
protected:
	unsigned char gflags[c_last_gflag + 1]; // Global flags.
	Keyring *keyring = nullptr;
	Conversation *conv = nullptr;     // Handles conversations
public:
	friend class Usecode_script;
	// Create Usecode_internal.
	static Usecode_machine *create();
	Usecode_machine();
	virtual ~Usecode_machine();
	// Read in usecode functions.
	virtual void read_usecode(std::istream &file, bool patch = false) = 0;
	// Possible events:
	enum Usecode_events {
	    npc_proximity = 0,
	    double_click = 1,
	    internal_exec = 2,  // Internal call via intr. 1 or 2.
	    egg_proximity = 3,
	    weapon = 4,     // From weapons.dat.
	    readied = 5,        // Wear an item.
	    unreadied = 6,      // Removed an item.
	    died = 7,       // In SI only, I think.
	    chat = 9    // When a NPC wants to talk to you in SI
	};
	enum Global_flag_names {
	    did_first_scene = 0x3b, // Went through 1st scene with Iolo.
	    have_trinsic_password = 0x3d,
	    found_stable_key = 0x3c,
	    left_trinsic = 0x57,
	    avatar_is_thief = 0x2eb
	};
	int get_global_flag(int i) { // Get/set ith flag.
		return gflags[i];
	}
	void set_global_flag(int i, int val = 1) {
		gflags[i] = (val == 1);
	}
	// Start speech, or show text.
	virtual void do_speech(int num) = 0;
	virtual bool in_usecode() = 0;  // Currently in a usecode function?
	virtual bool in_usecode_for(Game_object *item, Usecode_events event) = 0;
	Keyring *getKeyring() const {
		return keyring;
	}
	// Call desired function.
	virtual int call_usecode(int id, Game_object *item,
	                         Usecode_events event) = 0;
	virtual bool call_method(Usecode_value *inst, int id,
	                         Game_object *item) = 0;
	virtual int find_function(const char *nm, bool noerr = false) = 0;
	virtual const char *find_function_name(int funcid) = 0;
	virtual Usecode_class_symbol *get_class(int n) = 0;
	virtual Usecode_class_symbol *get_class(const char *nm) = 0;
	virtual int get_shape_fun(int n) = 0;
	virtual void write() = 0;   // Write out 'gamedat/usecode.dat'.
	virtual void read() = 0;    // Read in 'gamedat/usecode.dat'.

	void init_conversation();
	int get_num_faces_on_screen() const;

	// intercept the next click_on_item intrinsic
	virtual void intercept_click_on_item(Game_object *obj) = 0;
	virtual Game_object *get_intercept_click_on_item() const = 0;
	virtual void intercept_click_on_tile(Tile_coord *t) = 0;
	virtual Tile_coord *get_intercept_click_on_tile() const = 0;
	virtual void save_intercept(Game_object *&obj, Tile_coord *&t) = 0;
	virtual void restore_intercept(Game_object *obj, Tile_coord *t) = 0;

	virtual bool function_exists(int funcid) = 0;
};

#endif  /* INCL_USECODE */
