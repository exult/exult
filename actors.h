/*
 *  actors.h - Game actors.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
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

#ifndef INCL_ACTORS
#define INCL_ACTORS 1

#include "contain.h"
#include "utils.h"      // This is only included for Log2...
#include "flags.h"
#include "ready.h"
#include "ignore_unused_variable_warning.h"

class Image_window;
class Game_window;
class Npc_actor;
class Actor_action;
class Schedule;
class Schedule_change;
class Monster_info;
class Monster_actor;
class Weapon_info;
class Dead_body;
class Npc_timer_list;
class Frames_sequence;
class Animator;
class Actor_attributes;

/*
 *  An actor:
 */
class Actor : public Container_game_object, public Time_sensitive {
	static Actor *editing;      // NPC being edited by ExultStudio.
protected:
	std::string name;           // Its name.
	int usecode;            // # of usecode function.
	bool usecode_assigned;      // Usecode # explicitly assigned.
	std::string usecode_name;       // Name of usecode fun explicitly assigned.
	bool unused;            // If npc_num > 0, this NPC is unused
	//   in the game.
	short npc_num;          // # in Game_window::npcs list, or -1.
	short face_num;         // Which shape for conversations.
	short party_id;         // Index in party, or -1.
	int properties[12];     // Properties set/used in 'usecode'.
	Actor_attributes *atts;     // Generic atts. (for new games/mods).
	unsigned char temperature;  // Measure of coldness (0-63).
	short shape_save;       // Our old shape, or -1.
	short oppressor;        // NPC ID (>= 0) of oppressor, or -1.
	Game_object_weak target;        // Who/what we're attacking.
	short casting_mode;     //For displaying casting frames.
	int casting_shape;  //Shape of casting frames.
	// These 2 are set by the Usecode function 'set_to_attack':
	Game_object_weak target_object;
	Tile_coord target_tile;
	int attack_weapon;
public:
	enum Attack_mode {      // Setting from gump.+++++Save/restore.
	    nearest = 0,
	    weakest = 1,        // Attack weakest.
	    strongest = 2,
	    berserk = 3,        // Always attack, never retreat.
	    protect = 4,        // Protect NPC with halo.
	    defend = 5,
	    flank = 6,      // Attempt to attack from side.
	    flee = 7,
	    random = 8,     // Choose target at random.
	    manual = 9
	};
	enum Casting_mode {
	    not_casting = 0,        // The NPC is not casting.
	    init_casting = 1,       // When set, the next usecode script will
	    // display casting frames (shape 859).
	    show_casting_frames = 2 // Used for displaying the casting frames.
	                          // Also flags that the when the script finishes, the
	                          // casting frames should be disabled.
	};
protected:
	// Party positions
	const static short party_pos[4][10][2];

	Attack_mode attack_mode;
	// A frame sequence for each dir.:
	static Frames_sequence *avatar_frames[4];
	static Frames_sequence *npc_frames[4];
	Frames_sequence **frames;
	// Draw weapon in hand
	void paint_weapon();
	unsigned char schedule_type;    // Schedule type (Schedule_type).
	Tile_coord schedule_loc;    // Location (x,y) of Shedule
	Tile_coord old_schedule_loc;    // Location (x,y) of old Shedule
	unsigned char next_schedule;    // Used so correct schedule type
	//   will be saved
	Schedule *schedule;     // Current schedule.
	int restored_schedule;  // Just restored schedule type.
	bool dormant;           // I.e., off-screen.
	bool hit;           // Just hit in combat.
	bool combat_protected;      // 'Halo' on paperdoll screen.
	bool user_set_attack;       // True if player set attack_mode.
	short alignment;        // 'Feelings' towards Ava. See below.
	short charmalign;       // Alignment of charmed NPC.
	Game_object *spots[18];     // Where things can go.  See 'Spots'
	//   below for description.
	bool two_handed;        // Carrying a two-handed item.
	bool two_fingered;      // Carrying gauntlets (both fingers)
	bool use_scabbard;      // Carrying an item in scabbard (belt, back 2h, shield).
	bool use_neck;          // Carrying cloak (amulet, cloak)
	unsigned char light_sources;    // # of light sources readied.
	unsigned char usecode_dir;  // Direction (0-7) for usecode anim.

	unsigned type_flags: 32; // 32 flags used in movement among other things
	unsigned char  gear_immunities; // Damage immunities granted by gear.
	unsigned short gear_powers;     // Other powers granted by gear.

	unsigned char ident;

	int skin_color;
	Actor_action *action;       // Controls current animation.
	std::vector<Actor_action *> deletedactions;     // Halted actions.
	int frame_time;         // Time between frames in msecs.  0 if
	//   actor not moving.
	int step_index;         // Index into walking frames, 1 1st.
	int qsteps;             // # steps since last quake.

	Npc_timer_list *timers;     // Timers for poison, hunger, etc.
	Rectangle weapon_rect;      // Screen area weapon was drawn in.
	long rest_time;         // # msecs. of not doing anything.
	void init();            // Clear stuff during construction.
	// Move and change frame.
	void movef(Map_chunk *old_chunk, Map_chunk *new_chunk,
	           int new_sx, int new_sy, int new_frame, int new_lift);
	bool is_really_blocked(Tile_coord &t, bool force);
    // Empty one hand
    bool empty_hand(Game_object *obj, Game_object_shared *keep);
public:
	friend class Clear_casting;
	friend class Clear_hit;
	static void init_default_frames();  // Set usual frame sequence.
	Actor(const std::string &nm, int shapenum, int num = -1, int uc = -1);
	~Actor() override;
	// Blocked moving onto tile 't'?
	bool is_blocked(Tile_coord &t, Tile_coord *f = nullptr, const int move_flags = 0);
	Game_object *find_blocking(Tile_coord const &tile, int dir);

	void swap_ammo(Game_object *newammo);
	bool ready_ammo();      // Find and ready appropriate ammo.
	bool ready_best_weapon();   // Find best weapon and ready it.
	bool ready_best_shield();   // Find best shield and ready it.
	void empty_hands();     // Make sure both hands are empty.
	// Force repaint of area taken.
	int get_effective_weapon_shape();//For displaying casting frames.
	bool add_dirty(bool figure_rect = false);
	void change_frame(int frnum);   // Change frame & set to repaint.
	bool figure_weapon_pos(int &weapon_x, int &weapon_y, int &weapon_frame);
	void say_hunger_message();
	void use_food();        // Decrement food level.
	// Increment/decrement temperature.
	void check_temperature(bool freeze);
	// Get frame seq. for given dir.
	Frames_sequence *get_frames(int dir) {
		return frames[dir / 2];
	}
	int &get_step_index() {     // Get it (for updating).
		return step_index;
	}
	// Get attack frames.
	int get_attack_frames(int weapon, bool projectile,
	                      int dir, signed char *frames) const;
	enum Alignment {        // Describes alignment field.
	    neutral = 0,        // See [I]nspect NPC screen in SI for names.
	    good = 1,
	    evil = 2,
	    chaotic = 3
	};  // Bees have this, & don't attack until
	// Spots where items are carried.
	int free_hand() const {     // Get index of a free hand, or -1.
		// PREFER right hand.
		return two_handed ? -1 :
		       (!spots[rhand] ? rhand : (!spots[lhand] ? lhand : -1));
	}
	int free_finger() const {   // Get index of a free finger, or -1.
		return two_fingered ? -1 :
		       (!spots[lfinger] ? lfinger
		        : (!spots[rfinger] ? rfinger : -1));
	}
	inline bool is_two_handed() const {
		return two_handed;
	}
	inline bool is_two_fingered() const {
		return two_fingered;
	}
	inline bool is_scabbard_used() const {
		return use_scabbard;
	}
	inline bool is_neck_used() const {
		return use_neck;
	}
	bool has_light_source() const {  // Carrying a torch?
		return light_sources > 0;
	}
	void add_light_source() { // Add a torch
		light_sources++;
	}
	void remove_light_source() { // Remove a torch
		if (light_sources)
			light_sources--;
	}
	Attack_mode get_attack_mode() {
		return attack_mode;
	}
	void set_attack_mode(Attack_mode amode, bool byuser = false) {
		attack_mode = amode;
		user_set_attack = byuser;
	}
	bool did_user_set_attack() const {
		return user_set_attack;
	}
	bool is_combat_protected() const {
		return combat_protected;
	}
	void set_combat_protected(bool v) {
		combat_protected = v;
	}
	int get_oppressor() const {
		return oppressor;
	}
	void set_oppressor(int opp) {
		oppressor = opp;
	}
	enum type_flags {
	    tf_fly = 4,
	    tf_walk = 5,
	    tf_swim = 6,
	    tf_ethereal = 7,
	    tf_want_primary = 8,
	    tf_sex = 9,
	    tf_bleeding = 10,
	    tf_in_party = 12,
	    tf_in_action = 13,
	    tf_conjured = 14,
	    tf_summonned = 15
	};
	enum Item_properties {      // Trying to figure out properties.
	    strength = 0,       // This is also max. health.
	    dexterity = 1,
	    intelligence = 2,
	    health = 3,
	    combat = 4,
	    mana = 5,
	    magic = 6,      // Max. mana.
	    training = 7,       // Training points.
	    exp = 8,        // Experience.
	    food_level = 9,
	    sex_flag = 10,  // Seems to be get/set sex type flag in SI.
	    missile_weapon = 11 // Pretty sure it returns 1 if wearing
	                     // weapon with uses >= 2, 0 otherwise.
	};
	enum Frames {           // Frames 0-15.  16-31 are the same,
	    //   only S instead of N.
	    standing = 0,
	    step_right_frame = 1,
	    step_left_frame = 2,
	    ready_frame = 3,    // Ready to fight?
	    raise1_frame = 4,   // 1-handed strikes.
	    reach1_frame = 5,
	    strike1_frame = 6,
	    raise2_frame = 7,   // 2-handed strikes.
	    reach2_frame = 8,
	    strike2_frame = 9,
	    sit_frame = 10,
	    bow_frame = 11,
	    kneel_frame = 12,
	    sleep_frame = 13,
	    up_frame = 14,      // Both hands reach up.
	    out_frame = 15      // Both hands reach out.
	};

	int get_face_shapenum() const { // Get "portrait" shape #.
		return face_num;    // It's the NPC's #.
	}
	int get_usecode() const override {
		return usecode == -1 ? Game_object::get_usecode() : usecode;
	}
	bool set_usecode(int funid, const char *nm = nullptr) override {
		if (funid < 0) {
			usecode_assigned = false;
			usecode_name.clear();
			usecode = -1;
			return true;
		}
		if (nm)
			usecode_name = nm;
		else
			usecode_name.clear();
		usecode = funid;
		usecode_assigned = true;
		return true;
	}
	Schedule *get_schedule() const {
		return schedule;
	}
	int get_frame_time() const { // Return frame time if moving.
		return frame_time;
	}
	void set_frame_time(int ftime) { // Set walking speed.
		frame_time = ftime;
	}
	void stand_at_rest();       // Stand (if not doing anyting else).
	void clear_rest_time() {
		rest_time = 0;
	}
	void resting(int msecs) {   // Increment rest time.
		if ((rest_time += msecs) > 2000)
			stand_at_rest();// Stand (under certain conditions).
	}
	bool is_moving() const {
		return frame_time != 0;
	}
	bool is_dormant() const {   // Inactive (i.e., off-screen)?
		return dormant;
	}
	bool is_dead() const {
		return (flags & (1 << Obj_flags::dead)) != 0;
	}
	bool is_in_party() const {  // (Includes Avatar.)
		return (flags & (1 << Obj_flags::in_party)) != 0;
	}
	void set_dormant() {
		dormant = true;
	}
	Actor_action *get_action() { // Return action.
		return action;
	}
	// Set new action.
	void set_action(Actor_action *newact);
	void purge_deleted_actions();
	Tile_coord get_dest();      // Get destination.
	// Walk to a desired spot.
	void walk_to_tile(Tile_coord const &dest, int speed = 250, int delay = 0,
	                  int maxblk = 3);
	void walk_to_tile(int tx, int ty, int tz, int speed = 250,
	                  int delay = 0, int maxblk = 3) {
		walk_to_tile(Tile_coord(tx, ty, tz), speed, delay, maxblk);
	}
	// Get there, avoiding obstacles.
	int walk_path_to_tile(Tile_coord const &src, Tile_coord const &dest,
	                      int speed = 250, int delay = 0, int dist = 0, int maxblk = 3);
	int walk_path_to_tile(Tile_coord const &dest,
	                      int speed = 250, int delay = 0, int dist = 0, int maxblk = 3) {
		return walk_path_to_tile(get_tile(), dest,
		                         speed, delay, dist, maxblk);
	}
	// Start animation.
	void start(int speed = 250, int delay = 0);
	void start_std();       // Start with std. speed, delay.
	void stop();            // Stop animation.
	void follow(Actor *leader); // Follow the leader.
	// Approach another from offscreen.
	int approach_another(Actor *other, bool wait = false);
	// Get info. about tile to step onto.
	static void get_tile_info(Actor *actor,
	                          Game_window *gwin, Map_chunk *nlist,
	                          int tx, int ty, bool &water, bool &poison);
	// Set combat opponent.
	void set_target(Game_object *obj, bool start_combat = false);
	Game_object *get_target() { // Get who/what we're attacking.
	  return target.lock().get();
	}
	// Works out if an object fits in a spot
	bool fits_in_spot(Game_object *obj, int spot);
	// The prefered slot for an object
	void get_prefered_slots(Game_object *obj, int &prefered, int &alt1, int &alt2);
	// Find where to put object.
	int find_best_spot(Game_object *obj);
	int get_prev_schedule_type();   // Get previous schedule.
	void restore_schedule();    // Set schedule after reading in.
	void set_schedule_loc(Tile_coord const &loc) {  // For monsters ONLY.
		schedule_loc = loc;
	}
	// Set new schedule.
	void set_schedule_type(int new_schedule_type,
	                       Schedule *newsched = nullptr);
	// Change to new schedule at loc
	virtual void set_schedule_and_loc(int new_schedule_type,
	                                  Tile_coord const &dest, int delay = -1);
	bool teleport_offscreen_to_schedule(Tile_coord const &dest, int dist);
	int get_schedule_type() const {
		return schedule_type;
	}
	// Get/set 'alignment'.
	int get_alignment() const {
		return alignment;
	}
	void set_alignment(short a) {
		alignment = a;
	}
	int get_effective_alignment() const;    // Include 'charmed' flag.
	void set_effective_alignment(int newalign); // Include 'charmed' flag.
	void reset_effective_alignment() {
		charmalign = alignment;
	}
	// Update chunks after NPC moved.
	virtual void switched_chunks(Map_chunk *, Map_chunk *)
	{  }
	// Update schedule for new 3-hour time.
	virtual void update_schedule(int hour3, int delay = -1,
	                             Tile_coord *pos = nullptr) {
		ignore_unused_variable_warning(hour3, delay, pos);
	}
	// Render.
	void paint() override;
	// Run usecode function.
	void activate(int event = 1) override;
	bool edit() override;        // Edit in ExultStudio.
	// Saved from ExultStudio.
	static void update_from_studio(unsigned char *data, int datalen);
	// Drop another onto this.
	bool drop(Game_object *obj) override;
	std::string get_name() const override;
	std::string get_npc_name() const;
	std::string get_npc_name_string() const {
		return name;
	}
	void set_npc_name(const char *n);
	void set_property(int prop, int val);
	bool try_to_hit(Game_object *attacker, int attval) override;
	// Under attack.
	Game_object *attacked(Game_object *attacker, int weapon_shape = 0,
	                              int ammo_shape = 0, bool explosion = false) override;
	int figure_hit_points(Game_object *attacker, int weapon_shape = -1,
	                              int ammo_shape = -1, bool explosion = false) override;
	int apply_damage(Game_object *attacker, int str,
	                         int wpoints, int type, int bias = 0, int *exp = nullptr) override;
	// Lose HP's and check for death.
	int reduce_health(int delta, int damage_type, Game_object *attacker = nullptr,
	                          int *exp = nullptr) override;
	void fight_back(Game_object *attacker);
	bool get_attack_target(Game_object *&obj, Tile_coord &t) {
		static Tile_coord invalidloc(-1, -1, 0);
		Game_object_shared tobj = target_object.lock();
		obj = tobj.get();
		t = target_tile;
		return obj || target_tile != invalidloc;
	}
	void set_attack_target(Game_object *t, int w) {
		target_tile = Tile_coord(-1, -1, 0);
		target_object = weak_from_obj(t);
		attack_weapon = w;
	}
	void set_attack_target(Tile_coord const &t, int w) {
		target_object = Game_object_weak();
		target_tile = t;
		target_tile.fixme();
		attack_weapon = w;
	}
	int get_effective_range(const Weapon_info *winf = nullptr, int reach = -1) override;
	Game_object *find_weapon_ammo(int weapon, int needed = 1,
	                                      bool recursive = false) override;
	Game_object *find_best_ammo(int family, int needed = 1);
	bool usecode_attack();
	int get_property(int prop) const;
	int get_effective_prop(int prop) const;
	bool is_dying() const {     // Dead when health below -1/3 str.
		return properties[static_cast<int>(health)] <
		       -(properties[static_cast<int>(strength)] / 3);
	}
	bool is_knocked_out() const {
		return get_property(static_cast<int>(health)) <= 0;
	}
	int get_level() const {     // Get experience level.
		return 1 + Log2(get_property(exp) / 50);
	}
	// Get/set generic attribute.
	void set_attribute(const char *nm, int val);
	int get_attribute(const char *nm);
	using Atts_vector = std::vector<std::pair<const char *, int>>;
	void get_attributes(Atts_vector &attlist);
	// Set atts. from savegame.
	void read_attributes(const unsigned char *buf, int len) override;
	Npc_timer_list *need_timers();
	// Set/clear/get actor flag.
	void force_sleep();
	void clear_sleep() {
		flags &= ~(static_cast<uint32>(1) << Obj_flags::asleep);
	}
	void set_flag(int flag) override;
	void set_type_flag(int flag);
	void clear_flag(int flag) override;
	void clear_type_flag(int flag);
	bool get_type_flag(int flag) const override;
	void set_type_flags(unsigned short tflags);
	int get_skin_color() const {
		return skin_color;
	}
	void set_skin_color(int color) {
		skin_color = color;
		set_actor_shape();
	}
	virtual int get_type_flags() const {
		return type_flags;
	}
	short get_casting_mode() const {
		return casting_mode;
	}
	void end_casting_mode(int delay);
	int get_casting_shape() const {
		return casting_shape;
	}
	void begin_casting(int s) {
		casting_mode = init_casting;
		casting_shape = s;
	}
	void display_casting_frames() {
		casting_mode = show_casting_frames;
	}
	void hide_casting_frames() {
		casting_mode = not_casting;
	}
//++++++Is_dead() test messes up training.
//	unsigned char get_ident() { return is_dead() ? 0 : ident; }
	unsigned char get_ident() {
		return ident;
	}
	void set_ident(unsigned char id) {
		ident = id;
	}

	int get_temperature() const { // Get/set measure of coldness.
		return temperature;
	}
	void set_temperature(int v);
	int get_temperature_zone() const {
		// SI-verified.
		if (temperature < 15)
			return 1;
		else if (temperature < 25)
			return 2;
		else if (temperature < 40)
			return 3;
		else if (temperature < 50)
			return 4;
		else
			return 5;
	}
	int figure_warmth();        // Based on what's worn.
	bool is_unused() const {    // Free NPC?
		return unused;
	}
	void set_unused(bool tf) {
		unused = tf;
	}

	int get_npc_num() const {   // Get its ID (1-num_npcs).
		return npc_num;
	}
	// Get/set index within party.
	int get_party_id() const {
		return party_id;
	}
	void set_party_id(int i) {
		party_id = i;
	}
	// Set for Usecode animations.
	void set_usecode_dir(int d) {
		usecode_dir = d & 7;
	}
	int get_usecode_dir() const {
		return usecode_dir;
	}
	Actor *as_actor() override { // An actor?
		return this;
	}
	virtual bool is_slime() const {
		return false;
	}
	void init_readied();        // Call Usecode to init. readied objs.
	// Remove an object.
	void remove(Game_object *obj) override;
	// Add an object.
	bool add(Game_object *obj, bool dont_check = false,
	                 bool combine = false, bool noset = false) override;
	// Add to NPC 'readied' spot.
	bool add_readied(Game_object *obj, int index,
	                        bool dont_check = false, bool force_pos = false, bool noset = false) override;
	int find_readied(Game_object *obj) override;
	Game_object *get_readied(int index) const override;
	void call_readied_usecode(int index,
	                                  Game_object *obj, int eventid) override;
	int get_max_weight() const override;   // Get max. weight allowed.
	// Change member shape.
	void change_member_shape(Game_object *obj, int newshape) override;
	// Move out of the way.
	bool move_aside(Actor *for_actor, int dir) override;
	// Get frame if rotated clockwise.
	int get_rotated_frame(int quads) override;
	virtual int get_armor_points(); // Get total armor value.
	// Gets whether the actor is immune or vulnerable to a given
	// form of damage:
	int is_immune(int type) const;
	bool is_goblin() const;
	bool can_see_invisible() const;
	bool can_speak() const;
	bool is_sentient() const;
	void refigure_gear();
	bool check_gear_powers(int flags) const {
		return (gear_powers & flags) != 0;
	}
	// Get total weapon value.
	virtual const Weapon_info *get_weapon(int &points, int &shape,
	                                Game_object  *&obj);
	const Weapon_info *get_weapon(int &points) {
		int sh;
		Game_object *o;
		return get_weapon(points, sh, o);
	}
	static bool roll_to_win(int attacker, int defender);
	// Hit-point algorithm:
	bool can_act();
	bool can_act_charmed(); // checks for charmed and charmed_more_difficult
	void set_charmed_combat();
	virtual void fall_down();
	virtual void lay_down(bool die);
	virtual void die(Game_object *attacker);        // We're dead.
	Actor *resurrect(Dead_body *body);// Bring back to life.
	Game_object_shared clone();     // Create another nearby to this.
	void mend_wounds(bool mendmana);        // Restore HP's and MP's.
	// Read from file.
	void read(IDataSource *nfile, int num, bool has_usecode,
	          bool &fix_unused);
	// Don't write out to IREG file.
	void write_ireg(ODataSource *out) override {
		ignore_unused_variable_warning(out);
	}
	int get_ireg_size() override {
		return 0;
	}
	void write(ODataSource *nfile);// Write out (to 'npc.dat').
	void write_contents(ODataSource *out) override;   // Write contents
	void set_actor_shape();     // Set shape based on sex, skin color
	void set_polymorph(int shape);  // Set a polymorph shape
	void set_polymorph_default();   // Set the default shape
	// Get the polymorph shape
	int get_polymorph() {
		return shape_save;
	}
	// Get the non polymorph shape (note, doesn't returned skin coloured shapes)
	// For usecode
	int get_shape_real() const;
	// This does the same, but will return skin coloured shapes
	// For paperdolls/face stats
	int get_sexed_coloured_shape() const {
		return shape_save != -1 ? shape_save : get_shapenum();
	}

	// Set schedule list.
	virtual void set_schedules(Schedule_change *list, int cnt) {
		ignore_unused_variable_warning(list, cnt);
	}
	virtual void set_schedule_time_type(int time, int type) {
		ignore_unused_variable_warning(time, type);
	}
	virtual void set_schedule_time_location(int time, int x, int y) {
		ignore_unused_variable_warning(time, x, y);
	}
	virtual void remove_schedule(int time) {
		ignore_unused_variable_warning(time);
	}
	virtual void get_schedules(Schedule_change *&list, int &cnt) {
		list = nullptr;
		cnt = 0;
	}
	virtual int find_schedule_at_time(int hour3) {
		ignore_unused_variable_warning(hour3);
		return -1;
	}

	void show_inventory();
	int inventory_shapenum();

	bool was_hit() {
		return hit;
	}

	// Should be virtual???
	void cache_out();
	bool in_usecode_control() const;
	bool quake_on_walk();
};
using Actor_shared = std::shared_ptr<Actor>;

/*
 *  Actor frame descriptions:
    0   Standing
    1   Walk
    2   Walk
    3   Beginning to attack
    4-6 Attacking with one hand
    7-9 Attacking with two hands
    9   Also NPC shooting magic
    10  Sitting down
    11  Bending over (beginning to sit down)
    12  Kneeling
    11  Lying down
    14  Casting spell (hands raised)
    15  Casting spell (hands outstretched)

 */

/*
 *  The main actor.
 */
class Main_actor : public Actor {
public:
	Main_actor(const std::string &nm, int shapenum, int num = -1, int uc = -1)
		: Actor(nm, shapenum, num, uc) {
		frames = &avatar_frames[0];
	}
	~Main_actor() override;
	// For Time_sensitive:
	void handle_event(unsigned long curtime, uintptr udata) override;
	void get_followers();       // Get party to follow.
	// Step onto an (adjacent) tile.
	bool step(Tile_coord t, int frame, bool force = false) override;
	// Update chunks after NPC moved.
	void switched_chunks(Map_chunk *olist,
	                             Map_chunk *nlist) override;
	// Move to new abs. location.
	void move(int newtx, int newty, int newlift, int newmap = -1) override;
	void die(Game_object *attacker) override;        // We're dead.
};
using Main_actor_shared = std::shared_ptr<Main_actor>;

/*
 *  A non-player-character that one can converse (or fight) with:
 */
class Npc_actor : public Actor {
	bool nearby;       // Queued as a 'nearby' NPC.  This is
	//   to avoid being added twice.
protected:
	unsigned char num_schedules;    // # entries below.
	Schedule_change *schedules; // List of schedule changes.
	int find_schedule_change(int hour3);
public:
	Npc_actor(const std::string &nm, int shapenum, int num = -1,
	          int uc = -1);
	~Npc_actor() override;
	void set_nearby() {     // Set/clear/test 'nearby' flag.
		nearby = true;
	}
	void clear_nearby() {
		nearby = false;
	}
	bool is_nearby() const {
		return nearby;
	}
	// Set schedule list.
	void set_schedules(Schedule_change *list, int cnt) override;
	void set_schedule_time_type(int time, int type) override;
	void set_schedule_time_location(int time, int x, int y) override;
	void remove_schedule(int time) override;
	void get_schedules(Schedule_change *&list, int &cnt) override;
	// Move and change frame.
	void movef(Map_chunk *old_chunk, Map_chunk *new_chunk,
	           int new_sx, int new_sy, int new_frame, int new_lift);
	// Update schedule for new 3-hour time.
	void update_schedule(int hour3, int delay = -1,
	                             Tile_coord *pos = nullptr) override;
	int find_schedule_at_time(int hour3) override;
	// Render.
	void paint() override;
	// Run usecode function.
	void activate(int event = 1) override;
	// For Time_sensitive:
	void handle_event(unsigned long curtime, uintptr udata) override;
	// Step onto an (adjacent) tile.
	bool step(Tile_coord t, int frame, bool force = false) override;
	// Remove/delete this object.
	void remove_this(Game_object_shared *keep = nullptr) override;
	// Update chunks after NPC moved.
	void switched_chunks(Map_chunk *olist,
	                             Map_chunk *nlist) override;
	// Move to new abs. location.
	void move(int newtx, int newty, int newlift, int newmap = -1) override;

	Npc_actor *as_npc() override {
		return this;
	}
};
using Npc_actor_shared = std::shared_ptr<Npc_actor>;

/*
 *  An actor's dead body:
 */
class Dead_body : public Container_game_object {
	short npc_num;          // # of NPC it came from, or -1.
public:
	Dead_body(int shapenum, int framenum, unsigned int tilex,
	          unsigned int tiley, unsigned int lft, int n)
		: Container_game_object(shapenum, framenum, tilex, tiley, lft),
		  npc_num(n) {
	}
	~Dead_body() override;
	int get_live_npc_num() const override;
	// Under attack.
	Game_object *attacked(Game_object *attacker, int weapon_shape = 0,
	                              int ammo_shape = 0, bool explosion = false) override {
		ignore_unused_variable_warning(attacker, weapon_shape, ammo_shape, explosion);
		return this;    // Not affected.
	}
	void write_ireg(ODataSource *out) override;
	int get_ireg_size() override;
};
using Dead_body_shared = std::shared_ptr<Dead_body>;

#endif
