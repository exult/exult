/*
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *\
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Audio.h"
#include "Book_gump.h"
#include "Gump.h"
#include "Gump_manager.h"
#include "Scroll_gump.h"
#include "Sign_gump.h"
#include "actions.h"
#include "animate.h"
#include "barge.h"
#include "cheat.h"
#include "chunks.h"
#include "combat.h"
#include "conversation.h"
#include "effects.h"
#include "egg.h"
#include "exult.h"
#include "frflags.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "ignore_unused_variable_warning.h"
#include "items.h"
#include "keyring.h"
#include "monsters.h"
#include "monstinf.h"
#include "mouse.h"
#include "palette.h"
#include "party.h"
#include "ready.h"
#include "rect.h"
#include "schedule.h"
#include "spellbook.h"
#include "stackframe.h"
#include "touchui.h"
#include "ucfunction.h"
#include "ucinternal.h"
#include "ucsched.h"
#include "ucscriptop.h"
#include "ucsymtbl.h"
#include "useval.h"
#include "virstone.h"

#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <string_view>

using std::cerr;
using std::cout;
using std::endl;
using std::rand;
using std::strchr;

extern Usecode_value no_ret;

static Game_object* sailor = nullptr;    // The current barge captain.  Maybe
//   this needs to be saved/restored.

#define USECODE_INTRINSIC(NAME)                \
	Usecode_value Usecode_internal::UI_##NAME( \
			int num_parms, Usecode_value parms[12])

USECODE_INTRINSIC(NOP) {
	ignore_unused_variable_warning(num_parms, parms);
	return no_ret;
}

USECODE_INTRINSIC(UNKNOWN) {
	ignore_unused_variable_warning(num_parms, parms);
	//	Unhandled(num_parms, parms);
	return no_ret;
}

USECODE_INTRINSIC(get_random) {
	ignore_unused_variable_warning(num_parms);
	const int range = parms[0].get_int_value();
	if (range == 0) {
		Usecode_value u(0);
		return u;
	}
	Usecode_value u(1 + (rand() % range));
	return u;
}

USECODE_INTRINSIC(execute_usecode_array) {
	ignore_unused_variable_warning(num_parms);
	COUT("Executing intrinsic 1");
	// Start on next tick.
	create_script(parms[0], parms[1], gwin->get_std_delay());

	Usecode_value u(1);
	return u;
}

USECODE_INTRINSIC(delayed_execute_usecode_array) {
	ignore_unused_variable_warning(num_parms);
	// Delay = .20 sec.?
	// +++++Special problem with inf. loop:
	if (Game::get_game_type() == BLACK_GATE && frame->eventid == internal_exec
		&& parms[1].get_array_size() == 3
		&& parms[1].get_elem(2).get_int_value() == 0x6f7) {
		return no_ret;
	}
	const int delay = parms[2].get_int_value();
	create_script(parms[0], parms[1], delay * gwin->get_std_delay());
	COUT("Executing intrinsic 2");

	Usecode_value u(1);
	return u;
}

USECODE_INTRINSIC(show_npc_face) {
	ignore_unused_variable_warning(num_parms);
	show_npc_face(parms[0], parms[1]);
	return no_ret;
}

USECODE_INTRINSIC(remove_npc_face) {
	ignore_unused_variable_warning(num_parms);
	remove_npc_face(parms[0]);
	return no_ret;
}

USECODE_INTRINSIC(add_answer) {
	ignore_unused_variable_warning(num_parms);
	conv->add_answer(parms[0]);
	//  user_choice = 0;
	return no_ret;
}

USECODE_INTRINSIC(remove_answer) {
	ignore_unused_variable_warning(num_parms);
	conv->remove_answer(parms[0]);
	// Commented out 'user_choice = 0' 8/3/00 for Tseramed conversation.
	//	user_choice = 0;
	return no_ret;
}

USECODE_INTRINSIC(push_answers) {
	ignore_unused_variable_warning(num_parms, parms);
	conv->push_answers();
	return no_ret;
}

USECODE_INTRINSIC(pop_answers) {
	ignore_unused_variable_warning(num_parms, parms);
	if (!conv->stack_empty()) {
		conv->pop_answers();
		delete[] user_choice;
		user_choice = nullptr;    // Added 7/24/2000.
	}
	return no_ret;
}

USECODE_INTRINSIC(clear_answers) {
	ignore_unused_variable_warning(num_parms, parms);
	conv->clear_answers();
	return no_ret;
}

USECODE_INTRINSIC(select_from_menu) {
	ignore_unused_variable_warning(num_parms, parms);
	delete[] user_choice;
	user_choice = nullptr;
	Usecode_value u(get_user_choice());
	delete[] user_choice;
	user_choice = nullptr;
	return u;
}

USECODE_INTRINSIC(select_from_menu2) {
	ignore_unused_variable_warning(num_parms, parms);
	// Return index (1-n) of choice.
	delete[] user_choice;
	user_choice = nullptr;
	Usecode_value val(get_user_choice_num() + 1);
	delete[] user_choice;
	user_choice = nullptr;
	return val;
}

USECODE_INTRINSIC(input_numeric_value) {
	ignore_unused_variable_warning(num_parms);
	// Ask for # (min, max, step, default).  Be sure to show conversation.
	Usecode_value ret(gumpman->prompt_for_number(
			parms[0].get_int_value(), parms[1].get_int_value(),
			parms[2].get_int_value(), parms[3].get_int_value(), conv));
	conv->clear_text_pending();    // Answered a question.
	return ret;
}

USECODE_INTRINSIC(set_item_shape) {
	ignore_unused_variable_warning(num_parms);
	// Set item shape.
	set_item_shape(parms[0], parms[1]);
	return no_ret;
}

USECODE_INTRINSIC(find_nearest) {
	ignore_unused_variable_warning(num_parms);
	// Think it rets. nearest obj. near parm0.
	Usecode_value u(find_nearest(parms[0], parms[1], parms[2]));
	return u;
}

USECODE_INTRINSIC(die_roll) {
	ignore_unused_variable_warning(num_parms);
	// Rand. # within range.
	int low  = parms[0].get_int_value();
	int high = parms[1].get_int_value();
	if (low > high) {
		const int tmp = low;
		low           = high;
		high          = tmp;
	}
	const int     val = (rand() % (high - low + 1)) + low;
	Usecode_value u(val);
	return u;
}

USECODE_INTRINSIC(get_item_shape) {
	ignore_unused_variable_warning(num_parms);
	Game_object* item = get_item(parms[0]);
	// Want the actual, not polymorph'd.
	Actor* act = as_actor(item);
	return Usecode_value(
			item == nullptr
					? 0
					: (act ? act->get_shape_real() : item->get_shapenum()));
}

USECODE_INTRINSIC(get_item_frame) {
	ignore_unused_variable_warning(num_parms);
	// Returns frame without rotated bit.
	Game_object* item = get_item(parms[0]);
	// Don't count rotated frames.
	return Usecode_value(item == nullptr ? 0 : item->get_framenum() & 31);
}

USECODE_INTRINSIC(set_item_frame) {
	ignore_unused_variable_warning(num_parms);
	// Set frame, but don't change rotated bit.
	//++++++++Seems like in BG, this should be the same as
	// set_item_frame_rot()??
	set_item_frame(get_item(parms[0]), parms[1].get_int_value());
	return no_ret;
}

USECODE_INTRINSIC(get_item_quality) {
	ignore_unused_variable_warning(num_parms);
	Game_object* obj = get_item(parms[0]);
	if (!obj) {
		return Usecode_value(0);
	}
	const Shape_info& info = obj->get_info();
	return Usecode_value(info.has_quality() ? obj->get_quality() : 0);
}

USECODE_INTRINSIC(set_item_quality) {
	ignore_unused_variable_warning(num_parms);
	// Guessing it's
	//  set_quality(item, value).
	const int qual = parms[1].get_int_value();
	if (qual == c_any_qual) {    // Leave alone (happens in SI)?
		return Usecode_value(1);
	}
	Game_object* obj = get_item(parms[0]);
	if (obj) {
		const Shape_info& info = obj->get_info();
		if (info.has_quality()) {
			obj->set_quality(static_cast<unsigned int>(qual));
			return Usecode_value(1);
		}
	}
	return Usecode_value(0);
}

USECODE_INTRINSIC(get_item_quantity) {
	ignore_unused_variable_warning(num_parms);
	// Get quantity of an item.
	//   Get_quantity(item, mystery).
	Usecode_value ret(0);
	Game_object*  obj = get_item(parms[0]);
	if (obj) {
		ret = Usecode_value(obj->get_quantity());
	}
	return ret;
}

USECODE_INTRINSIC(set_item_quantity) {
	ignore_unused_variable_warning(num_parms);
	// Set_quantity (item, newcount).  Rets 1 iff item.has_quantity().
	Usecode_value ret(0);
	Game_object*  obj      = get_item(parms[0]);
	const int     newquant = parms[1].get_int_value();
	if (obj && obj->get_info().has_quantity()) {
		ret = Usecode_value(1);
		// If not in world, don't delete!
		if (newquant == 0 && obj->get_cx() == 255) {
			return ret;
		}
		const int oldquant = obj->get_quantity();
		const int delta    = newquant - oldquant;
		// Note:  This can delete the obj.
		obj->modify_quantity(delta);
	}
	return ret;
}

USECODE_INTRINSIC(get_object_position) {
	ignore_unused_variable_warning(num_parms);
	// Takes itemref.  ?Think it rets.
	//  hotspot coords: (x, y, z).
	Game_object* obj = get_item(parms[0]);
	Tile_coord   c(0, 0, 0);
	if (obj) {    // (Watch for animated objs' wiggles.)
		c = obj->get_outermost()->get_original_tile_coord();
	}
	Usecode_value vx(c.tx);
	Usecode_value vy(c.ty);
	Usecode_value vz(c.tz);
	Usecode_value arr(3, &vx);
	arr.put_elem(1, vy);
	arr.put_elem(2, vz);
	return arr;
}

USECODE_INTRINSIC(get_distance) {
	ignore_unused_variable_warning(num_parms);
	// Distance from parm[0] -> parm[1].  Guessing how it's computed.
	Game_object*  obj0 = get_item(parms[0]);
	Game_object*  obj1 = get_item(parms[1]);
	Usecode_value u(
			(obj0 && obj1)
					? obj0->get_outermost()->distance(obj1->get_outermost())
					: 0);
	return u;
}

USECODE_INTRINSIC(find_direction) {
	ignore_unused_variable_warning(num_parms);
	// Direction from parm[0] -> parm[1].
	// Rets. 0-7.  Is 0 east?
	Usecode_value u = find_direction(parms[0], parms[1]);
	return u;
}

USECODE_INTRINSIC(get_npc_object) {
	ignore_unused_variable_warning(num_parms);
	// Takes -npc.  Returns object, or array of objects.
	Usecode_value& v = parms[0];
	if (v.is_array()) {    // Do it for each element of array.
		const int     sz = v.get_array_size();
		Usecode_value ret(sz, nullptr);
		for (int i = 0; i < sz; i++) {
			Usecode_value elem(get_item(v.get_elem(i)));
			ret.put_elem(i, elem);
		}
		return ret;
	}
	Game_object*  obj = get_item(parms[0]);
	Usecode_value u(obj);
	return u;
}

USECODE_INTRINSIC(get_schedule_type) {
	ignore_unused_variable_warning(num_parms);
	// GetSchedule(npc).  Rets. schedtype.
	Actor* npc = as_actor(get_item(parms[0]));
	if (!npc) {
		return Usecode_value(0);
	}
	Schedule* schedule = npc->get_schedule();
	int       sched    = schedule ? schedule->get_actual_type(npc)
								  : npc->get_schedule_type();
	// Path_run_usecode?  (This is to fix
	//   a bug in the Fawn Trial.)
	//+++++Should be a better way to check.
	if (Game::get_game_type() == SERPENT_ISLE && npc->get_action()
		&& npc->get_action()->as_usecode_path()) {
		// Give a 'fake' schedule.
		sched = Schedule::walk_to_schedule;
	}
	Usecode_value u(sched);
	return u;
}

USECODE_INTRINSIC(set_schedule_type) {
	ignore_unused_variable_warning(num_parms);
	// SetSchedule?(npc, schedtype).
	// Looks like 15=wait here, 11=go home, 0=train/fight... This is the
	// 'bNum' field in schedules.
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc) {
		const int newsched = parms[1].get_int_value();
		npc->set_schedule_type(newsched);
		// Taking Avatar out of combat?
		if (npc == gwin->get_main_actor() && gwin->in_combat()
			&& newsched != Schedule::combat)
		// End combat mode (for L.Field).
		{
			Audio::get_ptr()->stop_music();
			gwin->toggle_combat();
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(add_to_party) {
	ignore_unused_variable_warning(num_parms);
	// NPC joins party.
	Actor* npc = as_actor(get_item(parms[0]));
	if (!partyman->add_to_party(npc)) {
		return no_ret;    // Can't add.
	}
	npc->set_schedule_type(Schedule::follow_avatar);
	npc->set_alignment(Actor::good);
	// cout << "NPC " << npc->get_npc_num() << " added to party." << endl;
	return no_ret;
}

USECODE_INTRINSIC(remove_from_party) {
	ignore_unused_variable_warning(num_parms);
	// NPC leaves party.
	Actor* npc = as_actor(get_item(parms[0]));
	if (partyman->remove_from_party(npc)) {
		npc->set_alignment(Actor::neutral);
	}
	return no_ret;
}

USECODE_INTRINSIC(get_npc_prop) {
	ignore_unused_variable_warning(num_parms);
	// Get NPC prop (item, prop_id).
	Game_object* obj = get_item(parms[0]);
	Actor*       npc = as_actor(obj);
	if (!obj) {
		return Usecode_value(0);
	}
	if (!npc) {
		if (parms[1].get_int_value() == static_cast<int>(Actor::health)) {
			return Usecode_value(obj->get_obj_hp());
		}
		return Usecode_value(0);
	}
	const char* att = parms[1].get_str_value();
	if (att) {
		return Usecode_value(npc->get_attribute(att));
	} else {
		return Usecode_value(npc->get_property(parms[1].get_int_value()));
	}
}

USECODE_INTRINSIC(set_npc_prop) {
	ignore_unused_variable_warning(num_parms);
	// Set NPC prop (item, prop_id, delta_value).
	Game_object* obj = get_item(parms[0]);
	Actor*       npc = as_actor(obj);
	if (npc) {
		// NOTE: 3rd parm. is a delta!
		const char* att = parms[1].get_str_value();
		if (att) {
			npc->set_attribute(
					att, npc->get_attribute(att) + parms[2].get_int_value());
		} else {
			const int prop  = parms[1].get_int_value();
			int       delta = parms[2].get_int_value();
			if (prop == static_cast<int>(Actor::exp)) {
				if (delta < 0) {
					// There is a bug in Silver Seed when you plant the silver
					// seed with more than 25600 exp: the intended exp bonus is
					// 0xC800, which gets misinterpreted as -0x3800.
					// In the original game, there seems to be a check that
					// prevents negative exp bonuses from usecode; we will
					// instead fix this here.
					// If this is a 16-bit negative value, make it positive.
					// Otherwise, bail out.
					if (delta >= std::numeric_limits<int16_t>::min()) {
						delta = static_cast<uint16_t>(delta);
					} else {
						return Usecode_value(1);
					}
				}
				delta /= 2;    // Verified.
			}
			if (prop != static_cast<int>(Actor::sex_flag)) {
				delta += npc->get_property(prop);    // NOT for gender.
			}
			npc->set_property(prop, delta);
		}
		return Usecode_value(1);    // SI needs return.
	} else if (obj) {
		// Verified. Needed by serpent statue at end of SI.
		const int prop  = parms[1].get_int_value();
		const int delta = parms[2].get_int_value();
		if (prop == static_cast<int>(Actor::health)) {
			obj->set_obj_hp(obj->get_obj_hp() + delta);
			return Usecode_value(1);
		}
	}
	return Usecode_value(0);
}

USECODE_INTRINSIC(get_avatar_ref) {
	ignore_unused_variable_warning(num_parms, parms);
	// Guessing it's Avatar's itemref.
	Usecode_value u(gwin->get_main_actor());
	return u;
}

USECODE_INTRINSIC(get_party_list) {
	ignore_unused_variable_warning(num_parms, parms);
	// Return array with party members.
	Usecode_value u(get_party());
	return u;
}

USECODE_INTRINSIC(create_new_object) {
	ignore_unused_variable_warning(num_parms);
	// create_new_object(shapenum).   Stores it in 'last_created'.
	const int                shapenum = parms[0].get_int_value();
	const Game_object_shared obj      = create_object(shapenum, false);
	Usecode_value            u(obj.get());
	return u;
}

USECODE_INTRINSIC(create_new_object2) {
	ignore_unused_variable_warning(num_parms);
	// create_new_object(shapenum, loc).
	// Pretty sure this is for creating monsters with equipment!

	const int shapenum = parms[0].get_int_value();
	// Create, and equip if monster.
	const Game_object_shared obj = create_object(shapenum, true);
	if (obj) {
		UI_update_last_created(1, &parms[1]);
	}
	Usecode_value u(obj);
	return u;
}

USECODE_INTRINSIC(set_last_created) {
	ignore_unused_variable_warning(num_parms);
	// Take itemref off map and set last_created to it.
	Game_object* obj = get_item(parms[0]);
	// Don't do it for same object if already there.
	for (auto& it : last_created) {
		if (it.get() == obj) {
			return Usecode_value(0);
		}
	}
	modified_map = true;
	Game_object_shared keep;
	if (obj) {
		add_dirty(obj);    // Set to repaint area.
		last_created.push_back(obj->shared_from_this());
		obj->remove_this(&keep);    // Remove, but don't delete.
	}
	Usecode_value u(obj);
	return u;
}

USECODE_INTRINSIC(update_last_created) {
	ignore_unused_variable_warning(num_parms);
	// Think it takes array from 0x18,
	//   updates last-created object.
	//   ??guessing??
	modified_map = true;
	if (last_created.empty()) {
		Usecode_value u(0);
		return u;
	}
	const Game_object_shared obj = last_created.back();
	last_created.pop_back();
	obj->set_invalid();    // It's already been removed.
	Usecode_value& arr = parms[0];
	const size_t   sz  = arr.get_array_size();
	if (sz >= 2) {
		// arr is loc (x, y, z, map) if sz == 4,
		//(x, y, z) for sz == 3 and (x, y) for sz == 2
		const Tile_coord dest(
				arr.get_elem(0).get_int_value(),
				arr.get_elem(1).get_int_value(),
				sz >= 3 ? arr.get_elem(2).get_int_value() : 0);
		obj->move(
				dest.tx, dest.ty, dest.tz,
				sz < 4 ? -1 : arr.get_elem(3).get_int_value());
	} else if (sz == 1) {
		obj->remove_this();
	}
#ifdef DEBUG
	else {
		cout << " { Intrinsic 0x26:  ";
		arr.print(cout);
		cout << endl << "} ";
	}
#endif
	//	gwin->paint_dirty(); // Problems in conversations.
	//	gwin->show();        // ??
	Usecode_value u(1);
	return u;
}

USECODE_INTRINSIC(get_npc_name) {
	ignore_unused_variable_warning(num_parms);
	// Get NPC name(s).  Works on arrays, too.
	static const char* unknown = "??name??";

	auto get_name = [&](const Usecode_value& val) -> std::string {
		Game_object* obj = get_item(val);
		if (obj != nullptr) {
			Actor *npc = as_actor(obj);
			return npc != nullptr ? npc->get_npc_name() : obj->get_name();
		}
		return unknown;
	};

	const int     cnt = parms[0].get_array_size();
	Usecode_value ret;
	if (cnt != 0) {
		// Do array.
		ret = Usecode_value(0, nullptr);
		int size = 0;
		for (int i = 0; i < cnt; i++) {
			const std::string name = get_name(parms[0].get_elem(i));
			if (name != unknown) {
				Usecode_value v(name);
				ret.resize(size + 1);
				ret.put_elem(size, v);
				size++;
			}
		}
		return ret;
	}
	ret = get_name(parms[0]);
	return ret;
}

USECODE_INTRINSIC(set_npc_name) {
	ignore_unused_variable_warning(num_parms);
	// Set NPC name.
	Actor* npc = as_actor(get_item(parms[0]));
	if (!npc) {
		return no_ret;
	}
	npc->set_npc_name(parms[1].get_str_value());
	return no_ret;
}

USECODE_INTRINSIC(count_objects) {
	ignore_unused_variable_warning(num_parms);
	// How many?
	// ((npc?-357==party, -356=avatar),
	//   item, quality, frame (c_any_framenum = any)).
	// Quality/frame -359 means any.
	Usecode_value u(count_objects(parms[0], parms[1], parms[2], parms[3]));
	return u;
}

USECODE_INTRINSIC(find_object) {
	ignore_unused_variable_warning(num_parms);
	// Find_object(container(-357=party) OR loc, shapenum, qual?? (-359=any),
	//                      frame??(-359=any)).
	const int shnum = parms[1].get_int_value();
	const int qual  = parms[2].get_int_value();
	const int frnum = parms[3].get_int_value();
	if (parms[0].get_array_size() == 3) {
		// Location (x, y).
		Game_object_vector vec;
		Game_object::find_nearby(
				vec,
				Tile_coord(
						parms[0].get_elem(0).get_int_value(),
						parms[0].get_elem(1).get_int_value(),
						parms[0].get_elem(2).get_int_value()),
				shnum, 1, 0, qual, frnum);
		if (vec.empty()) {
			return Usecode_value(static_cast<Game_object*>(nullptr));
		} else {
			return Usecode_value(vec.front());
		}
	}
	const int oval = parms[0].get_int_value();
	if (oval == -359) {    // Find on map (?)
		Game_object_vector vec;
		const TileRect     scr = gwin->get_win_tile_rect();
		Game_object::find_nearby(
				vec, Tile_coord(scr.x + scr.w / 2, scr.y + scr.h / 2, 0), shnum,
				scr.h / 2, 0, qual, frnum);
		return vec.empty() ? Usecode_value(static_cast<Game_object*>(nullptr))
						   : Usecode_value(vec[0]);
	}
	if (oval != -357) {    // Not the whole party?
		// Find inside owner.
		Game_object* obj = get_item(parms[0]);
		if (!obj) {
			return Usecode_value(static_cast<Game_object*>(nullptr));
		}
		Game_object* f = obj->find_item(shnum, qual, frnum);
		return Usecode_value(f);
	}
	// Look through whole party.
	Usecode_value party = get_party();
	const int     cnt   = party.get_array_size();
	for (int i = 0; i < cnt; i++) {
		Game_object* obj = get_item(party.get_elem(i));
		if (obj) {
			Game_object* f = obj->find_item(shnum, qual, frnum);
			if (f) {
				return Usecode_value(f);
			}
		}
	}
	return Usecode_value(static_cast<Game_object*>(nullptr));
}

USECODE_INTRINSIC(get_cont_items) {
	ignore_unused_variable_warning(num_parms);
	// Get cont. items(container, shape, qual, frame).
	// recursively find items in container
	Usecode_value u(get_objects(parms[0], parms[1], parms[2], parms[3]));
	return u;
}

USECODE_INTRINSIC(remove_party_items) {
	ignore_unused_variable_warning(num_parms);
	// Remove items(quantity, item, ??quality?? (-359), frame(-359), T/F).
	return remove_party_items(parms[0], parms[1], parms[2], parms[3], parms[4]);
}

USECODE_INTRINSIC(add_party_items) {
	ignore_unused_variable_warning(num_parms);
	// Add items(num, item, ??quality?? (-359), frame (or -359), T/F).
	// Returns array of NPC's (->'s) who got the items.
	Usecode_value u(
			add_party_items(parms[0], parms[1], parms[2], parms[3], parms[4]));
	return u;
}

USECODE_INTRINSIC(get_music_track) {
	ignore_unused_variable_warning(num_parms, parms);
	// Returns the song currently playing. In the original BG, this
	// returned a word: the high byte was the current song and the
	// low byte could be the current song (most cases) or, in some
	// cases, the song that was playing before and would be continued
	// after the current song ends. For example, if you played song 12
	// and then song 13, this function would return 0xD0C; after
	// playing song 13, BG would resume playing song 12 because it is
	// longer than song 13.
	// In SI, it simply returns the current playing song.
	// In Exult, we do it the SI way.
	if (Audio::get_ptr()->get_midi()) {
		return Usecode_value(Audio::get_ptr()->get_midi()->get_current_track());
	} else {
		return Usecode_value(-1);
	}
}

USECODE_INTRINSIC(play_music) {
	ignore_unused_variable_warning(num_parms);
	// Play music(songnum, item).
	// ??Show notes by item?
#ifdef DEBUG
	cout << "Music request in usecode" << endl;
	cout << "Parameter data follows" << endl;
	cout << "0: " << ((parms[0].get_int_value() >> 8) & 0xff) << " "
		 << ((parms[0].get_int_value()) & 0xff) << endl;
	cout << "1: " << ((parms[1].get_int_value() >> 8) & 0x01) << " "
		 << ((parms[1].get_int_value()) & 0x01) << endl;
#endif
	const int track = parms[0].get_int_value() & 0xff;
	if (track == 0xff) {                       // I think this is right:
		Audio::get_ptr()->cancel_streams();    // Stop playing.
	} else {
		Audio::get_ptr()->start_music(
				track, (parms[0].get_int_value() >> 8) & 0x01);

		// If a number but not an NPC, get out (for e.g.,
		// SI function 0x1D1).
		if (parms[1].is_int()
			&& (parms[1].get_int_value() >= 0
				|| (parms[1].get_int_value() != -356
					&& parms[1].get_int_value() < -gwin->get_num_npcs()))) {
			return no_ret;
		}

		// Show notes.
		Game_object* obj = get_item(parms[1]);
		if (obj && !obj->is_pos_invalid()) {
			gwin->get_effects()->add_effect(
					std::make_unique<Sprites_effect>(24, obj, 0, 0, -2, -2));
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(npc_nearby) {
	ignore_unused_variable_warning(num_parms);
	// NPC nearby? (item).
	Game_object* obj = get_item(parms[0]);
	if (!obj) {
		return Usecode_value(0);
	}
	const Tile_coord pos = obj->get_tile();
	Actor*           npc;
	const bool       is_near
			= gwin->get_win_tile_rect().has_world_point(pos.tx, pos.ty) &&
			  // Guessing: true if non-NPC, false if NPC is dead, asleep or
			  // paralyzed.
			  ((npc = as_actor(obj)) == nullptr || npc->can_act());
	Usecode_value u(is_near);
	return u;
}

USECODE_INTRINSIC(npc_nearby2) {
	ignore_unused_variable_warning(num_parms);
	// Guessing wildly (SI).  Handles start of Moonshade trial where
	//   companions are a fair distance away.

	Game_object* npc = get_item(parms[0]);
	const bool   is_near
			= (npc != nullptr &&
			   // Guessing; being asleep, paralyzed or dead doesn't seem to
			   // affect this.
			   npc->distance(gwin->get_main_actor()) < 40);
	Usecode_value u(is_near);
	return u;
}

USECODE_INTRINSIC(find_nearby_avatar) {
	ignore_unused_variable_warning(num_parms);
	// Find objs. with given shape near Avatar?
	Usecode_value av(gwin->get_main_actor());
	// Try bigger # for Test of Love tree.
	Usecode_value dist(/* 64 */ 192);
	Usecode_value mask(0);
	Usecode_value u(find_nearby(av, parms[0], dist, mask));
	return u;
}

USECODE_INTRINSIC(is_npc) {
	ignore_unused_variable_warning(num_parms);
	// Is item an NPC?
	Actor*        npc = as_actor(get_item(parms[0]));
	Usecode_value u(npc != nullptr);
	return u;
}

USECODE_INTRINSIC(display_runes) {
	ignore_unused_variable_warning(num_parms);
	// Render text into runes for signs, tombstones, plaques and the like
	// Display sign (gump #, array_of_text).
	int cnt = parms[1].get_array_size();
	if (!cnt) {
		cnt = 1;    // Try with 1 element.
	}
	{
		Sign_gump sign(parms[0].get_int_value(), cnt);
		for (int i = 0; i < cnt; i++) {
			// Paint each line.
			const Usecode_value& lval
					= !i ? parms[1].get_elem0() : parms[1].get_elem(i);
			const char* str = lval.get_str_value();
			sign.add_text(i, str);
		}
		int x;
		int y;    // Paint it, and wait for click.
		Get_click(x, y, Mouse::hand, nullptr, false, &sign);
	}
	gwin->paint();
	return no_ret;
}

USECODE_INTRINSIC(click_on_item) {
	ignore_unused_variable_warning(num_parms, parms);
	// Doesn't ret. until user single-
	//   clicks on an item.  Rets. item.
	Game_object* obj;
	Tile_coord   t;

	// intercept this click?
	if (intercept_item) {
		obj            = intercept_item;
		intercept_item = nullptr;
		if (intercept_tile) {
			delete intercept_tile;
			intercept_tile = nullptr;
		}
		t = obj->get_tile();
	} else if (intercept_tile) {
		obj = nullptr;
		t   = *intercept_tile;
		delete intercept_tile;
		intercept_tile = nullptr;
	}
	// Special case for weapon hit:
	else if (frame->eventid == weapon && caller_item) {
		// Special hack for weapons (needed for hitting Draygan with
		// sleep arrows (SI) and worms with worm hammer (also SI)).
		// Spells cast from readied spellbook in combat have been
		// changed to use the intercept_item instead, setting it
		// to the caster's target and restoring the old value after
		// it is used.
		obj = caller_item.get();
		t   = obj->get_tile();
	} else {
		int x;
		int y;    // Allow dragging while here:
		if (!Get_click(x, y, Mouse::greenselect, nullptr, true)) {
			return Usecode_value(0);
		}
		// Get abs. tile coords. clicked on.
		t = Tile_coord(
				gwin->get_scrolltx() + x / c_tilesize,
				gwin->get_scrollty() + y / c_tilesize, 0);
		// Look for obj. in open gump.
		Gump* gump = gumpman->find_gump(x, y);
		if (gump) {
			obj = gump->find_object(x, y);
			if (!obj) {
				obj = gump->find_actor(x, y);
			}
		} else {    // Search rest of world.
			obj = gwin->find_object(x, y);
			if (obj) {    // Found object?  Use its coords.
				t = obj->get_tile();
			}
		}
	}
	Usecode_value oval(obj);    // Ret. array with obj as 1st elem.
	Usecode_value ret(4, &oval);
	Usecode_value xval(t.tx);
	Usecode_value yval(t.ty);
	Usecode_value zval(t.tz);
	ret.put_elem(1, xval);
	ret.put_elem(2, yval);
	ret.put_elem(3, zval);
	return ret;
}

/*  Set item to be returned by next call to click_on_item().
 *  Added for Exult.
 */
USECODE_INTRINSIC(set_intercept_item) {
	ignore_unused_variable_warning(num_parms);
	intercept_item = get_item(parms[0]);
	if (intercept_item) {
		delete intercept_tile;
		intercept_tile = nullptr;
	} else {
		// Not an item, or null item.
		const int sz = parms[0].get_array_size();
		switch (sz) {
		case 2:
		case 3:
		case 4: {
			const int off = sz == 4 ? 1 : 0;
			// 2: (x, y) loc.
			// 3: (x, y, z) loc.
			// 4: (obj, x, y, z) loc.
			intercept_tile = new Tile_coord(
					parms[0].get_elem(0 + off).get_int_value(),
					parms[0].get_elem(1 + off).get_int_value(),
					sz >= 3 ? parms[0].get_elem(2 + off).get_int_value() : 0);
		} break;
		default: {
			// Fallback to avatar's position.
			// Maybe try avatar's target?
			intercept_tile = new Tile_coord(gwin->get_main_actor()->get_tile());
			break;
		}
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(find_nearby) {
	ignore_unused_variable_warning(num_parms);
	// Think it rets. objs. near parm0.
	Usecode_value u(find_nearby(parms[0], parms[1], parms[2], parms[3]));
	return u;
}

USECODE_INTRINSIC(give_last_created) {
	ignore_unused_variable_warning(num_parms);
	// Think it's give_last_created(container).
	Game_object* cont = get_item(parms[0]);
	bool         ret  = false;
	if (cont && !last_created.empty()) {
		// Get object, but don't pop yet.
		Game_object* obj = last_created.back().get();
		// Might not have been removed from world yet.
		if (!obj->get_owner() && obj->is_pos_invalid()) {
			// Don't check vol.  Causes failures.
			ret = cont->add(obj, true);
		}
		if (ret) {    // Pop only if added.  Fixes chest/
			//   tooth bug in SI.
			last_created.pop_back();
		}
	}
	Usecode_value u(ret);
	return u;
}

USECODE_INTRINSIC(is_dead) {
	ignore_unused_variable_warning(num_parms);
	// Return 1 if parm0 is a dead NPC.
	Actor*        npc = as_actor(get_item(parms[0]));
	Usecode_value u(npc && npc->is_dead());
	return u;
}

USECODE_INTRINSIC(game_day) {
	ignore_unused_variable_warning(num_parms, parms);
	Usecode_value u(gclock->get_day());
	return u;
}

USECODE_INTRINSIC(game_hour) {
	ignore_unused_variable_warning(num_parms, parms);
	// Return. game time hour (0-23).
	Usecode_value u(gclock->get_hour());
	return u;
}

USECODE_INTRINSIC(game_minute) {
	ignore_unused_variable_warning(num_parms, parms);
	// Return minute (0-59).
	Usecode_value u(gclock->get_minute());
	return u;
}

USECODE_INTRINSIC(get_npc_number) {
	ignore_unused_variable_warning(num_parms);
	// Returns NPC# of item. (-356 =
	//   avatar).
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc == gwin->get_main_actor()) {
		Usecode_value u(-356);
		return u;
	}
	const int     num = npc ? npc->get_npc_num() : 0;
	Usecode_value u(-num);
	return u;
}

USECODE_INTRINSIC(part_of_day) {
	ignore_unused_variable_warning(num_parms, parms);
	// Return 3-hour # (0-7, 0=midnight).
	Usecode_value u(gclock->get_hour() / 3);
	return u;
}

USECODE_INTRINSIC(get_alignment) {
	ignore_unused_variable_warning(num_parms);
	// Get npc's alignment.
	Actor*        npc = as_actor(get_item(parms[0]));
	Usecode_value u(npc ? npc->get_effective_alignment() : 0);
	return u;
}

USECODE_INTRINSIC(set_alignment) {
	ignore_unused_variable_warning(num_parms);
	// Set npc's alignment.
	// 2,3==bad towards Ava. 0==good.
	Actor*    npc = as_actor(get_item(parms[0]));
	const int val = parms[1].get_int_value();
	if (npc) {
		const int oldalign = npc->get_effective_alignment();
		npc->set_effective_alignment(val);
		if (oldalign != val) {    // Changed? Force search for new opp.
			Combat_schedule::stop_attacking_npc(npc);
			npc->set_target(nullptr);
		}
		// For fixing List Field fleeing:
		if (npc->get_attack_mode() == Actor::flee) {
			npc->set_attack_mode(Actor::nearest);
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(move_object) {
	// move_object(obj(-357=party), (tx, ty, tz)).
	Usecode_value&   p = parms[1];
	const Tile_coord tile(
			p.get_elem(0).get_int_value(), p.get_elem(1).get_int_value(),
			p.get_array_size() > 2 ? p.get_elem(2).get_int_value() : 0);
	const int map = p.get_array_size() < 4 ? -1 : p.get_elem(3).get_int_value();
	Actor*    ava = gwin->get_main_actor();
	modified_map  = true;
	if (parms[0].get_int_value() == -357) {
		// Move whole party.
		gwin->teleport_party(
				tile, false, map,
				num_parms > 2 ? parms[2].get_int_value() : false);
		return no_ret;
	}
	Game_object* obj = get_item(parms[0]);
	if (!obj) {
		return no_ret;
	}
	const Tile_coord oldpos = obj->get_tile();
	obj->move(tile.tx, tile.ty, tile.tz, map);
	Actor* act = as_actor(obj);
	if (act) {
		act->set_action(nullptr);
		if (act == ava) {
			// Teleported Avatar?
			// Make new loc. visible, test eggs.
			if (map != -1) {
				gwin->set_map(map);
			}
			gwin->center_view(tile);
			Map_chunk::try_all_eggs(
					ava, tile.tx, tile.ty, tile.tz, oldpos.tx, oldpos.ty);
		}
		// Close?  Add to 'nearby' list.
		else if (ava->distance(act) < gwin->get_game_width() / c_tilesize) {
			Npc_actor* npc = act->as_npc();
			if (npc) {
				gwin->add_nearby_npc(npc);
			}
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(remove_npc) {
	ignore_unused_variable_warning(num_parms);
	// Remove_npc(npc) - Remove npc from world.
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc) {
		modified_map = true;
		// Don't want him/her coming back!
		npc->set_schedule_type(Schedule::wait);
		// Fixes #1892 SI: sacrificed Dupre back in party
		// Might be able to always clear ident, but this is safer
		if (npc->get_ident() == Schedule::follow_avatar) {
			npc->set_ident(0);
		}
		gwin->add_dirty(npc);
		Game_object_shared keep;
		npc->remove_this(&keep);    // Remove, but don't delete.
	}
	return no_ret;
}

USECODE_INTRINSIC(item_say) {
	ignore_unused_variable_warning(num_parms);
	// Show str. near item (item, str).
	if (!conv->is_npc_text_pending()) {
		item_say(parms[0], parms[1]);    // Do it now.
	}
	return no_ret;
}

USECODE_INTRINSIC(clear_item_say) {
	ignore_unused_variable_warning(num_parms);
	// Clear str. near item (item).
	Game_object* item = get_item(parms[0]);
	if (item) {
		gwin->get_effects()->remove_text_effect(item);
		// Seems to be right (e.g., the avatar's barks after
		// the teleport storm/Karnax vs Thoxa battle).
		Usecode_script* scr = nullptr;
		while ((scr = Usecode_script::find(item, scr)) != nullptr) {
			scr->kill_barks();
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(set_to_attack) {
	ignore_unused_variable_warning(num_parms);
	// set_to_attack(fromnpc, to, weaponshape).
	// fromnpc attacks the target 'to' with weapon weaponshape.
	// 'to' can be a game object or the return of a click_on_item
	// call (including the possibility of being a tile target).
	Actor* from = as_actor(get_item(parms[0]));
	if (!from) {
		return Usecode_value(0);
	}
	const int shnum = parms[2].get_int_value();
	if (shnum < 0) {
		return Usecode_value(0);
	}
	const Weapon_info* winf = ShapeID::get_info(shnum).get_weapon_info();
	if (!winf) {
		return Usecode_value(0);
	}

	Usecode_value& tval = parms[1];
	Game_object*   to   = get_item(tval.get_elem0());
	int            nelems;
	if (to) {
		// It is an object.
		from->set_attack_target(to, shnum);
		return Usecode_value(1);
	} else if (tval.is_array() && (nelems = tval.get_array_size()) >= 3) {
		// Tile return of click_on_item. Allowing size to be < 4 for safety.
		const Tile_coord trg = Tile_coord(
				tval.get_elem(1).get_int_value(),
				tval.get_elem(2).get_int_value(),
				nelems >= 4 ? tval.get_elem(3).get_int_value() : 0);
		from->set_attack_target(trg, shnum);
		return Usecode_value(1);
	}

	return Usecode_value(0);    // Failure.
}

USECODE_INTRINSIC(get_lift) {
	ignore_unused_variable_warning(num_parms);
	// ?? Guessing rets. lift(item).
	Game_object*  obj = get_item(parms[0]);
	Usecode_value u(obj ? Usecode_value(obj->get_lift()) : Usecode_value(0));
	return u;
}

USECODE_INTRINSIC(set_lift) {
	ignore_unused_variable_warning(num_parms);
	// ?? Guessing setlift(item, lift).
	Game_object* obj = get_item(parms[0]);
	if (obj) {
		const Tile_coord t    = obj->get_tile();
		const int        lift = parms[1].get_int_value();
		if (lift >= 0 && lift < 20) {
			obj->move(t.tx, t.ty, lift);
		}
		gwin->paint();
		gwin->show();
		modified_map = true;
	}
	return no_ret;
}

USECODE_INTRINSIC(get_weather) {
	ignore_unused_variable_warning(num_parms, parms);
	// Get_weather()
	return Usecode_value(gwin->get_effects()->get_weather());
}

USECODE_INTRINSIC(set_weather) {
	ignore_unused_variable_warning(num_parms);
	// Set_weather(i)
	Egg_object::set_weather(parms[0].get_int_value());
	return no_ret;
}

USECODE_INTRINSIC(sit_down) {
	ignore_unused_variable_warning(num_parms);
	// Sit_down(npc, chair).
	Game_object* nobj = get_item(parms[0]);
	Actor*       npc  = as_actor(nobj);
	if (!npc) {
		return no_ret;    // Doesn't look like an NPC.
	}
	Game_object* chair = get_item(parms[1]);
	if (!chair) {
		return no_ret;
	}
	npc->set_schedule_type(
			Schedule::sit, std::make_unique<Sit_schedule>(npc, chair));
	return no_ret;
}

USECODE_INTRINSIC(summon) {
	ignore_unused_variable_warning(num_parms);
	// summon(shape, flag??).  Create monster of desired shape.

	const int           shapenum = parms[0].get_int_value();
	const Monster_info* info = ShapeID::get_info(shapenum).get_monster_info();
	if (!info) {
		return Usecode_value(0);
	}
	const Tile_coord start = gwin->get_main_actor()->get_tile();
	const Tile_coord dest  = Map_chunk::find_spot(
            start, 5, shapenum, 0, 1, -1,
            gwin->is_main_actor_inside() ? Map_chunk::inside
										  : Map_chunk::outside);
	if (dest.tx == -1) {
		return Usecode_value(0);
	}
	Actor* npc   = as_actor(caller_item.get());
	int    align = Actor::good;
	if (npc && !npc->is_in_party()) {
		align = npc->get_effective_alignment();
	}
	const Game_object_shared monst
			= Monster_actor::create(shapenum, dest, Schedule::combat, align);
	return Usecode_value(monst.get());
}

/*
 *  Class to paint a shape centered.
 */
class Paint_centered : public Paintable, public Game_singletons {
protected:
	ShapeID* sid;     // ->shape.
	int      x, y;    // Where to paint.
public:
	Paint_centered(ShapeID* si) : sid(si) {
		Shape_frame* s = sid->get_shape();
		// Get coords. for centered view.
		x = (gwin->get_game_width() - s->get_width()) / 2 + s->get_xleft();
		y = (gwin->get_game_height() - s->get_height()) / 2 + s->get_yabove();
	}

	void paint() override {
		sid->paint_shape(x, y);
	}
};

/*
 *  Paint map.
 */
class Paint_map : public Paint_centered {
	bool show_loc;

public:
	Paint_map(ShapeID* s, bool loc) : Paint_centered(s), show_loc(loc) {}

	void paint() override {
		Paint_centered::paint();
		if (show_loc) {
			// mark location
			int              xx;
			int              yy;
			const Tile_coord t = gwin->get_main_actor()->get_tile();
			if (Game::get_game_type() == BLACK_GATE) {
				xx = std::lround(t.tx / 16.05 + 5);
				yy = std::lround(t.ty / 15.95 + 4);
			} else if (Game::get_game_type() == SERPENT_ISLE) {
				xx = std::lround(t.tx / 16.0 + 18);
				yy = std::lround(t.ty / 16.0 + 9.4);
			} else {
				xx = std::lround(t.tx / 16.0 + 5);
				yy = std::lround(t.ty / 16.0 + 5);
			}
			Shape_frame* s = sid->get_shape();
			xx += x - s->get_xleft();
			yy += y - s->get_yabove();
			gwin->get_win()->fill8(50, 1, 5, xx, yy - 2);
			gwin->get_win()->fill8(50, 5, 1, xx - 2, yy);
		}
	}
};

USECODE_INTRINSIC(display_map) {
	ignore_unused_variable_warning(num_parms, parms);
	// count all sextants in party
	Usecode_value v_357(-357);
	Usecode_value v650(650);
	Usecode_value v_359(-359);
	const long    sextants
			= count_objects(v_357, v650, v_359, v_359).get_int_value();
	const bool loc = !gwin->is_main_actor_inside() && (sextants > 0);
	// Display map.
	if (touchui != nullptr) {
		touchui->hideGameControls();
	}
	ShapeID   msid(game->get_shape("sprites/map"), 0, SF_SPRITES_VGA);
	Paint_map map(&msid, loc);

	int xx;
	int yy;
	Get_click(xx, yy, Mouse::hand, nullptr, false, &map);
	gwin->paint();
	if (touchui != nullptr) {
		Gump_manager* gumpman = gwin->get_gump_man();
		if (!gumpman->gump_mode()) {
			touchui->showGameControls();
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(si_display_map) {
	const int mapnum = parms[0].get_int_value();
	int       shapenum;

	switch (mapnum) {
	case 0:
		return UI_display_map(num_parms, parms);
	case 1:
		shapenum = 57;
		break;
	case 2:
		shapenum = 58;
		break;
	case 3:
		shapenum = 59;
		break;
	case 4:
		shapenum = 60;
		break;
	case 5:
		shapenum = 52;
		break;
	default:
		return no_ret;
	}

	// Display map.

	// Display map.
	if (touchui != nullptr) {
		touchui->hideGameControls();
	}
	ShapeID        msid(shapenum, 0, SF_SPRITES_VGA);
	Paint_centered map(&msid);
	int            xx;
	int            yy;
	Get_click(xx, yy, Mouse::hand, nullptr, false, &map);
	gwin->paint();
	if (touchui != nullptr) {
		Gump_manager* gumpman = gwin->get_gump_man();
		if (!gumpman->gump_mode()) {
			touchui->showGameControls();
		}
	}

	return no_ret;
}

USECODE_INTRINSIC(display_map_ex) {
	ignore_unused_variable_warning(num_parms);
	const int  map_shp = parms[0].get_int_value();
	const bool loc     = parms[1].get_int_value() != 0;

	// Display map.
	if (touchui != nullptr) {
		touchui->hideGameControls();
	}
	ShapeID   msid(map_shp, 0, SF_SPRITES_VGA);
	Paint_map map(&msid, loc);

	int xx;
	int yy;
	Get_click(xx, yy, Mouse::hand, nullptr, false, &map);
	gwin->paint();
	if (touchui != nullptr) {
		Gump_manager* gumpman = gwin->get_gump_man();
		if (!gumpman->gump_mode()) {
			touchui->showGameControls();
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(kill_npc) {
	ignore_unused_variable_warning(num_parms);
	// kill_npc(npc).
	Game_object* item = get_item(parms[0]);
	Actor*       npc  = as_actor(item);
	if (npc) {
		npc->die(nullptr);
	}
	modified_map = true;
	return no_ret;
}

USECODE_INTRINSIC(roll_to_win) {
	ignore_unused_variable_warning(num_parms);
	// roll_to_win(attackpts, defendpts)
	const int attack = parms[0].get_int_value();
	const int defend = parms[1].get_int_value();
	return Usecode_value(static_cast<int>(Actor::roll_to_win(attack, defend)));
}

USECODE_INTRINSIC(set_attack_mode) {
	ignore_unused_variable_warning(num_parms);
	// set_attack_mode(npc, mode).
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc) {
		npc->set_attack_mode(
				static_cast<Actor::Attack_mode>(parms[1].need_int_value()));
	}
	return no_ret;
}

USECODE_INTRINSIC(get_attack_mode) {
	ignore_unused_variable_warning(num_parms);
	// get_attack_mode(npc).
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc) {
		return Usecode_value(static_cast<int>(npc->get_attack_mode()));
	}
	return Usecode_value(0);
}

USECODE_INTRINSIC(set_opponent) {
	ignore_unused_variable_warning(num_parms);
	// set_opponent(npc, new_opponent).
	Actor*       npc      = as_actor(get_item(parms[0]));
	Game_object* opponent = get_item(parms[1]);
	if (npc && opponent) {
		npc->set_target(opponent);
	}
	return no_ret;
}

USECODE_INTRINSIC(clone) {
	ignore_unused_variable_warning(num_parms);
	// clone(npc)
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc) {
		modified_map                       = true;
		const Game_object_shared new_npc   = npc->clone();
		auto*                    clonednpc = static_cast<Actor*>(new_npc.get());
		clonednpc->set_alignment(Actor::good);
		clonednpc->set_schedule_type(Schedule::combat);
		return Usecode_value(clonednpc);
	}
	return Usecode_value(0);
}

USECODE_INTRINSIC(get_oppressor) {
	ignore_unused_variable_warning(num_parms);
	// get_oppressor(npc) Returns 0-n, NPC # (0=avatar).
	Actor* npc = as_actor(get_item(parms[0]));
	return Usecode_value(npc ? npc->get_oppressor() : 0);
}

USECODE_INTRINSIC(set_oppressor) {
	ignore_unused_variable_warning(num_parms);
	// set_oppressor(npc, opp)
	Actor* npc = as_actor(get_item(parms[0]));
	Actor* opp = as_actor(get_item(parms[1]));
	if (npc && opp && npc != opp) {    // Just in case.
		if (opp == gwin->get_main_actor()) {
			npc->set_oppressor(0);
		} else {
			npc->set_oppressor(opp->get_npc_num());
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(get_weapon) {
	ignore_unused_variable_warning(num_parms);
	// get_weapon(npc).  Returns shape.
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc) {
		int          shape;
		int          points;
		Game_object* w;
		if (npc->get_weapon(points, shape, w)) {
			return Usecode_value(shape);
		}
	}
	return Usecode_value(0);
}

USECODE_INTRINSIC(display_area) {
	ignore_unused_variable_warning(num_parms);
	// display_area(tilepos) - used for crystal balls.
	const int size = parms[0].get_array_size();
	if (size >= 3) {
		const int tx = parms[0].get_elem(0).get_int_value();
		const int ty = parms[0].get_elem(1).get_int_value();
		// int unknown = parms[0].get_elem(2).get_int_value();
		//  Figure in tiles.
		const int newmap
				= size == 3 ? -1 : parms[0].get_elem(3).get_int_value();
		const int oldmap = gwin->get_map()->get_num();
		const int tw     = gwin->get_game_width() / c_tilesize;
		const int th     = gwin->get_game_height() / c_tilesize;
		gwin->clear_screen();    // Fill with black.
		if ((newmap != -1) && (newmap != oldmap)) {
			gwin->set_map(newmap);
		}
		Shape_frame* sprite = ShapeID(10, 0, SF_SPRITES_VGA).get_shape();
		// Center it.
		const int topx = (gwin->get_game_width() - sprite->get_width()) / 2;
		const int topy = (gwin->get_game_height() - sprite->get_height()) / 2;
		// Get area to show.
		int       x     = 0;
		int       y     = 0;
		const int w     = gwin->get_game_width();
		const int h     = gwin->get_game_height();
		const int sizex = (w - 320) / 2;
		const int sizey = (h - 200) / 2;
		// Show only inside the original resolution.
		if (w > 320) {
			x = sizex;
		}
		if (h > 200) {
			y = sizey;
		}
		const int save_dungeon = gwin->is_in_dungeon();
		// Paint game area.
		gwin->set_in_dungeon(0);    // Disable dungeon.
		gwin->paint_map_at_tile(x, y, 320, 200, tx - tw / 2, ty - th / 2, 4);
		// Paint sprite #10
		//   over it, transparently.
		sman->paint_shape(
				topx + sprite->get_xleft(), topy + sprite->get_yabove(), sprite,
				true);
		gwin->set_in_dungeon(save_dungeon);
		gwin->show();
		// Wait for click.
		Get_click(x, y, Mouse::hand, nullptr, false, nullptr);

		// BG orrery viewer needs this because it also calls UI_view_tile:
		gwin->center_view(gwin->get_main_actor()->get_tile());
		if ((newmap != -1) && (newmap != oldmap)) {
			gwin->set_map(oldmap);
		}
		gwin->paint();    // Repaint normal area.
	}
	return no_ret;
}

USECODE_INTRINSIC(wizard_eye) {
	ignore_unused_variable_warning(num_parms);
	// wizard_eye(#ticks, ??);
	extern void Wizard_eye(long);
	// Let's give 50% longer.
	Wizard_eye(parms[0].get_int_value() * (3 * gwin->get_std_delay()) / 2);
	return no_ret;
}

USECODE_INTRINSIC(resurrect) {
	ignore_unused_variable_warning(num_parms);
	// resurrect(body).  Returns actor if successful.
	Game_object* body    = get_item(parms[0]);
	const int    npc_num = body ? body->get_live_npc_num() : -1;
	if (npc_num < 0) {
		return Usecode_value(static_cast<Game_object*>(nullptr));
	}
	Actor* actor = gwin->get_npc(npc_num);
	if (actor) {
		// Want to resurrect after returning.
		auto* scr = new Usecode_script(body);
		(*scr) << Ucscript::resurrect;
		scr->start();
		modified_map = true;
	}
	return Usecode_value(actor);
}

USECODE_INTRINSIC(resurrect_npc) {
	ignore_unused_variable_warning(num_parms);
	// resurrect_npc(npc).
	// Behaves like the original does.
	Actor* actor = as_actor(get_item(parms[0]));
	if (actor) {
		// Want to resurrect after returning.
		actor->resurrect(nullptr);
		modified_map = true;
	}
	return no_ret;
}

USECODE_INTRINSIC(get_body_npc) {
	ignore_unused_variable_warning(num_parms);
	// get_body_npc(body).  Returns npc # (negative).
	Game_object* obj = get_item(parms[0]);
	const int    num = obj ? obj->get_live_npc_num() : -1;
	return Usecode_value(num > 0 ? -num : 0);
}

USECODE_INTRINSIC(add_spell) {
	ignore_unused_variable_warning(num_parms);
	// add_spell(spell# (0-71), ??, spellbook).
	// Returns 0 if book already has that spell.
	Game_object* obj  = get_item(parms[2]);
	auto*        book = obj->as_spellbook();
	if (!book) {
		cout << "Add_spell - Not a spellbook!" << endl;
		return Usecode_value(0);
	}
	return Usecode_value(book->add_spell(parms[0].get_int_value()));
}

USECODE_INTRINSIC(remove_all_spells) {
	ignore_unused_variable_warning(num_parms);
	// remove_all_spells(spellbook).
	// Removes all spells from spellbook.
	Game_object* obj  = get_item(parms[0]);
	auto*        book = obj->as_spellbook();
	if (!book) {
		cout << "remove_all_spells - Not a spellbook!" << endl;
		return no_ret;
	}
	book->clear_spells();
	return no_ret;
}

USECODE_INTRINSIC(has_spell) {
	ignore_unused_variable_warning(num_parms);
	// has_spell(spellbook, spell#).
	// Returns true if the spellbook has desired spell, false if not.
	Game_object* obj  = get_item(parms[0]);
	auto*        book = obj->as_spellbook();
	if (!book) {
		cout << "has_spell - Not a spellbook!" << endl;
		return Usecode_value(0);
	}
	return Usecode_value(book->has_spell(parms[1].get_int_value()));
}

USECODE_INTRINSIC(remove_spell) {
	ignore_unused_variable_warning(num_parms);
	// remove_spell(spellbook, spell#).
	// Returns true if the spellbook has desired spell, false if not.
	Game_object* obj  = get_item(parms[0]);
	auto*        book = obj->as_spellbook();
	if (!book) {
		cout << "remove_spell - Not a spellbook!" << endl;
		return Usecode_value(0);
	}
	return Usecode_value(book->remove_spell(parms[1].get_int_value()));
}

USECODE_INTRINSIC(sprite_effect) {
	ignore_unused_variable_warning(num_parms);
	// Display animation from sprites.vga.
	// show_sprite(sprite#, tx, ty, dx, dy, frame, length??);
	gwin->get_effects()->add_effect(std::make_unique<Sprites_effect>(
			parms[0].get_int_value(),
			Tile_coord(parms[1].get_int_value(), parms[2].get_int_value(), 0),
			parms[3].get_int_value(), parms[4].get_int_value(), 0,
			parms[5].get_int_value(), parms[6].get_int_value()));
	return no_ret;
}

USECODE_INTRINSIC(obj_sprite_effect) {
	ignore_unused_variable_warning(num_parms);
	// obj_sprite_effect(obj, sprite#, -xoff, -yoff, dx, dy,
	//                      frame, length??)
	Game_object* obj = get_item(parms[0]);
	if (obj) {
		gwin->get_effects()->add_effect(std::make_unique<Sprites_effect>(
				parms[1].get_int_value(), obj, -parms[2].get_int_value(),
				-parms[3].get_int_value(), parms[4].get_int_value(),
				parms[5].get_int_value(), parms[6].get_int_value(),
				parms[7].get_int_value()));
	}
	return no_ret;
}

USECODE_INTRINSIC(attack_object) {
	ignore_unused_variable_warning(num_parms);
	// attack_object(attacker, target, wshape).
	Game_object* att    = get_item(parms[0]);
	Game_object* trg    = get_item(parms[1]);
	const int    wshape = parms[2].get_int_value();

	if (!att || !trg) {
		return Usecode_value(0);
	}
	const Tile_coord tile(-1, -1, 0);
	return Usecode_value(
			Combat_schedule::attack_target(att, trg, tile, wshape));
}

USECODE_INTRINSIC(book_mode) {
	ignore_unused_variable_warning(num_parms);
	// Display book or scroll.
	Text_gump*   gump;
	Game_object* obj = get_item(parms[0]);
	if (!obj) {
		return no_ret;
	}

	// check for avatar read here
	const bool do_serp = !gwin->get_main_actor()->get_flag(Obj_flags::read);
	const int  fnt     = do_serp ? 8 : 4;

	if (obj->get_shapenum() == 707) {    // Serpentine Scroll - Make SI only???
		gump = new Scroll_gump(fnt);
	} else if (obj->get_shapenum() == 705) {    // Serpentine Book - Make SI
												// only???
		gump = new Book_gump(fnt);
	} else if (obj->get_shapenum() == 797) {
		gump = new Scroll_gump();
	} else {
		gump = new Book_gump();
	}
	set_book(gump);
	return no_ret;
}

USECODE_INTRINSIC(book_mode_ex) {
	// Display book or scroll.
	Text_gump* gump;
	const bool is_scroll = parms[0].get_int_value() != 0;
	const int  fnt       = parms[1].get_int_value();
	const int  gumpshp   = num_parms >= 3 ? parms[2].get_int_value() : -1;

	if (is_scroll) {
		gump = new Scroll_gump(fnt, gumpshp);
	} else {
		gump = new Book_gump(fnt, gumpshp);
	}
	set_book(gump);
	return no_ret;
}

USECODE_INTRINSIC(stop_time) {
	ignore_unused_variable_warning(num_parms);
	// stop_time(.25 secs).

	const int length = parms[0].get_int_value();
	gwin->set_time_stopped(length * 250);
	return no_ret;
}

USECODE_INTRINSIC(cause_light) {
	ignore_unused_variable_warning(num_parms);
	// Cause_light(game_minutes??)

	gwin->add_special_light(parms[0].get_int_value());
	return no_ret;
}

USECODE_INTRINSIC(get_barge) {
	ignore_unused_variable_warning(num_parms);
	// get_barge(obj) - returns barge object is part of or lying on.

	Game_object* obj = get_item(parms[0]);
	if (!obj) {
		return Usecode_value(static_cast<Game_object*>(nullptr));
	}
	return Usecode_value(Get_barge(obj));
}

USECODE_INTRINSIC(earthquake) {
	ignore_unused_variable_warning(num_parms);
	const int len = parms[0].get_int_value();
	gwin->get_tqueue()->add(Game::get_ticks() + 10, new Earthquake(len), this);
	return no_ret;
}

USECODE_INTRINSIC(is_pc_female) {
	ignore_unused_variable_warning(num_parms, parms);
	// Is player female?
	Usecode_value u(gwin->get_main_actor()->get_type_flag(Actor::tf_sex));
	return u;
}

static inline void Armageddon_death(
		Actor* npc, bool barks, const TileRect& screen) {
	// Leave a select few alive (like LB, Batlin).
	if (npc && !npc->is_dead() && !npc->get_info().survives_armageddon()) {
		constexpr static const std::array text{
				"Aiiiieee!", "Noooo!", "#!?*#%!"};
		const Tile_coord loc = npc->get_tile();
		if (barks && screen.has_world_point(loc.tx, loc.ty)) {
			npc->say(text[rand() % text.size()]);
		}
		// Lay down and lie animation.
		npc->lay_down(false);
		// Originals set health to -10; marking it this way allows
		// Actor::is_dying to return true in case it is needed.
		npc->set_property(
				static_cast<int>(Actor::health),
				-npc->get_property(static_cast<int>(Actor::health)) / 3 - 1);
		// This makes the NPC stay there unresponsive and immune to damage.
		npc->set_flag(Obj_flags::dead);
	}
}

USECODE_INTRINSIC(armageddon) {
	ignore_unused_variable_warning(num_parms, parms);
	const int      cnt    = gwin->get_num_npcs();
	const TileRect screen = gwin->get_win_tile_rect();
	for (int i = 1; i < cnt; i++) {    // Almost everyone dies.
		Armageddon_death(gwin->get_npc(i), true, screen);
	}
	Actor_vector vec;    // Get any monsters nearby.
	gwin->get_main_actor()->find_nearby_actors(vec, c_any_shapenum, 40, 0x28);
	for (auto* act : vec) {
		if (act->is_monster()) {
			Armageddon_death(act, false, screen);
		}
	}
	gwin->armageddon = true;
	return no_ret;
}

USECODE_INTRINSIC(halt_scheduled) {
	ignore_unused_variable_warning(num_parms);
	// Halt_scheduled(item)
	Game_object* obj = get_item(parms[0]);
	if (obj) {
		Usecode_script::terminate(obj);
	}
	return no_ret;
}

USECODE_INTRINSIC(lightning) {
	ignore_unused_variable_warning(num_parms, parms);
	// 1 sec. is long enough for 1 flash.
	gwin->get_effects()->remove_usecode_lightning();
	gwin->get_effects()->add_effect(
			std::make_unique<Lightning_effect>(1000, 0, true));
	return no_ret;
}

USECODE_INTRINSIC(get_array_size) {
	ignore_unused_variable_warning(num_parms);
	int cnt;
	if (parms[0].is_array()) {    // An array?  We might return 0.
		cnt = parms[0].get_array_size();
	} else {    // Not an array?  Usecode wants a 1.
		cnt = 1;
	}
	Usecode_value u(cnt);
	return u;
}

USECODE_INTRINSIC(mark_virtue_stone) {
	ignore_unused_variable_warning(num_parms);
	Game_object* obj = get_item(parms[0]);
	auto*        vs  = obj->as_virtstone();
	if (vs) {
		vs->set_target_pos(obj->get_outermost()->get_tile());
		vs->set_target_map(obj->get_outermost()->get_map_num());
	}
	return no_ret;
}

USECODE_INTRINSIC(recall_virtue_stone) {
	Game_object*       obj = get_item(parms[0]);
	Game_object_shared keep;
	auto*              vs = obj->as_virtstone();
	if (vs) {
		gumpman->close_all_gumps();
		// Pick it up if necessary.
		if (!obj->get_owner()) {
			// Go through whole party.
			obj->remove_this(&keep);
			Usecode_value party = get_party();
			const int     cnt   = party.get_array_size();
			int           i;
			for (i = 0; i < cnt; i++) {
				Game_object* npc = get_item(party.get_elem(i));
				if (npc && npc->add(obj)) {
					break;
				}
			}
			if (i == cnt) {    // Failed?  Force it on Avatar.
				gwin->get_main_actor()->add(obj, true);
			}
		}
		const Tile_coord t = vs->get_target_pos();
		if (t.tx > 0 || t.ty > 0) {
			gwin->teleport_party(
					t, false, vs->get_target_map(),
					num_parms > 1 ? parms[1].get_int_value() : false);
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(apply_damage) {
	ignore_unused_variable_warning(num_parms);
	// apply_damage(str, hps, type, obj);
	Game_object* obj = get_item(parms[3]);
	if (!obj) {    // No valid target.
		return Usecode_value(0);
	}

	const int base = parms[0].get_int_value();
	const int hps  = parms[1].get_int_value();
	const int type = parms[2].get_int_value();

	obj->apply_damage(nullptr, base, hps, type);
	return Usecode_value(1);
}

USECODE_INTRINSIC(is_pc_inside) {
	ignore_unused_variable_warning(num_parms, parms);
	Usecode_value u(gwin->is_main_actor_inside());
	return u;
}

USECODE_INTRINSIC(set_orrery) {
	ignore_unused_variable_warning(num_parms);
	// set_orrery(pos, state(0-9)).
	/*
	 *  This code is based on the Planets.txt document written
	 *  by Marzo Sette Torres Junior.
	 *
	 *  The table below contains the (x,y) offsets for each of the
	 *  8 planet frames in each possible state.
	 */
	static const short offsets[10][8][2] = {
			/* S0 */ {
					  {2, -3},
					  {3, -3},
					  {1, -6},
					  {6, -2},
					  {7, -1},
					  {8, 1},
					  {-4, 8},
					  {9, -2}  },
			/* S1 */
			{ {3, -1},
					  {4, -1},
					  {-5, -3},
					  {3, 6},
					  {7, 2},
					  {4, 7},
					  {-8, 4},
					  {8, 5}   },
			/* S2 */
			{  {3, 1},
					  {3, 2},
					  {-3, 4},
					  {-5, 4},
					  {2, 7},
					  {-2, 8},
					  {-9, 1},
					  {2, 9}   },
			/* S3 */
			{  {1, 3},
					  {1, 4},
					  {4, 3},
					  {-5, -3},
					  {-4, 6},
					  {-7, 4},
					  {-9, -1},
					  {-4, 9}  },
			/* S4 */
			{ {-2, 3},
					  {-2, 4},
					  {5, -2},
					  {5, -4},
					  {-7, 2},
					  {-8, 1},
					  {-8, -4},
					  {-8, 6}  },
			/* S5 */
			{ {-4, 1},
					  {-5, 1},
					  {-5, -3},
					  {6, 3},
					  {-7, -2},
					  {-7, -4},
					  {-7, -6},
					  {-10, 1} },
			/* S6 */
			{ {-4, 9},
					  {-5, -1},
					  {-3, 4},
					  {-3, 6},
					  {-6, -4},
					  {-5, -6},
					  {-7, -6},
					  {-10, -2}},
			/* S7 */
			{ {-4, 2},
					  {-4, -3},
					  {4, 3},
					  {-6, 1},
					  {-5, -5},
					  {-3, -7},
					  {-4, -8},
					  {-8, -6} },
			/* S8 */
			{{-3, -3},
					  {-3, -4},
					  {5, -2},
					  {-3, -5},
					  {-1, -7},
					  {0, -8},
					  {-1, -9},
					  {-5, -9} },
			/* S9 */
			{ {0, -4},
					  {0, -5},
					  {1, -6},
					  {1, -6},
					  {1, -7},
					  {1, -8},
					  {1, -9},
					  {-1, -10}}
    };

	const Tile_coord pos(
			parms[0].get_elem(0).get_int_value(),
			parms[0].get_elem(1).get_int_value(),
			parms[0].get_elem(2).get_int_value());
	const int state = parms[1].get_int_value();
	// Find Planet Britania.
	Game_object* brit = Game_object::find_closest(pos, 765, 24);
	if (brit && state >= 0 && state <= 9) {
		Game_object_vector planets;    // Remove existing planets.
		brit->find_nearby(planets, 988, 24, 0);
		for (auto* p : planets) {
			if (p->get_framenum() <= 7) {    // Leave the sun.
				p->remove_this();
			}
		}
		for (int frame = 0; frame <= 7; ++frame) {
			const Game_object_shared p = gmap->create_ireg_object(988, frame);
			p->move(pos.tx + offsets[state][frame][0],
					pos.ty + offsets[state][frame][1], pos.tz);
		}
	}
	gwin->set_all_dirty();
	return no_ret;
}

USECODE_INTRINSIC(get_timer) {
	ignore_unused_variable_warning(num_parms);
	const int tnum = parms[0].get_int_value();
	int       ret;
	auto      it = timers.find(tnum);
	if (it != timers.end() && timers[tnum] > 0) {
		ret = gclock->get_total_hours() - timers[tnum];
	} else {
		// Return random amount (up to half a day) if not set.
		ret = rand() % 13;
	}
	return Usecode_value(ret);
}

USECODE_INTRINSIC(set_timer) {
	ignore_unused_variable_warning(num_parms);
	const int tnum = parms[0].get_int_value();
	timers[tnum]   = gclock->get_total_hours();
	return no_ret;
}

USECODE_INTRINSIC(wearing_fellowship) {
	ignore_unused_variable_warning(num_parms, parms);
	Game_object* obj = gwin->get_main_actor()->get_readied(amulet);
	if (obj && obj->get_shapenum() == 955 && obj->get_framenum() == 1) {
		return Usecode_value(1);
	} else {
		return Usecode_value(0);
	}
}

USECODE_INTRINSIC(mouse_exists) {
	ignore_unused_variable_warning(num_parms, parms);
	Usecode_value u(1);
	return u;
}

USECODE_INTRINSIC(get_speech_track) {
	ignore_unused_variable_warning(num_parms, parms);
	// Get speech track set by 0x74 or 0x8f.
	return Usecode_value(speech_track);
}

USECODE_INTRINSIC(flash_mouse) {
	ignore_unused_variable_warning(num_parms);
	// flash_mouse(code)
	Mouse::Mouse_shapes shape;
	switch (parms[0].need_int_value()) {
	case 2:
		shape = Mouse::outofrange;
		break;
	case 3:
		shape = Mouse::outofammo;
		break;
	case 4:
		shape = Mouse::tooheavy;
		break;
	case 5:
		shape = Mouse::wontfit;
		break;
	case 7:
		shape = Mouse::blocked;
		break;
	case 0:
	case 1:
	default:
		shape = Mouse::redx;
		break;
	}
	Mouse::mouse->flash_shape(shape);
	return no_ret;
}

USECODE_INTRINSIC(get_item_frame_rot) {
	ignore_unused_variable_warning(num_parms);
	// Same as get_item_frame, but (guessing!) include rotated bit.
	Game_object* obj = get_item(parms[0]);
	return Usecode_value(obj ? obj->get_framenum() : 0);
}

USECODE_INTRINSIC(set_item_frame_rot) {
	ignore_unused_variable_warning(num_parms);
	// Set entire frame, including rotated bit.
	set_item_frame(get_item(parms[0]), parms[1].get_int_value(), 0, 1);
	return no_ret;
}

USECODE_INTRINSIC(on_barge) {
	ignore_unused_variable_warning(num_parms, parms);
	// Only used once for BG, in usecode for magic-carpet.
	// For SI, used for turtle.
	// on_barge()
	Barge_object* barge = Get_barge(gwin->get_main_actor());
	if (barge) {
		// See if party is on barge.
		const TileRect foot = barge->get_tile_footprint();
		Actor*         party[9];
		const int      cnt = gwin->get_party(party, 1);
		for (int i = 0; i < cnt; i++) {
			Actor*           act = party[i];
			const Tile_coord t   = act->get_tile();
			if (!foot.has_world_point(t.tx, t.ty)) {
				return Usecode_value(0);
			}
		}
		// Force 'gather()' for turtle.
		if (Game::get_game_type() == SERPENT_ISLE) {
			barge->done();
		}
		return Usecode_value(1);
	}
	return Usecode_value(0);
	//	return Usecode_value(1);
}

USECODE_INTRINSIC(get_container) {
	ignore_unused_variable_warning(num_parms);
	// Takes itemref, returns container.
	Game_object*  obj = get_item(parms[0]);
	Usecode_value u(static_cast<Game_object*>(nullptr));
	if (obj) {
		u = Usecode_value(obj->get_owner());
	}
	return u;
}

USECODE_INTRINSIC(remove_item) {
	ignore_unused_variable_warning(num_parms);
	// Think it's 'delete object'.
	remove_item(get_item(parms[0]));
	modified_map = true;
	return no_ret;
}

USECODE_INTRINSIC(reduce_health) {
	ignore_unused_variable_warning(num_parms);
	// Reduce_health(obj, amount, type).
	Game_object* obj  = get_item(parms[0]);
	const int    type = parms[2].get_int_value();
	if (obj) {    // Dies if health goes too low.
		obj->reduce_health(parms[1].get_int_value(), type);
	}
	return no_ret;
}

USECODE_INTRINSIC(is_readied) {
	ignore_unused_variable_warning(num_parms);
	// is_readied(npc, where, itemshape, frame (-359=any)).
	// Where:
	//   0=back,
	//   1=weapon hand,
	//   2=other hand,
	//   3=belt,
	//   4=neck,
	//   5=torso,
	//   6=one finger,
	//   7=other finger,
	//   8=quiver,
	//   9=head,
	//  10=legs,
	//  11=feet
	//  20=???
	// Appears to be the same for BG and SI; SI's get_readied
	// is far better in any case, and should be used instead.

	Actor* npc = as_actor(get_item(parms[0]));
	if (!npc) {
		return Usecode_value(0);
	}
	const int where = parms[1].get_int_value();
	const int shnum = parms[2].get_int_value();
	const int frnum = parms[3].get_int_value();
	// Spot defined in Actor class.
	const int spot
			= GAME_BG ? Ready_spot_from_BG(where) : Ready_spot_from_SI(where);
	if (spot >= 0 && spot <= ucont) {
		// See if it's the right one.
		Game_object* obj = npc->get_readied(spot);
		if (obj && obj->get_shapenum() == shnum
			&& (frnum == c_any_framenum || obj->get_framenum() == frnum)) {
			return Usecode_value(1);
		}
	} else if (spot < 0) {
		cerr << "Readied: invalid spot #: " << spot << endl;
	}
	return Usecode_value(0);
}

USECODE_INTRINSIC(get_readied) {
	ignore_unused_variable_warning(num_parms);
	// get_readied(npc, where)
	// Where:
	//   0=other hand,
	//   1=weapon hand,
	//   2=cloak,
	//   3=neck,
	//   4=head,
	//   5=gloves,
	//   6=usecode container,
	//   7=one finger,
	//   8=other finger,
	//   9=earrings,
	//  10=quiver,
	//  11=belt,
	//  12=torso,
	//  13=feet,
	//  14=legs,
	//  15=backpack,
	//  16=back shield,
	//  17=back spot

	Actor* npc = as_actor(get_item(parms[0]));
	if (!npc) {
		return Usecode_value(0);
	}
	const int where = parms[1].get_int_value();
	// Spot defined in Actor class.
	const int spot
			= GAME_BG ? Ready_spot_from_BG(where) : Ready_spot_from_SI(where);
	if (spot >= 0 && spot <= ucont) {
		return Usecode_value(npc->get_readied(spot));
	} else if (spot < 0) {
		cerr << "Readied: invalid spot #: " << spot << endl;
	}
	return Usecode_value(0);
}

USECODE_INTRINSIC(restart_game) {
	ignore_unused_variable_warning(num_parms, parms);
	// Think it's 'restart game'.
	// Happens if you die before leaving trinsic.
	Audio::get_ptr()->stop_music();
	quitting_time = QUIT_TIME_RESTART;    // Quit & restart.
	return no_ret;
}

static int get_speech_face(int speech_track) {
	// TODO: de-hard-code this.
	if (!GAME_SI) {
		return -1;
	}
	if (speech_track < 21) {
		// Balance Serpent.
		Actor* ava = Game_window::get_instance()->get_main_actor();
		// Wearing serpent ring?
		Game_object* obj = ava->get_readied(lfinger);
		if (obj && obj->get_shapenum() == 0x377 && obj->get_framenum() == 1) {
			// Solid.
			return 295;
		} else if (
				(obj = ava->get_readied(rfinger)) != nullptr
				&& obj->get_shapenum() == 0x377 && obj->get_framenum() == 1) {
			// Solid.
			return 295;
		}
		// Translucent.
		return 300;
	}
	if (speech_track < 23) {
		// Guardian.
		return 296;
	}
	if (speech_track < 25) {
		// Arcadion.
		return 256;
	}
	if (speech_track == 25) {
		// Chaos serpent.
		return 293;
	}
	if (speech_track == 26) {
		// Order serpent.
		return 294;
	}
	return -1;
}

USECODE_INTRINSIC(start_speech) {
	ignore_unused_variable_warning(num_parms);
	// Start_speech(num).  Also sets speech_track.
	bool okay    = false;
	speech_track = parms[0].get_int_value();
	if (speech_track >= 0) {
		ucmachine->do_speech(speech_track);
		okay = true;
	}
	if (!okay) {
		// Failed?  Clear faces.  (Fixes SI).
		init_conversation();
	} else {
		// Show guardian, serpent.
		const int face = get_speech_face(speech_track);
		if (face >= 0) {
			Usecode_value sh(face);
			Usecode_value fr(0);
			show_npc_face(sh, fr);
			int x;
			int y;    // Wait for click.
			Get_click(x, y, Mouse::hand);
			remove_npc_face(sh);
		}
	}
	return Usecode_value(okay ? 1 : 0);
}

USECODE_INTRINSIC(start_blocking_speech) {
	ignore_unused_variable_warning(num_parms);
	// Start_speech(num).  Also sets speech_track.
	bool okay    = false;
	speech_track = parms[0].get_int_value();
	if (speech_track >= 0) {
		ucmachine->do_speech(speech_track);
		okay = true;
	}
	if (!okay) {
		// Failed?  Clear faces.  (Fixes SI).
		init_conversation();
	} else {
		// Show guardian, serpent.
		eman->remove_text_effects();
		const int     face = get_speech_face(speech_track);
		Usecode_value sh(face);
		if (face >= 0) {
			Usecode_value fr(0);
			show_npc_face(sh, fr);
		}

		const bool os           = Mouse::mouse->is_onscreen();
		uint32     last_repaint = 0;    // For insuring animation repaints.
		do {
			Delay();                 // Wait a fraction of a second.
			Mouse::mouse->hide();    // Turn off mouse.
			Mouse::mouse_update = false;
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_EVENT_MOUSE_MOTION) {
					// Mouse scale factor
					int mx;
					int my;
					gwin->get_win()->screen_to_game(
							event.motion.x, event.motion.y,
							gwin->get_fastmouse(), mx, my);
					Mouse::mouse->move(mx, my);
					Mouse::mouse_update = true;
				}
			}
			// Get current time, & animate.
			const uint32 ticks = SDL_GetTicks();
			Game::set_ticks(ticks);
			// Show animation every 1/20 sec.
			if (ticks > last_repaint + 50 || gwin->was_painted()) {
				gwin->paint_dirty();
				while (ticks > last_repaint + 50) {
					last_repaint += 50;
				}
			}

			Mouse::mouse->show();    // Re-display mouse.
			// Blit to screen if necessary, or if mouse changed.
			if (!gwin->show() && Mouse::mouse_update) {
				Mouse::mouse->blit_dirty();
			}
		} while (Audio::get_ptr()->is_voice_playing());

		if (face >= 0) {
			int x;
			int y;    // Wait for click.
			Get_click(x, y, Mouse::hand);
			remove_npc_face(sh);
		}

		if (!os) {
			Mouse::mouse->hide();
		}
	}
	return Usecode_value(okay ? 1 : 0);
}

USECODE_INTRINSIC(is_water) {
	ignore_unused_variable_warning(num_parms);
	// Is_water(pos).
	const size_t size = parms[0].get_array_size();
	if (size >= 2 && size <= 4) {
		if (size == 4) {
			// Exult extention: if this is a [obj, x, y, z], check if the object
			// (if any) has the flag water set.
			auto* obj = get_item(parms[0].get_elem(0));
			if (obj != nullptr) {
				return Usecode_value(obj->get_info().is_water());
			}
		}
		const int off = size == 4 ? 1 : 0;
		// The original completely ignores the third coordinate.
		const int xpos = parms[0].get_elem(0 + off).get_int_value();
		const int ypos = parms[0].get_elem(1 + off).get_int_value();
		const int x    = (xpos - gwin->get_scrolltx()) * c_tilesize;
		const int y    = (ypos - gwin->get_scrollty()) * c_tilesize;
		ShapeID   sid  = gwin->get_flat(x, y);
		if (sid.is_invalid()) {
			return Usecode_value(0);
		}
		const Shape_info& info = sid.get_info();
		return Usecode_value(info.is_water());
	}
	return Usecode_value(0);
}

USECODE_INTRINSIC(run_endgame) {
	ignore_unused_variable_warning(num_parms);
	Audio::get_ptr()->stop_sound_effects();
	game->end_game(parms[0].get_int_value() != 0, true);
	// If successful enable menu entry and play credits afterwards
	if (parms[0].get_int_value() != 0) {
		U7open_out("<SAVEGAME>/endgame.flg");
		game->show_credits();
	}
	quitting_time = QUIT_TIME_YES;
	return no_ret;
}

USECODE_INTRINSIC(fire_projectile) {
	ignore_unused_variable_warning(num_parms);
	// fire_projectile(attacker, dir, missile, attval, wshape, ashape)

	Game_object* attacker = get_item(parms[0]);
	// Get direction (0-7).
	const int dir = parms[1].get_int_value();
	const int missile
			= parms[2].get_int_value();    // Sprite to use for missile.
	const int attval = parms[3].get_int_value();    // Attack value.
	const int wshape
			= parms[4].get_int_value();    // What to use for weapon info.
	const int ashape
			= parms[5].get_int_value();    // What to use for ammo info.

	const Tile_coord pos = attacker->get_missile_tile(dir);
	const Tile_coord adj = pos.get_neighbor(dir % 8);
	// Make it go dist tiles.
	const int  dx   = adj.tx - pos.tx;
	const int  dy   = adj.ty - pos.ty;
	Tile_coord dest = pos;
	const int  dist = 31;
	dest.tx += dist * dx;
	dest.ty += dist * dy;

	// Fire missile.
	gwin->get_effects()->add_effect(std::make_unique<Projectile_effect>(
			attacker, dest, wshape, ashape, missile, attval, 4));
	return no_ret;
}

USECODE_INTRINSIC(nap_time) {
	ignore_unused_variable_warning(num_parms);
	// nap_time(bed)
	Game_object* bed = get_item(parms[0]);
	if (!bed) {
		return no_ret;
	}
	// See if bed is occupied by an NPC.
	if (Sleep_schedule::is_bed_occupied(bed, gwin->get_main_actor())) {
		// Show party member's face.
		const int party_cnt = partyman->get_count();
		const int npcnum
				= party_cnt ? partyman->get_member(rand() % party_cnt) : 356;
		Usecode_value actval(-npcnum);
		Usecode_value frval(0);
		show_npc_face(actval, frval);
		conv->show_npc_message(
				get_text_msg(first_bed_occupied + rand() % num_bed_occupied));
		remove_npc_face(actval);
		gwin->get_main_actor()->set_schedule_type(Schedule::follow_avatar);
		return no_ret;
	}
	Schedule* sched = gwin->get_main_actor()->get_schedule();
	if (sched) {    // Tell (sleep) sched. to use bed.
		sched->set_bed(bed);
	}
	return no_ret;
}

USECODE_INTRINSIC(advance_time) {
	ignore_unused_variable_warning(num_parms);
	// Incr. clock by (parm[0]*.04min.).
	gclock->increment(parms[0].get_int_value() / ticks_per_minute);
	return no_ret;
}

USECODE_INTRINSIC(in_usecode) {
	ignore_unused_variable_warning(num_parms);
	// in_usecode(item):  Return 1 if executing usecode on parms[0].

	Game_object* obj = get_item(parms[0]);
	if (!obj) {
		return Usecode_value(0);
	}
	return Usecode_value(Usecode_script::find(obj) != nullptr);
}

USECODE_INTRINSIC(call_guards) {
	ignore_unused_variable_warning(num_parms, parms);
	// Attack thieving Avatar.
	gwin->call_guards();
	return no_ret;
}

USECODE_INTRINSIC(stop_arresting) {
	ignore_unused_variable_warning(num_parms, parms);
	// Seems to be what it does.
	gwin->stop_arresting();
	return no_ret;
}

USECODE_INTRINSIC(attack_avatar) {
	ignore_unused_variable_warning(num_parms, parms);
	// Attack thieving Avatar.
	gwin->attack_avatar();
	return no_ret;
}

USECODE_INTRINSIC(path_run_usecode) {
	// exec(loc(x,y,z)?, usecode#, itemref, eventid).
	// Think it should have Avatar walk path to loc, return 0
	//  if he can't get there (and return), 1 if he can.
	Usecode_value ava(gwin->get_main_actor());
	const bool simode = num_parms > 4 ? parms[4].get_int_value() != 0 : GAME_SI;
	return Usecode_value(path_run_usecode(
			ava, parms[0], parms[1], parms[2], parms[3],
			// SI:  Look for free spot. (Guess).
			simode, false,
			true));    // Bring companions.
}

USECODE_INTRINSIC(close_gump) {
	ignore_unused_variable_warning(num_parms);
	// close_gump(container)
	if (!gwin->is_dragging()) {    // NOT while dragging stuff.
		Game_object* obj  = get_item(parms[0]);
		Gump*        gump = gumpman->find_gump(obj, c_any_shapenum);
		if (gump) {
			gumpman->close_gump(gump);
			gwin->set_all_dirty();
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(close_gump2) {
	ignore_unused_variable_warning(num_parms);
	// close_gump(container)
	Game_object* obj  = get_item(parms[0]);
	Gump*        gump = gumpman->find_gump(obj, c_any_shapenum);
	if (gump) {
		gumpman->close_gump(gump);
		gwin->set_all_dirty();
	}
	return no_ret;
}

USECODE_INTRINSIC(close_gumps) {
	ignore_unused_variable_warning(num_parms, parms);
	if (!gwin->is_dragging()) {    // NOT while dragging stuff.
		gumpman->close_all_gumps();
	}
	return no_ret;
}

USECODE_INTRINSIC(close_gumps2) {
	ignore_unused_variable_warning(num_parms, parms);
	gumpman->close_all_gumps();
	return no_ret;
}

USECODE_INTRINSIC(in_gump_mode) {
	ignore_unused_variable_warning(num_parms, parms);
	// No persistent
	return Usecode_value(gumpman->showing_gumps(true));
}

USECODE_INTRINSIC(is_not_blocked) {
	ignore_unused_variable_warning(num_parms);
	// Is_not_blocked(tile, shape, frame (or -359).
	Usecode_value fail(0);
	// Parm. 0 should be tile coords.
	Usecode_value& pval = parms[0];
	if (pval.get_array_size() < 3) {
		return fail;
	}
	const Tile_coord tile(
			pval.get_elem(0).get_int_value(), pval.get_elem(1).get_int_value(),
			pval.get_elem(2).get_int_value());
	const int shapenum = parms[1].get_int_value();
	const int framenum = parms[2].get_int_value();
	// Find out about given shape.
	const Shape_info& info = ShapeID::get_info(shapenum);
	const TileRect    footprint(
            tile.tx - info.get_3d_xtiles(framenum) + 1,
            tile.ty - info.get_3d_ytiles(framenum) + 1,
            info.get_3d_xtiles(framenum), info.get_3d_ytiles(framenum));
	int        new_lift;
	const bool blocked = Map_chunk::is_blocked(
			info.get_3d_height(), tile.tz, footprint.x, footprint.y,
			footprint.w, footprint.h, new_lift, MOVE_ALL_TERRAIN, 1);
	// Okay?
	if (!blocked && new_lift == tile.tz) {
		return Usecode_value(1);
	} else {
		return Usecode_value(0);
	}
}

USECODE_INTRINSIC(direction_from) {
	ignore_unused_variable_warning(num_parms);
	// ?Direction from parm[0] -> parm[1].
	// Rets. 0-7, with 0 = North, 1 = Northeast, etc.
	// Same as 0x1a??
	Usecode_value u = find_direction(parms[0], parms[1]);
	return u;
}

/*
 *  Test for a 'moving barge' flag.
 */

static bool Is_moving_barge_flag(int fnum) {
	if (Game::get_game_type() == BLACK_GATE) {
		return fnum == static_cast<int>(Obj_flags::on_moving_barge)
			   || fnum == static_cast<int>(Obj_flags::active_barge);
	} else {    // SI.
		return fnum == static_cast<int>(Obj_flags::si_on_moving_barge) ||
			   // Ice raft needs this one:
			   fnum == static_cast<int>(Obj_flags::on_moving_barge)
			   || fnum == static_cast<int>(Obj_flags::active_barge);
	}
}

USECODE_INTRINSIC(get_item_flag) {
	ignore_unused_variable_warning(num_parms);
	// Get npc flag(item, flag#).
	Game_object* obj = get_item(parms[0]);
	if (!obj) {
		return Usecode_value(0);
	}
	const int fnum = parms[1].get_int_value();
	// Special cases:
	if (Is_moving_barge_flag(fnum)) {
		// Test for moving barge.
		Barge_object* barge;
		if (!gwin->get_moving_barge() || !(barge = Get_barge(obj))) {
			return Usecode_value(0);
		}
		return Usecode_value(barge == gwin->get_moving_barge());
	} else if (fnum == static_cast<int>(Obj_flags::okay_to_land)) {
		// Okay to land flying carpet?
		Barge_object* barge = Get_barge(obj);
		if (!barge) {
			return Usecode_value(0);
		}
		return Usecode_value(barge->okay_to_land());
	} else if (fnum == static_cast<int>(Obj_flags::immunities)) {
		const Actor*        npc = obj->as_actor();
		const Monster_info* inf = obj->get_info().get_monster_info();
		return Usecode_value(
				(inf != nullptr && inf->power_safe())
				|| (npc && npc->check_gear_powers(Frame_flags::power_safe)));
	} else if (fnum == static_cast<int>(Obj_flags::cant_die)) {
		const Actor*        npc = obj->as_actor();
		const Monster_info* inf = obj->get_info().get_monster_info();
		return Usecode_value(
				(inf != nullptr && inf->death_safe())
				|| (npc && npc->check_gear_powers(Frame_flags::death_safe)));
	} else if (fnum == Obj_flags::is_solid) {
		// Verified. The previous version worked well because of a bug in both
		// BG and SI gang-planck usecode, which checked the flag on the wrong
		// object. Also, no non-solid object is in a position where it could
		// block the gang-planck.
		return Usecode_value(obj->get_info().is_solid());
	} else if (fnum == static_cast<int>(Obj_flags::in_dungeon)) {
		return Usecode_value(
				obj == gwin->get_main_actor() && gwin->is_in_dungeon());
	} else if (fnum == Obj_flags::active_sailor) {
		// Must be the sailor, as this is used to check for Ferryman.
		return Usecode_value(sailor);
	}
	Usecode_value u(obj->get_flag(fnum));
	return u;
}

USECODE_INTRINSIC(set_item_flag) {
	ignore_unused_variable_warning(num_parms);
	// Set npc flag(item, flag#).
	Game_object* obj  = get_item(parms[0]);
	const int    flag = parms[1].get_int_value();
	if (!obj) {
		return no_ret;
	}
	switch (flag) {
	case Obj_flags::dont_move:
	case Obj_flags::bg_dont_move:
		obj->set_flag(flag);
		// Get out of combat mode.
		if (obj == gwin->get_main_actor() && gwin->in_combat()) {
			gwin->toggle_combat();
		}
		// Show change in status.
		gwin->set_all_dirty();
		break;
	case Obj_flags::charmed: {
		obj->set_flag(flag);
		Actor* npc = obj->as_actor();
		if (npc) {
			npc->set_effective_alignment(Actor::good);    // Verified.
		}
		break;
	}
	case Obj_flags::invisible:
		obj->set_flag(flag);
		gwin->add_dirty(obj);
		break;
	case Obj_flags::active_sailor:
		// The sailor (Ferryman or sails).
		sailor = obj;
		break;
	default:
		obj->set_flag(flag);
		if (Is_moving_barge_flag(flag)) {
			// Set barge in motion.
			Barge_object* barge = Get_barge(obj);
			if (barge) {
				gwin->set_moving_barge(barge);
			}
		}
		break;
	}
	return no_ret;
}

USECODE_INTRINSIC(clear_item_flag) {
	ignore_unused_variable_warning(num_parms);
	// Clear npc flag(item, flag#).
	Game_object* obj  = get_item(parms[0]);
	const int    flag = parms[1].get_int_value();
	if (obj) {
		Actor* npc = obj->as_actor();
		if (flag != Obj_flags::asleep || !npc || !npc->is_knocked_out()) {
			obj->clear_flag(flag);
		}
		if (flag == Obj_flags::dont_move || flag == Obj_flags::bg_dont_move) {
			// Show change in status.
			show_pending_text();    // Fixes Lydia-tatoo.
			gwin->set_all_dirty();
		} else if (Is_moving_barge_flag(flag)) {
			// Stop barge object is on or part of.
			Barge_object* barge = Get_barge(obj);
			if (barge && barge == gwin->get_moving_barge()) {
				gwin->set_moving_barge(nullptr);
			}
		} else if (flag == Obj_flags::active_sailor) {
			// Handles Ferryman and sails
			sailor = nullptr;
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(set_path_failure) {
	ignore_unused_variable_warning(num_parms);
	// set_path_failure(fun, itemref, eventid) for the last NPC in
	//  a path_run_usecode() call.

	const int    fun     = parms[0].get_int_value();
	const int    eventid = parms[2].get_int_value();
	Game_object* item    = get_item(parms[1]);
	if (path_npc && item) {    // Set in path_run_usecode().
		If_else_path_actor_action* action
				= path_npc->get_action()
						  ? path_npc->get_action()->as_usecode_path()
						  : nullptr;
		if (action) {    // Set in in path action.
			action->set_failure(new Usecode_actor_action(fun, item, eventid));
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(fade_palette) {
	ignore_unused_variable_warning(num_parms);
	// Fade(cycles?, ??(always 1), in_out (0=fade to black, 1=fade in)).
	const int cycles = parms[0].get_int_value();
	const int inout  = parms[2].get_int_value();
	if (inout == 0) {
		show_pending_text();    // Make sure prev. text was seen.
	} else {
		gclock->reset_palette();
	}
	gwin->get_pal()->fade(cycles, inout);
	if (inout == 0) {
		gwin->toggle_ambient_light(false);
	}
	return no_ret;
}

USECODE_INTRINSIC(fade_palette_sleep) {
	ignore_unused_variable_warning(num_parms);
	// Fade(cycles?, ??(always 1), in_out (0=fade to black, 1=fade in)).
	const int cycles = parms[0].get_int_value();
	const int inout  = parms[2].get_int_value();
	if (inout == 0) {
		show_pending_text();    // Make sure prev. text was seen.
		Audio::get_ptr()->start_music(Audio::game_music(33));
	} else {
		Audio::get_ptr()->start_music(Audio::game_music(31));
		gclock->reset_palette();
	}
	gwin->get_pal()->fade(cycles, inout);
	return no_ret;
}

USECODE_INTRINSIC(get_party_list2) {
	ignore_unused_variable_warning(num_parms, parms);
	// Return party.  Same as 0x23
	// Probably returns a list of everyone with (or without) some flag
	// List of live chars? Dead chars?
	Usecode_value u(get_party());
	return u;
}

USECODE_INTRINSIC(set_camera) {
	ignore_unused_variable_warning(num_parms);
	// Set_camera(actor)
	Actor* actor = as_actor(get_item(parms[0]));
	if (actor) {
		gwin->set_camera_actor(actor);
		activate_cached(actor->get_tile());    // Mar-10-01 - For Test of Love.
	} else {
		Game_object* obj = get_item(parms[0]);
		if (obj) {
			const Tile_coord t = obj->get_tile();
			gwin->center_view(t);
			activate_cached(t);    // Mar-10-01 - For Test of Love.
		}
	}

	return no_ret;
}

USECODE_INTRINSIC(in_combat) {
	ignore_unused_variable_warning(num_parms, parms);
	// Are we in combat mode?
	return Usecode_value(gwin->in_combat());
}

USECODE_INTRINSIC(center_view) {
	ignore_unused_variable_warning(num_parms);
	// Center view around given item.
	Game_object* obj = get_item(parms[0]);
	if (obj) {
		const Tile_coord t = obj->get_tile();
		gwin->center_view(t);
		activate_cached(t);    // Mar-10-01 - For Test of Love.
	}
	return no_ret;
}

USECODE_INTRINSIC(view_tile) {
	ignore_unused_variable_warning(num_parms);
	// Center view around given item.
	Tile_coord t;
	if (!parms[0].is_array() || parms[0].get_array_size() < 2) {
		return no_ret;
	} else {
		t = Tile_coord(
				parms[0].get_elem(0).get_int_value(),
				parms[0].get_elem(1).get_int_value(), 0);
	}
	gwin->center_view(t);
	return no_ret;
}

USECODE_INTRINSIC(get_dead_party) {
	ignore_unused_variable_warning(num_parms);
	// Return list of dead companions' bodies.
	Game_object* obj = get_item(parms[0]);
	if (!obj) {
		return no_ret;
	}
	const int     cnt = partyman->get_dead_count();
	Usecode_value ret(cnt, nullptr);
	for (int i = 0; i < cnt; i++) {
		Game_object* body = gwin->get_body(partyman->get_dead_member(i));
		// Body within 50 tiles (a guess)?
		if (body && body->distance(obj) < 50) {
			Usecode_value v(body);
			ret.put_elem(i, v);
		}
	}
	return ret;
}

USECODE_INTRINSIC(play_sound_effect) {
	if (num_parms < 1) {
		return no_ret;
	}
	// Play music(isongnum).
	COUT("Sound effect " << parms[0].get_int_value() << " request in usecode");

	Audio::get_ptr()->play_sound_effect(parms[0].get_int_value());
	return no_ret;
}

USECODE_INTRINSIC(play_sound_effect2) {
	if (num_parms < 2) {
		return no_ret;
	}
	// Play music(songnum, item).
	Game_object* obj = get_item(parms[1]);
	Object_sfx::Play(obj, parms[0].get_int_value(), 0);
#ifdef DEBUG
	cout << "Sound effect(2) " << parms[0].get_int_value()
		 << " request in usecode" << endl;
#endif
	return no_ret;
}

USECODE_INTRINSIC(get_npc_id) {
	ignore_unused_variable_warning(num_parms);
	Actor* actor = as_actor(get_item(parms[0]));
	if (!actor) {
		return Usecode_value(0);
	}
	return Usecode_value(actor->get_ident());
}

USECODE_INTRINSIC(set_npc_id) {
	ignore_unused_variable_warning(num_parms);
	Actor* actor = as_actor(get_item(parms[0]));
	if (actor) {
		actor->set_ident(parms[1].get_int_value());
	}
	return no_ret;
}

USECODE_INTRINSIC(add_cont_items) {
	ignore_unused_variable_warning(num_parms);
	// Add items(num, item, ??quality?? (-359), frame (or -359), T/F).
	return add_cont_items(
			parms[0], parms[1], parms[2], parms[3], parms[4], parms[5]);
}

// Is this SI Only
USECODE_INTRINSIC(remove_cont_items) {
	ignore_unused_variable_warning(num_parms);
	// Add items(num, item, ??quality?? (-359), frame (or -359), T/F).
	return remove_cont_items(
			parms[0], parms[1], parms[2], parms[3], parms[4], parms[5]);
}

/*
 *  SI-specific functions.
 */

USECODE_INTRINSIC(show_npc_face0) {
	ignore_unused_variable_warning(num_parms);
	// Show_npc_face0(npc, frame).  Show in position 0.
	show_npc_face(parms[0], parms[1], 0);
	return no_ret;
}

USECODE_INTRINSIC(show_npc_face1) {
	ignore_unused_variable_warning(num_parms);
	// Show_npc_face1(npc, frame).  Show in position 1.
	show_npc_face(parms[0], parms[1], 1);
	return no_ret;
}

USECODE_INTRINSIC(remove_npc_face0) {
	ignore_unused_variable_warning(num_parms, parms);
	show_pending_text();
	conv->remove_slot_face(0);
	return no_ret;
}

USECODE_INTRINSIC(remove_npc_face1) {
	ignore_unused_variable_warning(num_parms, parms);
	show_pending_text();
	conv->remove_slot_face(1);
	return no_ret;
}

USECODE_INTRINSIC(change_npc_face0) {
	ignore_unused_variable_warning(num_parms);
	show_pending_text();
	conv->change_face_frame(parms[0].get_int_value(), 0);
	return no_ret;
}

USECODE_INTRINSIC(change_npc_face1) {
	ignore_unused_variable_warning(num_parms);
	show_pending_text();
	conv->change_face_frame(parms[0].get_int_value(), 1);
	return no_ret;
}

USECODE_INTRINSIC(reset_conv_face) {
	// Seems to be right.
	ignore_unused_variable_warning(num_parms, parms);
	show_pending_text();
	conv->change_face_frame(0, 0);
	return no_ret;
}

USECODE_INTRINSIC(set_conversation_slot) {
	ignore_unused_variable_warning(num_parms);
	// set_conversation_slot(0 or 1) - Choose which face is talking.
	conv->set_slot(parms[0].get_int_value());
	return no_ret;
}

USECODE_INTRINSIC(init_conversation) {
	ignore_unused_variable_warning(num_parms, parms);
	init_conversation();
	return no_ret;
}

USECODE_INTRINSIC(end_conversation) {
	ignore_unused_variable_warning(num_parms, parms);
	show_pending_text();    // Wait for click if needed.
	conv->init_faces();     // Removes faces from screen.
	gwin->set_all_dirty();
	return no_ret;
}

USECODE_INTRINSIC(si_path_run_usecode) {
	ignore_unused_variable_warning(num_parms);
	// exec(npc, loc(x,y,z), eventid, itemref, usecode#, flag_always).
	// Schedule Npc to walk to loc and then execute usecode.
	const int always = parms[5].get_int_value();
	path_run_usecode(
			parms[0], parms[1], parms[4], parms[3], parms[2], true,
			always != 0);
	return no_ret;
}

USECODE_INTRINSIC(sib_path_run_usecode) {
	ignore_unused_variable_warning(num_parms);
	// exec(npc, loc(x,y,z), usecode#, itemref, eventid).
	// Schedule Npc to walk to loc and then execute usecode.
	return Usecode_value(path_run_usecode(
			parms[0], parms[1], parms[2], parms[3], parms[4], false, false));
}

USECODE_INTRINSIC(error_message) {
	// exec(array)
	// Output everything to stdout

	for (int i = 0; i < num_parms; i++) {
		if (parms[i].is_int()) {
			std::cout << parms[i].get_int_value();
		} else if (parms[i].is_ptr()) {
			std::cout << parms[i].get_ptr_value();
		} else if (!parms[i].is_array()) {
			std::cout << parms[i].get_str_value();
		} else {
			for (size_t j = 0; j < parms[i].get_array_size(); j++) {
				if (parms[i].get_elem(j).is_int()) {
					std::cout << parms[i].get_elem(j).get_int_value();
				} else if (parms[i].get_elem(j).is_ptr()) {
					std::cout << parms[i].get_elem(j).get_ptr_value();
				} else if (!parms[i].get_elem(j).is_array()) {
					std::cout << parms[i].get_elem(j).get_str_value();
				}
			}
		}
	}

	std::cout << std::endl;
	return no_ret;
}

USECODE_INTRINSIC(set_polymorph) {
	ignore_unused_variable_warning(num_parms);
	// exec(npc, shape).
	// Npc's shape is change to shape.
	Actor* actor = as_actor(get_item(parms[0]));
	if (actor) {
		actor->set_polymorph(parms[1].get_int_value());
	}
	return no_ret;
}

USECODE_INTRINSIC(set_new_schedules) {
	ignore_unused_variable_warning(num_parms);
	// set_new_schedules ( npc, time, activity, [x, y] )
	//
	// or
	//
	// set_new_schedules ( npc, [   time1, time2, ...],
	//          [activity1, activity2, ...],
	//          [x1,y1, x2, y2, ...] )
	//

	Actor* actor = as_actor(get_item(parms[0]));

	// If no actor return
	if (!actor) {
		return no_ret;
	}

	const int count = parms[1].is_array() ? parms[1].get_array_size() : 1;
	auto*     list  = new Schedule_change[count];

	if (!parms[1].is_array()) {
		const int time  = parms[1].get_int_value();
		const int sched = parms[2].get_int_value();
		const int tx    = parms[3].get_elem(0).get_int_value();
		const int ty    = parms[3].get_elem(1).get_int_value();
		list[0].set(tx, ty, 0, sched, time);
	} else {
		for (int i = 0; i < count; i++) {
			const int time  = parms[1].get_elem(i).get_int_value();
			const int sched = parms[2].get_elem(i).get_int_value();
			const int tx    = parms[3].get_elem(i * 2).get_int_value();
			const int ty    = parms[3].get_elem(i * 2 + 1).get_int_value();
			list[i].set(tx, ty, 0, sched, time);
		}
	}

	actor->set_schedules(list, count);

	return no_ret;
}

USECODE_INTRINSIC(revert_schedule) {
	ignore_unused_variable_warning(num_parms);
	// revert_schedule(npc)
	// Reverts the schedule of the npc to the saved state in
	// <STATIC>/schedule.dat

	Actor* actor = as_actor(get_item(parms[0]));
	if (actor) {
		gwin->revert_schedules(actor);
	}

	return no_ret;
}

USECODE_INTRINSIC(run_schedule) {
	ignore_unused_variable_warning(num_parms);
	// run_schedule(npc)
	// I think this is actually reset activity to current
	// scheduled activity - Colourless
	Actor* actor = as_actor(get_item(parms[0]));

	if (actor) {
		actor->update_schedule(gclock->get_hour() / 3);
	}

	return no_ret;
}

USECODE_INTRINSIC(modify_schedule) {
	ignore_unused_variable_warning(num_parms);
	// modify_schedule ( npc, time, activity, [x, y] )

	Actor* actor = as_actor(get_item(parms[0]));

	// If no actor return
	if (!actor) {
		return no_ret;
	}

	const int time  = parms[1].get_int_value();
	const int sched = parms[2].get_int_value();
	const int tx    = parms[3].get_elem(0).get_int_value();
	const int ty    = parms[3].get_elem(1).get_int_value();

	actor->set_schedule_time_type(time, sched);
	actor->set_schedule_time_location(time, tx, ty);

	return no_ret;
}

USECODE_INTRINSIC(get_temperature) {
	ignore_unused_variable_warning(num_parms);
	Actor* npc = as_actor(get_item(parms[0]));
	return Usecode_value(npc ? npc->get_temperature() : 0);
}

USECODE_INTRINSIC(set_temperature) {
	ignore_unused_variable_warning(num_parms);
	// set_temperature(npc, value (0-63)).
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc) {
		npc->set_temperature(parms[1].get_int_value());
	}
	return no_ret;
}

USECODE_INTRINSIC(get_temperature_zone) {
	ignore_unused_variable_warning(num_parms);
	// get_temperature_zone(npc).
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc) {
		return Usecode_value(npc->get_temperature_zone());
	}
	return Usecode_value(0);
}

USECODE_INTRINSIC(get_npc_warmth) {
	ignore_unused_variable_warning(num_parms);
	// get_npc_warmth(npc).
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc) {
		return Usecode_value(npc->figure_warmth());
	}
	return Usecode_value(-75);
}

#if 0 /* +++++Not used at the moment. */
USECODE_INTRINSIC(add_removed_npc) {
	// move_offscreen(npc, x, y) - I think (seems good) I think
	//
	// returns false if not moved
	// else, moves the object to the closest position off

	// Actor we want to move
	Actor *actor = as_actor(get_item(parms[0]));

	// Need to check superchunk
	int cx = actor->get_cx();
	int cy = actor->get_cx();
	int scx = cx / c_chunks_per_schunk;
	int scy = cy / c_chunks_per_schunk;
	int scx2 = parms[1].get_int_value() / c_tiles_per_schunk;
	int scy2 = parms[2].get_int_value() / c_tiles_per_schunk;

	// Are the coords are good
	if ((cx == 0xff && scx2) || (cx != 0xff && scx != scx2) ||
	        (cy == 0xff && scy2) || (cy != 0xff && scy != scy2))
		return Usecode_value(false);


	// Ok they were good, now move it


	// Get the tiles around the edge of the screen
	TileRect rect = gwin->get_win_tile_rect();

	int sx = rect.x;        // Tile coord of x start
	int ex = rect.x + rect.w;   // Tile coord of x end
	int sy = rect.y;        // y start
	int ey = rect.y + rect.h;   // x end

	// The height of the Actor we are checking
	int height = actor->get_info().get_3d_height();

	int i = 0, nlift = 0;
	int tx, ty;

	// Avatars coords
	Tile_coord av = gwin->get_main_actor()->get_tile();;

	Tile_coord close;   // The tile coords of the closest tile
	int dist = -1;      // The distance

	cy = sy / c_tiles_per_chunk;
	ty = sy % c_tiles_per_chunk;
	cout << "1" << endl;
	for (i = 0; i < rect.w; i++) {
		cx = (sx + i) / c_tiles_per_chunk;
		tx = (sx + i) % c_tiles_per_chunk;

		Map_chunk *clist = gmap->get_chunk_safely(cx, cy);
		if (!clist) continue;
		clist->setup_cache();
		if (!clist->is_blocked(height, 0, tx, ty, nlift, actor->get_type_flags(), 1)) {
			Tile_coord cur(tx + cx * c_tiles_per_chunk, ty + cy * c_tiles_per_chunk, nlift);
			if (cur.distance(av) < dist || dist == -1) {
				dist = cur.distance(av);
				cout << "(" << cur.tx << ", " << cur.ty << ") " << dist << endl;
				close = cur;
			}
		}
	}

	cx = ex / c_tiles_per_chunk;
	tx = ex % c_tiles_per_chunk;
	cout << "2" << endl;
	for (i = 0; i < rect.h; i++) {
		cy = (sy + i) / c_tiles_per_chunk;
		ty = (sy + i) % c_tiles_per_chunk;

		Map_chunk *clist = gmap->get_chunk_safely(cx, cy);
		if (!clist) continue;
		clist->setup_cache();
		if (!clist->is_blocked(height, 0, tx, ty, nlift, actor->get_type_flags(), 1)) {
			Tile_coord cur(tx + cx * c_tiles_per_chunk, ty + cy * c_tiles_per_chunk, nlift);
			if (cur.distance(av) < dist || dist == -1) {
				dist = cur.distance(av);
				cout << "(" << cur.tx << ", " << cur.ty << ") " << dist << endl;
				close = cur;
			}
		}
	}

	cy = ey / c_tiles_per_chunk;
	ty = ey % c_tiles_per_chunk;
	cout << "3" << endl;
	for (i = 0; i < rect.w; i++) {
		cx = (ex - i) / c_tiles_per_chunk;
		tx = (ex - i) % c_tiles_per_chunk;

		Map_chunk *clist = gmap->get_chunk_safely(cx, cy);
		if (!clist) continue;
		clist->setup_cache();
		if (!clist->is_blocked(height, 0, tx, ty, nlift, actor->get_type_flags(), 1)) {
			Tile_coord cur(tx + cx * c_tiles_per_chunk, ty + cy * c_tiles_per_chunk, nlift);
			if (cur.distance(av) < dist || dist == -1) {
				dist = cur.distance(av);
				cout << "(" << cur.tx << ", " << cur.ty << ") " << dist << endl;
				close = cur;
			}
		}
	}

	cx = sx / c_tiles_per_chunk;
	tx = sx % c_tiles_per_chunk;
	cout << "4" << endl;
	for (i = 0; i < rect.h; i++) {
		cy = (ey - i) / c_tiles_per_chunk;
		ty = (ey - i) % c_tiles_per_chunk;

		Map_chunk *clist = gmap->get_chunk_safely(cx, cy);
		if (!clist) continue;
		clist->setup_cache();
		if (!clist->is_blocked(height, 0, tx, ty, nlift, actor->get_type_flags(), 1)) {
			Tile_coord cur(tx + cx * c_tiles_per_chunk, ty + cy * c_tiles_per_chunk, nlift);
			if (cur.distance(av) < dist || dist == -1) {
				dist = cur.distance(av);
				cout << "(" << cur.tx << ", " << cur.ty << ") " << dist << endl;
				close = cur;
			}
		}
	}

	if (dist != -1) {
		actor->move(close);
		return Usecode_value(true);
	}

	return Usecode_value(false);
}
#endif

USECODE_INTRINSIC(approach_avatar) {
	ignore_unused_variable_warning(num_parms);
	// Approach_avatar(npc, ?, ?).
	// In the original, this intrinsic seems to create 'npc' off-screen, while
	// it approaches the avatar due to si_path_run_usecode or the 'TALK'
	// schedule. Need to investigate this further. Actor we want to move
	Actor* actor = as_actor(get_item(parms[0]));
	if (!actor || actor->is_dead()) {
		return Usecode_value(0);
	}
	// Guessing!! If already close...
	if (actor->distance(gwin->get_main_actor()) < 10) {
		return Usecode_value(1);
	}
	// Approach, and wait.
	if (!actor->approach_another(gwin->get_main_actor(), true)) {
		return Usecode_value(0);
	}
	return Usecode_value(1);
}

USECODE_INTRINSIC(set_barge_dir) {
	ignore_unused_variable_warning(num_parms);
	// set_barge_dir(barge, dir (0-7)).
	Game_object*  obj   = get_item(parms[0]);
	const int     dir   = parms[1].get_int_value();
	Barge_object* barge = obj ? obj->as_barge() : nullptr;
	if (barge) {
		barge->face_direction(dir);
	}
	return no_ret;
}

USECODE_INTRINSIC(telekenesis) {
	ignore_unused_variable_warning(num_parms);
	// telekenesis(fun#) - Save item for executing Usecode on.
	telekenesis_fun = parms[0].get_int_value();
	return no_ret;
}

USECODE_INTRINSIC(a_or_an) {
	ignore_unused_variable_warning(num_parms);
	// a_or_an (word)
	// return a/an depending on 'word'

	const char* str = parms[0].get_str_value();
	if (str && strchr("aeiouyAEIOUY", str[0]) == nullptr) {
		return Usecode_value("a");
	} else {
		return Usecode_value("an");
	}
}

USECODE_INTRINSIC(remove_from_area) {
	ignore_unused_variable_warning(num_parms);
	// Remove_from_area(shapenum, framenum, [x,y]from, [x,y]to).
	const int      shnum = parms[0].get_int_value();
	const int      frnum = parms[1].get_int_value();
	const int      fromx = parms[2].get_elem(0).get_int_value();
	const int      fromy = parms[2].get_elem(1).get_int_value();
	const int      tox   = parms[3].get_elem(0).get_int_value();
	const int      toy   = parms[3].get_elem(1).get_int_value();
	const TileRect area(fromx, fromy, tox - fromx + 1, toy - fromy + 1);
	if (area.w <= 0 || area.h <= 0) {
		return no_ret;
	}
	Game_object_vector vec;    // Find objects.
	Map_chunk::find_in_area(vec, area, shnum, frnum);
	// Remove them.
	for (auto* obj : vec) {
		gwin->add_dirty(obj);
		obj->remove_this();
	}
	return no_ret;
}

USECODE_INTRINSIC(set_light) {
	ignore_unused_variable_warning(num_parms);
	// set_light(npc, onoff)
	Game_object* light = get_item(parms[0]);
	if (!light) {
		return no_ret;
	}

	Actor* npc = as_actor(light->get_outermost());
	if (npc) {
		// This counts the light sources now too. This matches the originals;
		// torches and other light sources need it to be done this way, and
		// the other "obvious" manners fail.
		npc->refigure_gear();
		if (!parms[1].get_int_value()) {
			npc->remove_light_source(light);
		}
	}
	return no_ret;
}

USECODE_INTRINSIC(set_time_palette) {
	ignore_unused_variable_warning(num_parms, parms);
	// set_time_palette()
	gclock->reset_palette();
	return no_ret;
}

USECODE_INTRINSIC(ambient_light) {
	ignore_unused_variable_warning(num_parms);
	// ambient_light(onoff)
	// E.g., the cutscene with Batlin and Cantra.
	gwin->toggle_ambient_light(parms[0].get_int_value() == 0);
	gclock->set_palette();
	return no_ret;
}

USECODE_INTRINSIC(infravision) {
	ignore_unused_variable_warning(num_parms);
	// infravision(npc, onoff)
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc && npc->is_in_party()) {
		gwin->toggle_infravision(parms[1].get_int_value() != 0);
		gclock->set_palette();
	}
	return no_ret;
}

// parms[0] = quality of key to be added
USECODE_INTRINSIC(add_to_keyring) {
	ignore_unused_variable_warning(num_parms);
	getKeyring()->addkey(parms[0].get_int_value());

	return no_ret;
}

// parms[0] = quality of key to check
// returns true if key is on keyring

USECODE_INTRINSIC(is_on_keyring) {
	ignore_unused_variable_warning(num_parms);
	if (getKeyring()->checkkey(parms[0].get_int_value())) {
		return Usecode_value(true);
	} else {
		return Usecode_value(false);
	}
}

// parms[0] = quality of key to be removed
USECODE_INTRINSIC(remove_from_keyring) {
	ignore_unused_variable_warning(num_parms);
	const bool ret = getKeyring()->removekey(parms[0].get_int_value());
	return Usecode_value(ret);
}

USECODE_INTRINSIC(save_pos) {
	ignore_unused_variable_warning(num_parms);
	// save_pos(item).
	Game_object* item = get_item(parms[0]);
	if (item) {
		saved_pos = item->get_tile();
		saved_map = gwin->get_map()->get_num();
	}
	return no_ret;
}

USECODE_INTRINSIC(teleport_to_saved_pos) {
	// teleport_to_saved_pos(actor).  Only supported for Avatar for now.
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc == gwin->get_main_actor()) {
		// Bad value?
		if (saved_pos.tx < 0 || saved_pos.tx >= c_num_tiles) {
			// Fix old games.  Send to Monitor.
			saved_pos = Tile_coord(719, 2608, 1);
		}
		gwin->teleport_party(
				saved_pos, false, saved_map,
				num_parms > 1 ? parms[1].get_int_value() : false);
	}
	return no_ret;
}

USECODE_INTRINSIC(get_item_weight) {
	ignore_unused_variable_warning(num_parms);
	Game_object* obj = get_item(parms[0]);
	if (!obj) {
		return Usecode_value(0);
	} else {
		return Usecode_value(obj->get_weight());
	}
}

USECODE_INTRINSIC(get_skin_colour) {
	ignore_unused_variable_warning(num_parms, parms);
	// Gets skin colour of avatar. 0 (wh), 1 (br) or 2 (bl)
	Main_actor* av = gwin->get_main_actor();
	return Usecode_value(av->get_skin_color());
}

/*
 *  This is like the C version, but only '%s' is supported, and all the
 *  parms should be packed into one array.
 *  Added for Exult.
 */
USECODE_INTRINSIC(printf) {
	ignore_unused_variable_warning(num_parms);
	Usecode_value ret("");
	const char*   fmt = parms[0].get_elem0().get_str_value();
	int           count;
	cout << endl;    // For now...
	if (!fmt || (count = parms[0].get_array_size()) <= 1) {
		parms[0].print(cout);
		return ret;
	}
	int i = 1;    // Parm. #.
	while (*fmt) {
		const char* spec = strchr(fmt, '%');
		if (!spec) {
			spec = fmt + std::strlen(fmt);
		}
		cout.write(fmt, spec - fmt);
		if (*spec == '%') {
			if (spec[1] == 's') {
				const Usecode_value p
						= i < count ? parms[0][i] : Usecode_value(0);
				if (p.get_type() == Usecode_value::int_type) {
					cout << p.get_int_value();
				} else {
					p.print(cout);
				}
				spec += 2;
				i++;
			} else {
				cout << '%';
				spec++;
			}
		}
		fmt = spec;
	}
	return ret;
}

USECODE_INTRINSIC(begin_casting_mode) {
	Actor* npc = as_actor(get_item(parms[0]));
	if (npc) {
		// Have custom casting frames been specified?
		// TODO: Need to de-hard-code.
		const int cframes = num_parms > 1 ? parms[1].need_int_value() : 859;
		npc->begin_casting(cframes);
	}
	return no_ret;
}

USECODE_INTRINSIC(get_usecode_fun) {
	ignore_unused_variable_warning(num_parms);
	Game_object* obj = get_item(parms[0]);
	if (!obj) {
		return Usecode_value(0);
	}
	return Usecode_value(obj->get_usecode());
}

USECODE_INTRINSIC(set_usecode_fun) {
	ignore_unused_variable_warning(num_parms);
	Game_object* obj = get_item(parms[0]);
	if (!obj) {
		return no_ret;
	}
	const int       usefun = parms[1].get_int_value();
	Usecode_symbol* ucsym  = symtbl ? (*symtbl)[usefun] : nullptr;
	obj->set_usecode(usefun, ucsym ? ucsym->get_name() : nullptr);
	return no_ret;
}

USECODE_INTRINSIC(get_map_num) {
	ignore_unused_variable_warning(num_parms);
	Game_object* obj = get_item(parms[0]);
	if (!obj) {
		return Usecode_value(-1);
	}
	return Usecode_value(obj->get_map_num());
}

USECODE_INTRINSIC(is_dest_reachable) {
	ignore_unused_variable_warning(num_parms);
	Usecode_value ret(0);
	Actor*        npc = as_actor(get_item(parms[0]));
	if (!npc || parms[1].get_array_size() < 2) {
		return ret;
	}
	const Tile_coord dest = Tile_coord(
			parms[1].get_elem(0).get_int_value(),
			parms[1].get_elem(1).get_int_value(),
			parms[1].get_array_size() == 2
					? 0
					: parms[1].get_elem(2).get_int_value());
	ret = Usecode_value(is_dest_reachable(npc, dest));
	return ret;
}

USECODE_INTRINSIC(sib_is_dest_reachable) {
	// Note: this function did  not work in SI Beta. This implementation is
	// based on what usecode expects.
	ignore_unused_variable_warning(num_parms);
	Usecode_value ret(0);
	Actor*        npc = as_actor(get_item(parms[1]));
	if (!npc || parms[0].get_array_size() < 2) {
		return ret;
	}
	const Tile_coord dest = Tile_coord(
			parms[0].get_elem(0).get_int_value(),
			parms[0].get_elem(1).get_int_value(),
			parms[0].get_array_size() == 2
					? 0
					: parms[0].get_elem(2).get_int_value());
	ret = Usecode_value(is_dest_reachable(npc, dest));
	return ret;
}

USECODE_INTRINSIC(can_avatar_reach_pos) {
	ignore_unused_variable_warning(num_parms);
	Usecode_value ret(0);
	if (parms[0].get_array_size() < 2) {
		return ret;
	}
	const Tile_coord dest = Tile_coord(
			parms[0].get_elem(0).get_int_value(),
			parms[0].get_elem(1).get_int_value(),
			parms[0].get_array_size() == 2
					? 0
					: parms[0].get_elem(2).get_int_value());
	ret = Usecode_value(is_dest_reachable(gwin->get_main_actor(), dest));
	return ret;
}

USECODE_INTRINSIC(create_barge_object) {
	// create_barge_object (width, height, dir(0-7)).   Stores it in
	// 'last_created'.
	if (num_parms < 2) {
		return Usecode_value(0);
	}

	auto b = std::make_shared<Barge_object>(
			961, 0, 0, 0, 0, parms[0].get_int_value(), parms[1].get_int_value(),
			num_parms >= 3 ? ((parms[2].get_int_value() >> 1) & 3) : 0);
	b->set_invalid();    // Not in world yet.
	b->set_flag(Obj_flags::okay_to_take);
	last_created.push_back(b);

	Usecode_value u(b);
	return u;
}

USECODE_INTRINSIC(in_usecode_path) {
	ignore_unused_variable_warning(num_parms);
	// in_usecode_path (npc).   Returns true if actor is in a usecode path.

	Actor* npc = as_actor(get_item(parms[0]));
	if (!npc) {
		return Usecode_value(0);
	}

	if (npc->get_action() && npc->get_action()->as_usecode_path()) {
		return Usecode_value(1);
	}

	return Usecode_value(0);
}
