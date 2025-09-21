/*
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "ucsched.h"

#include "Audio.h"
#include "actors.h"
#include "barge.h"
#include "databuf.h"
#include "effects.h"
#include "egg.h"
#include "frameseq.h"
#include "game.h"
#include "gameclk.h"
#include "gamewin.h"
#include "headers/ios_state.hpp"
#include "ucinternal.h"
#include "ucscriptop.h"
#include "useval.h"

#include <iomanip>
#include <iostream>

using std::cout;
using std::endl;
using std::hex;
using std::setfill;
using std::setw;

using namespace Ucscript;

extern bool intrinsic_trace;

int             Usecode_script::count = 0;
Usecode_script* Usecode_script::first = nullptr;

/*
 *  Create for a 'restore'.
 */

Usecode_script::Usecode_script(
		Game_object* item, Usecode_value* cd, int findex, int nhalt, int del)
		: next(nullptr), prev(nullptr), obj(weak_from_obj(item)), code(cd),
		  i(0), frame_index(findex), started(false), no_halt(nhalt != 0),
		  must_finish(false), killed_barks(false), delay(del) {
	cnt = code->get_array_size();
}

/*
 *  Create.
 */

Usecode_script::Usecode_script(
		Game_object*   o,
		Usecode_value* cd    // May be nullptr for empty script.
		)
		: next(nullptr), prev(nullptr), obj(weak_from_obj(o)), code(cd), cnt(0),
		  i(0), frame_index(0), started(false), no_halt(false),
		  must_finish(false), killed_barks(false), delay(0) {
	if (!code) {    // Empty?
		code = new Usecode_value(0, nullptr);
	} else {
		cnt = code->get_array_size();
		if (!cnt) {    // Not an array??  (This happens.)
			// Create with single element.
			code = new Usecode_value(1, code);
			cnt  = 1;
		}
	}
}

/*
 *  Delete.
 */

Usecode_script::~Usecode_script() {
	delete code;
	if (!started) {
		return;
	}
	count--;
	if (next) {
		next->prev = prev;
	}
	if (prev) {
		prev->next = next;
	} else {
		first = next;
	}
}

/*
 *  Enter into the time-queue and our own chain.  Terminate existing
 *  scripts for this object unless 'dont_halt' is set.
 */

void Usecode_script::start(long d    // Start after this many msecs.
) {
	Game_window* gwin = Game_window::get_instance();
	const int    cnt  = code->get_array_size();    // Check initial elems.
	for (int i = 0; i < cnt; i++) {
		const int opval0 = code->get_elem(i).get_int_value();
		if (opval0 == Ucscript::dont_halt) {
			no_halt = true;
		} else if (opval0 == Ucscript::finish) {
			must_finish = true;
		} else {
			break;
		}
	}
	if (!is_no_halt()) {    // If flag not set,
							// Remove other entries that aren't
							//   'no_halt'.
		const Game_object_shared o = obj.lock();
		if (o) {
			Usecode_script::terminate(o.get());
		}
	}
	count++;         // Keep track of total.
	next = first;    // Put in chain.
	prev = nullptr;
	if (first) {
		first->prev = this;
	}
	first   = this;
	started = true;
	//++++ Messes up Moonshade Trial.
	//	gwin->get_tqueue()->add(d + Game::get_ticks(), this,
	// gwin->get_usecode());
	gwin->get_tqueue()->add(d + SDL_GetTicks(), this, gwin->get_usecode());
}

/*
 *  Set this script to halt.
 */

void Usecode_script::halt() {
	if (!no_halt) {
		i = cnt;
	}
}

/*
 *  Append instructions.
 */
void Usecode_script::add(int v1) {
	code->append(&v1, 1);
	cnt++;
}

void Usecode_script::add(int v1, int v2) {
	int vals[2];
	vals[0] = v1;
	vals[1] = v2;
	code->append(vals, 2);
	cnt += 2;
}

void Usecode_script::add(int v1, std::string str) {
	const int sz = code->get_array_size();
	code->resize(sz + 2);
	(*code)[sz]     = v1;
	(*code)[sz + 1] = std::move(str);
	cnt += 2;
}

void Usecode_script::add(int* vals, int c) {
	code->append(vals, cnt);
	cnt += c;
}

/*
 *  Search list for one for a given item.
 *
 *  Output: ->Usecode_script if found, else nullptr.
 */

Usecode_script* Usecode_script::find(
		const Game_object* srch,
		Usecode_script*    last_found    // Find next after this.
) {
	Usecode_script* start = last_found ? last_found->next : first;
	for (Usecode_script* each = start; each; each = each->next) {
		const Game_object_shared obj = each->obj.lock();
		if (obj.get() == srch) {
			return each;    // Found it.
		}
	}
	return nullptr;
}

/*
 *  Search list for one for a given item.
 *
 *  Output: ->Usecode_script if found, else nullptr.
 */

Usecode_script* Usecode_script::find_active(
		const Game_object* srch,
		Usecode_script*    last_found    // Find next after this.
) {
	Usecode_script* start = last_found ? last_found->next : first;
	for (Usecode_script* each = start; each; each = each->next) {
		const Game_object_shared obj = each->obj.lock();
		if (obj.get() == srch && each->is_activated()) {
			return each;    // Found it.
		}
	}
	return nullptr;
}

/*
 *  Terminate all scripts for a given object.
 */

void Usecode_script::terminate(const Game_object* obj) {
	Usecode_script* next = nullptr;
	for (Usecode_script* each = first; each; each = next) {
		next = each->next;    // Get next in case we delete 'each'.
		const Game_object_shared each_obj = each->obj.lock();
		if (each_obj.get() == obj) {
			each->halt();
		}
	}
}

/*
 *  Remove all from global list (assuming they've already been cleared
 *  from the time queue).
 */

void Usecode_script::clear() {
	while (first) {
		delete first;
	}
}

/*
 *  Terminate all scripts for objects that are more than a given distance
 *  from a particular spot.
 */

void Usecode_script::purge(
		const Tile_coord& spot,
		int               dist    // In tiles.
) {
	Usecode_script* next = nullptr;
	Game_window*    gwin = Game_window::get_instance();
	auto* usecode        = static_cast<Usecode_internal*>(gwin->get_usecode());
	for (Usecode_script* each = first; each; each = next) {
		next = each->next;    // Get next in case we delete 'each'.
							  // Only purge if not yet started.
		const Game_object_shared o = each->obj.lock();
		if (o && !each->i && o->get_outermost()->distance(spot) > dist) {
			// Force it to halt.
			each->no_halt = false;
			if (each->must_finish) {
				cout << "MUST finish this script" << endl;
				each->exec(usecode, true);
			}
			each->halt();
		}
	}
}

inline void Usecode_script::activate_egg(
		Usecode_internal* usecode, Game_object* e) {
	ignore_unused_variable_warning(usecode);
	if (!e || !e->is_egg()) {
		return;
	}
	Egg_object* egg  = e->as_egg();
	const int   type = egg->get_type();
	// Guess:  Only certain types:
	if (type == Egg_object::monster || type == Egg_object::button
		|| type == Egg_object::missile) {
		egg->hatch(Usecode_internal::gwin->get_main_actor(), true);
	}
}

/*
 *  Execute an array of usecode, generally one instruction per tick.
 */

void Usecode_script::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata       // ->usecode machine.
) {
	const Game_object_shared o   = obj.lock();
	Actor*                   act = o ? o->as_actor() : nullptr;
	if (act && act->get_casting_mode() == Actor::init_casting) {
		act->display_casting_frames();
	}
	auto* usecode = reinterpret_cast<Usecode_internal*>(udata);
#ifdef DEBUG
	if (intrinsic_trace) {
		cout << "Executing script (" << i << ":" << cnt << ") for " << o
			 << ", time: " << curtime << endl;
	}
#endif
	const int delay = exec(usecode, false);
	if (i < cnt) {    // More to do?
		Usecode_internal::gwin->get_tqueue()->add(curtime + delay, this, udata);
		return;
	}
	if (act && act->get_casting_mode() == Actor::show_casting_frames) {
		act->end_casting_mode(delay);
	}
#ifdef DEBUG
	if (intrinsic_trace) {
		cout << "Ending script for " << o << endl;
	}
#endif
	delete this;    // Hope this is safe.
}

static inline bool IsActorNear(Actor* avatar, Game_object* obj, int maxdist) {
	return obj->get_tile().distance_2d(avatar->get_tile()) <= maxdist
		   && (obj->get_info().get_shape_class() != Shape_info::hatchable
			   || obj->get_lift() == avatar->get_lift());
}

/*
 *  Execute an array of usecode, generally one instruction per tick.
 *
 *  Output: Delay for next execution.
 */

int Usecode_script::exec(
		Usecode_internal* usecode,
		bool              finish    // If set, keep going to end.
) {
	Game_window* gwin  = Usecode_internal::gwin;
	int          delay = gwin->get_std_delay();    // Start with default delay.
	bool         do_another = true;                // Flag to keep going.
	int          opcode;
	// If a 1 follows, keep going.
	for (;
		 i < cnt
		 && ((opcode = code->get_elem(i).get_int_value()) == 0x1 || do_another);
		 i++) {
		const Game_object_shared optr = obj.lock();
		if (!optr) {
			i = cnt;
			return delay;
		}
		do_another = finish;
		switch (opcode) {
		case cont:    // Means keep going without painting.
			do_another = true;
			gwin->set_painted();    // Want to paint when done.
			break;
		case reset:
			if (!finish) {    // Appears to be right.
				i = -1;       // Matches originals.
			}
			break;
		case repeat:       // Loop(offset, count).
		case repeat2: {    // Loop(offset, count, reset).
			// repeat:
			// Use 'count' as loop var. When it reaches 0, the opcode won't
			// loop again.
			// repeat2:
			// Use 'count' as loop var. When it reaches 0, reset it to 'reset'.
			// This is necessary for loop nesting.
			// (used in mining machine, orb of the moons)

			do_another                 = true;
			Usecode_value& cntval      = code->get_elem(i + 2);
			const int      num_repeats = cntval.get_int_value();
			if (num_repeats <= 0) {
				// Done.
				if (opcode == repeat2) {
					cntval = code->get_elem(i + 3);    // restore counter
					i += 3;
				} else {
					i += 2;
				}
			} else {
				// A count of 255 means infinite loop.
				if (num_repeats != 255) {
					// Decr. and loop.
					cntval = Usecode_value(num_repeats - 1);
				}
				const Usecode_value& offval = code->get_elem(i + 1);
				i += offval.get_int_value() - 1;
				if (i < -1) {    // Before start?
					i = -1;
				}
			}
			break;
		}
		case Ucscript::wait_while_near: {
			const int dist = code->get_elem(++i).get_int_value();
			if (!finish
				&& IsActorNear(gwin->get_main_actor(), optr.get(), dist)) {
				i -= 2;    // Stay in this opcode.
			}
			break;
		}
		case Ucscript::wait_while_far: {
			const int dist = code->get_elem(++i).get_int_value();
			if (!finish
				&& !IsActorNear(gwin->get_main_actor(), optr.get(), dist)) {
				i -= 2;    // Stay in this opcode.
			}
			break;
		}
		case Ucscript::nop1:
		case Ucscript::nop2:
			// Just a nop.
			break;
		case Ucscript::finish:    // Flag to finish if deleted.
			must_finish = true;
			do_another  = true;
			break;
		case dont_halt:
			// Seems to mean 'don't let intrinsic 5c halt it' as
			// well as 'allow actor to move during script'
			no_halt    = true;
			do_another = true;
			break;
		case delay_ticks: {    // 1 parm.
							   //   delay before next instruction.
			const Usecode_value& delayval = code->get_elem(++i);
			// It's # of ticks.
			Actor* act = usecode->as_actor(optr.get());
			if (act) {
				act->clear_rest_time();
			}
			delay = delay * delayval.get_int_value();
			break;
		}
		case delay_minutes: {    // 1 parm., game minutes.
			const Usecode_value& delayval = code->get_elem(++i);
			// Convert to real miliseconds.
			delay = delay * ticks_per_minute * delayval.get_int_value();
			break;
		}
		case delay_hours: {    // 1 parm., game hours.
			const Usecode_value& delayval = code->get_elem(++i);
			// Convert to real miliseconds.
			delay = delay * 60 * ticks_per_minute * delayval.get_int_value();
			break;
		}
		case Ucscript::remove:    // Remove obj.
			usecode->remove_item(optr.get());
			break;
		case rise: {    // (For flying carpet.
			Tile_coord t = optr->get_tile();
			if (t.tz < 10) {
				t.tz++;
			}
			optr->move(t);
			break;
		}
		case descend: {
			Tile_coord t = optr->get_tile();
			if (t.tz > 0) {
				t.tz--;
			}
			optr->move(t);
			break;
		}
		case frame:    // Set frame.
			usecode->set_item_frame(
					optr.get(), code->get_elem(++i).get_int_value());
			break;
		case egg:    // Guessing:  activate egg.
			activate_egg(usecode, optr.get());
			break;
		case set_egg: {    // Set_egg(criteria, dist).
			const int   crit = code->get_elem(++i).get_int_value();
			const int   dist = code->get_elem(++i).get_int_value();
			Egg_object* egg  = optr->as_egg();
			if (egg) {
				egg->set(crit, dist);
			}
			break;
		}
		case next_frame_max: {    // Stop at last frame.
			const int nframes = optr->get_num_frames();
			if (optr->get_framenum() % 32 < nframes - 1) {
				usecode->set_item_frame(optr.get(), 1 + optr->get_framenum());
			}
			break;
		}
		case next_frame: {
			const int nframes = optr->get_num_frames();
			if (nframes > 0) {
				usecode->set_item_frame(
						optr.get(), (1 + optr->get_framenum()) % nframes);
			}
			break;
		}
		case prev_frame_min:
			if (optr->get_framenum() > 0) {
				usecode->set_item_frame(optr.get(), optr->get_framenum() - 1);
			}
			break;
		case prev_frame: {
			const int nframes = optr->get_num_frames();
			if (nframes > 0) {
				const int pframe = optr->get_framenum() - 1;
				usecode->set_item_frame(
						optr.get(), (pframe + nframes) % nframes);
			}
			break;
		}
		case say: {    // Say string.
			Usecode_value& strval = code->get_elem(++i);
			Usecode_value  objval(optr.get());
			if (!killed_barks) {
				usecode->item_say(objval, strval);
			}
			break;
		}
		case Ucscript::step: {    // Parm. is dir. (0-7).  0=north.
			// Get dir.
			const int val = code->get_elem(++i).get_int_value();
			// Height change (verified).
			++i;
			const int dz = size_t(i) < code->get_array_size()
								   ? code->get_elem(i).get_int_value()
								   : 0;
			// Watch for buggy SI usecode!
			const int destz = optr->get_lift() + dz;
			if (destz < 0 || dz > 15 || dz < -15) {
				// Here, the originals would flash the step frame,
				// but not step or change height. Not worth emulating.
				// I am also allowing a high limit to height change.
				do_another = true;
				break;
			}
			// It may be 0x3x.
			step(usecode, val >= 0 ? val & 7 : -1, dz);
			break;
		}
		case music: {    // Unknown.
			const Usecode_value& val  = code->get_elem(++i);
			int                  song = val.get_int_value();
			// Verified.
			bool continuous = false;
			if (song < 0 || song > 0xff) {
				// Supporting this because it works in the original. But this
				// is wrong. Supporting this also keeps compatibility with older
				// versions of UCC that did this.
				continuous = (song >> 8) != 0;
				song &= 0xff;
			} else {
				++i;
				const bool in_range = size_t(i) < code->get_array_size();
				const int  value
						= in_range ? code->get_elem(i).get_int_value() : 0;
				if (in_range && (value == 0 || value == 1)) {
					continuous = value != 0;
				} else {
					// Put the element back. Works around some buggy SI usecode.
					--i;
				}
			}
			Audio::get_ptr()->start_music(song, continuous);
			break;
		}
		case Ucscript::usecode: {    // Call?
			const Usecode_value& val = code->get_elem(++i);
			int                  fun = val.get_int_value();
			// Functions 0xff or less have an extra zero element after in the
			// original usecode. I modified UCC to match this as well, but
			// we need to support old versions of UCC.
			if (fun >= 0 && fun < 0x100) {
				++i;
				const bool in_range = size_t(i) < code->get_array_size();
				const int  value
						= in_range ? code->get_elem(i).get_int_value() : 0;
				if (!in_range || value != 0) {
					// If this is not a 0, or not in range, then lets put the
					// element back.
					--i;
				}
			}
			// HACK: Prevent function 0x6fb in FoV from triggering while
			// the screen is faded out, as it can cause the screen to
			// remain faded due to eliminating the script that does the
			// fade in.
			if (GAME_BG && fun == 0x6fb && gwin->get_pal()->is_faded_out()) {
				// Simply put back the opcode for later.
				i -= 2;
				break;
			}
			// Watch for eggs:
			Usecode_internal::Usecode_events ev
					= Usecode_internal::internal_exec;
			if (optr
				&& optr->is_egg()
				// Fixes the Blacksword's 'Fire' power in BG:
				&& optr->as_egg()->get_type() < Egg_object::fire_field) {
				ev = Usecode_internal::egg_proximity;
			}
			// And for telekenesis spell fun:
			else if (fun == usecode->telekenesis_fun) {
				ev                       = Usecode_internal::double_click;
				usecode->telekenesis_fun = -1;
			}
			usecode->call_usecode(fun, optr.get(), ev);
			break;
		}
		case Ucscript::usecode2: {    // Call(fun, eventid).
			const Usecode_value& val  = code->get_elem(++i);
			const int            evid = code->get_elem(++i).get_int_value();
			usecode->call_usecode(
					val.get_int_value(), optr.get(),
					static_cast<Usecode_internal::Usecode_events>(evid));
			break;
		}
		case speech: {    // Play speech track.
			const Usecode_value& val   = code->get_elem(++i);
			const int            track = val.get_int_value();
			if (track >= 0) {
				Audio::get_ptr()->start_speech(track);
			}
			break;
		}
		case sfx: {    // Play sound effect!
			const Usecode_value& val    = code->get_elem(++i);
			const int            sfx_id = val.get_int_value();
			// Skip if this exact SFX is already playing
			if (!Audio::get_ptr()->is_sfx_playing(sfx_id)) {
				Audio::get_ptr()->play_sound_effect(sfx_id, optr.get());
			}
			break;
		}
		case face_dir: {    // Parm. is dir. (0-7).  0=north.
							// Look in that dir.
			const Usecode_value& val = code->get_elem(++i);
			// It may be 0x3x.  Face dir?
			const int dir = val.get_int_value() & 7;
			Actor*    npc = optr->as_actor();
			if (npc) {
				npc->set_usecode_dir(dir);
			}
			usecode->set_item_frame(
					optr.get(),
					optr->get_dir_framenum(dir, optr->get_framenum()), 1, 1);
			frame_index = 0;    // Reset walking frame index.
			break;
		}
		case weather: {
			// Set weather to that type.
			const Usecode_value& val  = code->get_elem(++i);
			const int            type = val.get_int_value() & 0xff;
			// Seems to match the originals:
			if (type == 0xff || gwin->get_effects()->get_weather() != 0) {
				Egg_object::set_weather(type == 0xff ? 0 : type);
			}
			break;
		}
		case hit: {    // Hit(hps, type).
			const Usecode_value hps  = code->get_elem(++i);
			const Usecode_value type = code->get_elem(++i);
			optr->reduce_health(hps.get_int_value(), type.get_int_value());
			break;
		}
		case attack: {    // Finish 'set_to_attack()'.
			Actor* act = optr->as_actor();
			if (act) {
				act->usecode_attack();
			}
			break;
		}
		case resurrect: {
			auto* body = optr->as_body();
			if (!body) {
				break;
			}
			Actor* act = gwin->get_npc(body->get_live_npc_num());
			if (act) {
				act->resurrect(body);
			}
			break;
		}
		default:
			// Frames with dir.  U7-verified!
			if (opcode >= 0x61 && opcode <= 0x70) {
				// But don't show empty frames.
				// Get the actor's actual facing:
				const int v = (optr->get_framenum() & 48) | (opcode - 0x61);
				usecode->set_item_frame(optr.get(), v, 1, 1);
			} else if (opcode >= 0x30 && opcode < 0x38) {
				// Step in dir. opcode&7.
				step(usecode, opcode & 7, 0);
				do_another = true;    // Guessing.
			} else {
				const boost::io::ios_flags_saver flags(cout);
				const boost::io::ios_fill_saver  fill(cout);
				cout << "Und sched. opcode " << hex << "0x" << setfill('0')
					 << setw(2) << opcode << endl;
			}
			break;
		}
	}
	return delay;
}

/*
 *  Step in given direction.
 */

void Usecode_script::step(
		Usecode_internal* usecode,
		int               dir,    // 0-7.
		int               dz) {
	const Game_object_shared optr = obj.lock();
	Actor*                   act  = usecode->as_actor(optr.get());
	if (act) {
		Tile_coord tile = optr->get_tile();
		if (dir != -1) {
			tile = tile.get_neighbor(dir);
		}
		tile.tz += dz;
		act->clear_rest_time();
		Frames_sequence* frames = act->get_frames(dir);
		// Get frame (updates frame_index).
		const int frame = frames->get_next(frame_index);
		if (tile.tz < 0) {
			tile.tz = 0;
		}
		optr->step(tile, frame, true);
	} else if (optr && optr->as_barge() != nullptr) {
		for (int i = 0; i < 4; i++) {
			Tile_coord t = optr->get_tile();
			if (dir != -1) {
				t = t.get_neighbor(dir);
			}
			t.tz += dz / 4 + (!i ? dz % 4 : 0);
			if (t.tz < 0) {
				t.tz = 0;
			}
			optr->step(t, 0, true);
		}
	}
}

/*
 *  Save (serialize).
 *
 *  Output: Length written, or -1 if error.
 */

int Usecode_script::save(ODataSource* out) const {
	// Get delay to when due.
	const long when = Game_window::get_instance()->get_tqueue()->find_delay(
			this, SDL_GetTicks());
	if (when < 0) {
		return -1;
	}
	out->write2(cnt);    // # of values we'll store.
	out->write2(i);      // Current spot.
	for (int j = 0; j < cnt; j++) {
		Usecode_value& val = code->get_elem(j);
		if (!val.save(out)) {
			return -1;
		}
	}
	out->write2(frame_index);
	out->write2(no_halt ? 1 : 0);
	out->write4(when);
	return out->getSize();
}

/*
 *  Restore (serialize).
 *
 *  Output: ->entry, which is also stored in our global chain, but is NOT
 *      added to the time queue yet.
 */

Usecode_script* Usecode_script::restore(
		Game_object* item,    // Object this is executed for.
		IDataSource* in) {
	const int cnt      = in->read2();    // Get # instructions.
	const int curindex = in->read2();    // Where it is.
	// Create empty array.
	auto* code = new Usecode_value(cnt, nullptr);
	for (int i = 0; i < cnt; i++) {
		Usecode_value& val = code->get_elem(i);
		if (!val.restore(in)) {
			delete code;
			return nullptr;
		}
	}
	if (in->getAvail() < 8) {    // Enough room left?
		delete code;
		return nullptr;
	}
	const int frame_index = in->read2();
	const int no_halt     = in->read2();
	const int delay       = in->read4();
	auto*     scr = new Usecode_script(item, code, frame_index, no_halt, delay);
	scr->i        = curindex;    // Set index.
	return scr;
}

/*
 *  Print for debugging.
 */

void Usecode_script::print(std::ostream& out) const {
	const boost::io::ios_flags_saver flags(out);
	const boost::io::ios_fill_saver  fill(out);
	out << hex << "Obj = 0x" << setfill('0') << setw(2) << obj.lock().get()
		<< ": "
		   "(";
	for (int i = 0; i < cnt; i++) {
		if (i > 0) {
			out << ", ";
		}
		code->get_elem(i).print(out);
	}
	out << ") = ";
}
