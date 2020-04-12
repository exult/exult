/*
 *  Schedule.h - Schedules for characters.
 *
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

#ifndef SCHEDULE_H
#define SCHEDULE_H  1

#include "tiles.h"
#include "singles.h"
#include <memory>
#include <vector>
#include "ignore_unused_variable_warning.h"

#ifdef _WIN32
#include <windows.h>
#define Rectangle RECTX
#endif

class Game_object;
class Actor;
class Rectangle;
class Actor_action;
class Usecode_value;

using Game_object_weak = std::weak_ptr<Game_object>;
using Game_object_vector = std::vector<Game_object *>;

/*
 *  A Schedule controls the NPC it is assigned to.
 */
class Schedule : public Game_singletons {
protected:
	Actor *npc;         // Who this controls.
	Tile_coord blocked;     // Tile where actor was blocked.
	Tile_coord start_pos;	// When schedule created.
	short prev_type;        // Actor's previous schedule.
	int street_maintenance_failures;// # times failed to find path.
	long street_maintenance_time;   // Time (msecs) when last tried.
public:
	Schedule(Actor *n);
	virtual ~Schedule() = default;
	int get_prev_type() const {
		return prev_type;
	}
	void set_blocked(Tile_coord const &b) {
		blocked = b;
	}
	Tile_coord get_start_pos() const {
		return start_pos;
	}
	enum Schedule_types {
	    combat = 0, horiz_pace = 1,
	    vert_pace = 2,  talk = 3,
	    dance = 4,  eat = 5,
	    farm = 6,   tend_shop = 7,
	    miner = 8,  hound = 9,
	    stand = 10, loiter = 11,
	    wander = 12,    blacksmith = 13,
	    sleep = 14, wait = 15,
	    sit = 16,   graze = 17,
	    bake = 18,  sew = 19,
	    shy = 20,   lab = 21,
	    thief = 22, waiter = 23,
	    special = 24,   kid_games = 25,
	    eat_at_inn = 26, duel = 27,
	    preach = 28,    patrol = 29,
	    desk_work = 30, follow_avatar = 31,
	    // Our own:
	    walk_to_schedule = 32,
	    street_maintenance = 33,
	    arrest_avatar = 34,
	    first_scripted_schedule = 0x80
	};
	// Set actor to walk somewhere, then
	//   do something.
	static void set_action_sequence(Actor *actor, Tile_coord const &dest,
	                                Actor_action *when_there, bool from_off_screen = false,
	                                int delay = 0);
	static Game_object *set_procure_item_action(Actor *actor, Game_object *obj,
	        int dist, int shnum, int frnum);
	static bool set_pickup_item_action(Actor *actor, Game_object *obj,
	                                   int delay);
	static Game_object *find_nearest(Actor *actor, Game_object_vector &nearby);
	int try_street_maintenance();   // Handle street-lamps, shutters.
	virtual void now_what() = 0;    // Npc calls this when it's done
	//   with its last task.
	virtual void im_dormant();  // Npc calls this when it goes from
	//  being active to dormant.
	virtual void ending(int newtype) { // Switching to another schedule.
		ignore_unused_variable_warning(newtype);
	}
	virtual void set_weapon(bool removed = false) { // Set weapon info.
		ignore_unused_variable_warning(removed);
	}
	// Set where to sleep.
	virtual void set_bed(Game_object *b) {
		ignore_unused_variable_warning(b);
	}
	// For Usecode intrinsic.
	virtual int get_actual_type(Actor *npc) const;
	// Look for foes.
	bool seek_foes();
	bool try_proximity_usecode(int odds);
};

/*
 *	A schedule that creates objects that need to be cleaned up after.
 */
class Schedule_with_objects : public Schedule {
	std::vector<Game_object_weak> created;	// Items we created.
	Game_object_weak current_item;		// One we're using/walking to.
protected:
    Game_object *get_current_item() {
	  return current_item.lock().get();
	}
	void set_current_item(Game_object *obj);
    int items_in_hand; 	  	// # NPC's desk items.
	void cleanup();				// Remove items we created.
public:
	Schedule_with_objects(Actor *n) : Schedule(n), items_in_hand(0) {
	}
	~Schedule_with_objects() override;
	void add_object(Game_object *obj);
	// Find desk or waiter items.
	virtual int find_items(Game_object_vector& vec, int dist) = 0;
	bool walk_to_random_item(int dist = 16);
	bool drop_item(Game_object *to_drop, Game_object *table);
};

/*
 *  Schedule is implemented as Usecode functions.
 */
class Scripted_schedule : public Schedule {
	Usecode_value *inst;        // Usecode schedule instance.
	// Usecode function #'s:
	int now_what_id, im_dormant_id, ending_id, set_weapon_id, set_bed_id;
	void run(int id);
public:
	Scripted_schedule(Actor *n, int type);
	~Scripted_schedule() override;
	void now_what() override {
		run(now_what_id);
	}
	void im_dormant() override {
		run(im_dormant_id);
	}
	void ending(int newtype) override {
		ignore_unused_variable_warning(newtype);
		run(ending_id);
	}
	void set_weapon(bool removed = false) override {
		ignore_unused_variable_warning(removed);
		run(set_weapon_id);
	}
	void set_bed(Game_object *b) override {
		ignore_unused_variable_warning(b);
		run(set_bed_id);
	}
};

/*
 *  Street maintenance (turn lamps on/off).
 */
class Street_maintenance_schedule : public Schedule {
	Game_object_weak obj;       // Lamp/shutters.
	int shapenum, framenum;     // Save original shapenum.
	Actor_action *paction;      // Path to follow to get there.
	Tile_coord oldloc;
public:
	Street_maintenance_schedule(Actor *n, Actor_action *p, Game_object *o);
	void now_what() override;
	// For Usecode intrinsic.
	int get_actual_type(Actor *npc) const override;
	void ending(int newtype) override;
};

/*
 *  For following the Avatar (by party members):
 */
class Follow_avatar_schedule : public Schedule {
	unsigned long next_path_time;   // Next time we're allowed to use
	//   pathfinding to follow leader.
public:
	Follow_avatar_schedule(Actor *n) : Schedule(n), next_path_time(0)
	{  }
	void now_what() override;    // Now what should NPC do?
};

/*
 *  A 'do nothing' schedule.
 */
class Wait_schedule : public Schedule {
public:
	Wait_schedule(Actor *n) : Schedule(n)
	{  }
	void now_what() override;    // Now what should NPC do?
};

/*
 *  A schedule for pacing between two obstacles:
 */
class Pace_schedule : public Schedule {
	char which;     // 0 for north-south, 1 for east-west
	Tile_coord loc; // The starting position of the schedule
	int phase;      // Current phase
public:
	Pace_schedule(Actor *n, char dir, Tile_coord const &pos)
		: Schedule(n), which(dir), loc(pos), phase(0)
	{  }
	// Create common schedules:
	static Pace_schedule *create_horiz(Actor *n);
	static Pace_schedule *create_vert(Actor *n);
	static void pace(Actor *npc, char &which, int &phase, Tile_coord &blocked, int delay);
	void now_what() override;    // Now what should NPC do?
};

/*
 *  A schedule for eating at an inn.
 */
class Eat_at_inn_schedule : public Schedule {
	bool sitting_at_chair;
public:
	Eat_at_inn_schedule(Actor *n) : Schedule(n), sitting_at_chair(false)
	{  }
	void now_what() override;    // Now what should NPC do?
	void ending(int new_type) override; // Switching to another schedule
	void im_dormant() override;  // Just went dormant.
};

/*
 *  A schedule for preaching.
 */
class Preach_schedule : public Schedule {
	enum {
	    find_podium,
	    at_podium,
	    exhort,
	    visit,
	    talk_member,
	    find_icon,
	    pray
	} state;
public:
	Preach_schedule(Actor *n) : Schedule(n), state(find_podium)
	{  }
	void now_what() override;    // Now what should NPC do?
};

/*
 *  A schedule for patrolling along 'path' objects.
 */
class Patrol_schedule : public Schedule {
	enum {PATH_SHAPE = 607};
	static int num_path_eggs;
	std::vector<Game_object *> paths; // Each 'path' object.
	int pathnum;            // # of next we're heading towards.
	int dir;                // 1 or -1;
	int state;              // The patrol state.
	Tile_coord center;      // For 'loiter' and 'pace' path eggs.
	char whichdir;          // For 'pace' path eggs.
	int phase;              // For 'pace' path eggs.
	int pace_count;         // For 'pace' path eggs.
	Game_object_weak hammer;    // For 'hammer' path eggs.
	Game_object_weak book;      // For 'read' path eggs.
	bool seek_combat;       // The NPC should seek enemies while patrolling.
	bool forever;           // If should keep executing last path egg.
public:
	Patrol_schedule(Actor *n);
	void now_what() override;    // Now what should NPC do?
	void ending(int new_type) override; // Switching to another schedule
};

/*
 *  Talk to avatar.
 */
class Talk_schedule : public Schedule {
	int firstbark, lastbark;
	int eventid;
protected:
	int phase;          // 0=walk to Av., 1=talk, 2=done.
	Talk_schedule(Actor *n, int fb, int lb, int eid);
public:
	Talk_schedule(Actor *n);
	void now_what() override;    // Now what should NPC do?
};

/*
 *  Arrest avatar.
 */
class Arrest_avatar_schedule : public Talk_schedule {
public:
	Arrest_avatar_schedule(Actor *n);
	// For Usecode intrinsic.
	int get_actual_type(Actor *npc) const override;
	void ending(int newtype) override;// Switching to another schedule.
};

/*
 *  Loiter within a rectangle.
 */
class Loiter_schedule : public Schedule {
protected:
	Tile_coord center;      // Center of rectangle.
	int dist;           // Distance in tiles to roam in each
	//   dir.
public:
	Loiter_schedule(Actor *n, int d = 12);
	void now_what() override;    // Now what should NPC do?
};

/*
 *  Graze within a rectangle.
 */
class Graze_schedule : public Loiter_schedule {
protected:
	int phase;
public:
	Graze_schedule(Actor *n, int d = 12) : Loiter_schedule(n, d), phase(0) {  }
	void now_what() override;    // Now what should NPC do?
};

/*
 *  Kid games.
 */
class Kid_games_schedule : public Loiter_schedule {
	std::vector<Actor *> kids;           // Other kids playing.
public:
	Kid_games_schedule(Actor *n) : Loiter_schedule(n, 10)
	{  }
	void now_what() override;    // Now what should NPC do?
};

/*
 *  Dance.
 */
class Dance_schedule : public Loiter_schedule {
public:
	Dance_schedule(Actor *n) : Loiter_schedule(n, 4)
	{  }
	void now_what() override;    // Now what should NPC do?
};

/*
 *  Miner/farmer:
 */
class Tool_schedule : public Loiter_schedule {
protected:
	int toolshape;          // Pick/scythe shape.
	Game_object_weak tool;
	void get_tool();
public:
	Tool_schedule(Actor *n, int shnum) : Loiter_schedule(n, 12),
		toolshape(shnum)
	{  }
	void now_what() override = 0;    // Now what should NPC do?
	void ending(int newtype) override;// Switching to another schedule.
};

/*
 *  Farmer.
 */
class Farmer_schedule : public Tool_schedule {
	Game_object_weak crop;
	int grow_cnt;
	enum {
	    start,
	    find_crop,
	    attack_crop,
	    crop_attacked,
	    wander
	} state;
public:
	Farmer_schedule(Actor *n) : Tool_schedule(n, 618),
		grow_cnt(0), state(start)
	{  }
	void now_what() override;    // Now what should NPC do?
};

/*
 *  Miner.
 */
class Miner_schedule : public Tool_schedule {
	Game_object_weak ore;
	enum {
	    find_ore,
	    attack_ore,
	    ore_attacked,
	    wander
	} state;
public:
	Miner_schedule(Actor *n) : Tool_schedule(n, 624), state(find_ore)
	{  }
	void now_what() override;    // Now what should NPC do?
};

/*
 *  Hound the Avatar.
 */
class Hound_schedule : public Schedule {
public:
	Hound_schedule(Actor *n) : Schedule(n)
	{  }
	void now_what() override;    // Now what should NPC do?
};

/*
 *  Wander all over the place, using pathfinding.
 */
class Wander_schedule : public Loiter_schedule {
public:
	Wander_schedule(Actor *n) : Loiter_schedule(n, 128)
	{  }
	void now_what() override;    // Now what should NPC do?
};

/*
 *  Sleep in a  bed.
 */
class Sleep_schedule : public Schedule {
	Tile_coord floorloc;        // Where NPC was standing before.
	Game_object_weak bed;       // Bed being slept on, or 0.
	int state;
	int spread0, spread1;       // Range of bedspread frames.
	bool for_nap_time;
public:
	Sleep_schedule(Actor *n);
	void now_what() override;    // Now what should NPC do?
	void ending(int new_type) override;// Switching to another schedule.
	// Set where to sleep.
	void set_bed(Game_object *b) override;
	void im_dormant() override;  // Just went dormant.
	static bool is_bed_occupied(Game_object *bed, Actor *npc);
};

/*
 *  Sit in a chair.
 */
class Sit_schedule : public Schedule {
	Game_object_weak chair;     // What to sit in.
	bool sat;           // True if we already sat down.
	bool did_barge_usecode;     // So we only call it once.
public:
	Sit_schedule(Actor *n, Game_object *ch = nullptr);
	void now_what() override;    // Now what should NPC do?
	void im_dormant() override;  // Just went dormant.
	static bool is_occupied(Game_object *chairobj, Actor *actor);
	static bool set_action(Actor *actor, Game_object *chairobj = nullptr,
	                       int delay = 0, Game_object **chair_found = nullptr);
};

/*
 *  Desk work - Just sit in front of desk.
 */
class Desk_schedule : public Schedule_with_objects {
	Game_object_weak chair;     // What to sit in.
	Game_object_weak desk, table;
	std::vector<Game_object_weak> tables;	// Other tables to work at.
	enum {
	    desk_setup,
	    sit_at_desk,
	    get_desk_item,
	    picked_up_item,
	    work_at_table
	} state;
	int find_items(Game_object_vector& vec, int dist) override;
	void find_tables(int shapenum);
	bool walk_to_table();
public:
	Desk_schedule(Actor *n);
	void now_what() override;    // Now what should NPC do?
	void ending(int new_type) override;// Switching to another schedule.
	void im_dormant() override;  // Just went dormant.
};

/*
 *  Shy away from Avatar.
 */
class Shy_schedule : public Schedule {
public:
	Shy_schedule(Actor *n) : Schedule(n)
	{  }
	void now_what() override;    // Now what should NPC do?
};

/*
 *  Lab work.
 */
class Lab_schedule : public Schedule {
	std::vector<Game_object_weak> tables;
	Game_object_weak chair;     // Chair to sit in.
	Game_object_weak book;      // Book to read.
	Game_object_weak cauldron;
	Tile_coord spot_on_table;
	enum {
	    start,
	    walk_to_cauldron,
	    use_cauldron,
	    sit_down,
	    read_book,
	    stand_up,
	    walk_to_table,
	    use_potion
	} state;
	void init();
public:
	Lab_schedule(Actor *n);
	void now_what() override;    // Now what should NPC do?
};

/*
 *  Be a thief.
 */
class Thief_schedule : public Schedule {
	unsigned long next_steal_time;  // Next time we can try to steal.
	void steal(Actor *from);
public:
	Thief_schedule(Actor *n) : Schedule(n), next_steal_time(0)
	{  }
	void now_what() override;
};

/*
 *  Wait tables.
 */
class Waiter_schedule : public Schedule_with_objects {
	Tile_coord startpos;        // Starting position.
	Actor *customer;        // Current customer.
	Game_object_weak prep_table;    // Table we're working at.
	bool cooking;
	std::vector<Actor *> customers;  // List of customers.
	std::vector<Actor *> customers_ordered;  // Taken orders from these.
	std::vector<Game_object_weak > prep_tables; // Prep. tables.
	std::vector<Game_object_weak > counters;    // Places to hang out.
	std::vector<Game_object_weak > eating_tables; // Tables with chairs around them.
	std::vector<Game_object_weak > unattended_plates;
	enum {
	    waiter_setup,
	    get_customer,
	    get_order,
	    took_order,
	    give_plate,
	    prep_food,
	    bring_food,
	    serve_food,
	    served_food,
	    wait_at_counter,
	    get_waiter_item,
	    picked_up_item,
	    walk_to_cleanup_food,
	    cleanup_food
	} state;
	int find_items(Game_object_vector& vec, int dist) override;
	bool find_unattended_plate();
	bool find_customer();
	void find_tables(int shapenum, int dist, bool is_prep = false);
	void find_prep_tables();
	bool walk_to_customer(int min_delay = 0);
	bool walk_to_work_spot(bool counter);
	bool walk_to_prep() {
		return walk_to_work_spot(false);
	}
	bool walk_to_counter() {
		return walk_to_work_spot(true);
	}
	Game_object *create_customer_plate();
	Game_object *find_serving_spot(Tile_coord &spot);
public:
	Waiter_schedule(Actor *n);
	void now_what() override;    // Now what should NPC do?
	void ending(int new_type) override;// Switching to another schedule.
};

/*
 *  Sew/weave schedule.
 */
class Sew_schedule : public Schedule {
	Game_object_weak bale;      // Bale of wool.
	Game_object_weak spinwheel;
	Game_object_weak chair;     // In front of spinning wheel.
	Game_object_weak spindle;       // Spindle of thread.
	Game_object_weak loom;
	Game_object_weak cloth;
	Game_object_weak work_table, wares_table;
	int sew_clothes_cnt;
	enum {
	    get_wool,
	    sit_at_wheel,
	    spin_wool,
	    get_thread,
	    weave_cloth,
	    get_cloth,
	    to_work_table,
	    set_to_sew,
	    sew_clothes,
	    get_clothes,
	    display_clothes,
	    done
	} state;
public:
	Sew_schedule(Actor *n);
	void now_what() override;    // Now what should NPC do?
	void ending(int new_type) override;// Switching to another schedule.
};

/*
 *  Bake schedule
 */
class Bake_schedule : public Schedule {
	Game_object_weak oven;
	Game_object_weak worktable;
	Game_object_weak displaytable;
	Game_object_weak flourbag;
	Game_object_weak dough;
	Game_object_weak dough_in_oven;
	bool clearing;
	enum {
	    find_leftovers,     // Look for misplaced dough already made by this schedule
	    to_flour,           // Looks for flourbag and walks to it if found
	    get_flour,          // Bend over flourbag and change the frame to zero if nonzero
	    to_table,           // Walk over to worktable and create flour
	    make_dough,         // Changes flour to flat dough then dough ball
	    remove_from_oven,   // Changes dough in oven to food %7 and picks it up
	    display_wares,      // Walk to displaytable. Put food on it. If table full, go to
	    // clear_display which eventualy comes back here to place food
	    clear_display,      // Mark food for deletion by remove_food
	    remove_food,        // Delete food on display table one by one with a slight delay
	    get_dough,          // Walk to work table and pick up dough
	    put_in_oven         // Walk to oven and put dough on in.
	} state;
public:
	Bake_schedule(Actor *n);
	void now_what() override;
	void ending(int new_type) override;
};

/*
 *  Blacksmith schedule
 */
class Forge_schedule : public Schedule {
	Game_object_weak tongs;
	Game_object_weak hammer;
	Game_object_weak blank;
	Game_object_weak firepit;
	Game_object_weak anvil;
	Game_object_weak trough;
	Game_object_weak bellows;
	enum {
	    put_sword_on_firepit,
	    use_bellows,
	    get_tongs,
	    sword_on_anvil,
	    get_hammer,
	    use_hammer,
	    walk_to_trough,
	    fill_trough,
	    get_tongs2,
	    use_trough,
	    done
	} state;
public:
	Forge_schedule(Actor *n);
	void now_what() override;    // Now what should NPC do?
	void ending(int new_type) override; // Switching to another schedule
};

/*
 *  Eat without a server
 */
class Eat_schedule : public Schedule {
	Game_object_weak plate;
	enum {
	    eat,        // eat food and say food barks
	    find_plate, // make sure there is a plate, create one if not
	    serve_food  // put food on the plate
	} state;
public:
	Eat_schedule(Actor *n);
	void now_what() override;    // Now what should NPC do?
	void ending(int new_type) override; // Switching to another schedule
	void im_dormant() override;  // Just went dormant.
};

/*
 *  Walk to the destination for a new schedule.
 */
class Walk_to_schedule : public Schedule {
	Tile_coord dest;        // Where we're going.
	int first_delay;        // Starting delay (1/1000's sec.)
	int new_schedule;       // Schedule to set when we get there.
	int retries;            // # failures at finding path.
	int legs;           // # times restarted walk.
	// Set to walk off screen.
	void walk_off_screen(Rectangle &screen, Tile_coord &goal);
public:
	Walk_to_schedule(Actor *n, Tile_coord const &d, int new_sched,
	                 int delay = -1);
	void now_what() override;    // Now what should NPC do?
	void im_dormant() override;  // Just went dormant.
	// For Usecode intrinsic.
	int get_actual_type(Actor *npc) const override;
};

/*
 *  An NPC schedule change:
 */
class Schedule_change {
	static std::vector<std::string> script_names; // For Scripted_schedule's.
	unsigned char time = 0;     // Time*3hours when this takes effect.
	unsigned char type = 0;     // Schedule_type value.
	unsigned char days = 0x7f;  // A bit for each day (0-6).  We don't
	//   yet use this.
	Tile_coord pos;         // Location.
public:
	static void clear();
	static std::vector<std::string> &get_script_names() {
		return script_names;
	}
	void set4(const unsigned char *ent);  // Create from 4-byte entry.
	void set8(const unsigned char *ent);  // Create from Exult entry (v. -1).
	void write8(unsigned char *ent) const;// Write out 8-byte Exult entry.
	void set(int ax, int ay, int az,
	         unsigned char stype, unsigned char stime);
	int get_type() const {
		return type;
	}
	int get_time() const {
		return time;
	}
	Tile_coord get_pos() const {
		return pos;
	}
	static const char *get_script_name(int ty) {
		return ty >= Schedule::first_scripted_schedule ?
		       script_names[ty - Schedule::first_scripted_schedule].c_str() : nullptr;
	}
};

#endif
