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
#include "span.h"
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

// Minimum supplied argument count, excluding optional trailing parameters.
#define USECODE_INTRINSIC_DECL(NAME, MIN_ARGS)            \
	static constexpr unsigned min_args_##NAME = MIN_ARGS; \
	Usecode_value             UI_##NAME(tcb::span<Usecode_value> parms)

/*
 *  Here's our virtual machine for running usecode.
 */
class Usecode_internal : public Usecode_machine {
	// I'th entry contains funs for ID's
	//    256*i + n.
	using Funs256 = std::vector<Usecode_function*>;
	std::vector<Funs256>                 funs;
	std::vector<Usecode_value>           statics;                   // Global persistent vars.
	Usecode_symbol_table*                symtbl = nullptr;          // (optional) symbol table.
	std::deque<Stack_frame*>             call_stack;                // the call stack
	std::map<Stack_frame*, const uint8*> except_stack;              // the exception handling stack
	Stack_frame*                         frame        = nullptr;    // One intrinsic uses this for now...
	bool                                 modified_map = false;      // We add/deleted/moved an object.
	std::map<int, uint32>                timers;                    // Each has time in hours when set.
	int                                  speech_track = -1;         // Set/read by some intrinsics.
	Text_gump*                           book         = nullptr;    // Book/scroll being displayed.
	Game_object_shared                   caller_item;               // Item this is being called on.
	std::vector<Game_object_shared>      last_created;              // Stack of last items created with
	//   intrins. x24.
	Actor*      path_npc     = nullptr;    // Last NPC in path_run_usecode().
	const char* user_choice  = nullptr;    // String user clicked on.
	bool        found_answer = false;      // Did we already handle the
	//   conversation option?
	Tile_coord saved_pos       = {-1, -1, -1};    // For a couple SI intrinsics.
	int        saved_map       = -1;              // Improvements for these intrinsics.
	char*      String          = nullptr;         // The single string register.
	int        telekenesis_fun = -1;              // For next Usecode call from spell.

	void append_string(const uint8* txt) {
		append_string(reinterpret_cast<const char*>(txt));
	}

	void           append_string(const char* str);    // Append to string.
	void           show_pending_text();               // Make sure user's seen all text.
	void           show_book();                       // "Say" book/scroll text.
	void           say_string();                      // "Say" the string.
	Usecode_value* stack;                             // Stack.
	Usecode_value* sp;                                // Stack ptr.  Grows upwards.
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
	using UsecodeIntrinsicFn = Usecode_value (Usecode_internal::*)(tcb::span<Usecode_value> parms);

	int get_face_shape(Usecode_value& arg1, Actor*& npc, int& frame);

	int get_face_shape(Usecode_value& arg1, Actor*& npc) {
		int frame;
		return get_face_shape(arg1, npc, frame);
	}

	void          show_npc_face(Usecode_value& arg1, Usecode_value& arg2, int slot = -1);
	void          remove_npc_face(Usecode_value& arg1);
	void          set_item_shape(Usecode_value& item_arg, Usecode_value& shape_arg);
	void          set_item_frame(Game_object* item, int frame, int check_empty = 0, int set_rotated = 0);
	void          add_dirty(Game_object* obj);
	void          remove_item(Game_object* obj);
	Usecode_value get_party();
	void          item_say(Usecode_value& objval, Usecode_value& strval);
	void          activate_cached(const Tile_coord& pos);
	Usecode_value find_nearby(Usecode_value& objval, Usecode_value& shapeval, Usecode_value& distval, Usecode_value& mval);
	Usecode_value find_nearest(Usecode_value& objval, Usecode_value& shapeval, Usecode_value& distval);
	Usecode_value find_direction(Usecode_value& from, Usecode_value& to);
	Usecode_value count_objects(Usecode_value& objval, Usecode_value& shapeval, Usecode_value& qualval, Usecode_value& frameval);
	Usecode_value get_objects(Usecode_value& objval, Usecode_value& shapeval, Usecode_value& qualval, Usecode_value& frameval);
	Usecode_value remove_party_items(
			Usecode_value& quantval, Usecode_value& shapeval, Usecode_value& qualval, Usecode_value& frameval);
	Usecode_value add_party_items(
			Usecode_value& quantval, Usecode_value& shapeval, Usecode_value& qualval, Usecode_value& frameval,
			Usecode_value& temporary);
	Usecode_value add_cont_items(
			Usecode_value& container, Usecode_value& quantval, Usecode_value& shapeval, Usecode_value& qualval,
			Usecode_value& frameval, Usecode_value& temporary);
	Usecode_value remove_cont_items(
			Usecode_value& container, Usecode_value& quantval, Usecode_value& shapeval, Usecode_value& qualval,
			Usecode_value& frameval);
	Game_object_shared create_object(int shapenum, bool equip);

	bool path_run_usecode(
			Usecode_value& npcval, Usecode_value& locval, Usecode_value& useval, Usecode_value& itemval, Usecode_value& eventval,
			bool find_free = false, bool always = false, bool companions = false);
	void create_script(Usecode_value& objval, Usecode_value& codeval, long delay);
	bool is_dest_reachable(Actor* npc, const Tile_coord& dest);

	/*
	 *  Embedded intrinsics
	 */

	struct IntrinsicTableEntry {
		UsecodeIntrinsicFn func;
		const char*        name;
		unsigned           min_args;
	};

	static IntrinsicTableEntry intrinsics_bg[];
	static IntrinsicTableEntry intrinsics_si[];
	static IntrinsicTableEntry intrinsics_sib[];

	Usecode_value Execute_Intrinsic(
			UsecodeIntrinsicFn func, const char* name, int intrinsic, tcb::span<Usecode_value> parms);
	USECODE_INTRINSIC_DECL(NOP, 0);
	USECODE_INTRINSIC_DECL(UNKNOWN, 0);
	USECODE_INTRINSIC_DECL(get_random, 1);
	USECODE_INTRINSIC_DECL(execute_usecode_array, 2);
	USECODE_INTRINSIC_DECL(delayed_execute_usecode_array, 3);
	USECODE_INTRINSIC_DECL(show_npc_face, 2);
	USECODE_INTRINSIC_DECL(remove_npc_face, 1);
	USECODE_INTRINSIC_DECL(add_answer, 1);
	USECODE_INTRINSIC_DECL(remove_answer, 1);
	USECODE_INTRINSIC_DECL(push_answers, 0);
	USECODE_INTRINSIC_DECL(pop_answers, 0);
	USECODE_INTRINSIC_DECL(clear_answers, 0);
	USECODE_INTRINSIC_DECL(select_from_menu, 0);
	USECODE_INTRINSIC_DECL(select_from_menu2, 0);
	USECODE_INTRINSIC_DECL(input_numeric_value, 4);
	USECODE_INTRINSIC_DECL(set_item_shape, 2);
	USECODE_INTRINSIC_DECL(find_nearest, 3);
	USECODE_INTRINSIC_DECL(die_roll, 2);
	USECODE_INTRINSIC_DECL(get_item_shape, 1);
	USECODE_INTRINSIC_DECL(get_item_frame, 1);
	USECODE_INTRINSIC_DECL(set_item_frame, 2);
	USECODE_INTRINSIC_DECL(get_item_quality, 1);
	USECODE_INTRINSIC_DECL(set_item_quality, 2);
	USECODE_INTRINSIC_DECL(get_item_quantity, 1);
	USECODE_INTRINSIC_DECL(set_item_quantity, 2);
	USECODE_INTRINSIC_DECL(get_object_position, 1);
	USECODE_INTRINSIC_DECL(get_distance, 2);
	USECODE_INTRINSIC_DECL(find_direction, 2);
	USECODE_INTRINSIC_DECL(get_npc_object, 1);
	USECODE_INTRINSIC_DECL(get_schedule_type, 1);
	USECODE_INTRINSIC_DECL(set_schedule_type, 2);
	USECODE_INTRINSIC_DECL(add_to_party, 1);
	USECODE_INTRINSIC_DECL(remove_from_party, 1);
	USECODE_INTRINSIC_DECL(get_npc_prop, 2);
	USECODE_INTRINSIC_DECL(set_npc_prop, 3);
	USECODE_INTRINSIC_DECL(get_avatar_ref, 0);
	USECODE_INTRINSIC_DECL(get_party_list, 0);
	USECODE_INTRINSIC_DECL(create_new_object, 1);
	USECODE_INTRINSIC_DECL(create_new_object2, 2);
	USECODE_INTRINSIC_DECL(create_new_egg, 7);
	USECODE_INTRINSIC_DECL(set_last_created, 1);
	USECODE_INTRINSIC_DECL(update_last_created, 1);
	USECODE_INTRINSIC_DECL(get_npc_name, 1);
	USECODE_INTRINSIC_DECL(count_objects, 4);
	USECODE_INTRINSIC_DECL(find_object, 4);
	USECODE_INTRINSIC_DECL(get_cont_items, 4);
	USECODE_INTRINSIC_DECL(remove_party_items, 4);
	USECODE_INTRINSIC_DECL(add_party_items, 4);
	USECODE_INTRINSIC_DECL(get_music_track, 0);
	USECODE_INTRINSIC_DECL(play_music, 2);
	USECODE_INTRINSIC_DECL(npc_nearby, 1);
	USECODE_INTRINSIC_DECL(npc_nearby2, 1);
	USECODE_INTRINSIC_DECL(find_nearby_avatar, 1);
	USECODE_INTRINSIC_DECL(is_npc, 1);
	USECODE_INTRINSIC_DECL(display_runes, 2);
	USECODE_INTRINSIC_DECL(click_on_item, 0);
	USECODE_INTRINSIC_DECL(set_intercept_item, 1);
	USECODE_INTRINSIC_DECL(find_nearby, 4);
	USECODE_INTRINSIC_DECL(give_last_created, 1);
	USECODE_INTRINSIC_DECL(is_dead, 1);
	USECODE_INTRINSIC_DECL(game_day, 0);
	USECODE_INTRINSIC_DECL(game_hour, 0);
	USECODE_INTRINSIC_DECL(game_minute, 0);
	USECODE_INTRINSIC_DECL(get_npc_number, 1);
	USECODE_INTRINSIC_DECL(part_of_day, 0);
	USECODE_INTRINSIC_DECL(get_alignment, 1);
	USECODE_INTRINSIC_DECL(set_alignment, 2);
	USECODE_INTRINSIC_DECL(move_object, 2);
	USECODE_INTRINSIC_DECL(remove_npc, 1);
	USECODE_INTRINSIC_DECL(item_say, 2);
	USECODE_INTRINSIC_DECL(clear_item_say, 1);
	USECODE_INTRINSIC_DECL(set_to_attack, 3);
	USECODE_INTRINSIC_DECL(get_lift, 1);
	USECODE_INTRINSIC_DECL(set_lift, 2);
	USECODE_INTRINSIC_DECL(get_weather, 0);
	USECODE_INTRINSIC_DECL(set_weather, 1);
	USECODE_INTRINSIC_DECL(sit_down, 2);
	USECODE_INTRINSIC_DECL(summon, 1);
	USECODE_INTRINSIC_DECL(display_map, 0);
	USECODE_INTRINSIC_DECL(si_display_map, 1);
	USECODE_INTRINSIC_DECL(kill_npc, 1);
	USECODE_INTRINSIC_DECL(roll_to_win, 2);
	USECODE_INTRINSIC_DECL(set_attack_mode, 2);
	USECODE_INTRINSIC_DECL(get_attack_mode, 1);
	USECODE_INTRINSIC_DECL(set_opponent, 2);
	USECODE_INTRINSIC_DECL(clone, 1);
	USECODE_INTRINSIC_DECL(get_oppressor, 1);
	USECODE_INTRINSIC_DECL(set_oppressor, 2);
	USECODE_INTRINSIC_DECL(get_weapon, 1);
	USECODE_INTRINSIC_DECL(display_area, 1);
	USECODE_INTRINSIC_DECL(wizard_eye, 1);
	USECODE_INTRINSIC_DECL(resurrect, 1);
	USECODE_INTRINSIC_DECL(resurrect_npc, 1);
	USECODE_INTRINSIC_DECL(get_body_npc, 1);
	USECODE_INTRINSIC_DECL(add_spell, 3);
	USECODE_INTRINSIC_DECL(sprite_effect, 7);
	USECODE_INTRINSIC_DECL(obj_sprite_effect, 8);
	USECODE_INTRINSIC_DECL(attack_object, 3);
	USECODE_INTRINSIC_DECL(book_mode, 1);
	USECODE_INTRINSIC_DECL(stop_time, 1);
	USECODE_INTRINSIC_DECL(cause_light, 1);
	USECODE_INTRINSIC_DECL(get_barge, 1);
	USECODE_INTRINSIC_DECL(earthquake, 1);
	USECODE_INTRINSIC_DECL(is_pc_female, 0);
	USECODE_INTRINSIC_DECL(armageddon, 0);
	USECODE_INTRINSIC_DECL(halt_scheduled, 1);
	USECODE_INTRINSIC_DECL(lightning, 0);
	USECODE_INTRINSIC_DECL(get_array_size, 1);
	USECODE_INTRINSIC_DECL(mark_virtue_stone, 1);
	USECODE_INTRINSIC_DECL(recall_virtue_stone, 1);
	USECODE_INTRINSIC_DECL(apply_damage, 4);
	USECODE_INTRINSIC_DECL(is_pc_inside, 0);
	USECODE_INTRINSIC_DECL(set_orrery, 2);
	USECODE_INTRINSIC_DECL(get_timer, 1);
	USECODE_INTRINSIC_DECL(set_timer, 1);
	USECODE_INTRINSIC_DECL(wearing_fellowship, 0);
	USECODE_INTRINSIC_DECL(mouse_exists, 0);
	USECODE_INTRINSIC_DECL(get_speech_track, 0);
	USECODE_INTRINSIC_DECL(flash_mouse, 1);
	USECODE_INTRINSIC_DECL(get_item_frame_rot, 1);
	USECODE_INTRINSIC_DECL(set_item_frame_rot, 2);
	USECODE_INTRINSIC_DECL(on_barge, 0);
	USECODE_INTRINSIC_DECL(get_container, 1);
	USECODE_INTRINSIC_DECL(remove_item, 1);
	USECODE_INTRINSIC_DECL(reduce_health, 3);
	USECODE_INTRINSIC_DECL(is_readied, 4);
	USECODE_INTRINSIC_DECL(get_readied, 2);
	USECODE_INTRINSIC_DECL(restart_game, 0);
	USECODE_INTRINSIC_DECL(start_speech, 1);
	USECODE_INTRINSIC_DECL(start_blocking_speech, 1);
	USECODE_INTRINSIC_DECL(is_water, 1);
	USECODE_INTRINSIC_DECL(get_shape_flag, 2);
	USECODE_INTRINSIC_DECL(run_endgame, 1);
	USECODE_INTRINSIC_DECL(fire_projectile, 6);
	USECODE_INTRINSIC_DECL(nap_time, 1);
	USECODE_INTRINSIC_DECL(advance_time, 1);
	USECODE_INTRINSIC_DECL(in_usecode, 1);
	USECODE_INTRINSIC_DECL(call_guards, 0);
	USECODE_INTRINSIC_DECL(attack_avatar, 0);
	USECODE_INTRINSIC_DECL(path_run_usecode, 4);
	USECODE_INTRINSIC_DECL(close_gump, 1);
	USECODE_INTRINSIC_DECL(close_gump2, 1);
	USECODE_INTRINSIC_DECL(close_gumps, 0);
	USECODE_INTRINSIC_DECL(close_gumps2, 0);
	USECODE_INTRINSIC_DECL(in_gump_mode, 0);
	USECODE_INTRINSIC_DECL(set_light, 2);
	USECODE_INTRINSIC_DECL(set_time_palette, 0);
	USECODE_INTRINSIC_DECL(ambient_light, 1);
	USECODE_INTRINSIC_DECL(is_not_blocked, 3);
	USECODE_INTRINSIC_DECL(direction_from, 2);
	USECODE_INTRINSIC_DECL(get_item_flag, 2);
	USECODE_INTRINSIC_DECL(set_item_flag, 2);
	USECODE_INTRINSIC_DECL(clear_item_flag, 2);
	USECODE_INTRINSIC_DECL(set_path_failure, 3);
	USECODE_INTRINSIC_DECL(fade_palette, 3);
	USECODE_INTRINSIC_DECL(fade_palette_sleep, 3);
	USECODE_INTRINSIC_DECL(get_party_list2, 0);
	USECODE_INTRINSIC_DECL(set_camera, 1);
	USECODE_INTRINSIC_DECL(in_combat, 0);
	USECODE_INTRINSIC_DECL(center_view, 1);
	USECODE_INTRINSIC_DECL(view_tile, 1);
	USECODE_INTRINSIC_DECL(get_dead_party, 1);
	USECODE_INTRINSIC_DECL(play_sound_effect, 1);
	USECODE_INTRINSIC_DECL(play_sound_effect2, 2);
	USECODE_INTRINSIC_DECL(get_npc_id, 1);
	USECODE_INTRINSIC_DECL(set_npc_id, 2);
	USECODE_INTRINSIC_DECL(add_cont_items, 5);
	USECODE_INTRINSIC_DECL(remove_cont_items, 5);
	USECODE_INTRINSIC_DECL(error_message, 0);
	// Serpent Isle:
	USECODE_INTRINSIC_DECL(si_path_run_usecode, 5);
	USECODE_INTRINSIC_DECL(can_avatar_reach_pos, 1);
	USECODE_INTRINSIC_DECL(remove_from_area, 4);
	USECODE_INTRINSIC_DECL(infravision, 2);
	USECODE_INTRINSIC_DECL(set_polymorph, 2);
	USECODE_INTRINSIC_DECL(show_npc_face0, 2);
	USECODE_INTRINSIC_DECL(show_npc_face1, 2);
	USECODE_INTRINSIC_DECL(remove_npc_face0, 0);
	USECODE_INTRINSIC_DECL(remove_npc_face1, 0);
	USECODE_INTRINSIC_DECL(set_conversation_slot, 1);
	USECODE_INTRINSIC_DECL(change_npc_face0, 1);
	USECODE_INTRINSIC_DECL(change_npc_face1, 1);
	USECODE_INTRINSIC_DECL(reset_conv_face, 0);
	USECODE_INTRINSIC_DECL(init_conversation, 0);
	USECODE_INTRINSIC_DECL(end_conversation, 0);
	USECODE_INTRINSIC_DECL(stop_arresting, 0);
	USECODE_INTRINSIC_DECL(set_new_schedules, 4);
	USECODE_INTRINSIC_DECL(revert_schedule, 1);
	USECODE_INTRINSIC_DECL(run_schedule, 1);
	USECODE_INTRINSIC_DECL(modify_schedule, 4);
	USECODE_INTRINSIC_DECL(get_temperature, 1);
	USECODE_INTRINSIC_DECL(get_temperature_zone, 1);
	USECODE_INTRINSIC_DECL(set_temperature, 2);
	USECODE_INTRINSIC_DECL(get_npc_warmth, 1);
	//	USECODE_INTRINSIC_DECL(add_removed_npc, 3);
	USECODE_INTRINSIC_DECL(approach_avatar, 1);
	USECODE_INTRINSIC_DECL(set_barge_dir, 2);
	USECODE_INTRINSIC_DECL(remove_all_spells, 1);
	USECODE_INTRINSIC_DECL(telekenesis, 1);
	USECODE_INTRINSIC_DECL(a_or_an, 1);
	USECODE_INTRINSIC_DECL(add_to_keyring, 1);
	USECODE_INTRINSIC_DECL(is_on_keyring, 1);
	USECODE_INTRINSIC_DECL(remove_from_keyring, 1);
	USECODE_INTRINSIC_DECL(save_pos, 1);
	USECODE_INTRINSIC_DECL(teleport_to_saved_pos, 1);
	USECODE_INTRINSIC_DECL(get_item_weight, 1);
	USECODE_INTRINSIC_DECL(get_skin_colour, 0);
	USECODE_INTRINSIC_DECL(printf, 1);
	// SI Beta:
	USECODE_INTRINSIC_DECL(sib_path_run_usecode, 5);
	USECODE_INTRINSIC_DECL(sib_is_dest_reachable, 2);
	// Exult only:
	USECODE_INTRINSIC_DECL(begin_casting_mode, 1);
	USECODE_INTRINSIC_DECL(get_usecode_fun, 1);
	USECODE_INTRINSIC_DECL(get_map_num, 1);
	USECODE_INTRINSIC_DECL(display_map_ex, 2);
	USECODE_INTRINSIC_DECL(book_mode_ex, 2);
	USECODE_INTRINSIC_DECL(is_dest_reachable, 2);
	USECODE_INTRINSIC_DECL(set_npc_name, 2);
	USECODE_INTRINSIC_DECL(set_usecode_fun, 2);
	USECODE_INTRINSIC_DECL(has_spell, 2);
	USECODE_INTRINSIC_DECL(remove_spell, 2);
	USECODE_INTRINSIC_DECL(create_barge_object, 2);
	USECODE_INTRINSIC_DECL(in_usecode_path, 1);
	USECODE_INTRINSIC_DECL(play_scene, 1);
	USECODE_INTRINSIC_DECL(get_item_gump_position, 1);
	USECODE_INTRINSIC_DECL(set_item_gump_position, 3);
	USECODE_INTRINSIC_DECL(set_gump_text, 3);
	USECODE_INTRINSIC_DECL(set_gump_shape, 4);
	USECODE_INTRINSIC_DECL(set_slider_value, 3);
	USECODE_INTRINSIC_DECL(get_slider_value, 2);
	USECODE_INTRINSIC_DECL(set_gump_text_font, 3);

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
			int funcid, int event, Game_object* caller = nullptr, bool entrypoint = false, bool orig = false, int givenargs = 0);
	void previous_stack_frame();
	void return_from_function(Usecode_value& retval);
	void return_from_procedure();
	void abort_function(Usecode_value& retval);
	int  run();

	// debugging functions
	void uc_trace_disasm(Stack_frame* frame);
	void uc_trace_disasm(
			Usecode_value* locals, int num_locals, std::vector<Usecode_value>& locstatics, const uint8* data,
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
	void read_usecode() override;
	// Call desired function.
	int         call_usecode(int id, Game_object* item, Usecode_events event) override;
	bool        call_method(Usecode_value* inst, int id, Game_object* item) override;
	int         find_function(const char* nm, bool noerr = false) override;
	const char* find_function_name(int funcid) override;
	void        do_speech(int num) override;    // Start speech, or show text.

	bool in_usecode() override {    // Currently in a usecode function?
		return !call_stack.empty();
	}

	bool                  in_usecode_for(Game_object* item, Usecode_events event) override;
	Usecode_class_symbol* get_class(int n) override;
	Usecode_class_symbol* get_class(const char* nm) override;
	int                   get_shape_fun(int n) override;
	void                  write() override;    // Write out 'gamedat/usecode.dat'.
	void                  read() override;     // Read in 'gamedat/usecode.dat'.

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
	int get_num_funcs() override {
		if (funs.empty()) {
			return 0;
		}
		return (funs.size()-1) * 0x100 + funs.back().size();
	}
};

#endif
