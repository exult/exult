/*
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

#ifndef INCL_EGG
#define INCL_EGG 1

class Egg_object;
class Animator;
class Monster_actor;
class Missile_launcher;

#include "ignore_unused_variable_warning.h"
#include "iregobjs.h"

using Egg_object_shared = std::shared_ptr<Egg_object>;

/*
 *  Here's a class for eggs and paths; i.e., objects that generally aren't
 *  visible.
 */
class Egglike_game_object : public Ireg_game_object {
public:
	Egglike_game_object(
			int shapenum, int framenum, unsigned int tilex, unsigned int tiley,
			unsigned int lft = 0)
			: Ireg_game_object(shapenum, framenum, tilex, tiley, lft) {}

	// Render.
	void paint() override;
	// Can this be clicked on?
	bool is_findable() override;
};

/*
 *  An "egg" is a special object that activates under certain
 *  circumstances.
 */
class Egg_object : public Egglike_game_object {
	static Game_object_shared editing;    // Egg being edited by ExultStudio.
protected:
	unsigned char  type;                   // One of the below types.
	unsigned char  probability;            // 1-100, chance of egg activating.
	unsigned char  criteria : 3;           // How it's activated.  See below.
	unsigned       distance : 6;           // Distance for activation (0-31).
	unsigned       flags    : 4;           // Formed from below flags.
	unsigned short data1, data2, data3;    // More data, dep. on type.
	TileRect       area;                   // Active area.
	unsigned char  solid_area;             // 1 if area is solid, 0 if outline.
	Animator*      animator;               // Controls animation.
	void           init_field(unsigned char ty);

public:
	friend class Button_egg;

	// Create egg from type and parameters
	static Egg_object_shared create_egg(
			bool animated, int shnum, int frnum, unsigned int tx,
			unsigned int ty, unsigned int tz, unsigned short itype,
			unsigned char prob, short data1, short data2, short data3,
			const char* str1 = nullptr);

	enum Egg_types {    // Types of eggs:
		monster  = 1,
		jukebox  = 2,
		soundsfx = 3,
		voice    = 4,
		usecode  = 5,
		missile  = 6,
		teleport = 7,
		weather  = 8,
		path     = 9,
		button   = 10,
		intermap = 11,
		// Our own:
		fire_field     = 128,
		sleep_field    = 129,
		poison_field   = 130,
		caltrops_field = 131,
		campfire_field = 132,
		mirror_object  = 133
	};

	enum Egg_flag_shifts {
		nocturnal  = 0,
		once       = 1,
		hatched    = 2,
		auto_reset = 3
	};

	bool is_hatched() const {
		return (flags & (1 << static_cast<int>(hatched))) != 0;
	}

	bool is_once() const {
		return (flags & (1 << static_cast<int>(once))) != 0;
	}

	bool is_nocturnal() const {
		return (flags & (1 << static_cast<int>(nocturnal))) != 0;
	}

	bool is_auto_reset() const {
		return (flags & (1 << static_cast<int>(auto_reset))) != 0;
	}

	enum Egg_criteria {
		cached_in         = 0,    // Activated when chunk read in?
		party_near        = 1,
		avatar_near       = 2,    // Avatar steps into area.
		avatar_far        = 3,    // Avatar steps outside area.
		avatar_footpad    = 4,    // Avatar must step on it.
		party_footpad     = 5,
		something_on      = 6,    // Something placed on/near it.
		external_criteria = 7     // Appears on Isle of Avatar.  Guessing
								  //   these set off all nearby.
	};

	static Egg_object_shared create_egg(
			const unsigned char* entry, int entlen, bool animated, int shnum,
			int frnum, unsigned int tx, unsigned int ty, unsigned int tz);
	// Create normal eggs.
	Egg_object(
			int shapenum, int framenum, unsigned int tilex, unsigned int tiley,
			unsigned int lft, unsigned short itype, unsigned char prob,
			short d1, short d2, short d3 = 0);
	// Ctor. for fields:
	Egg_object(
			int shapenum, int framenum, unsigned int tilex, unsigned int tiley,
			unsigned int lft, unsigned char ty);
	~Egg_object() override;
	virtual void set_area();    // Set up active area.

	int get_distance() const {
		return distance;
	}

	int get_criteria() const {
		return criteria;
	}

	int get_type() const {
		return type;
	}

	virtual const char* get_str1() const {
		return "";
	}

	virtual void set_str1(const char* s) {
		ignore_unused_variable_warning(s);
	}

	// Clone this egg with all its data
	Egg_object_shared clone_egg(int tilex, int tiley, int lift) const;

	// Can this be clicked on?
	bool         is_findable() override;
	virtual void set(int crit, int dist);
	// Can it be activated?
	virtual bool is_active(
			Game_object* obj, int tx, int ty, int tz, int from_tx, int from_ty);

	// This is pretty much the opposite to is_active it is used to test if
	// unhatch should be called largely because the avatar was in proximity and
	// moved out.
	virtual bool test_unhatch(
			Game_object* obj, int tx, int ty, int tz, int from_tx, int from_ty);

	// returns true by subclasses if they want unhatched to be called
	virtual bool can_unhatch() {
		return false;
	}

	TileRect get_area() const {    // Get active area.
		return area;
	}

	int is_solid_area() const {
		return solid_area;
	}

	void set_animator(Animator* a);
	void stop_animation();
	void paint() override;
	// Run usecode function.
	void activate(int event = 1) override;
	bool edit() override;            // Edit in ExultStudio.
	bool edit_basic_properties();    // Edit basic properties in ExultStudio.
	// Saved from ExultStudio.
	static void update_from_studio(unsigned char* data, int datalen);

	virtual void hatch_now(Game_object* obj, bool must) {
		ignore_unused_variable_warning(obj, must);
	}

	virtual bool unhatch_now(Game_object* obj, bool must) {
		ignore_unused_variable_warning(obj, must);
		return false;
	}

	virtual void hatch(Game_object* obj, bool must = false);
	virtual void unhatch(Game_object* obj, bool must = false);
	void         print_debug();
	static void  set_weather(
			 int weather, int len = 15, Game_object* egg = nullptr);
	// Move to new abs. location.
	void move(int newtx, int newty, int newlift, int newmap = -1) override;
	// Remove/delete this object.
	void remove_this(Game_object_shared* keep = nullptr) override;

	int is_egg() const override {    // An egg?
		return 1;
	}

	// Write out to IREG file.
	void write_ireg(ODataSource* out) override;
	// Get size of IREG. Returns -1 if can't write to buffer
	int get_ireg_size() override;

	virtual bool reset() {
		// being unloaded last change call unhatch_now
		if (is_hatched() && can_unhatch()) {
			unhatch_now(nullptr, true);
		}
		flags &= ~(1 << hatched);
		return true;
	}

	Egg_object* as_egg() override {
		return this;
	}

	const Egg_object* as_egg() const override {
		return this;
	}
};

/*
 *  Fields are activated like eggs.
 */

class Field_object : public Egg_object {
	bool field_effect(Actor* actor);    // Apply field.
public:
	Field_object(
			int shapenum, int framenum, unsigned int tilex, unsigned int tiley,
			unsigned int lft, unsigned char ty);
	void paint() override;
	// Run usecode function.
	void activate(int event = 1) override;
	void hatch(Game_object* obj, bool must = false) override;
	// Write out to IREG file.
	void write_ireg(ODataSource* out) override;
	// Get size of IREG. Returns -1 if can't write to buffer
	int get_ireg_size() override;

	bool reset() override {
		return false;
	}

	bool is_findable() override {
		return Ireg_game_object::is_findable();
	}

	bool edit() override {
		return Ireg_game_object::edit();
	}

	static void update_from_studio(unsigned char* data, int datalen) {
		Ireg_game_object::update_from_studio(data, datalen);
	}
};

/*
 *  Mirrors are handled like eggs.
 */

class Mirror_object : public Egg_object {
public:
	Mirror_object(
			int shapenum, int framenum, unsigned int tilex, unsigned int tiley,
			unsigned int lft);

	// Run usecode function.
	void activate(int event = 1) override;
	void hatch(Game_object* obj, bool must = false) override;

	// Can it be activated?
	bool is_active(
			Game_object* obj, int tx, int ty, int tz, int from_tx,
			int from_ty) override;

	void set_area() override;    // Set up active area.

	// Render.
	void paint() override;

	// Can this be clicked on?
	bool is_findable() override {
		return Ireg_game_object::is_findable();
	}

	void write_ireg(ODataSource* out) override;
	// Get size of IREG. Returns -1 if can't write to buffer
	int get_ireg_size() override;

	bool reset() override {
		return false;
	}

	bool edit() override {
		return Ireg_game_object::edit();
	}

	static void update_from_studio(unsigned char* data, int datalen) {
		Ireg_game_object::update_from_studio(data, datalen);
	}
};
#endif
