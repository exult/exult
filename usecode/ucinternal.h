/*
 *  ucinternal.h - Interpreter for usecode.
 *
 *  Usecode_internal is the implementation, so this header should only
 *  be included within .cc's in the 'usecode' directory.
 *
 *
 *  Copyright (C) 2001-2025  The Exult Team
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

#ifndef UCINTERNAL_H
#define UCINTERNAL_H

#include "common_types.h"
#include "tiles.h"
#include "ucdebugging.h"
#include "ucmachine.h"
#include "useval.h"

#include <deque>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

class Actor;
class Barge_object;
class Npc_actor;
class Usecode_value;
class Text_gump;
class Vector;
class Stack_frame;
class Usecode_function;
class Usecode_symbol_table;
class Usecode_class_symbol;

/*
 *  Recursively look for a barge that an object is a part of, or on.
 *
 *  Output: ->barge if found, else nullptr.
 */

Barge_object* Get_barge(Game_object* obj);

#define USECODE_INTRINSIC_DECL(NAME) \
	Usecode_value UI_##NAME(int num_parms, Usecode_value parms[12])

/*
 *  Here's our virtual machine for running usecode.
 */
class Usecode_internal : public Usecode_machine {
	// I'th entry contains funs for ID's
	//    256*i + n.
	using Funs256 = std::vector<Usecode_function*>;
	std::vector<Funs256>       funs;
	std::vector<Usecode_value> statics;             // Global persistent vars.
	Usecode_symbol_table*      symtbl = nullptr;    // (optional) symbol table.
	std::deque<Stack_frame*>   call_stack;          // the call stack
	std::map<Stack_frame*, const uint8*>
				 except_stack;            // the exception handling stack
	Stack_frame* frame = nullptr;         // One intrinsic uses this for now...
	bool         modified_map = false;    // We add/deleted/moved an object.
	std::map<int, uint32> timers;         // Each has time in hours when set.
	int                   speech_track = -1;    // Set/read by some intrinsics.
	Text_gump*            book = nullptr;       // Book/scroll being displayed.
	Game_object_shared    caller_item;          // Item this is being called on.
	std::vector<Game_object_shared>
			last_created;    // Stack of last items created with
	//   intrins. x24.
	Actor*      path_npc     = nullptr;    // Last NPC in path_run_usecode().
	const char* user_choice  = nullptr;    // String user clicked on.
	bool        found_answer = false;      // Did we already handle the
	//   conversation option?
	Tile_coord saved_pos       = {-1, -1, -1};    // For a couple SI intrinsics.
	int        saved_map       = -1;    // Improvements for these intrinsics.
	char*      String          = nullptr;    // The single string register.
	int        telekenesis_fun = -1;    // For next Usecode call from spell.

	void append_string(const uint8* txt) {
		append_string(reinterpret_cast<const char*>(txt));
	}

	void           append_string(const char* str);    // Append to string.
	void           show_pending_text();    // Make sure user's seen all text.
	void           show_book();            // "Say" book/scroll text.
	void           say_string();           // "Say" the string.
	Usecode_value* stack;                  // Stack.
	Usecode_value* sp;                     // Stack ptr.  Grows upwards.
	void           push(const Usecode_value& val);    // Push/pop stack.
	Usecode_value  pop();
	Usecode_value  peek();
	void           pushref(Game_object* obj);    // Push itemref
	void           pushref(Game_object_shared obj);
	void           pushi(long val);    // Push/pop integers.
	int            popi();
	// Push/pop strings.
	void pushs(const char* s);

	void pushs(const uint8* s) {
		pushs(reinterpret_cast<const char*>(s));
	}

	// Get ->obj. from 'itemref'.
	Game_object* get_item(const Usecode_value& itemref);
	// "Safe" cast to Actor and Npc_actor.
	Actor* as_actor(Game_object* obj);
	// Get position.
	Tile_coord get_position(Usecode_value& itemval);
	/*
	 *  Built-in usecode functions:
	 */
	using UsecodeIntrinsicFn = Usecode_value (Usecode_internal::*)(
			int num_parms, Usecode_value parms[12]);

	int get_face_shape(Usecode_value& arg1, Actor*& npc, int& frame);

	int get_face_shape(Usecode_value& arg1, Actor*& npc) {
		int frame;
		return get_face_shape(arg1, npc, frame);
	}

	void show_npc_face(Usecode_value& arg1, Usecode_value& arg2, int slot = -1);
	void remove_npc_face(Usecode_value& arg1);
	void set_item_shape(Usecode_value& item_arg, Usecode_value& shape_arg);
	void set_item_frame(
			Game_object* item, int frame, int check_empty = 0,
			int set_rotated = 0);
	void          add_dirty(Game_object* obj);
	void          remove_item(Game_object* obj);
	Usecode_value get_party();
	void          item_say(Usecode_value& objval, Usecode_value& strval);
	void          activate_cached(const Tile_coord& pos);
	Usecode_value find_nearby(
			Usecode_value& objval, Usecode_value& shapeval,
			Usecode_value& distval, Usecode_value& mval);
	Usecode_value find_nearest(
			Usecode_value& objval, Usecode_value& shapeval,
			Usecode_value& distval);
	Usecode_value find_direction(Usecode_value& from, Usecode_value& to);
	Usecode_value count_objects(
			Usecode_value& objval, Usecode_value& shapeval,
			Usecode_value& qualval, Usecode_value& frameval);
	Usecode_value get_objects(
			Usecode_value& objval, Usecode_value& shapeval,
			Usecode_value& qualval, Usecode_value& frameval);
	Usecode_value remove_party_items(
			Usecode_value& quantval, Usecode_value& shapeval,
			Usecode_value& qualval, Usecode_value& frameval,
			Usecode_value& flagval);
	Usecode_value add_party_items(
			Usecode_value& quantval, Usecode_value& shapeval,
			Usecode_value& qualval, Usecode_value& frameval,
			Usecode_value& temporary);
	Usecode_value add_cont_items(
			Usecode_value& container, Usecode_value& quantval,
			Usecode_value& shapeval, Usecode_value& qualval,
			Usecode_value& frameval, Usecode_value& temporary);
	Usecode_value remove_cont_items(
			Usecode_value& container, Usecode_value& quantval,
			Usecode_value& shapeval, Usecode_value& qualval,
			Usecode_value& frameval, Usecode_value& flagval);
	Game_object_shared create_object(int shapenum, bool equip);

	bool path_run_usecode(
			Usecode_value& npcval, Usecode_value& locval, Usecode_value& useval,
			Usecode_value& itemval, Usecode_value& eventval,
			bool find_free = false, bool always = false,
			bool companions = false);
	void create_script(
			Usecode_value& objval, Usecode_value& codeval, long delay);
	bool is_dest_reachable(Actor* npc, const Tile_coord& dest);

	/*
	 *  Embedded intrinsics
	 */

	struct IntrinsicTableEntry {
		UsecodeIntrinsicFn func;
		const char*        name;
	};

	static IntrinsicTableEntry intrinsics_bg[];
	static IntrinsicTableEntry intrinsics_si[];
	static IntrinsicTableEntry intrinsics_sib[];

	Usecode_value Execute_Intrinsic(
			UsecodeIntrinsicFn func, const char* name, int intrinsic,
			int num_parms, Usecode_value parms[12]);
	USECODE_INTRINSIC_DECL(NOP);
	USECODE_INTRINSIC_DECL(UNKNOWN);
	USECODE_INTRINSIC_DECL(get_random);
	USECODE_INTRINSIC_DECL(execute_usecode_array);
	USECODE_INTRINSIC_DECL(delayed_execute_usecode_array);
	USECODE_INTRINSIC_DECL(show_npc_face);
	USECODE_INTRINSIC_DECL(remove_npc_face);
	USECODE_INTRINSIC_DECL(add_answer);
	USECODE_INTRINSIC_DECL(remove_answer);
	USECODE_INTRINSIC_DECL(push_answers);
	USECODE_INTRINSIC_DECL(pop_answers);
	USECODE_INTRINSIC_DECL(clear_answers);
	USECODE_INTRINSIC_DECL(select_from_menu);
	USECODE_INTRINSIC_DECL(select_from_menu2);
	USECODE_INTRINSIC_DECL(input_numeric_value);
	USECODE_INTRINSIC_DECL(set_item_shape);
	USECODE_INTRINSIC_DECL(find_nearest);
	USECODE_INTRINSIC_DECL(die_roll);
	USECODE_INTRINSIC_DECL(get_item_shape);
	USECODE_INTRINSIC_DECL(get_item_frame);
	USECODE_INTRINSIC_DECL(set_item_frame);
	USECODE_INTRINSIC_DECL(get_item_quality);
	USECODE_INTRINSIC_DECL(set_item_quality);
	USECODE_INTRINSIC_DECL(get_item_quantity);
	USECODE_INTRINSIC_DECL(set_item_quantity);
	USECODE_INTRINSIC_DECL(get_object_position);
	USECODE_INTRINSIC_DECL(get_distance);
	USECODE_INTRINSIC_DECL(find_direction);
	USECODE_INTRINSIC_DECL(get_npc_object);
	USECODE_INTRINSIC_DECL(get_schedule_type);
	USECODE_INTRINSIC_DECL(set_schedule_type);
	USECODE_INTRINSIC_DECL(add_to_party);
	USECODE_INTRINSIC_DECL(remove_from_party);
	USECODE_INTRINSIC_DECL(get_npc_prop);
	USECODE_INTRINSIC_DECL(set_npc_prop);
	USECODE_INTRINSIC_DECL(get_avatar_ref);
	USECODE_INTRINSIC_DECL(get_party_list);
	USECODE_INTRINSIC_DECL(create_new_object);
	USECODE_INTRINSIC_DECL(create_new_object2);
	USECODE_INTRINSIC_DECL(create_new_egg);
	USECODE_INTRINSIC_DECL(set_last_created);
	USECODE_INTRINSIC_DECL(update_last_created);
	USECODE_INTRINSIC_DECL(get_npc_name);
	USECODE_INTRINSIC_DECL(count_objects);
	USECODE_INTRINSIC_DECL(find_object);
	USECODE_INTRINSIC_DECL(get_cont_items);
	USECODE_INTRINSIC_DECL(remove_party_items);
	USECODE_INTRINSIC_DECL(add_party_items);
	USECODE_INTRINSIC_DECL(get_music_track);
	USECODE_INTRINSIC_DECL(play_music);
	USECODE_INTRINSIC_DECL(npc_nearby);
	USECODE_INTRINSIC_DECL(npc_nearby2);
	USECODE_INTRINSIC_DECL(find_nearby_avatar);
	USECODE_INTRINSIC_DECL(is_npc);
	USECODE_INTRINSIC_DECL(display_runes);
	USECODE_INTRINSIC_DECL(click_on_item);
	USECODE_INTRINSIC_DECL(set_intercept_item);
	USECODE_INTRINSIC_DECL(find_nearby);
	USECODE_INTRINSIC_DECL(give_last_created);
	USECODE_INTRINSIC_DECL(is_dead);
	USECODE_INTRINSIC_DECL(game_day);
	USECODE_INTRINSIC_DECL(game_hour);
	USECODE_INTRINSIC_DECL(game_minute);
	USECODE_INTRINSIC_DECL(get_npc_number);
	USECODE_INTRINSIC_DECL(part_of_day);
	USECODE_INTRINSIC_DECL(get_alignment);
	USECODE_INTRINSIC_DECL(set_alignment);
	USECODE_INTRINSIC_DECL(move_object);
	USECODE_INTRINSIC_DECL(remove_npc);
	USECODE_INTRINSIC_DECL(item_say);
	USECODE_INTRINSIC_DECL(clear_item_say);
	USECODE_INTRINSIC_DECL(set_to_attack);
	USECODE_INTRINSIC_DECL(get_lift);
	USECODE_INTRINSIC_DECL(set_lift);
	USECODE_INTRINSIC_DECL(get_weather);
	USECODE_INTRINSIC_DECL(set_weather);
	USECODE_INTRINSIC_DECL(sit_down);
	USECODE_INTRINSIC_DECL(summon);
	USECODE_INTRINSIC_DECL(display_map);
	USECODE_INTRINSIC_DECL(si_display_map);
	USECODE_INTRINSIC_DECL(kill_npc);
	USECODE_INTRINSIC_DECL(roll_to_win);
	USECODE_INTRINSIC_DECL(set_attack_mode);
	USECODE_INTRINSIC_DECL(get_attack_mode);
	USECODE_INTRINSIC_DECL(set_opponent);
	USECODE_INTRINSIC_DECL(clone);
	USECODE_INTRINSIC_DECL(get_oppressor);
	USECODE_INTRINSIC_DECL(set_oppressor);
	USECODE_INTRINSIC_DECL(get_weapon);
	USECODE_INTRINSIC_DECL(display_area);
	USECODE_INTRINSIC_DECL(wizard_eye);
	USECODE_INTRINSIC_DECL(resurrect);
	USECODE_INTRINSIC_DECL(resurrect_npc);
	USECODE_INTRINSIC_DECL(get_body_npc);
	USECODE_INTRINSIC_DECL(add_spell);
	USECODE_INTRINSIC_DECL(sprite_effect);
	USECODE_INTRINSIC_DECL(obj_sprite_effect);
	USECODE_INTRINSIC_DECL(attack_object);
	USECODE_INTRINSIC_DECL(book_mode);
	USECODE_INTRINSIC_DECL(stop_time);
	USECODE_INTRINSIC_DECL(cause_light);
	USECODE_INTRINSIC_DECL(get_barge);
	USECODE_INTRINSIC_DECL(earthquake);
	USECODE_INTRINSIC_DECL(is_pc_female);
	USECODE_INTRINSIC_DECL(armageddon);
	USECODE_INTRINSIC_DECL(halt_scheduled);
	USECODE_INTRINSIC_DECL(lightning);
	USECODE_INTRINSIC_DECL(get_array_size);
	USECODE_INTRINSIC_DECL(mark_virtue_stone);
	USECODE_INTRINSIC_DECL(recall_virtue_stone);
	USECODE_INTRINSIC_DECL(apply_damage);
	USECODE_INTRINSIC_DECL(is_pc_inside);
	USECODE_INTRINSIC_DECL(set_orrery);
	USECODE_INTRINSIC_DECL(get_timer);
	USECODE_INTRINSIC_DECL(set_timer);
	USECODE_INTRINSIC_DECL(wearing_fellowship);
	USECODE_INTRINSIC_DECL(mouse_exists);
	USECODE_INTRINSIC_DECL(get_speech_track);
	USECODE_INTRINSIC_DECL(flash_mouse);
	USECODE_INTRINSIC_DECL(get_item_frame_rot);
	USECODE_INTRINSIC_DECL(set_item_frame_rot);
	USECODE_INTRINSIC_DECL(on_barge);
	USECODE_INTRINSIC_DECL(get_container);
	USECODE_INTRINSIC_DECL(remove_item);
	USECODE_INTRINSIC_DECL(reduce_health);
	USECODE_INTRINSIC_DECL(is_readied);
	USECODE_INTRINSIC_DECL(get_readied);
	USECODE_INTRINSIC_DECL(restart_game);
	USECODE_INTRINSIC_DECL(start_speech);
	USECODE_INTRINSIC_DECL(start_blocking_speech);
	USECODE_INTRINSIC_DECL(is_water);
	USECODE_INTRINSIC_DECL(run_endgame);
	USECODE_INTRINSIC_DECL(fire_projectile);
	USECODE_INTRINSIC_DECL(nap_time);
	USECODE_INTRINSIC_DECL(advance_time);
	USECODE_INTRINSIC_DECL(in_usecode);
	USECODE_INTRINSIC_DECL(call_guards);
	USECODE_INTRINSIC_DECL(attack_avatar);
	USECODE_INTRINSIC_DECL(path_run_usecode);
	USECODE_INTRINSIC_DECL(close_gump);
	USECODE_INTRINSIC_DECL(close_gump2);
	USECODE_INTRINSIC_DECL(close_gumps);
	USECODE_INTRINSIC_DECL(close_gumps2);
	USECODE_INTRINSIC_DECL(in_gump_mode);
	USECODE_INTRINSIC_DECL(set_light);
	USECODE_INTRINSIC_DECL(set_time_palette);
	USECODE_INTRINSIC_DECL(ambient_light);
	USECODE_INTRINSIC_DECL(is_not_blocked);
	USECODE_INTRINSIC_DECL(direction_from);
	USECODE_INTRINSIC_DECL(get_item_flag);
	USECODE_INTRINSIC_DECL(set_item_flag);
	USECODE_INTRINSIC_DECL(clear_item_flag);
	USECODE_INTRINSIC_DECL(set_path_failure);
	USECODE_INTRINSIC_DECL(fade_palette);
	USECODE_INTRINSIC_DECL(fade_palette_sleep);
	USECODE_INTRINSIC_DECL(get_party_list2);
	USECODE_INTRINSIC_DECL(set_camera);
	USECODE_INTRINSIC_DECL(in_combat);
	USECODE_INTRINSIC_DECL(center_view);
	USECODE_INTRINSIC_DECL(view_tile);
	USECODE_INTRINSIC_DECL(get_dead_party);
	USECODE_INTRINSIC_DECL(play_sound_effect);
	USECODE_INTRINSIC_DECL(play_sound_effect2);
	USECODE_INTRINSIC_DECL(get_npc_id);
	USECODE_INTRINSIC_DECL(set_npc_id);
	USECODE_INTRINSIC_DECL(add_cont_items);
	USECODE_INTRINSIC_DECL(remove_cont_items);
	USECODE_INTRINSIC_DECL(error_message);
	// Serpent Isle:
	USECODE_INTRINSIC_DECL(si_path_run_usecode);
	USECODE_INTRINSIC_DECL(can_avatar_reach_pos);
	USECODE_INTRINSIC_DECL(remove_from_area);
	USECODE_INTRINSIC_DECL(infravision);
	USECODE_INTRINSIC_DECL(set_polymorph);
	USECODE_INTRINSIC_DECL(show_npc_face0);
	USECODE_INTRINSIC_DECL(show_npc_face1);
	USECODE_INTRINSIC_DECL(remove_npc_face0);
	USECODE_INTRINSIC_DECL(remove_npc_face1);
	USECODE_INTRINSIC_DECL(set_conversation_slot);
	USECODE_INTRINSIC_DECL(change_npc_face0);
	USECODE_INTRINSIC_DECL(change_npc_face1);
	USECODE_INTRINSIC_DECL(reset_conv_face);
	USECODE_INTRINSIC_DECL(init_conversation);
	USECODE_INTRINSIC_DECL(end_conversation);
	USECODE_INTRINSIC_DECL(stop_arresting);
	USECODE_INTRINSIC_DECL(set_new_schedules);
	USECODE_INTRINSIC_DECL(revert_schedule);
	USECODE_INTRINSIC_DECL(run_schedule);
	USECODE_INTRINSIC_DECL(modify_schedule);
	USECODE_INTRINSIC_DECL(get_temperature);
	USECODE_INTRINSIC_DECL(get_temperature_zone);
	USECODE_INTRINSIC_DECL(set_temperature);
	USECODE_INTRINSIC_DECL(get_npc_warmth);
	//	USECODE_INTRINSIC_DECL(add_removed_npc);
	USECODE_INTRINSIC_DECL(approach_avatar);
	USECODE_INTRINSIC_DECL(set_barge_dir);
	USECODE_INTRINSIC_DECL(remove_all_spells);
	USECODE_INTRINSIC_DECL(telekenesis);
	USECODE_INTRINSIC_DECL(a_or_an);
	USECODE_INTRINSIC_DECL(add_to_keyring);
	USECODE_INTRINSIC_DECL(is_on_keyring);
	USECODE_INTRINSIC_DECL(remove_from_keyring);
	USECODE_INTRINSIC_DECL(save_pos);
	USECODE_INTRINSIC_DECL(teleport_to_saved_pos);
	USECODE_INTRINSIC_DECL(get_item_weight);
	USECODE_INTRINSIC_DECL(get_skin_colour);
	USECODE_INTRINSIC_DECL(printf);
	// SI Beta:
	USECODE_INTRINSIC_DECL(sib_path_run_usecode);
	USECODE_INTRINSIC_DECL(sib_is_dest_reachable);
	// Exult only:
	USECODE_INTRINSIC_DECL(begin_casting_mode);
	USECODE_INTRINSIC_DECL(get_usecode_fun);
	USECODE_INTRINSIC_DECL(get_map_num);
	USECODE_INTRINSIC_DECL(display_map_ex);
	USECODE_INTRINSIC_DECL(book_mode_ex);
	USECODE_INTRINSIC_DECL(is_dest_reachable);
	USECODE_INTRINSIC_DECL(set_npc_name);
	USECODE_INTRINSIC_DECL(set_usecode_fun);
	USECODE_INTRINSIC_DECL(has_spell);
	USECODE_INTRINSIC_DECL(remove_spell);
	USECODE_INTRINSIC_DECL(create_barge_object);
	USECODE_INTRINSIC_DECL(in_usecode_path);
	USECODE_INTRINSIC_DECL(play_scene);

	/*
	 *  Other private methods:
	 */
	// Call instrinsic function.
	Usecode_value     call_intrinsic(int intrinsic, int num_parms);
	void              click_to_continue();       // Wait for user to click.
	void              set_book(Text_gump* b);    // Set book/scroll to display.
	const char*       get_user_choice();         // Get user's choice.
	int               get_user_choice_num();
	void              clear_usevars();
	void              read_usevars();    // Read static variables.
	Usecode_function* find_function(int funcid);

	Game_object* intercept_item = nullptr;
	Tile_coord*  intercept_tile = nullptr;

	// execution functions
	bool call_function(
			int funcid, int event, Game_object* caller = nullptr,
			bool entrypoint = false, bool orig = false, int givenargs = 0);
	void previous_stack_frame();
	void return_from_function(Usecode_value& retval);
	void return_from_procedure();
	void abort_function(Usecode_value& retval);
	int  run();

	// debugging functions
	void uc_trace_disasm(Stack_frame* frame);
	void uc_trace_disasm(
			Usecode_value* locals, int num_locals,
			std::vector<Usecode_value>& locstatics, const uint8* data,
			const uint8* externals, const uint8* code, const uint8* ip);
	static int get_opcode_length(int opcode);
	void       stack_trace(std::ostream& out);
	bool       is_object_fun(int n);

#ifdef USECODE_DEBUGGER
public:
	bool is_on_breakpoint() const {
		return on_breakpoint;
	}

	void set_breakpoint_action(int a) {
		breakpoint_action = a;
	}

	void set_breakpoint();
	int  set_location_breakpoint(int funcid, int ip);

	bool clear_breakpoint(int id) {
		return breakpoints.remove(id);
	}

	void transmit_breakpoints(int fd) {
		breakpoints.transmit(fd);
	}

	void dbg_stepover();
	void dbg_finish();

	int          get_callstack_size() const;
	Stack_frame* get_stackframe(int i);

	int            get_stack_size() const;
	Usecode_value* peek_stack(int depth) const;
	void           poke_stack(int depth, Usecode_value& val);

private:
	Breakpoints breakpoints;

	bool on_breakpoint     = false;    // are we on a breakpoint?
	int  breakpoint_action = -1;       // stay on breakpoint/continue/abort?
#endif

public:
	friend class Usecode_script;
	Usecode_internal();
	~Usecode_internal() override;
	// Read in usecode functions.
	void read_usecode(std::istream& file, bool patch = false) override;
	// Call desired function.
	int  call_usecode(int id, Game_object* item, Usecode_events event) override;
	bool call_method(Usecode_value* inst, int id, Game_object* item) override;
	int  find_function(const char* nm, bool noerr = false) override;
	const char* find_function_name(int funcid) override;
	void        do_speech(int num) override;    // Start speech, or show text.

	bool in_usecode() override {    // Currently in a usecode function?
		return !call_stack.empty();
	}

	bool in_usecode_for(Game_object* item, Usecode_events event) override;
	Usecode_class_symbol* get_class(int n) override;
	Usecode_class_symbol* get_class(const char* nm) override;
	int                   get_shape_fun(int n) override;
	void write() override;    // Write out 'gamedat/usecode.dat'.
	void read() override;     // Read in 'gamedat/usecode.dat'.

	void intercept_click_on_item(Game_object* obj) override {
		intercept_item = obj;
		delete intercept_tile;
		intercept_tile = nullptr;
	}

	Game_object* get_intercept_click_on_item() const override {
		return intercept_item;
	}

	void intercept_click_on_tile(Tile_coord* t) override {
		intercept_item = nullptr;
		delete intercept_tile;
		intercept_tile = t;
	}

	Tile_coord* get_intercept_click_on_tile() const override {
		return intercept_tile;
	}

	void save_intercept(Game_object*& obj, Tile_coord*& t) override {
		obj            = intercept_item;
		t              = intercept_tile;
		intercept_item = nullptr;
		intercept_tile = nullptr;
	}

	void restore_intercept(Game_object* obj, Tile_coord* t) override {
		intercept_item = obj;
		delete intercept_tile;
		intercept_tile = t;
	}

	bool function_exists(int funcid) override {
		Usecode_function* fun = find_function(funcid);
		return fun != nullptr;
	}
};

#endif
